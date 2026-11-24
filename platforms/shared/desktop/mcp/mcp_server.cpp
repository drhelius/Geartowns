/*
 * Geartowns - FM Towns Emulator
 * Copyright (C) 2026  Ignacio Sanchez

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see http://www.gnu.org/licenses/
 *
 */

#include "mcp_server.h"
#include "../utils.h"
#include <sstream>
#include <iomanip>
#include <fstream>
#include <stdlib.h>
#include <ctype.h>
#include "common/log.h"

bool g_mcp_router_enabled = false;

static bool parse_trace_filter(const std::string& filter, u32* flags, u32* event_filters)
{
    std::string group = filter + ".";
    bool found = false;

    for (int i = 0; i < k_mcp_trace_filter_count; i++)
    {
        const McpTraceFilter& trace_filter = k_mcp_trace_filters[i];
        std::string name = trace_filter.name;

        if ((filter != name) && (name.compare(0, group.size(), group) != 0))
            continue;

        *flags = SET_BIT(*flags, trace_filter.type);
        event_filters[trace_filter.type] |= trace_filter.mask;
        found = true;
    }

    return found;
}

#define MCP_ADDRESS_DESCRIPTION "Linear hex, SR:offset or SSSS:offset."

void McpServer::ReaderLoop()
{
    while (m_running.load())
    {
        std::string line;

        if (m_transport->recv(line))
        {
            if (!line.empty())
            {
                HandleLine(line);
            }
        }
        else
        {
            m_running.store(false);
            m_responseQueue.Stop();
            break;
        }
    }
}

void McpServer::Run()
{
    while (m_running.load())
    {
        DebugResponse* resp = m_responseQueue.WaitAndPop();

        if (resp == NULL)
            break;

        if (resp->isError)
        {
            SendError(resp->requestId, resp->errorCode, resp->errorMessage);
        }
        else
        {
            json mcpResult;
            mcpResult["content"] = json::array();

            if (resp->result.contains("__mcp_image") && resp->result["__mcp_image"] == true)
            {
                mcpResult["content"].push_back({
                    {"type", "image"},
                    {"data", resp->result["data"]},
                    {"mimeType", resp->result["mimeType"]}
                });
            }
            else
            {
                std::ostringstream result_ss;
                result_ss << resp->result.dump(2, ' ', false, json::error_handler_t::replace);

                mcpResult["content"].push_back({
                    {"type", "text"},
                    {"text", result_ss.str()}
                });
            }

            json response;
            response["jsonrpc"] = "2.0";
            response["id"] = resp->requestId;
            mcpResult["isError"] = resp->isToolError;
            response["result"] = mcpResult;

            SendResponse(response);
        }

        SafeDelete(resp);
        m_commandQueue.Complete();
    }
}

void McpServer::HandleLine(const std::string& line)
{
    json request;

    if (!json::accept(line))
    {
        if (!m_transport->validate_protocol_version(""))
            return;

        SendError(json(), MCP_ERROR_PARSE, "Parse error: Invalid JSON");
        return;
    }

    request = json::parse(line);

    if (!request.is_object())
    {
        if (!m_transport->validate_protocol_version(""))
            return;

        SendError(json(), MCP_ERROR_INVALID_REQUEST, "Invalid Request: expected an object");
        return;
    }

    std::string method;

    if (request.contains("method") && request["method"].is_string())
        method = request["method"];

    if (!m_transport->validate_protocol_version(method))
        return;

    bool is_notification = !request.contains("id");

    if (request.contains("id") && !request["id"].is_string() &&
        !request["id"].is_number_integer() && !request["id"].is_number_unsigned())
    {
        RejectOrSendError(is_notification, json(), MCP_ERROR_INVALID_REQUEST,
            "Invalid Request: id must be a string or integer");
        return;
    }

    json request_id = request.contains("id") ? request["id"] : json();

    if (!request.contains("jsonrpc") || request["jsonrpc"] != "2.0")
    {
        RejectOrSendError(is_notification, json(), MCP_ERROR_INVALID_REQUEST,
            "Invalid Request: missing or invalid jsonrpc version");
        return;
    }

    if (!request.contains("method") || !request["method"].is_string())
    {
        RejectOrSendError(is_notification, json(), MCP_ERROR_INVALID_REQUEST,
            "Invalid Request: missing method");
        return;
    }

    method = request["method"];

    if (request.contains("params") && !request["params"].is_object())
    {
        RejectOrSendError(is_notification, request_id, MCP_ERROR_INVALID_PARAMS,
            "Invalid params: expected an object");
        return;
    }

    if (method == "initialize" && is_notification)
    {
        RejectOrSendError(is_notification, json(), MCP_ERROR_INVALID_REQUEST,
            "Initialize must be a request");
        return;
    }

    if (!m_initialized && method != "initialize" && method != "ping")
    {
        RejectOrSendError(is_notification, request_id, MCP_ERROR_INVALID_REQUEST,
            "Server not initialized");
        return;
    }

    if (is_notification)
    {
        m_transport->acknowledge_notification();
        return;
    }

    if (method == "initialize")
    {
        HandleInitialize(request);
    }
    else if (method == "ping")
    {
        json response;
        response["jsonrpc"] = "2.0";
        response["id"] = request_id;
        response["result"] = json::object();
        SendResponse(response);
    }
    else if (method == "tools/list")
    {
        HandleToolsList(request);
    }
    else if (method == "tools/call")
    {
        HandleToolsCall(request);
    }
    else if (method == "resources/list")
    {
        HandleResourcesList(request);
    }
    else if (method == "resources/templates/list")
    {
        HandleResourceTemplatesList(request);
    }
    else if (method == "resources/read")
    {
        HandleResourcesRead(request);
    }
    else
    {
        SendError(request_id, MCP_ERROR_METHOD_NOT_FOUND, "Method not found: " + method);
    }
}

static bool ValidateInitializeParams(const json& params, std::string& error)
{
    if (!params.contains("protocolVersion"))
    {
        error = "Missing required parameter 'protocolVersion'";
        return false;
    }

    if (!params["protocolVersion"].is_string())
    {
        error = "Parameter 'protocolVersion' must be a string";
        return false;
    }

    if (!params.contains("capabilities"))
    {
        error = "Missing required parameter 'capabilities'";
        return false;
    }

    if (!params["capabilities"].is_object())
    {
        error = "Parameter 'capabilities' must be an object";
        return false;
    }

    if (!params.contains("clientInfo"))
    {
        error = "Missing required parameter 'clientInfo'";
        return false;
    }

    if (!params["clientInfo"].is_object())
    {
        error = "Parameter 'clientInfo' must be an object";
        return false;
    }

    const json& client_info = params["clientInfo"];

    if (!client_info.contains("name"))
    {
        error = "Missing required parameter 'clientInfo.name'";
        return false;
    }

    if (!client_info["name"].is_string())
    {
        error = "Parameter 'clientInfo.name' must be a string";
        return false;
    }

    if (!client_info.contains("version"))
    {
        error = "Missing required parameter 'clientInfo.version'";
        return false;
    }

    if (!client_info["version"].is_string())
    {
        error = "Parameter 'clientInfo.version' must be a string";
        return false;
    }

    return true;
}

void McpServer::HandleInitialize(const json& request)
{
    const json& id = request["id"];

    if (!request.contains("params"))
    {
        SendError(id, MCP_ERROR_INVALID_PARAMS, "Invalid params: missing params");
        return;
    }

    std::string validation_error;

    if (!ValidateInitializeParams(request["params"], validation_error))
    {
        SendError(id, MCP_ERROR_INVALID_PARAMS, "Invalid params: " + validation_error);
        return;
    }

    std::string protocolVersion = MCP_PROTOCOL_VERSION;

    json response;
    response["jsonrpc"] = "2.0";
    response["id"] = id;
    response["result"] = {
        {"protocolVersion", protocolVersion},
        {"capabilities", {
            {"tools", json::object()},
            {"resources", json::object()}
        }},
        {"serverInfo", {
            {"name", "geartowns-mcp-server"},
            {"title", GT_TITLE " MCP Server"},
            {"description", "Debug/control " GT_TITLE " FM Towns: execution, breakpoints, memory, Intel 80386 CPU, "
                "descriptor tables, paging, disassembly, symbols, call stack, trace logger, profiler, system, video, "
                "audio, CD-ROM and floppy hardware, media, save states, rewind, input, keyboard, screenshots, video recording."},
            {"version", GT_VERSION}
        }}
    };

    response["result"]["instructions"] =
        "Use this server for FM Towns game and software debugging, reverse engineering, ROM hacking, translation, "
        "memory inspection, Intel 80386 debugging in real, protected and VM86 mode, execute, data, I/O and interrupt "
        "breakpoints, disassembly, symbols, trace logging, profiling, inspecting the video, audio, CD-ROM, floppy and "
        "system hardware, save states, rewind, pad, mouse and keyboard input, screenshots, and video recording. "
        "Addresses take linear hex (1234ABCD, 0x1234ABCD, $1234ABCD), SR:offset with a segment register (CS:1234) or "
        "SSSS:offset with a selector or real-mode segment (0008:00001234).";

    if (g_mcp_router_enabled)
    {
        response["result"]["instructions"] =
            response["result"]["instructions"].get<std::string>() +
            " The tool router is enabled. Common tools are directly callable. Other generic tools are routed: "
            "call search_tools to find a tool, call get_tool_info to obtain its exact input schema, then "
            "call execute_tool with the returned tool name and arguments. Never call a routed tool directly.";
    }

    m_initialized = true;
    m_transport->set_protocol_version(protocolVersion);
    SendResponse(response);
}

struct McpToolHints
{
    bool read_only;
    bool destructive;
    bool idempotent;
    bool open_world;
};

static const McpToolHints k_mcp_tool_read = { true, false, true, false };
static const McpToolHints k_mcp_tool_read_world = { true, false, true, true };
static const McpToolHints k_mcp_tool_destructive = { false, true, false, false };
static const McpToolHints k_mcp_tool_destructive_idempotent = { false, true, true, false };
static const McpToolHints k_mcp_tool_destructive_world = { false, true, false, true };
static const McpToolHints k_mcp_tool_destructive_idempotent_world = { false, true, true, true };
static const McpToolHints k_mcp_tool_write = { false, false, false, false };
static const McpToolHints k_mcp_tool_write_idempotent = { false, false, true, false };
static const McpToolHints k_mcp_tool_write_world = { false, false, false, true };
static const McpToolHints k_mcp_tool_write_idempotent_world = { false, false, true, true };

static json make_tool(const std::string& name, const std::string& title, const std::string& description,
    const McpToolHints& hints, const json& schema)
{
    return {
        {"name", name},
        {"title", title},
        {"description", description},
        {"annotations", {{"readOnlyHint", hints.read_only}, {"destructiveHint", hints.destructive},
            {"idempotentHint", hints.idempotent}, {"openWorldHint", hints.open_world}}},
        {"inputSchema", schema}
    };
}

static json empty_schema()
{
    return {
        {"type", "object"},
        {"properties", json::object()},
        {"additionalProperties", false}
    };
}

json McpServer::BuildToolList()
{
    json tools = json::array();

    // Execution control tools
    tools.push_back(make_tool("debug_pause", "Debug Pause",
        "Pause at the current instruction and enter the debugger.", k_mcp_tool_destructive_idempotent, empty_schema()));

    tools.push_back(make_tool("debug_continue", "Debug Continue",
        "Resume execution from a pause or breakpoint.", k_mcp_tool_destructive_idempotent, empty_schema()));

    tools.push_back(make_tool("debug_step_into", "Debug Step Into",
        "Step one instruction, entering calls and interrupts; a REP instruction runs all its elements.",
        k_mcp_tool_destructive, empty_schema()));

    tools.push_back(make_tool("debug_step_over", "Debug Step Over",
        "Step one instruction, running through CALLs, INT n handlers and IRQs.", k_mcp_tool_destructive,
        empty_schema()));

    tools.push_back(make_tool("debug_step_out", "Debug Step Out",
        "Run until the current subroutine or interrupt handler returns; with an empty call stack it steps "
        "one instruction.", k_mcp_tool_destructive, empty_schema()));

    tools.push_back(make_tool("debug_step_frame", "Debug Step Frame",
        "Run frames with breakpoints active, then pause. By default it answers when they finish or a "
        "breakpoint stops them, with the debug_get_status fields, frames run and breakpoint_hit.",
        k_mcp_tool_destructive,
        {
            {"type", "object"},
            {"properties", {
                {"frames", {
                    {"type", "integer"},
                    {"description", "Default 1, at most 1800 when waiting."},
                    {"minimum", 1},
                    {"maximum", 216000}
                }},
                {"wait", {
                    {"type", "boolean"},
                    {"description", "Default true. false answers at once; poll debug_get_status or debug_pause."}
                }}
            }},
            {"additionalProperties", false}
        }));

    tools.push_back(make_tool("debug_reset", "Debug Reset",
        "Reset the FM Towns; boot holds the system ROM's boot device keys through this reset.", k_mcp_tool_destructive,
        {
            {"type", "object"},
            {"properties", {
                {"boot", {
                    {"type", "string"},
                    {"description", "Boot device: C+D, F+0/1, H+0-4 or I+C+M held until the first CD, floppy or SCSI "
                        "read or IC memory card status read."},
                    {"enum", json::array({"cdrom", "floppy1", "floppy2", "hard_disk0", "hard_disk1", "hard_disk2",
                        "hard_disk3", "hard_disk4", "memory_card"})}
                }}
            }},
            {"additionalProperties", false}
        }));

    tools.push_back(make_tool("debug_get_status", "Debug Get Status",
        "Read the debugger state: paused, breakpoint hit, linear and logical PC, CPU mode, halted, media "
        "and frame.", k_mcp_tool_read, empty_schema()));

    tools.push_back(make_tool("debug_run_to_cursor", "Debug Run To Cursor",
        "Run until the CPU reaches an address.", k_mcp_tool_destructive,
        {
            {"type", "object"},
            {"properties", {
                {"address", {
                    {"type", "string"},
                    {"description", MCP_ADDRESS_DESCRIPTION}
                }}
            }},
            {"required", json::array({"address"})}
        }));

    tools.push_back(make_tool("toggle_fast_forward", "Toggle Fast Forward",
        "Turn fast forward on or off, optionally setting its speed.", k_mcp_tool_destructive_idempotent,
        {
            {"type", "object"},
            {"properties", {
                {"enabled", {
                    {"type", "boolean"},
                    {"description", "true turns it on."}
                }},
                {"speed", {
                    {"type", "integer"},
                    {"description", "0=1.5x, 1=2x, 2=2.5x, 3=3x, 4=unlimited. Omit to keep the current speed."},
                    {"minimum", 0},
                    {"maximum", 4}
                }}
            }},
            {"required", json::array({"enabled"})}
        }));

    // Breakpoint tools
    json breakpoint_type = {
        {"type", "string"},
        {"description", "Default execute for linear, read for physical and io; access is read or write."},
        {"enum", json::array({"execute", "read", "write", "access"})}
    };

    json breakpoint_space = {
        {"type", "string"},
        {"description", "Default linear."},
        {"enum", json::array({"linear", "physical", "io"})}
    };

    tools.push_back(make_tool("set_breakpoint", "Set Breakpoint",
        "Add or update a breakpoint, over a range with end_address. Execute stops before the instruction, "
        "read, write and access after it. linear and physical match CPU data accesses, physical (and linear without "
        "paging) also DMA, io matches IN/OUT/INS/OUTS. Descriptor table, TSS and page table accesses don't trigger.",
        k_mcp_tool_write_idempotent,
        {
            {"type", "object"},
            {"properties", {
                {"address", {
                    {"type", "string"},
                    {"description", "Address or range start. linear: linear hex, SR:offset or SSSS:offset; physical and "
                        "io: hex address or port."}
                }},
                {"end_address", {
                    {"type", "string"},
                    {"description", "Range end, inclusive, in the same form."}
                }},
                {"type", breakpoint_type},
                {"space", breakpoint_space},
                {"enabled", {
                    {"type", "boolean"},
                    {"description", "false keeps it in the list, disabled. Default true."}
                }}
            }},
            {"required", json::array({"address"})}
        }));

    tools.push_back(make_tool("remove_breakpoint", "Remove Breakpoint",
        "Remove the breakpoint matching address, end_address, type and space.", k_mcp_tool_destructive_idempotent,
        {
            {"type", "object"},
            {"properties", {
                {"address", {
                    {"type", "string"},
                    {"description", "Address or range start, as given to set_breakpoint."}
                }},
                {"end_address", {
                    {"type", "string"},
                    {"description", "Range end, for range breakpoints."}
                }},
                {"type", breakpoint_type},
                {"space", breakpoint_space}
            }},
            {"required", json::array({"address"})}
        }));

    tools.push_back(make_tool("set_breakpoints_active", "Set Breakpoints Active",
        "Turn all breakpoints on or off at once, like Disable All; each keeps its own enabled state.",
        k_mcp_tool_write_idempotent,
        {
            {"type", "object"},
            {"properties", {
                {"active", {
                    {"type", "boolean"},
                    {"description", "false: none stops; true: the enabled ones stop."}
                }}
            }},
            {"required", json::array({"active"})},
            {"additionalProperties", false}
        }));

    tools.push_back(make_tool("list_breakpoints", "List Breakpoints",
        "List address, interrupt and IRQ breakpoints with their enabled state, and whether all are off.",
        k_mcp_tool_read, empty_schema()));

    json interrupt_properties = {
        {"vector", {
            {"type", "integer"},
            {"description", "Vector 0-255 (exceptions 0-31, BIOS IRQs at 40h-4Fh). Give vector or irq."},
            {"minimum", 0},
            {"maximum", 255}
        }},
        {"source", {
            {"type", "string"},
            {"enum", json::array({"any", "exception", "hardware", "software"})},
            {"description", "With vector: what raised it, hardware for a PIC IRQ, software for INT n. Default any."}
        }},
        {"irq", {
            {"type", "integer"},
            {"minimum", 0},
            {"maximum", 15},
            {"description", "IRQ line: 0 timer, 1 keyboard, 6 floppy, 9 CD-ROM, 11 VSYNC, 13 sound."}
        }}
    };

    json set_interrupt_properties = interrupt_properties;
    set_interrupt_properties["enabled"] = {{"type", "boolean"}, {"description", "false keeps it disabled. Default true."}};

    tools.push_back(make_tool("set_breakpoint_on_interrupt", "Set Breakpoint On Interrupt",
        "Add or update a break on entering an interrupt vector, or on an IRQ line the PIC delivers (it "
        "follows the line if the program moves the vector bases), before the handler's first instruction.",
        k_mcp_tool_write_idempotent,
        {
            {"type", "object"},
            {"properties", set_interrupt_properties}
        }));

    tools.push_back(make_tool("clear_breakpoint_on_interrupt", "Clear Breakpoint On Interrupt",
        "Remove an interrupt breakpoint by vector and source, or an IRQ breakpoint by irq.",
        k_mcp_tool_destructive_idempotent,
        {
            {"type", "object"},
            {"properties", interrupt_properties}
        }));

    // Memory tools
    json memory_area = {
        {"type", "integer"},
        {"description", "Area ID from list_memory_areas."}
    };

    json memory_offset = {
        {"type", "string"},
        {"description", "Hex offset in the area."}
    };

    tools.push_back(make_tool("list_memory_areas", "List Memory Areas",
        "List memory areas with IDs, sizes, flags and descriptions: LINEAR and PHYSICAL spaces, I/O PORTS "
        "(side-effect free reads), RAM, ROMs, CMOS, VRAM, sprite and PCM RAM, and CPU windows.", k_mcp_tool_read,
        empty_schema()));

    tools.push_back(make_tool("read_memory", "Read Memory",
        "Read bytes from a memory area. Unmapped bytes read as ??.", k_mcp_tool_read,
        {
            {"type", "object"},
            {"properties", {
                {"area", memory_area},
                {"offset", {
                    {"type", "string"},
                    {"description", "Hex offset; LINEAR also takes SR:offset or SSSS:offset."}
                }},
                {"size", {
                    {"type", "integer"},
                    {"description", "Bytes, 1-65536."}
                }}
            }},
            {"required", json::array({"area", "offset", "size"})}
        }));

    tools.push_back(make_tool("write_memory", "Write Memory",
        "Write hex bytes to a memory area, repeated to fill a range with repeat. ROMs and I/O PORTS are "
        "read-only; write_io_port writes a port.", k_mcp_tool_destructive_idempotent,
        {
            {"type", "object"},
            {"properties", {
                {"area", memory_area},
                {"offset", {
                    {"type", "string"},
                    {"description", "Hex offset; LINEAR also takes SR:offset or SSSS:offset."}
                }},
                {"bytes", {
                    {"type", "string"},
                    {"description", "Hex bytes, spaces optional, e.g. 'B8 00 4C CD 21'."}
                }},
                {"repeat", {
                    {"type", "integer"},
                    {"description", "Times bytes is written back to back. Default 1, at most 1 MB in total."},
                    {"minimum", 1},
                    {"maximum", 1048576}
                }}
            }},
            {"required", json::array({"area", "offset", "bytes"})}
        }));

    tools.push_back(make_tool("write_io_port", "Write I/O Port",
        "Write an I/O port as an OUT instruction does, between instructions: the device acts on it and the "
        "io.writes trace logs it; breakpoints don't stop it. Reading the I/O PORTS area doesn't touch devices.",
        k_mcp_tool_destructive,
        {
            {"type", "object"},
            {"properties", {
                {"port", {
                    {"type", "string"},
                    {"description", "Hex port, 0-FFFF, e.g. '4D8'."}
                }},
                {"value", {
                    {"type", "string"},
                    {"description", "Hex value that fits size."}
                }},
                {"size", {
                    {"type", "integer"},
                    {"description", "Bits, as OUT DX,AL, AX or EAX. Default 8."},
                    {"enum", {8, 16, 32}}
                }}
            }},
            {"required", json::array({"port", "value"})},
            {"additionalProperties", false}
        }));

    tools.push_back(make_tool("translate_address", "Translate Address",
        "Translate an address through segmentation and paging: linear, PDE and PTE, physical, bus and "
        "memory region, or why it fails.", k_mcp_tool_read,
        {
            {"type", "object"},
            {"properties", {
                {"address", {
                    {"type", "string"},
                    {"description", MCP_ADDRESS_DESCRIPTION}
                }}
            }},
            {"required", json::array({"address"})}
        }));

    tools.push_back(make_tool("select_memory_range", "Select Memory Range",
        "Select a range in the Memory Workspace.", k_mcp_tool_destructive_idempotent,
        {
            {"type", "object"},
            {"properties", {
                {"area", memory_area},
                {"start_address", {
                    {"type", "string"},
                    {"description", "Start hex offset."}
                }},
                {"end_address", {
                    {"type", "string"},
                    {"description", "End hex offset, inclusive."}
                }}
            }},
            {"required", json::array({"area", "start_address", "end_address"})}
        }));

    tools.push_back(make_tool("get_memory_selection", "Get Memory Selection",
        "Read the Memory Workspace selection in an area.", k_mcp_tool_read,
        {
            {"type", "object"},
            {"properties", {
                {"area", memory_area}
            }},
            {"required", json::array({"area"})}
        }));

    tools.push_back(make_tool("add_memory_bookmark", "Add Memory Bookmark",
        "Add a memory bookmark.", k_mcp_tool_write,
        {
            {"type", "object"},
            {"properties", {
                {"area", memory_area},
                {"address", memory_offset},
                {"name", {
                    {"type", "string"},
                    {"description", "Optional name."}
                }}
            }},
            {"required", json::array({"area", "address"})}
        }));

    tools.push_back(make_tool("remove_memory_bookmark", "Remove Memory Bookmark",
        "Remove a memory bookmark.", k_mcp_tool_destructive_idempotent,
        {
            {"type", "object"},
            {"properties", {
                {"area", memory_area},
                {"address", memory_offset}
            }},
            {"required", json::array({"area", "address"})}
        }));

    tools.push_back(make_tool("list_memory_bookmarks", "List Memory Bookmarks",
        "List the memory bookmarks of an area.", k_mcp_tool_read,
        {
            {"type", "object"},
            {"properties", {
                {"area", memory_area}
            }},
            {"required", json::array({"area"})}
        }));

    tools.push_back(make_tool("add_memory_watch", "Add Memory Watch",
        "Add a memory watch, with optional notes and size. freeze holds the value it reads now, written "
        "back every frame like a cheat, until remove_memory_watch.", k_mcp_tool_write,
        {
            {"type", "object"},
            {"properties", {
                {"area", memory_area},
                {"address", memory_offset},
                {"notes", {
                    {"type", "string"},
                    {"description", "Optional notes."}
                }},
                {"size", {
                    {"type", "integer"},
                    {"description", "Bits, little endian. Default 8."},
                    {"enum", {8, 16, 32, 64}}
                }},
                {"freeze", {
                    {"type", "boolean"},
                    {"description", "Hold the current value. Default false."}
                }}
            }},
            {"required", json::array({"area", "address"})}
        }));

    tools.push_back(make_tool("remove_memory_watch", "Remove Memory Watch",
        "Remove a memory watch.", k_mcp_tool_destructive_idempotent,
        {
            {"type", "object"},
            {"properties", {
                {"area", memory_area},
                {"address", memory_offset}
            }},
            {"required", json::array({"area", "address"})}
        }));

    tools.push_back(make_tool("list_memory_watches", "List Memory Watches",
        "List the memory watches of an area with their current values.", k_mcp_tool_read,
        {
            {"type", "object"},
            {"properties", {
                {"area", memory_area}
            }},
            {"required", json::array({"area"})}
        }));

    tools.push_back(make_tool("memory_search_capture", "Memory Search Capture",
        "Snapshot a memory area, or part of it, for memory_search.", k_mcp_tool_destructive,
        {
            {"type", "object"},
            {"properties", {
                {"area", memory_area},
                {"start", {
                    {"type", "string"},
                    {"description", "Hex start offset. Default 0; needed with size above 64 MB."}
                }},
                {"size", {
                    {"type", "string"},
                    {"description", "Hex byte count, at most 4000000. Default to the area end."}
                }},
                {"width", {
                    {"type", "integer"},
                    {"description", "Bits of the aligned little-endian values. Default 8."},
                    {"enum", {8, 16, 32}}
                }}
            }},
            {"required", json::array({"area"})}
        }));

    tools.push_back(make_tool("memory_search", "Memory Search",
        "Filter the captured values against the previous snapshot, a value, or the value at an offset.",
        k_mcp_tool_destructive,
        {
            {"type", "object"},
            {"properties", {
                {"area", memory_area},
                {"operator", {
                    {"type", "string"},
                    {"enum", json::array({"<", ">", "==", "!=", "<=", ">="})}
                }},
                {"compare_type", {
                    {"type", "string"},
                    {"enum", json::array({"previous", "value", "address"})}
                }},
                {"compare_value", {
                    {"type", "integer"},
                    {"description", "Value, or area offset for address."}
                }},
                {"data_type", {
                    {"type", "string"},
                    {"description", "Default unsigned."},
                    {"enum", json::array({"unsigned", "signed", "hex"})}
                }}
            }},
            {"required", json::array({"area", "operator", "compare_type"})}
        }));

    tools.push_back(make_tool("memory_find", "Find Bytes or Text in Memory",
        "Find hex bytes or text in a memory area; returns offsets.", k_mcp_tool_read,
        {
            {"type", "object"},
            {"properties", {
                {"area", memory_area},
                {"hex_bytes", {
                    {"type", "string"},
                    {"description", "Hex bytes, e.g. '66B8FF00'. Give hex_bytes or text."},
                    {"minLength", 1}
                }},
                {"text", {
                    {"type", "string"},
                    {"description", "Text, byte for byte."},
                    {"minLength", 1}
                }},
                {"case_sensitive", {
                    {"type", "boolean"},
                    {"description", "For text; false folds ASCII letters. Default true."}
                }},
                {"start", {
                    {"type", "string"},
                    {"description", "Hex start offset; needed with size above 64 MB."}
                }},
                {"size", {
                    {"type", "string"},
                    {"description", "Hex byte count. Default to the area end."}
                }}
            }},
            {"required", json::array({"area"})},
            {"oneOf", json::array({
                {{"required", json::array({"hex_bytes"})}},
                {{"required", json::array({"text"})}}
            })},
            {"additionalProperties", false}
        }));

    // CPU tools
    tools.push_back(make_tool("get_i386_status", "Get Intel 80386 Status",
        "Read the CPU: registers, EFLAGS, CS:EIP with linear and physical PC, CR0-CR3, mode, CPL, IOPL, "
        "code and stack size, last exception, segment caches, GDTR/IDTR/LDTR/TR, debug and test registers, the 486's "
        "AC, CR0 bits and TR3-TR5 on a 486, CR4, the TSC and the MSRs on a Pentium, and the x87 (stack, CW, SW, tags, "
        "last instruction) when one is fitted.", k_mcp_tool_read, empty_schema()));

    tools.push_back(make_tool("get_i386_descriptors", "Get i386 Descriptors",
        "Decode GDT, LDT or IDT entries, with gate targets and symbols; in real and VM86 mode the IDT is "
        "the IVT.", k_mcp_tool_read,
        {
            {"type", "object"},
            {"properties", {
                {"table", {
                    {"type", "string"},
                    {"enum", json::array({"gdt", "ldt", "idt"})}
                }},
                {"start", {
                    {"type", "integer"},
                    {"description", "First entry. Default 0."},
                    {"minimum", 0}
                }},
                {"count", {
                    {"type", "integer"},
                    {"description", "Entries. Default 64."},
                    {"minimum", 1},
                    {"maximum", 256}
                }}
            }},
            {"required", json::array({"table"})}
        }));

    tools.push_back(make_tool("get_page_directory", "Get Page Directory",
        "List the present page-directory entries, or the present pages of one page table, with linear "
        "and physical addresses and flags.", k_mcp_tool_read,
        {
            {"type", "object"},
            {"properties", {
                {"index", {
                    {"type", "integer"},
                    {"description", "Directory entry whose page table to list; omit for the directory."},
                    {"minimum", 0},
                    {"maximum", 1023}
                }}
            }}
        }));

    tools.push_back(make_tool("write_i386_register", "Write Intel 80386 Register",
        "Write a CPU register. Segment registers load like the CPU does, failing on an invalid "
        "descriptor; LDTR and TR load from the GDT like LLDT and LTR.", k_mcp_tool_destructive_idempotent,
        {
            {"type", "object"},
            {"properties", {
                {"name", {
                    {"type", "string"},
                    {"description", "EAX, EBX, ECX, EDX, ESI, EDI, EBP, ESP, EIP, EFLAGS, CR0, CR2, CR3, CR4 (Pentium), "
                        "DR0-DR7, TR3-TR5 (486), TR6, TR7, CS, DS, ES, FS, GS, SS, LDTR, TR, GDTR_BASE, GDTR_LIMIT, "
                        "IDTR_BASE or IDTR_LIMIT."}
                }},
                {"value", {
                    {"type", "string"},
                    {"description", "Hex value."}
                }}
            }},
            {"required", json::array({"name", "value"})}
        }));

    // Disassembly tools
    tools.push_back(make_tool("get_disassembly", "Get Disassembly",
        "Disassemble current memory in Intel syntax: addresses, bytes, mnemonic and operands. Give "
        "end_address or count.", k_mcp_tool_read,
        {
            {"type", "object"},
            {"properties", {
                {"start_address", {
                    {"type", "string"},
                    {"description", MCP_ADDRESS_DESCRIPTION}
                }},
                {"end_address", {
                    {"type", "string"},
                    {"description", "Last byte, inclusive; at most 0x10000 bytes on."}
                }},
                {"count", {
                    {"type", "integer"},
                    {"description", "Instructions, instead of end_address."},
                    {"minimum", 1},
                    {"maximum", 1000}
                }},
                {"code_size", {
                    {"type", "string"},
                    {"description", "Default auto, from the current CS."},
                    {"enum", json::array({"auto", "16", "32"})}
                }},
                {"resolve_symbols", {
                    {"type", "boolean"},
                    {"description", "Add symbols. Default false."}
                }},
                {"detailed", {
                    {"type", "boolean"},
                    {"description", "Add control flow and targets, port names for IN/OUT, vector names for INT and, at "
                        "the current instruction, the BIOS or DOS function in AX. Default false."}
                }}
            }},
            {"required", json::array({"start_address"})}
        }));

    tools.push_back(make_tool("get_call_stack", "Get Call Stack",
        "List the call stack: calls, interrupts and exceptions with source, destination and return "
        "addresses and symbols.", k_mcp_tool_read, empty_schema()));

    tools.push_back(make_tool("add_disassembler_bookmark", "Add Disassembler Bookmark",
        "Add a disassembler bookmark.", k_mcp_tool_write,
        {
            {"type", "object"},
            {"properties", {
                {"address", {
                    {"type", "string"},
                    {"description", MCP_ADDRESS_DESCRIPTION}
                }},
                {"name", {
                    {"type", "string"},
                    {"description", "Optional name."}
                }}
            }},
            {"required", json::array({"address"})}
        }));

    tools.push_back(make_tool("remove_disassembler_bookmark", "Remove Disassembler Bookmark",
        "Remove a disassembler bookmark.", k_mcp_tool_destructive_idempotent,
        {
            {"type", "object"},
            {"properties", {
                {"address", {
                    {"type", "string"},
                    {"description", MCP_ADDRESS_DESCRIPTION}
                }}
            }},
            {"required", json::array({"address"})}
        }));

    tools.push_back(make_tool("list_disassembler_bookmarks", "List Disassembler Bookmarks",
        "List disassembler bookmarks.", k_mcp_tool_read, empty_schema()));

    // Symbol tools
    tools.push_back(make_tool("add_symbol", "Add Symbol",
        "Add or rename a user symbol; user symbols win over automatic labels.", k_mcp_tool_write_idempotent,
        {
            {"type", "object"},
            {"properties", {
                {"address", {
                    {"type", "string"},
                    {"description", MCP_ADDRESS_DESCRIPTION}
                }},
                {"name", {
                    {"type", "string"},
                    {"description", "Letters, digits, _ . @ ? $, not starting with a digit."}
                }}
            }},
            {"required", json::array({"address", "name"})}
        }));

    tools.push_back(make_tool("remove_symbol", "Remove Symbol",
        "Remove the user symbol at an address.", k_mcp_tool_destructive_idempotent,
        {
            {"type", "object"},
            {"properties", {
                {"address", {
                    {"type", "string"},
                    {"description", MCP_ADDRESS_DESCRIPTION}
                }}
            }},
            {"required", json::array({"address"})}
        }));

    tools.push_back(make_tool("load_symbols", "Load Symbols",
        "Load user symbols from a file: 'ADDRESS NAME', 'NAME = ADDRESS' (or EQU), GNU nm output, or GNU "
        "ld and NASM maps; ; and # start comments. Addresses are linear hex or SSSS:OOOOOOOO.",
        k_mcp_tool_write_idempotent_world,
        {
            {"type", "object"},
            {"properties", {
                {"file_path", {
                    {"type", "string"},
                    {"description", "Absolute path."}
                }}
            }},
            {"required", json::array({"file_path"})}
        }));

    tools.push_back(make_tool("list_symbols", "List Symbols",
        "List user symbols and automatic labels a page at a time (total and has_more tell what is left), "
        "or look up the symbols with an exact name or at an address.", k_mcp_tool_read,
        {
            {"type", "object"},
            {"properties", {
                {"filter", {
                    {"type", "string"},
                    {"description", "Case-insensitive part of the name."}
                }},
                {"name", {
                    {"type", "string"},
                    {"description", "Exact name; returns every match."}
                }},
                {"address", {
                    {"type", "string"},
                    {"description", "Symbol at this address: linear hex, SR:offset or SSSS:offset."}
                }},
                {"start", {
                    {"type", "integer"},
                    {"minimum", 0},
                    {"description", "First symbol. Default 0."}
                }},
                {"count", {
                    {"type", "integer"},
                    {"minimum", 1},
                    {"maximum", 1000},
                    {"description", "Symbols per page. Default 200."}
                }}
            }},
            {"additionalProperties", false}
        }));

    // System hardware tools
    tools.push_back(make_tool("get_pic_status", "Get PIC Status",
        "Read both 8259A PICs: per IRQ source, level, request, in-service, mask and vector, and each "
        "PIC's decoded ICWs and modes.", k_mcp_tool_read, empty_schema()));

    tools.push_back(make_tool("get_pit_status", "Get PIT Status",
        "Read the timers: 0060 control, the six 8253 counters with mode, reload, count, OUT and period, "
        "Interval Timer II (0068-006B) and the 0026 microsecond timer where present.", k_mcp_tool_read,
        empty_schema()));

    tools.push_back(make_tool("get_dma_status", "Get DMA Status",
        "Read the uPD71071 DMA controller: control, mask, requests, terminal count and the four "
        "channels' device, mode, addresses and counts.", k_mcp_tool_read, empty_schema()));

    tools.push_back(make_tool("get_rtc_status", "Get RTC Status",
        "Read the MSM58321 clock: date, time, 12/24 hour, the 0070/0080 interface and the registers.", k_mcp_tool_read,
        empty_schema()));

    tools.push_back(make_tool("get_system_status", "Get System Status",
        "Read system control: model, CPU, clock, RAM, drives, firmware set and where each setting came "
        "from (user, model, Auto, game database) with warnings; reset cause, memory windows, NMI, RAM wait, the "
        "later models' registers and the serial ID ROM.", k_mcp_tool_read, empty_schema()));

    tools.push_back(make_tool("get_keyboard_status", "Get Keyboard Status",
        "Read the keyboard interface: data, status, IRQ, last command, queued bytes (make, break or "
        "typematic events and key codes) and held keys. The queue holds 32 bytes; with nothing reading 0600h it "
        "fills and new key presses are lost.", k_mcp_tool_read, empty_schema()));

    // Video tools
    tools.push_back(make_tool("get_crtc_status", "Get CRTC Status",
        "Read the CRTC: dot clock, timing, interlace, beam position, VSYNC IRQ, and each layer's format, "
        "window, VRAM start, stride, zoom and visible size.", k_mcp_tool_read, empty_schema()));

    tools.push_back(make_tool("get_crtc_registers", "Get CRTC Registers",
        "Read the 32 CRTC registers with their names and the 0440 index.", k_mcp_tool_read, empty_schema()));

    tools.push_back(make_tool("write_crtc_register", "Write CRTC Register",
        "Write a CRTC register through 0440/0442 like the CPU, so timing updates and the trace logger "
        "sees it.", k_mcp_tool_destructive_idempotent,
        {
            {"type", "object"},
            {"properties", {
                {"register", {
                    {"type", "integer"},
                    {"description", "R00-R1F."},
                    {"minimum", 0},
                    {"maximum", 31}
                }},
                {"value", {
                    {"type", "string"},
                    {"description", "16-bit hex value."}
                }}
            }},
            {"required", json::array({"register", "value"})}
        }));

    tools.push_back(make_tool("get_hires_crtc_status", "Get High-Res CRTC Status",
        "Read the high-resolution CRTC of the MA, MX and HC: selected and running, clock, timing, display window, "
        "beam position, layout, both pages' window, format, display start, zoom and key, the palette select, the hardware "
        "cursor and the second gateway.", k_mcp_tool_read, empty_schema()));

    tools.push_back(make_tool("write_hires_crtc_register", "Write High-Res CRTC Register",
        "Write an indirect register of the high-resolution CRTC of the MA, MX and HC through 0472/0474 like the CPU, "
        "byte by byte, so 004h starts and stops it, 133h writes a color and 209h a pattern byte. Keeps the address selected.",
        k_mcp_tool_destructive,
        {
            {"type", "object"},
            {"properties", {
                {"register", {
                    {"type", "string"},
                    {"description", "Hex register, 0-2FF."}
                }},
                {"value", {
                    {"type", "string"},
                    {"description", "32-bit hex value."}
                }}
            }},
            {"required", json::array({"register", "value"})}
        }));

    tools.push_back(make_tool("get_video_output_status", "Get Video Output Status",
        "Read the output controller (0448/044A): mode, layer formats, front layer, palette select, FDA0 "
        "layer enables, 044C status, VRAM write mask and the FM-R display.", k_mcp_tool_read, empty_schema()));

    tools.push_back(make_tool("get_palettes", "Get Palettes",
        "Read the palettes with RGB888 colors: the two 16 color banks, the 256 color palette and the FM-R "
        "digital palette; in_use tells which ones the layers read.", k_mcp_tool_read,
        {
            {"type", "object"},
            {"properties", {
                {"palette", {
                    {"type", "string"},
                    {"enum", json::array({"all", "layer0", "layer1", "256", "digital", "hires"})},
                    {"description", "Default all."}
                }}
            }}
        }));

    tools.push_back(make_tool("get_frame_buffer", "Get Frame Buffer",
        "Decode VRAM as PNG: a layer's page, a sprite page, or a custom view to find off-screen graphics.",
        k_mcp_tool_read,
        {
            {"type", "object"},
            {"properties", {
                {"buffer", {
                    {"type", "string"},
                    {"enum", json::array({"layer0", "layer1", "sprite_display", "sprite_draw", "custom"})}
                }},
                {"offset", {
                    {"type", "string"},
                    {"description", "custom: hex offset in the VRAM as stored, 0-7FFFF, 0-FFFFF on the MA, MX and HC."}
                }},
                {"format", {
                    {"type", "string"},
                    {"enum", json::array({"16_colors", "256_colors", "32k_colors", "16m_colors"})},
                    {"description", "custom: default 16_colors. 16m_colors only on the MA, MX and HC."}
                }},
                {"width", {
                    {"type", "integer"},
                    {"description", "custom: default 512."},
                    {"minimum", 1},
                    {"maximum", 1024}
                }},
                {"height", {
                    {"type", "integer"},
                    {"description", "custom: default 256."},
                    {"minimum", 1},
                    {"maximum", 512}
                }},
                {"palette", {
                    {"type", "string"},
                    {"enum", json::array({"layer0", "layer1", "256"})},
                    {"description", "custom 16 color data: default layer0."}
                }}
            }},
            {"required", json::array({"buffer"})}
        }));

    tools.push_back(make_tool("list_sprites", "List Sprites",
        "Read the sprite engine state and list entries with position, pattern, colors and flags.", k_mcp_tool_read,
        {
            {"type", "object"},
            {"properties", {
                {"start", {
                    {"type", "integer"},
                    {"description", "First entry. Default 0."},
                    {"minimum", 0},
                    {"maximum", 1023}
                }},
                {"count", {
                    {"type", "integer"},
                    {"description", "Entries. Default 64."},
                    {"minimum", 1},
                    {"maximum", 1024}
                }},
                {"filter", {
                    {"type", "string"},
                    {"enum", json::array({"all", "drawn", "visible"})},
                    {"description", "drawn: from the first drawn entry on; visible: drawn, shown and inside the page. "
                        "Default all."}
                }}
            }}
        }));

    tools.push_back(make_tool("get_sprite", "Get Sprite",
        "Read a sprite entry: its pattern as an 8x PNG, or its details.", k_mcp_tool_read,
        {
            {"type", "object"},
            {"properties", {
                {"index", {
                    {"type", "integer"},
                    {"minimum", 0},
                    {"maximum", 1023}
                }},
                {"format", {
                    {"type", "string"},
                    {"enum", json::array({"image", "info"})},
                    {"description", "Default image."}
                }}
            }},
            {"required", json::array({"index"})}
        }));

    // Audio tools
    tools.push_back(make_tool("get_ym3438_status", "Get YM3438 Status",
        "Read the YM3438: LFO, channel 3 mode, DAC, timers, status, and each channel's frequency, note, "
        "algorithm, feedback, pan, modulation and operators with their envelopes.", k_mcp_tool_read,
        {
            {"type", "object"},
            {"properties", {
                {"channel", {
                    {"type", "integer"},
                    {"description", "Default all."},
                    {"minimum", 1},
                    {"maximum", 6}
                }}
            }}
        }));

    tools.push_back(make_tool("get_ym3438_registers", "Get YM3438 Registers",
        "Read one YM3438 part's registers as last written, with names, and the address latch.", k_mcp_tool_read,
        {
            {"type", "object"},
            {"properties", {
                {"part", {
                    {"type", "integer"},
                    {"description", "0 (04D8/04DA, channels 1-3 and mode) or 1 (04DC/04DE, channels 4-6). Default 0."},
                    {"minimum", 0},
                    {"maximum", 1}
                }}
            }}
        }));

    tools.push_back(make_tool("get_rf5c68_status", "Get RF5C68 Status",
        "Read the RF5C68 PCM: enable, banks, IRQ mask and flags, and each channel's envelope, pan, rate, "
        "loop start, start and play address.", k_mcp_tool_read, empty_schema()));

    tools.push_back(make_tool("get_pcm16_status", "Get 16-bit PCM Status",
        "Read the 16-bit AD/DA PCM of the later models (0510-051F): direction, format, rate, transfer "
        "mode, IRQ route and causes, DMA base and current address and count, buffer stages and errors, level "
        "monitors and volume.", k_mcp_tool_read, empty_schema()));

    tools.push_back(make_tool("get_midi_status", "Get MIDI Status",
        "Read the MIDI card (0E50-0E77): outputs, interrupt masks and requests, and timer; and MIDI out: capture, "
        "messages, SysEx log, kind of music, port, and the built-in synth playing it.", k_mcp_tool_read, empty_schema()));

    tools.push_back(make_tool("get_sound_status", "Get Sound Status",
        "Read the sound board: MB87078 volumes (04E0/04E2), FM and PCM mutes (04D5), output gate (04EC), "
        "04E9-04EB interrupts and the debugger mutes.", k_mcp_tool_read, empty_schema()));

    tools.push_back(make_tool("set_audio_mute", "Set Audio Mute",
        "Mute or unmute a source or channel in the debugger; emulation is unchanged.", k_mcp_tool_write_idempotent,
        {
            {"type", "object"},
            {"properties", {
                {"source", {
                    {"type", "string"},
                    {"enum", json::array({"fm", "pcm", "cdda", "buzzer", "pcm16", "midi"})}
                }},
                {"channel", {
                    {"type", "integer"},
                    {"description", "1-6 for fm, 1-8 for pcm; omit for the whole source."},
                    {"minimum", 1},
                    {"maximum", 8}
                }},
                {"mute", {
                    {"type", "boolean"},
                    {"description", "true mutes."}
                }}
            }},
            {"required", json::array({"source", "mute"})}
        }));

    // CD-ROM tools
    tools.push_back(make_tool("get_cdrom_status", "Get CD-ROM Status",
        "Read the CD-ROM controller (04C0 status, last command, status queue, transfer, interrupts, "
        "drive, head, read range, disc), CD audio playback and the .m3u playlist.", k_mcp_tool_read, empty_schema()));

    tools.push_back(make_tool("list_cdrom_tracks", "List CD-ROM Tracks",
        "List the disc TOC: track type, absolute MSF and LBA ranges, length, pregap, and the track under "
        "the head.", k_mcp_tool_read, empty_schema()));

    tools.push_back(make_tool("read_cdrom_sector", "Read CD-ROM Sector",
        "Read a CD image sector as hex: 2048 data bytes or the raw 2352. It doesn't move the head; not "
        "for physical drives.", k_mcp_tool_read,
        {
            {"type", "object"},
            {"properties", {
                {"lba", {
                    {"type", "integer"},
                    {"description", "LBA from the program area start (MSF minus 00:02:00)."},
                    {"minimum", 0}
                }},
                {"mode", {
                    {"type", "string"},
                    {"enum", json::array({"user", "raw"})},
                    {"description", "Default user."}
                }}
            }},
            {"required", json::array({"lba"})}
        }));

    // Floppy tools
    json floppy_drive = {
        {"type", "integer"},
        {"minimum", 0},
        {"maximum", 1}
    };

    tools.push_back(make_tool("get_fdc_status", "Get FDC Status",
        "Read the MB8877 floppy controller: status bits, last command, registers, BUSY/DRQ/INTRQ, IRQ6, "
        "drive control, status and select.", k_mcp_tool_read, empty_schema()));

    tools.push_back(make_tool("list_floppy_drives", "List Floppy Drives",
        "List the floppy drives: image, media, geometry, RPM, write protect, modified, head cylinder, "
        "motor, ready and selected.", k_mcp_tool_read, empty_schema()));

    tools.push_back(make_tool("read_floppy_sector", "Read Floppy Sector",
        "Read the first sector with ID R on a track (IDs, density, deleted mark, status, hex data), or "
        "without sector list the track's sectors in disk order.", k_mcp_tool_read,
        {
            {"type", "object"},
            {"properties", {
                {"drive", floppy_drive},
                {"cylinder", {
                    {"type", "integer"},
                    {"minimum", 0},
                    {"maximum", 81}
                }},
                {"head", {
                    {"type", "integer"},
                    {"minimum", 0},
                    {"maximum", 1}
                }},
                {"sector", {
                    {"type", "integer"},
                    {"description", "Sector ID R; omit to list the track."},
                    {"minimum", 0},
                    {"maximum", 255}
                }}
            }},
            {"required", json::array({"drive", "cylinder", "head"})}
        }));

    tools.push_back(make_tool("get_scsi_status", "Get SCSI Status",
        "Read the SCSI host interface: whether Fast Disk Timing is on, control and status bits, data, bus phase, target "
        "and LUN, last message, DRQ, IRQ8 and word DMA, the 0C34h flag on the UG and HG, the command in progress (CDB, "
        "status, bytes moved) and each disk's sense.", k_mcp_tool_read, empty_schema()));

    // Media tools
    tools.push_back(make_tool("get_media_info", "Get Media Info",
        "Read the loaded media, the firmware, where each model takes its firmware from, the floppy "
        "drives, the hard disks and the IC memory card.", k_mcp_tool_read, empty_schema()));

    tools.push_back(make_tool("list_recent_media", "List Recent Media",
        "List recent CD images with file_path values for load_media, recent floppies, hard disks and IC "
        "memory cards.", k_mcp_tool_read, empty_schema()));

    tools.push_back(make_tool("load_media", "Load Media",
        "Load FM Towns media from a local file; a hard disk image (.hdd, .h0-.h6, .hdm over 8 MB, hard disk .chd) attaches to its "
        "ID or the first free one and an IC memory card (.icm) goes in the slot, both powering on an empty machine.",
        k_mcp_tool_destructive_world,
        {
            {"type", "object"},
            {"properties", {
                {"file_path", {{"type", "string"}, {"description", "Absolute path."}}}
            }},
            {"required", json::array({"file_path"})},
            {"additionalProperties", false}
        }));

    tools.push_back(make_tool("eject_cdrom", "Eject CD-ROM",
        "Eject the CD; the machine keeps running and load_media inserts another.", k_mcp_tool_write_idempotent,
        empty_schema()));

    tools.push_back(make_tool("insert_playlist_disc", "Insert Playlist Disc",
        "Insert a playlist disc, ejecting the current one, keeping the machine running.",
        k_mcp_tool_write_world,
        {
            {"type", "object"},
            {"properties", {
                {"index", {
                    {"type", "integer"},
                    {"description", "Disc index from the get_cdrom_status playlist."},
                    {"minimum", 0}
                }}
            }},
            {"required", json::array({"index"})},
            {"additionalProperties", false}
        }));

    tools.push_back(make_tool("insert_floppy", "Insert Floppy",
        "Insert a floppy image (.d77, .d88, .hdm, .img, .xdf, .mfm, .m3u or .zip); the previous disk's "
        "changes are saved first.", k_mcp_tool_write_world,
        {
            {"type", "object"},
            {"properties", {
                {"drive", floppy_drive},
                {"file_path", {
                    {"type", "string"},
                    {"description", "Absolute path."}
                }},
                {"write_protected", {
                    {"type", "boolean"},
                    {"description", "Default keeps the drive's setting."}
                }}
            }},
            {"required", json::array({"drive", "file_path"})}
        }));

    tools.push_back(make_tool("create_blank_floppy", "Create Blank Floppy",
        "Create a blank floppy image file and insert it, like New Blank Disk; an existing file is never "
        "replaced.", k_mcp_tool_write_world,
        {
            {"type", "object"},
            {"properties", {
                {"drive", floppy_drive},
                {"file_path", {
                    {"type", "string"},
                    {"description", "Absolute path: .d77, or .hdm, .img or .xdf for a raw image."}
                }},
                {"type", {
                    {"type", "string"},
                    {"enum", json::array({"2hd_1232kb", "2dd_640kb", "2dd_720kb", "2hd_1440kb"})},
                    {"description", "Default 2hd_1232kb."}
                }},
                {"formatted", {
                    {"type", "boolean"},
                    {"description", "true (default) for an empty FAT12 disk, false for an unformatted one (.d77 only)."}
                }}
            }},
            {"required", json::array({"drive", "file_path"})},
            {"additionalProperties", false}
        }));

    tools.push_back(make_tool("eject_floppy", "Eject Floppy",
        "Eject a drive's disk; its changes are saved first.", k_mcp_tool_write_idempotent,
        {
            {"type", "object"},
            {"properties", {
                {"drive", floppy_drive}
            }},
            {"required", json::array({"drive"})}
        }));

    json hard_disk_id = {
        {"type", "integer"},
        {"description", "SCSI ID."},
        {"minimum", 0},
        {"maximum", 6}
    };

    tools.push_back(make_tool("list_hard_disks", "List Hard Disks",
        "List SCSI IDs 0-6: image, size, write protect, blocks not yet written to the file, and changes "
        "waiting for the next reset; and the machine's SCSI interface (built_in, card or none).", k_mcp_tool_read,
        empty_schema()));

    tools.push_back(make_tool("attach_hard_disk", "Attach Hard Disk",
        "Attach a raw image of 512-byte sectors to a SCSI ID; a running machine takes it at the next "
        "reset. create_size_mb first creates a blank or formatted image, never replacing a file.",
        k_mcp_tool_write_world,
        {
            {"type", "object"},
            {"properties", {
                {"id", hard_disk_id},
                {"file_path", {
                    {"type", "string"},
                    {"description", "Absolute path, usually .hdd, .h0-.h6, .hdm or a hard disk .chd."}
                }},
                {"write_protected", {
                    {"type", "boolean"},
                    {"description", "Default: the ID's Write Protected setting; read-only files attach protected."}
                }},
                {"create_size_mb", {
                    {"type", "integer"},
                    {"minimum", 1},
                    {"maximum", 2047}
                }},
                {"formatted", {
                    {"type", "boolean"},
                    {"description", "With create_size_mb: true (default) for empty drives of up to 127 MB, "
                        "partitioned and formatted like Towns OS, false for a blank disk."}
                }}
            }},
            {"required", json::array({"id", "file_path"})},
            {"additionalProperties", false}
        }));

    tools.push_back(make_tool("detach_hard_disk", "Detach Hard Disk",
        "Detach a SCSI ID's disk after writing its changes; a running machine at the next reset.",
        k_mcp_tool_write_idempotent,
        {
            {"type", "object"},
            {"properties", {
                {"id", hard_disk_id}
            }},
            {"required", json::array({"id"})},
            {"additionalProperties", false}
        }));

    tools.push_back(make_tool("flush_hard_disks", "Flush Hard Disks",
        "Write the hard disks' changes to their image files now instead of after 2 s without writes.",
        k_mcp_tool_write_idempotent_world, empty_schema()));

    tools.push_back(make_tool("insert_memory_card", "Insert IC Memory Card",
        "Insert a raw IC memory card image (whole KB, up to 64 MB), writing the previous card's changes "
        "first; inserting sets CHANGE in 048Ah. create_size_kb first creates a blank card, never replacing a file.",
        k_mcp_tool_write_world,
        {
            {"type", "object"},
            {"properties", {
                {"file_path", {
                    {"type", "string"},
                    {"description", "Absolute path, usually .icm."}
                }},
                {"write_protected", {
                    {"type", "boolean"},
                    {"description", "Default: the Write Protected setting; read-only files insert protected."}
                }},
                {"create_size_kb", {
                    {"type", "integer"},
                    {"minimum", 1},
                    {"maximum", 65536}
                }}
            }},
            {"required", json::array({"file_path"})},
            {"additionalProperties", false}
        }));

    tools.push_back(make_tool("eject_memory_card", "Eject IC Memory Card",
        "Eject the IC memory card after writing its changes to the image file.", k_mcp_tool_write_idempotent,
        {
            {"type", "object"},
            {"properties", {
                {"discard_changes", {
                    {"type", "boolean"},
                    {"description", "true ejects even when the changes can't be written."}
                }}
            }},
            {"additionalProperties", false}
        }));

    tools.push_back(make_tool("read_hard_disk_sector", "Read Hard Disk Sector",
        "Read a 512-byte sector as the guest sees it, unwritten changes included, without bus traffic.",
        k_mcp_tool_read,
        {
            {"type", "object"},
            {"properties", {
                {"id", hard_disk_id},
                {"lba", {
                    {"type", "integer"},
                    {"minimum", 0}
                }}
            }},
            {"required", json::array({"id", "lba"})},
            {"additionalProperties", false}
        }));

    tools.push_back(make_tool("swap_floppies", "Swap Floppies",
        "Swap the disks in drives 0 and 1.", k_mcp_tool_write, empty_schema()));

    tools.push_back(make_tool("set_floppy_write_protect", "Set Floppy Write Protect",
        "Set or clear the write protect tab of a drive's disk.", k_mcp_tool_write_idempotent,
        {
            {"type", "object"},
            {"properties", {
                {"drive", floppy_drive},
                {"write_protected", {
                    {"type", "boolean"},
                    {"description", "true protects."}
                }}
            }},
            {"required", json::array({"drive", "write_protected"})}
        }));

    tools.push_back(make_tool("load_bios", "Load BIOS",
        "Set the firmware folder (subfolders and MAME zips included), load the next machine's firmware, "
        "reset and stop at the reset vector.", k_mcp_tool_destructive_world,
        {
            {"type", "object"},
            {"properties", {
                {"directory_path", {{"type", "string"}, {"description", "Absolute folder with FMT_SYS.ROM and the other "
                    "firmware."}}}
            }},
            {"required", json::array({"directory_path"})},
            {"additionalProperties", false}
        }));

    // Capture tools
    tools.push_back(make_tool("get_screenshot", "Get Screenshot",
        "Capture the screen as a PNG.", k_mcp_tool_read, empty_schema()));

    tools.push_back(make_tool("start_video_recording", "Start Video Recording",
        "Record video and audio to an AVI file (MJPEG or uncompressed, PCM) and return its path; options "
        "also update the recording settings.", k_mcp_tool_destructive_world,
        {
            {"type", "object"},
            {"properties", {
                {"file_path", {
                    {"type", "string"},
                    {"description", "Absolute .avi path. Default an automatic name in the recordings folder."}
                }},
                {"scale", {
                    {"type", "integer"},
                    {"description", "Height multiplier."},
                    {"minimum", 1},
                    {"maximum", 20}
                }},
                {"aspect_ratio", {
                    {"type", "string"},
                    {"description", "screen follows the display settings."},
                    {"enum", json::array({"screen", "square", "4:3", "16:9", "16:10"})}
                }},
                {"quality", {
                    {"type", "string"},
                    {"description", "low and medium halve color resolution; lossless is uncompressed and huge; "
                        "lossless_compressed stores PNG frames and needs an FFmpeg based player."},
                    {"enum", json::array({"low", "medium", "high", "lossless", "lossless_compressed"})}
                }}
            }},
            {"additionalProperties", false}
        }));

    tools.push_back(make_tool("stop_video_recording", "Stop Video Recording",
        "Stop recording and finalize the AVI; returns its path and frame count.", k_mcp_tool_write_world,
        empty_schema()));

    // Save state tools
    json save_state_properties = {
        {"slot", {
            {"type", "integer"},
            {"description", "Slot, which becomes the selected one. Default the selected slot."},
            {"minimum", 1},
            {"maximum", 5}
        }},
        {"file_path", {
            {"type", "string"},
            {"description", "Absolute file path, instead of a slot."}
        }}
    };

    tools.push_back(make_tool("list_save_state_slots", "List Save State Slots",
        "List the 5 save state slots: media name, timestamp and whether it has a screenshot.", k_mcp_tool_read_world,
        empty_schema()));

    tools.push_back(make_tool("save_state", "Save State",
        "Save the state to a slot or a file.", k_mcp_tool_destructive_world,
        {
            {"type", "object"},
            {"properties", save_state_properties},
            {"additionalProperties", false}
        }));

    tools.push_back(make_tool("load_state", "Load State",
        "Load a save state from a slot or a file.", k_mcp_tool_destructive_idempotent_world,
        {
            {"type", "object"},
            {"properties", save_state_properties},
            {"additionalProperties", false}
        }));

    // Rewind tools
    tools.push_back(make_tool("get_rewind_status", "Get Rewind Status",
        "Read the rewind buffer: snapshots, recorded seconds, memory use and budget, and settings.", k_mcp_tool_read,
        empty_schema()));

    tools.push_back(make_tool("rewind_seek", "Rewind Seek",
        "Load a rewind snapshot without dropping the others.", k_mcp_tool_destructive_idempotent,
        {
            {"type", "object"},
            {"properties", {
                {"snapshot", {
                    {"type", "integer"},
                    {"description", "1 oldest, snapshot_count newest."},
                    {"minimum", 1}
                }}
            }},
            {"required", json::array({"snapshot"})},
            {"additionalProperties", false}
        }));

#if defined(REWIND_CHECK)
    tools.push_back(make_tool("rewind_check", "Rewind Check",
        "Check builds only: leave debug mode, or pause and rewind like the hotkey, and read the check counts.",
        k_mcp_tool_destructive,
        {
            {"type", "object"},
            {"properties", {
                {"normal_mode", {
                    {"type", "boolean"},
                    {"description", "Leave debug mode and run."}
                }},
                {"pop_seconds", {
                    {"type", "number"},
                    {"description", "Pause and rewind this many emulated seconds."},
                    {"minimum", 0}
                }}
            }},
            {"additionalProperties", false}
        }));
#endif

    // Controller input tools
    json game_port = {
        {"type", "integer"},
        {"minimum", 1},
        {"maximum", 2}
    };

    json mouse_port = {
        {"type", "integer"},
        {"description", "Default the first port set to mouse."},
        {"minimum", 1},
        {"maximum", 2}
    };

    tools.push_back(make_tool("controller_button", "Controller Button",
        "Press, release or tap a button on a game port. On a mouse port the directions move it (4 counts "
        "per frame held or per tap) and A/B are the left/right buttons.", k_mcp_tool_destructive,
        {
            {"type", "object"},
            {"properties", {
                {"player", game_port},
                {"button", {
                    {"type", "string"},
                    {"enum", json::array({"up", "down", "left", "right", "select", "run", "A", "B", "C", "X", "Y", "Z", "zoom"})}
                }},
                {"action", {
                    {"type", "string"},
                    {"description", "press_and_release releases after some frames."},
                    {"enum", json::array({"press", "release", "press_and_release"})}
                }}
            }},
            {"required", json::array({"player", "button", "action"})}
        }));

    tools.push_back(make_tool("controller_set_type", "Set Controller Type",
        "Set a game port's device; auto takes the game database's device, or original_gamepad on port 1 "
        "and mouse on port 2.", k_mcp_tool_destructive_idempotent,
        {
            {"type", "object"},
            {"properties", {
                {"player", game_port},
                {"type", {
                    {"type", "string"},
                    {"enum", json::array({"auto", "none", "original_gamepad", "marty_gamepad", "six_button_gamepad", "mouse"})}
                }}
            }},
            {"required", json::array({"player", "type"})}
        }));

    tools.push_back(make_tool("get_input_state", "Get Input State",
        "Read each port's device, setting (auto or a device), its source (explicit, database, default or "
        "fallback) and pressed buttons, the held keys and pending tap releases.", k_mcp_tool_read, empty_schema()));

    tools.push_back(make_tool("keyboard_key", "Keyboard Key",
        "Press, release or tap a JIS keyboard key by name, e.g. RETURN, SPACE, A, 1, PF1, SHIFT, CTRL, "
        "ESCAPE, UP, KP_ENTER, HIRAGANA, alone or with modifiers for a combination such as Ctrl+C or Shift+PF1. A "
        "press the keyboard can't queue returns dropped true with the reason. Keys stay held until released, even "
        "when the window loses focus.", k_mcp_tool_destructive,
        {
            {"type", "object"},
            {"properties", {
                {"key", {
                    {"type", "string"},
                    {"description", "Name as in get_input_state keys_held; ENTER and ESC work too."}
                }},
                {"action", {
                    {"type", "string"},
                    {"description", "press_and_release releases after some frames."},
                    {"enum", json::array({"press", "release", "press_and_release"})}
                }},
                {"modifiers", {
                    {"type", "array"},
                    {"items", {{"type", "string"}}},
                    {"maxItems", 4},
                    {"description", "Keys held around the key, e.g. [\"CTRL\"] or [\"CTRL\", \"SHIFT\"]: pressed in order "
                        "before it, released in reverse after it. A release releases them too."}
                }}
            }},
            {"required", json::array({"key", "action"})}
        }));

    tools.push_back(make_tool("get_key_map", "Get Key Map",
        "The host key map of the running content: whether mapping is on, the layout (host: keys type "
        "the host layout's characters; jis: keys by position on a JIS keyboard), whether the keyboard mapper is open, "
        "the per-game file, the user's bindings, every host key that doesn't drive its built-in key, what each "
        "layout key drives at each level (plain, Shift, then AltGr/Option and AltGr/Option+Shift when the layout has "
        "them), and the Towns keys no host key reaches. Host keys are USB HID key names.", k_mcp_tool_read,
        empty_schema()));

    tools.push_back(make_tool("set_key_binding", "Set Key Binding",
        "Bind a host key to Towns keys for this game, saved to its .keys file, as the keyboard mapper "
        "does. keys: one key, up to 4 joined with '+', 'none', or 'default' to drop the user's binding.",
        k_mcp_tool_destructive_idempotent,
        {
            {"type", "object"},
            {"properties", {
                {"host", {
                    {"type", "string"},
                    {"description", "Host key name, e.g. up, w, f13, right_gui, kp_8."}
                }},
                {"keys", {
                    {"type", "string"},
                    {"description", "Towns key names, e.g. kp_8, execute, ctrl + pf1, none, default."}
                }}
            }},
            {"required", json::array({"host", "keys"})}
        }));

    tools.push_back(make_tool("set_key_mapping", "Set Key Mapping",
        "Turn the game's key mapping on or off (off, every host key types its built-in key), and set "
        "the keyboard layout as the Input menu does. At least one of them.", k_mcp_tool_write_idempotent,
        {
            {"type", "object"},
            {"properties", {
                {"enabled", {{"type", "boolean"}}},
                {"layout", {
                    {"type", "string"},
                    {"description", "host: keys type the host layout's characters. jis: keys by position on a JIS keyboard."},
                    {"enum", json::array({"host", "jis"})}
                }}
            }}
        }));

    tools.push_back(make_tool("host_key", "Host Key",
        "Press, release or tap a host key through the key map, as the host keyboard would. drives is "
        "what it types at the current Shift and AltGr/Option level. While the keyboard mapper is open it goes to the "
        "mapper instead.", k_mcp_tool_destructive,
        {
            {"type", "object"},
            {"properties", {
                {"key", {
                    {"type", "string"},
                    {"description", "Host key name, e.g. a, up, kp_8, left_ctrl, f13."}
                }},
                {"action", {
                    {"type", "string"},
                    {"description", "press_and_release releases after some frames."},
                    {"enum", json::array({"press", "release", "press_and_release"})}
                }}
            }},
            {"required", json::array({"key", "action"})}
        }));

    tools.push_back(make_tool("open_key_mapper", "Open Key Mapper",
        "Open the full screen keyboard mapper. The machine stops until it closes.", k_mcp_tool_write_idempotent,
        empty_schema()));

    tools.push_back(make_tool("close_key_mapper", "Close Key Mapper",
        "Close the keyboard mapper, saving the game's bindings.", k_mcp_tool_write_idempotent, empty_schema()));

    tools.push_back(make_tool("keyboard_type", "Keyboard Type",
        "Type ASCII text on the JIS keyboard (Shift where needed, newline is RETURN), for Towns OS and "
        "DOS prompts. dropped counts the key presses the keyboard couldn't queue.", k_mcp_tool_destructive,
        {
            {"type", "object"},
            {"properties", {
                {"text", {
                    {"type", "string"},
                    {"description", "At most 256 characters."},
                    {"minLength", 1}
                }},
                {"frames_per_key", {
                    {"type", "integer"},
                    {"description", "Frames each key is held, and the gap after it. Default 3."},
                    {"minimum", 1},
                    {"maximum", 60}
                }}
            }},
            {"required", json::array({"text"})}
        }));

    tools.push_back(make_tool("controller_macro", "Controller Macro",
        "Run a frame-timed input script of tap, press, release and wait commands on the game port pads "
        "(buttons as controller_button; on a mouse port the directions move it and A/B are its buttons) and the "
        "JIS keyboard (key names as keyboard_key). A tap holds tap_frames and leaves the same gap. Runs frames, "
        "kept in rewind; breakpoints stop it and release what it holds. dropped counts the key presses the keyboard "
        "couldn't queue.", k_mcp_tool_destructive,
        {
            {"type", "object"},
            {"properties", {
                {"player", {
                    {"type", "integer"},
                    {"description", "Default game port of the pad commands, 1-2. Default 1."},
                    {"minimum", 1},
                    {"maximum", 2}
                }},
                {"device", {
                    {"type", "string"},
                    {"description", "Default device of the commands. Default pad."},
                    {"enum", json::array({"pad", "keyboard"})}
                }},
                {"tap_frames", {
                    {"type", "integer"},
                    {"description", "Frames a tap holds, and the gap after it. Default 3."},
                    {"minimum", 1},
                    {"maximum", 60}
                }},
                {"commands", {
                    {"type", "array"},
                    {"description", "Ordered commands, each with one of tap, press, release (a name) or wait (1-1000 "
                        "frames), and optionally player or device, e.g. [{\"tap\":\"run\"},{\"wait\":30},"
                        "{\"press\":\"right\"},{\"wait\":60},{\"release\":\"right\"},"
                        "{\"tap\":\"RETURN\",\"device\":\"keyboard\"}]."},
                    {"minItems", 1},
                    {"items", {{"type", "object"}}}
                }}
            }},
            {"required", json::array({"commands"})},
            {"additionalProperties", false}
        }));

    tools.push_back(make_tool("mouse_get_cursor", "Get Mouse Cursor",
        "Read where mouse_move or mouse_click last measured the pointer (screenshot pixels, top-left of "
        "its image), its box, pixels per count and whether it's still drawn there; estimated after count moves "
        "or drags. Read-only, so unknown before the first move.", k_mcp_tool_read,
        {
            {"type", "object"},
            {"properties", {
                {"player", mouse_port}
            }},
            {"additionalProperties", false}
        }));

    tools.push_back(make_tool("mouse_move", "Mouse Move",
        "Move the pointer to screenshot pixel x, y (top-left of its image) or by dx, dy counts. x, y finds "
        "the pointer by its image when it was seen before, or with a move out and back (32 counts, up to 1024 for "
        "software that turns many counts into a pixel), then moves about 4 pixels per frame (4 to 64 counts) and "
        "checks again until it's there; with a button held it moves straight from the last known position instead. "
        "dx, dy use the same pace once the pointer was measured. Runs frames, kept in rewind; breakpoints stop it.",
        k_mcp_tool_destructive,
        {
            {"type", "object"},
            {"properties", {
                {"player", mouse_port},
                {"x", {
                    {"type", "integer"},
                    {"description", "Column, with y."},
                    {"minimum", 0}
                }},
                {"y", {
                    {"type", "integer"},
                    {"description", "Row, with x."},
                    {"minimum", 0}
                }},
                {"dx", {
                    {"type", "integer"},
                    {"description", "Counts, instead of x and y."},
                    {"minimum", -4096},
                    {"maximum", 4096}
                }},
                {"dy", {
                    {"type", "integer"},
                    {"description", "Counts, instead of x and y."},
                    {"minimum", -4096},
                    {"maximum", 4096}
                }}
            }},
            {"additionalProperties", false}
        }));

    tools.push_back(make_tool("mouse_click", "Mouse Click",
        "Click a mouse button, at screenshot pixel x, y when given (moved there like mouse_move). drag "
        "presses at x, y, moves straight to to_x, to_y and releases, also on errors. Runs frames, kept in rewind.",
        k_mcp_tool_destructive,
        {
            {"type", "object"},
            {"properties", {
                {"player", mouse_port},
                {"button", {
                    {"type", "string"},
                    {"description", "Default left."},
                    {"enum", json::array({"left", "right"})}
                }},
                {"action", {
                    {"type", "string"},
                    {"description", "Default click."},
                    {"enum", json::array({"click", "double_click", "press", "release", "drag"})}
                }},
                {"x", {
                    {"type", "integer"},
                    {"description", "Column, with y."},
                    {"minimum", 0}
                }},
                {"y", {
                    {"type", "integer"},
                    {"description", "Row, with x."},
                    {"minimum", 0}
                }},
                {"to_x", {
                    {"type", "integer"},
                    {"description", "drag end column."},
                    {"minimum", 0}
                }},
                {"to_y", {
                    {"type", "integer"},
                    {"description", "drag end row."},
                    {"minimum", 0}
                }},
                {"press_frames", {
                    {"type", "integer"},
                    {"description", "Frames held down. Default 4."},
                    {"minimum", 1},
                    {"maximum", 120}
                }},
                {"release_frames", {
                    {"type", "integer"},
                    {"description", "Frames up after a release. Default 4."},
                    {"minimum", 1},
                    {"maximum", 120}
                }}
            }},
            {"additionalProperties", false}
        }));

    // Trace and profiler tools
    std::string trace_filters = "Event streams, or a whole group:";
    std::string trace_group;

    for (int i = 0; i < k_mcp_trace_filter_count; i++)
    {
        std::string name = k_mcp_trace_filters[i].name;
        size_t dot = name.find('.');
        std::string group = name.substr(0, dot);

        if (group != trace_group)
        {
            trace_filters += (trace_group.empty() ? " " : "), ") + group + " (";
            trace_group = group;
        }
        else
            trace_filters += ", ";

        trace_filters += name.substr(dot + 1);
    }

    trace_filters += "). Default cpu.instructions, cpu.irqs and cpu.exceptions.";

    tools.push_back(make_tool("get_trace_log", "Get Trace Log",
        "Read trace entries: instructions and hardware events, each line starting with the machine clock "
        "(16 MHz) and its signed delta. With types, contains or pc_range up to 500000 entries are scanned from "
        "start; sequences gives each line's sequence and next_sequence where to go on. overwritten counts entries the "
        "ring dropped. enabled says whether it records; stopped_by says what stopped it.", k_mcp_tool_read,
        {
            {"type", "object"},
            {"properties", {
                {"start", {
                    {"type", "integer"},
                    {"description", "Sequence number, or negative to read that many from the tail. Default -100."}
                }},
                {"count", {
                    {"type", "integer"},
                    {"description", "Entries. Default 100."},
                    {"minimum", 1},
                    {"maximum", 1000}
                }},
                {"types", {
                    {"type", "array"},
                    {"description", "Only these groups, the set_trace_log filter groups plus mark."},
                    {"items", {
                        {"type", "string"}
                    }}
                }},
                {"contains", {
                    {"type", "string"},
                    {"description", "Only lines containing this text, case sensitive."}
                }},
                {"pc_range", {
                    {"type", "string"},
                    {"description", "Only entries whose code address is in this linear hex range, like 1000-1FFF."}
                }},
                {"compact", {
                    {"type", "boolean"},
                    {"description", "Leave out registers, segments, flags, bytes and the linear address on CPU lines."}
                }}
            }},
            {"additionalProperties", false}
        }));

    tools.push_back(make_tool("set_trace_log", "Set Trace Logger",
        "Start or stop the trace logger, to memory or disk, with event filters and capture filters. It "
        "records while the debugger runs the machine, so starting it turns the debugger on. Only the CPU instruction "
        "stream makes the debugger step instruction by instruction; device and I/O streams record with the dynarec. "
        "A reset, power or media change writes a marker and recording goes on, unless keep_across_resets is false.",
        k_mcp_tool_destructive_idempotent,
        {
            {"type", "object"},
            {"properties", {
                {"enabled", {
                    {"type", "boolean"},
                    {"description", "true starts it and opens the Trace Logger window; false stops, keeping entries."}
                }},
                {"output", {
                    {"type", "string"},
                    {"description", "Default memory when starting."},
                    {"enum", json::array({"memory", "disk"})}
                }},
                {"memory_size", {
                    {"type", "string"},
                    {"description", "Entries kept in memory."},
                    {"enum", json::array({"100K", "500K", "1M", "2M", "5M"})}
                }},
                {"disk_size", {
                    {"type", "string"},
                    {"description", "Disk file limit."},
                    {"enum", json::array({"10MB", "50MB", "100MB", "250MB", "500MB", "1GB", "unbounded"})}
                }},
                {"output_path", {
                    {"type", "string"},
                    {"description", "Folder for the disk trace file."}
                }},
                {"vblank_watch_address", {
                    {"type", "string"},
                    {"description", "Linear hex address video.missed_vblank watches. Omit to keep."}
                }},
                {"vblank_watch_operation", {
                    {"type", "string"},
                    {"description", "Access that marks a frame on time. Omit to keep."},
                    {"enum", json::array({"read", "write", "read_write"})}
                }},
                {"filters", {
                    {"type", "array"},
                    {"description", trace_filters},
                    {"items", {
                        {"type", "string"}
                    }},
                    {"minItems", 1},
                    {"uniqueItems", true}
                }},
                {"code", {
                    {"type", "string"},
                    {"description", "Instructions recorded: up to 4 linear hex ranges like F8000-FFFFF, and up to 4 "
                        "excluded ones with !. Empty records all. Giving code, ports or interrupts replaces all three."}
                }},
                {"ports", {
                    {"type", "string"},
                    {"description", "I/O recorded: up to 4 port ranges like 4D8-4DE and 4 excluded like !450-452, :r or :w "
                        "for one direction."}
                }},
                {"interrupts", {
                    {"type", "string"},
                    {"description", "Software INTs recorded: up to 8 vectors in hex, each with :AH or :AX, like 21:3D, "
                        "31, 2F:1687. Empty records all."}
                }},
                {"keep_across_resets", {
                    {"type", "boolean"},
                    {"description", "Default true: reset, power and media changes write a marker and recording goes on."}
                }}
            }},
            {"required", json::array({"enabled"})},
            {"additionalProperties", false}
        }));

    tools.push_back(make_tool("set_profiler", "Set Profiler",
        "Start (opens the Profiler window), stop (closes it) or reset the function profiler. It collects "
        "while the window is open and the debugger runs the machine.", k_mcp_tool_destructive_idempotent,
        {
            {"type", "object"},
            {"properties", {
                {"action", {
                    {"type", "string"},
                    {"enum", json::array({"start", "stop", "reset"})}
                }}
            }},
            {"required", json::array({"action"})},
            {"additionalProperties", false}
        }));

    tools.push_back(make_tool("get_profiler_data", "Get Profiler Data",
        "Read profiler results: totals and per function symbol, address, vector, calls, calls per frame, "
        "inclusive and exclusive cycles and percentages, and average, minimum and maximum cycles.", k_mcp_tool_read,
        {
            {"type", "object"},
            {"properties", {
                {"sort", {
                    {"type", "string"},
                    {"description", "Highest first. Default inclusive."},
                    {"enum", json::array({"inclusive", "exclusive", "calls", "average", "max"})}
                }},
                {"count", {
                    {"type", "integer"},
                    {"description", "Functions. Default 50."},
                    {"minimum", 1},
                    {"maximum", 1000}
                }},
                {"filter", {
                    {"type", "string"},
                    {"description", "Case-insensitive part of the name or hex address."}
                }}
            }},
            {"additionalProperties", false}
        }));

    for (json::iterator it = tools.begin(); it != tools.end(); ++it)
    {
        if (it->contains("inputSchema") && (*it)["inputSchema"].is_object() &&
            !(*it)["inputSchema"].contains("additionalProperties"))
        {
            (*it)["inputSchema"]["additionalProperties"] = false;
        }
    }

    return tools;
}

void McpServer::HandleToolsList(const json& request)
{
    const json& id = request["id"];

    json tools = BuildToolList();

    m_toolRegistry.SetTools(tools);

    if (g_mcp_router_enabled)
    {
        json visibleTools = m_toolRegistry.GetDirectTools();
        AddRouterTools(visibleTools);
        tools = visibleTools;
    }

    json response;
    response["jsonrpc"] = "2.0";
    response["id"] = id;
    response["result"] = {
        {"tools", tools}
    };

    SendResponse(response);
}

void McpServer::EnsureToolRegistry()
{
    if (!m_toolRegistry.IsEmpty())
        return;

    m_toolRegistry.SetTools(BuildToolList());
}


void McpServer::AddRouterTools(json& tools)
{
    tools.push_back({
        {"name", "list_tool_categories"},
        {"title", "List Tool Categories"},
        {"description", "List routed MCP tool categories with descriptions and tool counts."},
        {"annotations", {{"readOnlyHint", true}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"additionalProperties", false}
        }}
    });

    tools.push_back({
        {"name", "get_category_tools"},
        {"title", "Get Category Tools"},
        {"description", "List routed tools in a category with compact descriptions. "
            "Use category names returned by list_tool_categories, "
            "then call get_tool_info for one tool's input schema."},
        {"annotations", {{"readOnlyHint", true}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"category", {{"type", "string"}}}
            }},
            {"required", json::array({"category"})},
            {"additionalProperties", false}
        }}
    });

    tools.push_back({
        {"name", "get_tool_info"},
        {"title", "Get Tool Info"},
        {"description", "Return one MCP tool's title, description, category, direct/routed status, "
            "and real input schema. "
            "Use this after search_tools or get_category_tools before execute_tool."},
        {"annotations", {{"readOnlyHint", true}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"name", {{"type", "string"}}}
            }},
            {"required", json::array({"name"})},
            {"additionalProperties", false}
        }}
    });

    tools.push_back({
        {"name", "search_tools"},
        {"title", "Search Tools"},
        {"description", "Search direct and routed MCP tools by keyword, category, title, description, and aliases. "
            "Use this when you know what you want to do but not the tool name."},
        {"annotations", {{"readOnlyHint", true}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"query", {{"type", "string"}}}
            }},
            {"required", json::array({"query"})},
            {"additionalProperties", false}
        }}
    });

    tools.push_back({
        {"name", "execute_tool"},
        {"title", "Execute Routed Tool"},
        {"description", "Execute a routed MCP tool by name. First use search_tools or get_category_tools to discover the tool, then call get_tool_info to obtain its exact input schema."},
        {"annotations", {{"readOnlyHint", false}, {"destructiveHint", true}, {"idempotentHint", false}, {"openWorldHint", true}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"name", {{"type", "string"}}},
                {"arguments", {
                    {"type", "object"},
                    {"additionalProperties", true}
                }}
            }},
            {"required", json::array({"name"})},
            {"additionalProperties", false}
        }}
    });

}

json McpServer::HandleRouterListCategories()
{
    EnsureToolRegistry();

    json stats = m_toolRegistry.GetStats();
    stats["categories"] = m_toolRegistry.GetCategories();

    return stats;
}

json McpServer::HandleRouterGetCategoryTools(const json& arguments)
{
    EnsureToolRegistry();

    std::string category = arguments.value("category", "");

    if (!m_toolRegistry.HasCategory(category))
    {
        return {
            {"error", "Unknown category"},
            {"category", category},
            {"available_categories", m_toolRegistry.GetCategoryNames()}
        };
    }

    return {
        {"category", category},
        {"title", m_toolRegistry.GetCategoryTitle(category)},
        {"description", m_toolRegistry.GetCategoryDescription(category)},
        {"tool_count", m_toolRegistry.GetCategoryToolCount(category)},
        {"tools", m_toolRegistry.GetToolsInCategory(category)}
    };
}

json McpServer::HandleRouterSearchTools(const json& arguments)
{
    EnsureToolRegistry();

    std::string query = arguments.value("query", "");
    json tools = m_toolRegistry.SearchTools(query);

    return {
        {"query", query},
        {"count", tools.size()},
        {"limit", m_toolRegistry.GetSearchToolLimit()},
        {"matches", tools}
    };
}

json McpServer::HandleRouterGetToolInfo(const json& arguments)
{
    EnsureToolRegistry();

    std::string tool_name = arguments.value("name", "");
    json tool = m_toolRegistry.GetToolInfo(tool_name);

    if (tool.empty())
    {
        return {
            {"error", "Unknown tool"},
            {"name", tool_name},
            {"hint", "Use search_tools or get_category_tools to discover available tool names."}
        };
    }

    return tool;
}

void McpServer::SendToolResult(const json& id, const json& result)
{
    json response;
    response["jsonrpc"] = "2.0";
    response["id"] = id;
    response["result"] = {
        {"content", json::array({
            {
                {"type", "text"},
                {"text", result.dump(2, ' ', false, json::error_handler_t::replace)}
            }
        })}
    };
    response["result"]["isError"] = result.contains("error");

    SendResponse(response);
}

void McpServer::HandleToolsCall(const json& request)
{
    const json& id = request["id"];

    if (!request.contains("params") || !request["params"].contains("name") || !request["params"]["name"].is_string())
    {
        SendError(id, MCP_ERROR_INVALID_PARAMS, "Invalid params: missing tool name");
        return;
    }

    std::string toolName = request["params"]["name"];

    if (request["params"].contains("arguments") && !request["params"]["arguments"].is_object())
    {
        SendError(id, MCP_ERROR_INVALID_PARAMS, "Invalid params: arguments must be an object");
        return;
    }

    json arguments = request["params"].contains("arguments") ? request["params"]["arguments"] : json::object();

    EnsureToolRegistry();

    if (g_mcp_router_enabled && m_toolRegistry.IsRouterTool(toolName, "list_tool_categories"))
    {
        if (!arguments.empty())
        {
            SendError(id, MCP_ERROR_INVALID_PARAMS, "Invalid params: list_tool_categories takes no arguments");
            return;
        }

        SendToolResult(id, HandleRouterListCategories());
        return;
    }

    if (g_mcp_router_enabled && m_toolRegistry.IsRouterTool(toolName, "get_category_tools"))
    {
        if (arguments.size() != 1 || !arguments.contains("category") || !arguments["category"].is_string())
        {
            SendError(id, MCP_ERROR_INVALID_PARAMS, "Invalid params: category must be a string");
            return;
        }

        SendToolResult(id, HandleRouterGetCategoryTools(arguments));
        return;
    }

    if (g_mcp_router_enabled && m_toolRegistry.IsRouterTool(toolName, "get_tool_info"))
    {
        if (arguments.size() != 1 || !arguments.contains("name") || !arguments["name"].is_string())
        {
            SendError(id, MCP_ERROR_INVALID_PARAMS, "Invalid params: name must be a string");
            return;
        }

        SendToolResult(id, HandleRouterGetToolInfo(arguments));
        return;
    }

    if (g_mcp_router_enabled && m_toolRegistry.IsRouterTool(toolName, "search_tools"))
    {
        if (arguments.size() != 1 || !arguments.contains("query") || !arguments["query"].is_string())
        {
            SendError(id, MCP_ERROR_INVALID_PARAMS, "Invalid params: query must be a string");
            return;
        }

        SendToolResult(id, HandleRouterSearchTools(arguments));
        return;
    }

    if (g_mcp_router_enabled && m_toolRegistry.IsRouterTool(toolName, "execute_tool"))
    {
        if (arguments.size() > 2 || !arguments.contains("name") || !arguments["name"].is_string() ||
            (arguments.size() == 2 && !arguments.contains("arguments")))
        {
            SendError(id, MCP_ERROR_INVALID_PARAMS, "Invalid params: execute_tool accepts only name and arguments");
            return;
        }

        toolName = arguments["name"].get<std::string>();

        if (!m_toolRegistry.HasTool(toolName))
        {
            SendError(id, MCP_ERROR_INVALID_PARAMS, "Invalid params: unknown routed tool '" + toolName + "'");
            return;
        }

        if (arguments.contains("arguments") && !arguments["arguments"].is_object())
        {
            SendError(id, MCP_ERROR_INVALID_PARAMS, "Invalid params: routed arguments must be an object");
            return;
        }

        if (arguments.contains("arguments"))
            arguments = arguments["arguments"];
        else
            arguments = json::object();
    }

    std::string validation_error;

    if (!m_toolRegistry.ValidateArguments(toolName, arguments, validation_error))
    {
        SendError(id, MCP_ERROR_INVALID_PARAMS, "Invalid params: " + validation_error);
        return;
    }

    DebugCommand* cmd = new DebugCommand();
    cmd->requestId = id;
    cmd->toolName = toolName;
    cmd->arguments = arguments;

    if (!m_commandQueue.Push(cmd))
    {
        SafeDelete(cmd);
        SendError(id, MCP_ERROR_INTERNAL, "Server busy");
    }
}


// Like the Breakpoints window, the type defaults to execute for linear addresses and to read for physical and io
static bool ParseBreakpointType(const json& arguments, u8& type, u8& space, std::string& error)
{
    std::string space_name = arguments.value("space", "linear");

    space = space_name == "linear" ? I386_BREAKPOINT_LINEAR : space_name == "physical" ? I386_BREAKPOINT_PHYSICAL :
        space_name == "io" ? I386_BREAKPOINT_IO : I386_BREAKPOINT_SPACE_COUNT;

    std::string type_name = arguments.value("type", space == I386_BREAKPOINT_LINEAR ? "execute" : "read");

    type = type_name == "execute" ? I386_BREAKPOINT_EXECUTE : type_name == "read" ? I386_BREAKPOINT_READ :
        type_name == "write" ? I386_BREAKPOINT_WRITE : type_name == "access" ? I386_BREAKPOINT_READ | I386_BREAKPOINT_WRITE : 0;

    if (type == 0 || space == I386_BREAKPOINT_SPACE_COUNT)
    {
        error = "type must be execute, read, write or access, and space linear, physical or io";
        return false;
    }

    if (type == I386_BREAKPOINT_EXECUTE && space == I386_BREAKPOINT_IO)
    {
        error = "Execute breakpoints are linear or physical";
        return false;
    }

    return true;
}

static bool ParseHexText(const std::string& text, u32* value)
{
    return parse_hex_with_prefix(trim_chars(text, " \t"), value);
}

static bool ParseHexArgument(const json& arguments, const char* name, u32* value)
{
    return arguments.contains(name) && arguments[name].is_string() &&
        ParseHexText(arguments[name].get<std::string>(), value);
}

// Linear space takes any i386 address form, physical and io a plain hex value
static bool ParseBreakpointAddress(DebugAdapter& adapter, const json& value, u8 space, u32& address, std::string& error)
{
    if (space == I386_BREAKPOINT_LINEAR)
    {
        McpAddress parsed;

        if (!adapter.ParseAddress(value, parsed, error))
            return false;

        address = parsed.linear;
        return true;
    }

    if (!value.is_string() || !ParseHexText(value.get<std::string>(), &address))
    {
        error = "Invalid hex address";
        return false;
    }

    if (space == I386_BREAKPOINT_IO && address > 0xFFFF)
    {
        error = "I/O ports are 0000-FFFF";
        return false;
    }

    return true;
}

static bool ParseHexBytes(const std::string& text, std::vector<u8>& data)
{
    return parse_hex_bytes(text.c_str(), text.size(), &data, " \t\n\v\f\r") && !data.empty();
}

json McpServer::ExecuteCommand(const std::string& toolName, const json& arguments)
{
    // Normalize tool name: VS Code converts underscores to dots
    std::string normalizedTool = toolName;
    size_t pos = 0;

    while ((pos = normalizedTool.find('.', pos)) != std::string::npos)
    {
        normalizedTool[pos] = '_';
        pos++;
    }

    McpAddress address;
    std::string error;

    // Execution control
    if (normalizedTool == "debug_pause")
    {
        m_debugAdapter.Pause();
        return {{"success", true}};
    }
    else if (normalizedTool == "debug_continue")
    {
        m_debugAdapter.Resume();
        return {{"success", true}};
    }
    else if (normalizedTool == "debug_step_into")
    {
        m_debugAdapter.StepInto();
        return {{"success", true}, {"pending", true}};
    }
    else if (normalizedTool == "debug_step_over")
    {
        m_debugAdapter.StepOver();
        return {{"success", true}, {"pending", true}};
    }
    else if (normalizedTool == "debug_step_out")
    {
        if (!m_debugAdapter.StepOut())
            return {{"success", true}, {"pending", true},
                {"note", "The call stack is empty, so it steps one instruction instead"}};

        return {{"success", true}, {"pending", true}};
    }
    else if (normalizedTool == "debug_reset")
    {
        return m_debugAdapter.Reset(arguments.value("boot", ""));
    }
    else if (normalizedTool == "debug_get_status")
    {
        return m_debugAdapter.GetDebugStatus();
    }
    else if (normalizedTool == "debug_run_to_cursor")
    {
        if (!m_debugAdapter.ParseAddress(arguments["address"], address, error))
            return {{"error", error}};

        return m_debugAdapter.RunToAddress(address);
    }
    else if (normalizedTool == "toggle_fast_forward")
    {
        if (arguments.contains("speed"))
        {
            json speed = m_debugAdapter.SetFastForwardSpeed(arguments["speed"]);

            if (speed.contains("error"))
                return speed;
        }

        return m_debugAdapter.ToggleFastForward(arguments["enabled"]);
    }
    // Breakpoints
    else if (normalizedTool == "set_breakpoint")
    {
        u8 type = 0;
        u8 space = 0;
        u32 start = 0;
        u32 end = 0;
        bool has_end = arguments.contains("end_address");

        if (!ParseBreakpointType(arguments, type, space, error) ||
            !ParseBreakpointAddress(m_debugAdapter, arguments.value("address", json()), space, start, error) ||
            (has_end && !ParseBreakpointAddress(m_debugAdapter, arguments["end_address"], space, end, error)))
            return {{"error", error}};

        if (!has_end)
            end = start;

        if (start > end)
            return {{"error", "address must be <= end_address"}};

        bool range = start != end;
        bool enabled = arguments.value("enabled", true);
        json result = m_debugAdapter.SetBreakpoint(start, end, range, type, space);

        if (result.contains("error"))
            return result;

        if (!enabled)
            m_debugAdapter.EnableBreakpoint(start, end, range, type, space, false);

        result["enabled"] = enabled;
        return result;
    }
    else if (normalizedTool == "remove_breakpoint")
    {
        u8 type = 0;
        u8 space = 0;
        u32 start = 0;
        u32 end = 0;
        bool range = arguments.contains("end_address");

        if (!ParseBreakpointType(arguments, type, space, error) ||
            !ParseBreakpointAddress(m_debugAdapter, arguments.value("address", json()), space, start, error) ||
            (range && !ParseBreakpointAddress(m_debugAdapter, arguments["end_address"], space, end, error)))
            return {{"error", error}};

        return m_debugAdapter.RemoveBreakpoint(start, end, range, type, space);
    }
    else if ((normalizedTool == "set_breakpoint_on_interrupt") || (normalizedTool == "clear_breakpoint_on_interrupt"))
    {
        bool irq = arguments.contains("irq");
        bool set = normalizedTool == "set_breakpoint_on_interrupt";

        if (irq == arguments.contains("vector"))
            return {{"error", "Give vector (with an optional source) or irq"}};

        if (irq && arguments.contains("source"))
            return {{"error", "source goes with vector, not irq"}};

        int line = arguments.value("irq", -1);
        int vector = arguments.value("vector", -1);
        std::string source = arguments.value("source", "any");
        bool enabled = arguments.value("enabled", true);

        if (!set)
            return irq ? m_debugAdapter.ClearIRQBreakpoint(line) : m_debugAdapter.ClearInterruptBreakpoint(vector, source);

        json result = irq ? m_debugAdapter.SetIRQBreakpoint(line) : m_debugAdapter.SetInterruptBreakpoint(vector, source);

        if (result.contains("error"))
            return result;

        if (irq)
            m_debugAdapter.EnableIRQBreakpoint(line, enabled);
        else
            m_debugAdapter.EnableInterruptBreakpoint(vector, source, enabled);

        result["enabled"] = enabled;
        return result;
    }
    else if (normalizedTool == "list_breakpoints")
    {
        json result = m_debugAdapter.ListBreakpoints();
        result["interrupt_breakpoints"] = m_debugAdapter.ListInterruptBreakpoints()["interrupt_breakpoints"];
        result["irq_breakpoints"] = m_debugAdapter.ListIRQBreakpoints()["irq_breakpoints"];
        return result;
    }
    else if (normalizedTool == "set_breakpoints_active")
    {
        return m_debugAdapter.SetBreakpointsActive(arguments.value("active", true));
    }
    // Memory
    else if (normalizedTool == "list_memory_areas")
    {
        return m_debugAdapter.ListMemoryAreas();
    }
    else if (normalizedTool == "read_memory")
    {
        int area = arguments["area"];
        int size = arguments["size"];

        if (size < 1)
            return {{"error", "size must be 1-65536 bytes"}};

        return m_debugAdapter.ReadMemory(area, arguments["offset"], (size_t)size);
    }
    else if (normalizedTool == "write_memory")
    {
        std::vector<u8> data;
        int repeat = arguments.value("repeat", 1);

        if (!ParseHexBytes(arguments["bytes"], data))
            return {{"error", "Invalid byte format"}};

        if ((repeat < 1) || ((u64)data.size() * (u64)repeat > 0x100000))
            return {{"error", "repeat must be 1 or more, at most 1 MB in total"}};

        std::vector<u8> pattern = data;
        data.reserve(pattern.size() * repeat);

        for (int i = 1; i < repeat; i++)
            data.insert(data.end(), pattern.begin(), pattern.end());

        return m_debugAdapter.WriteMemory(arguments["area"], arguments["offset"], data);
    }
    else if (normalizedTool == "write_io_port")
    {
        u32 port = 0;
        u32 value = 0;

        if (!ParseHexArgument(arguments, "port", &port))
            return {{"error", "Invalid port format"}};

        if (!ParseHexArgument(arguments, "value", &value))
            return {{"error", "Invalid value format"}};

        return m_debugAdapter.WriteIOPort(port, value, arguments.value("size", 8));
    }
    else if (normalizedTool == "translate_address")
    {
        return m_debugAdapter.TranslateAddress(arguments["address"]);
    }
    else if (normalizedTool == "select_memory_range")
    {
        u32 start_address = 0;
        u32 end_address = 0;

        if (!ParseHexArgument(arguments, "start_address", &start_address))
            return {{"error", "Invalid start_address format"}};

        if (!ParseHexArgument(arguments, "end_address", &end_address))
            return {{"error", "Invalid end_address format"}};

        return m_debugAdapter.SelectMemoryRange(arguments["area"], start_address, end_address);
    }
    else if (normalizedTool == "get_memory_selection")
    {
        return m_debugAdapter.GetMemorySelection(arguments["area"]);
    }
    else if (normalizedTool == "add_memory_bookmark")
    {
        u32 offset = 0;

        if (!ParseHexArgument(arguments, "address", &offset))
            return {{"error", "Invalid address format"}};

        return m_debugAdapter.AddMemoryBookmark(arguments["area"], offset, arguments.value("name", ""));
    }
    else if (normalizedTool == "remove_memory_bookmark")
    {
        u32 offset = 0;

        if (!ParseHexArgument(arguments, "address", &offset))
            return {{"error", "Invalid address format"}};

        return m_debugAdapter.RemoveMemoryBookmark(arguments["area"], offset);
    }
    else if (normalizedTool == "list_memory_bookmarks")
    {
        return m_debugAdapter.ListMemoryBookmarks(arguments["area"]);
    }
    else if (normalizedTool == "add_memory_watch")
    {
        u32 offset = 0;

        if (!ParseHexArgument(arguments, "address", &offset))
            return {{"error", "Invalid address format"}};

        return m_debugAdapter.AddMemoryWatch(arguments["area"], offset, arguments.value("notes", ""),
            arguments.value("size", 8), arguments.value("freeze", false));
    }
    else if (normalizedTool == "remove_memory_watch")
    {
        u32 offset = 0;

        if (!ParseHexArgument(arguments, "address", &offset))
            return {{"error", "Invalid address format"}};

        return m_debugAdapter.RemoveMemoryWatch(arguments["area"], offset);
    }
    else if (normalizedTool == "list_memory_watches")
    {
        return m_debugAdapter.ListMemoryWatches(arguments["area"]);
    }
    else if (normalizedTool == "memory_search_capture")
    {
        return m_debugAdapter.MemorySearchCapture(arguments["area"], arguments);
    }
    else if (normalizedTool == "memory_search")
    {
        int area = arguments["area"];
        std::string op = arguments["operator"];
        std::string compare_type = arguments["compare_type"];
        u64 compare_value = arguments.value("compare_value", (u64)0);
        std::string data_type = arguments.value("data_type", "unsigned");
        return m_debugAdapter.MemorySearch(area, op, compare_type, compare_value, data_type);
    }
    else if (normalizedTool == "memory_find")
    {
        bool has_hex_bytes = arguments.contains("hex_bytes");
        bool has_text = arguments.contains("text");

        if (has_hex_bytes == has_text)
            return {{"error", "Exactly one of hex_bytes or text is required"}};

        std::string value = has_text ? arguments["text"].get<std::string>() : arguments["hex_bytes"].get<std::string>();
        bool case_sensitive = arguments.value("case_sensitive", true);
        return m_debugAdapter.MemoryFind(arguments["area"], value, has_text, case_sensitive, arguments);
    }
    // CPU
    else if (normalizedTool == "get_i386_status")
    {
        return m_debugAdapter.GetI386Status();
    }
    else if (normalizedTool == "get_i386_descriptors")
    {
        return m_debugAdapter.GetI386Descriptors(arguments.value("table", ""), arguments.value("start", 0),
            arguments.value("count", 64));
    }
    else if (normalizedTool == "get_page_directory")
    {
        return m_debugAdapter.GetPageDirectory(arguments.value("index", -1));
    }
    else if (normalizedTool == "write_i386_register")
    {
        u32 value = 0;

        if (!ParseHexArgument(arguments, "value", &value))
            return {{"error", "Invalid value format"}};

        return m_debugAdapter.WriteI386Register(arguments["name"], value);
    }
    // Disassembly
    else if (normalizedTool == "get_disassembly")
    {
        McpAddress end_address;
        int count = arguments.value("count", 0);
        std::string code_size = arguments.value("code_size", "auto");

        if (!m_debugAdapter.ParseAddress(arguments["start_address"], address, error))
            return {{"error", error}};

        if (count == 0 && !arguments.contains("end_address"))
            return {{"error", "Give end_address or count"}};

        if (count == 0)
        {
            if (!m_debugAdapter.ParseAddress(arguments["end_address"], end_address, error))
                return {{"error", error}};

            if (address.linear > end_address.linear)
                return {{"error", "start_address must be <= end_address"}};

            if ((u64)end_address.linear - address.linear + 1 > 0x10000)
                return {{"error", "Address range too large; maximum is 0x10000 bytes"}};
        }
        else
            end_address.linear = address.linear;

        return m_debugAdapter.GetDisassembly(address.linear, end_address.linear, count,
            code_size == "16" ? 16 : code_size == "32" ? 32 : 0, arguments.value("resolve_symbols", false),
            arguments.value("detailed", false));
    }
    else if (normalizedTool == "get_call_stack")
    {
        return m_debugAdapter.ListCallStack();
    }
    else if (normalizedTool == "add_disassembler_bookmark")
    {
        if (!m_debugAdapter.ParseAddress(arguments["address"], address, error))
            return {{"error", error}};

        return m_debugAdapter.AddDisassemblerBookmark(address.linear, arguments.value("name", ""));
    }
    else if (normalizedTool == "remove_disassembler_bookmark")
    {
        if (!m_debugAdapter.ParseAddress(arguments["address"], address, error))
            return {{"error", error}};

        return m_debugAdapter.RemoveDisassemblerBookmark(address.linear);
    }
    else if (normalizedTool == "list_disassembler_bookmarks")
    {
        return m_debugAdapter.ListDisassemblerBookmarks();
    }
    // Symbols
    else if (normalizedTool == "add_symbol")
    {
        if (!m_debugAdapter.ParseAddress(arguments.value("address", json()), address, error))
            return {{"error", error}};

        return m_debugAdapter.AddSymbol(address.linear, arguments.value("name", ""));
    }
    else if (normalizedTool == "remove_symbol")
    {
        if (!m_debugAdapter.ParseAddress(arguments.value("address", json()), address, error))
            return {{"error", error}};

        return m_debugAdapter.RemoveSymbol(address.linear);
    }
    else if (normalizedTool == "load_symbols")
    {
        return m_debugAdapter.LoadSymbols(arguments.value("file_path", ""));
    }
    else if (normalizedTool == "list_symbols")
    {
        if ((int)arguments.contains("filter") + (int)arguments.contains("name") + (int)arguments.contains("address") > 1)
            return {{"error", "Give one of filter, name or address"}};

        if (arguments.contains("address"))
        {
            if (!m_debugAdapter.ParseAddress(arguments["address"], address, error))
                return {{"error", error}};

            return m_debugAdapter.LookupSymbolAtAddress(address.linear);
        }

        if (arguments.contains("name"))
            return m_debugAdapter.LookupSymbolByName(arguments["name"], false);

        return m_debugAdapter.ListSymbols(arguments.value("filter", ""), arguments.value("start", 0),
            arguments.value("count", 200));
    }
    // System hardware
    else if (normalizedTool == "get_pic_status")
    {
        return m_debugAdapter.GetPICStatus();
    }
    else if (normalizedTool == "get_pit_status")
    {
        return m_debugAdapter.GetPITStatus();
    }
    else if (normalizedTool == "get_dma_status")
    {
        return m_debugAdapter.GetDMAStatus();
    }
    else if (normalizedTool == "get_rtc_status")
    {
        return m_debugAdapter.GetRTCStatus();
    }
    else if (normalizedTool == "get_system_status")
    {
        return m_debugAdapter.GetSystemStatus();
    }
    else if (normalizedTool == "get_keyboard_status")
    {
        return m_debugAdapter.GetKeyboardStatus();
    }
    // Video hardware
    else if (normalizedTool == "get_crtc_status")
    {
        return m_debugAdapter.GetCRTCStatus();
    }
    else if (normalizedTool == "get_crtc_registers")
    {
        return m_debugAdapter.GetCRTCRegisters();
    }
    else if (normalizedTool == "write_crtc_register")
    {
        u16 value = 0;

        if (!arguments.contains("register") || !arguments["register"].is_number_integer())
            return {{"error", "register must be an integer 0-31"}};

        if (!arguments.contains("value") || !arguments["value"].is_string() ||
            !parse_hex_with_prefix(arguments["value"].get<std::string>(), &value))
            return {{"error", "Invalid value format"}};

        return m_debugAdapter.WriteCRTCRegister(arguments["register"].get<int>(), value);
    }
    else if (normalizedTool == "get_hires_crtc_status")
    {
        return m_debugAdapter.GetHiresCRTCStatus();
    }
    else if (normalizedTool == "write_hires_crtc_register")
    {
        u32 reg = 0;
        u32 value = 0;

        if (!arguments.contains("register") || !arguments["register"].is_string() ||
            !parse_hex_with_prefix(arguments["register"].get<std::string>(), &reg))
            return {{"error", "Invalid register format"}};

        if (!arguments.contains("value") || !arguments["value"].is_string() ||
            !parse_hex_with_prefix(arguments["value"].get<std::string>(), &value))
            return {{"error", "Invalid value format"}};

        return m_debugAdapter.WriteHiresCRTCRegister((int)reg, value);
    }
    else if (normalizedTool == "get_video_output_status")
    {
        return m_debugAdapter.GetVideoOutputStatus();
    }
    else if (normalizedTool == "get_palettes")
    {
        return m_debugAdapter.GetPalettes(arguments.value("palette", "all"));
    }
    else if (normalizedTool == "get_frame_buffer")
    {
        return m_debugAdapter.GetFrameBuffer(arguments.value("buffer", ""), arguments);
    }
    else if (normalizedTool == "list_sprites")
    {
        return m_debugAdapter.ListSprites(arguments.value("start", 0), arguments.value("count", 64),
            arguments.value("filter", "all"));
    }
    else if (normalizedTool == "get_sprite")
    {
        if (!arguments.contains("index") || !arguments["index"].is_number_integer())
            return {{"error", "index must be an integer 0-1023"}};

        return m_debugAdapter.GetSprite(arguments["index"].get<int>(), arguments.value("format", "image"));
    }
    // Audio hardware
    else if (normalizedTool == "get_ym3438_status")
    {
        return m_debugAdapter.GetYM3438Status(arguments.value("channel", 0));
    }
    else if (normalizedTool == "get_ym3438_registers")
    {
        return m_debugAdapter.GetYM3438Registers(arguments.value("part", 0));
    }
    else if (normalizedTool == "get_rf5c68_status")
    {
        return m_debugAdapter.GetRF5C68Status();
    }
    else if (normalizedTool == "get_midi_status")
    {
        return m_debugAdapter.GetMIDIStatus();
    }
    else if (normalizedTool == "get_pcm16_status")
    {
        return m_debugAdapter.GetPCM16Status();
    }
    else if (normalizedTool == "get_sound_status")
    {
        return m_debugAdapter.GetSoundStatus();
    }
    else if (normalizedTool == "set_audio_mute")
    {
        if (!arguments.contains("mute") || !arguments["mute"].is_boolean())
            return {{"error", "mute must be true or false"}};

        return m_debugAdapter.SetAudioMute(arguments.value("source", ""), arguments.value("channel", 0),
            arguments["mute"].get<bool>());
    }
    // CD-ROM and floppy hardware
    else if (normalizedTool == "get_cdrom_status")
    {
        json result = m_debugAdapter.GetCDROMStatus();
        result["audio"] = m_debugAdapter.GetCDROMAudioStatus();
        result["playlist"] = m_debugAdapter.ListCdromPlaylist();
        return result;
    }
    else if (normalizedTool == "list_cdrom_tracks")
    {
        return m_debugAdapter.ListCDROMTracks();
    }
    else if (normalizedTool == "read_cdrom_sector")
    {
        if (!arguments.contains("lba") || !arguments["lba"].is_number_integer() || arguments["lba"].get<s64>() < 0)
            return {{"error", "lba must be a non-negative integer"}};

        return m_debugAdapter.ReadCDROMSector((u32)arguments["lba"].get<s64>(), arguments.value("mode", "user"));
    }
    else if (normalizedTool == "get_fdc_status")
    {
        return m_debugAdapter.GetFDCStatus();
    }
    else if (normalizedTool == "list_floppy_drives")
    {
        return m_debugAdapter.ListFloppyDrives();
    }
    else if (normalizedTool == "read_floppy_sector")
    {
        if (!arguments.contains("sector"))
            return m_debugAdapter.ListFloppySectors(arguments.value("drive", -1), arguments.value("cylinder", -1),
                arguments.value("head", -1));

        return m_debugAdapter.ReadFloppySector(arguments.value("drive", -1), arguments.value("cylinder", -1),
            arguments.value("head", -1), arguments.value("sector", -1));
    }
    else if (normalizedTool == "get_scsi_status")
    {
        return m_debugAdapter.GetSCSIStatus();
    }
    else if (normalizedTool == "insert_floppy")
    {
        json write_protected = arguments.contains("write_protected") ? arguments["write_protected"] : json();
        return m_debugAdapter.InsertFloppy(arguments.value("drive", -1), arguments.value("file_path", ""), write_protected);
    }
    else if (normalizedTool == "create_blank_floppy")
    {
        return m_debugAdapter.CreateBlankFloppy(arguments.value("drive", -1), arguments.value("file_path", ""),
            arguments.value("type", "2hd_1232kb"), arguments.value("formatted", true));
    }
    else if (normalizedTool == "eject_floppy")
    {
        return m_debugAdapter.EjectFloppy(arguments.value("drive", -1));
    }
    else if (normalizedTool == "swap_floppies")
    {
        return m_debugAdapter.SwapFloppies();
    }
    else if (normalizedTool == "list_hard_disks")
    {
        return m_debugAdapter.ListHardDisks();
    }
    else if (normalizedTool == "attach_hard_disk")
    {
        json write_protected = arguments.contains("write_protected") ? arguments["write_protected"] : json();
        json create_size_mb = arguments.contains("create_size_mb") ? arguments["create_size_mb"] : json();
        return m_debugAdapter.AttachHardDisk(arguments.value("id", -1), arguments.value("file_path", ""), write_protected,
            create_size_mb, arguments.value("formatted", true));
    }
    else if (normalizedTool == "detach_hard_disk")
    {
        return m_debugAdapter.DetachHardDisk(arguments.value("id", -1));
    }
    else if (normalizedTool == "flush_hard_disks")
    {
        return m_debugAdapter.FlushHardDisks();
    }
    else if (normalizedTool == "read_hard_disk_sector")
    {
        return m_debugAdapter.ReadHardDiskSector(arguments.value("id", -1), arguments.value("lba", (s64)-1));
    }
    else if (normalizedTool == "insert_memory_card")
    {
        json write_protected = arguments.contains("write_protected") ? arguments["write_protected"] : json();
        json create_size_kb = arguments.contains("create_size_kb") ? arguments["create_size_kb"] : json();
        return m_debugAdapter.InsertMemoryCard(arguments.value("file_path", ""), write_protected, create_size_kb);
    }
    else if (normalizedTool == "eject_memory_card")
    {
        return m_debugAdapter.EjectMemoryCard(arguments.value("discard_changes", false));
    }
    else if (normalizedTool == "set_floppy_write_protect")
    {
        if (!arguments.contains("write_protected") || !arguments["write_protected"].is_boolean())
            return {{"error", "write_protected must be true or false"}};

        return m_debugAdapter.SetFloppyWriteProtect(arguments.value("drive", -1), arguments["write_protected"].get<bool>());
    }
    // Media
    else if (normalizedTool == "get_media_info")
    {
        return m_debugAdapter.GetMediaInfo();
    }
    else if (normalizedTool == "list_recent_media")
    {
        return m_debugAdapter.ListRecentMedia();
    }
    else if (normalizedTool == "load_media")
    {
        return {{"error", "load_media must be handled by the MCP manager"}};
    }
    else if (normalizedTool == "eject_cdrom")
    {
        return m_debugAdapter.EjectCdrom();
    }
    else if (normalizedTool == "insert_playlist_disc")
    {
        return {{"error", "insert_playlist_disc must be handled by the MCP manager"}};
    }
    else if (normalizedTool == "load_bios")
    {
        return m_debugAdapter.LoadBios(arguments["directory_path"]);
    }
    // Trace and profiler
    else if (normalizedTool == "get_trace_log")
    {
        u32 types = 0;
        u32 pc_first = 0;
        u32 pc_last = 0xFFFFFFFFU;

        if (arguments.contains("types"))
        {
            const json& list = arguments["types"];

            for (json::const_iterator it = list.begin(); it != list.end(); ++it)
            {
                std::string name = it->get<std::string>();
                u32 mask = name == "mark" ? TRACE_FLAG_MARK : 0;

                for (int i = 0; i < k_mcp_trace_filter_count; i++)
                {
                    std::string filter = k_mcp_trace_filters[i].name;

                    if (filter.compare(0, filter.find('.'), name) == 0 && filter.find('.') == name.size())
                        mask |= 1U << k_mcp_trace_filters[i].type;
                }

                if (mask == 0)
                    return {{"error", "Unknown trace type: " + name}};

                types |= mask;
            }
        }

        if (arguments.contains("pc_range"))
        {
            std::string range = arguments.value("pc_range", "");
            size_t dash = range.find('-');

            if (dash == std::string::npos || !ParseHexText(range.substr(0, dash), &pc_first) ||
                !ParseHexText(range.substr(dash + 1), &pc_last) || pc_first > pc_last)
                return {{"error", "Invalid pc_range, use start-end in hex"}};
        }

        return m_debugAdapter.GetTraceLog(arguments.value("start", (s64)-100), arguments.value("count", 100), types,
            arguments.value("contains", ""), pc_first, pc_last, arguments.value("compact", false));
    }
    else if (normalizedTool == "set_trace_log")
    {
        bool enabled = arguments["enabled"];
        u32 flags = TRACE_FLAG_CPU | TRACE_FLAG_CPU_INTERRUPT;
        u32 event_filters[TRACE_TYPE_COUNT] = {};
        event_filters[TRACE_CPU_INTERRUPT] = TRACE_CPU_INTERRUPT_EVENT_DEFAULT;
        event_filters[TRACE_IO] = TRACE_IO_EVENT_ALL;
        event_filters[TRACE_PIC] = TRACE_PIC_EVENT_DEFAULT;
        event_filters[TRACE_TIMER] = TRACE_TIMER_EVENT_ALL;
        event_filters[TRACE_DMA] = TRACE_DMA_EVENT_ALL;
        event_filters[TRACE_VIDEO] = TRACE_VIDEO_EVENT_DEFAULT;
        event_filters[TRACE_SPRITE] = TRACE_SPRITE_EVENT_ALL;
        event_filters[TRACE_FM] = TRACE_FM_EVENT_DEFAULT;
        event_filters[TRACE_PCM] = TRACE_PCM_EVENT_ALL;
        event_filters[TRACE_PCM16] = TRACE_PCM16_EVENT_ALL;
        event_filters[TRACE_MIDI] = TRACE_MIDI_EVENT_DEFAULT;
        event_filters[TRACE_MIXER] = TRACE_MIXER_EVENT_ALL;
        event_filters[TRACE_CDROM] = TRACE_CDROM_EVENT_ALL;
        event_filters[TRACE_FDC] = TRACE_FDC_EVENT_ALL;
        event_filters[TRACE_SCSI] = TRACE_SCSI_EVENT_DEFAULT;
        event_filters[TRACE_KEYBOARD] = TRACE_KEYBOARD_EVENT_ALL;
        event_filters[TRACE_INPUT] = TRACE_INPUT_EVENT_DEFAULT;
        event_filters[TRACE_SYSTEM] = TRACE_SYSTEM_EVENT_DEFAULT;

        if (enabled && arguments.contains("filters"))
        {
            flags = 0;

            for (int i = 0; i < TRACE_TYPE_COUNT; i++)
                event_filters[i] = 0;

            const json& filters = arguments["filters"];

            for (json::const_iterator it = filters.begin(); it != filters.end(); ++it)
            {
                std::string filter = it->get<std::string>();

                if (!parse_trace_filter(filter, &flags, event_filters))
                    return {{"error", "Unknown trace filter: " + filter + ", use a group or group.stream from the "
                        "set_trace_log filters description"}};
            }

            if (flags == 0)
                return {{"error", "At least one trace filter is required"}};
        }

        std::string output = arguments.value("output", "");
        std::string memory_size = arguments.value("memory_size", "");
        std::string disk_size = arguments.value("disk_size", "");
        std::string output_path = arguments.value("output_path", "");
        std::string vblank_watch_address = arguments.value("vblank_watch_address", "");
        std::string vblank_watch_operation = arguments.value("vblank_watch_operation", "");

        if (arguments.contains("keep_across_resets"))
            m_debugAdapter.SetTraceKeepAcrossResets(arguments.value("keep_across_resets", true));

        if (enabled && (arguments.contains("code") || arguments.contains("ports") || arguments.contains("interrupts")) &&
            !m_debugAdapter.SetTraceCaptureFilters(arguments.value("code", ""), arguments.value("ports", ""),
            arguments.value("interrupts", "")))
            return {{"error", "Invalid code, ports or interrupts filter: up to 4 hex ranges or ! ranges, ports "
                "with :r or :w, up to 8 vectors with :AH or :AX"}};

        return m_debugAdapter.SetTraceLog(enabled, flags, output, memory_size, disk_size, output_path, event_filters,
            vblank_watch_address, vblank_watch_operation);
    }
    else if (normalizedTool == "set_profiler")
    {
        return m_debugAdapter.SetProfiler(arguments.value("action", ""));
    }
    else if (normalizedTool == "get_profiler_data")
    {
        return m_debugAdapter.GetProfilerData(arguments.value("sort", "inclusive"), arguments.value("count", 50),
            arguments.value("filter", ""));
    }
    // Capture
    else if (normalizedTool == "get_screenshot")
    {
        return m_debugAdapter.GetScreenshot();
    }
    else if (normalizedTool == "start_video_recording")
    {
        std::string file_path = arguments.value("file_path", "");
        int scale = arguments.value("scale", 0);
        std::string aspect_ratio = arguments.value("aspect_ratio", "");
        std::string quality = arguments.value("quality", "");
        return m_debugAdapter.StartVideoRecording(file_path, scale, aspect_ratio, quality);
    }
    else if (normalizedTool == "stop_video_recording")
    {
        return m_debugAdapter.StopVideoRecording();
    }
    // Save states
    else if (normalizedTool == "list_save_state_slots")
    {
        return m_debugAdapter.ListSaveStateSlots();
    }
    else if ((normalizedTool == "save_state") || (normalizedTool == "load_state"))
    {
        bool save = normalizedTool == "save_state";

        if (arguments.contains("slot") && arguments.contains("file_path"))
            return {{"error", "Give slot or file_path, not both"}};

        if (arguments.contains("file_path"))
            return save ? m_debugAdapter.SaveStateFile(arguments["file_path"]) :
                m_debugAdapter.LoadStateFile(arguments["file_path"]);

        if (arguments.contains("slot"))
        {
            json slot = m_debugAdapter.SelectSaveStateSlot(arguments["slot"]);

            if (slot.contains("error"))
                return slot;
        }

        return save ? m_debugAdapter.SaveState() : m_debugAdapter.LoadState();
    }
    // Rewind
    else if (normalizedTool == "get_rewind_status")
    {
        return m_debugAdapter.GetRewindStatus();
    }
    else if (normalizedTool == "rewind_seek")
    {
        return m_debugAdapter.RewindSeek(arguments["snapshot"]);
    }
#if defined(REWIND_CHECK)
    else if (normalizedTool == "rewind_check")
    {
        return m_debugAdapter.RewindCheck(arguments.value("normal_mode", false), arguments.value("pop_seconds", 0.0));
    }
#endif
    // Input
    else if (normalizedTool == "controller_button")
    {
        return m_debugAdapter.ControllerButton(arguments["player"], arguments["button"], arguments["action"]);
    }
    else if (normalizedTool == "controller_set_type")
    {
        return m_debugAdapter.ControllerSetType(arguments["player"], arguments["type"]);
    }
    else if ((normalizedTool == "keyboard_type") || (normalizedTool == "mouse_move") ||
        (normalizedTool == "mouse_click") || (normalizedTool == "debug_step_frame") ||
        (normalizedTool == "controller_macro"))
    {
        return {{"error", normalizedTool + " must be handled by the MCP manager"}};
    }
    else if (normalizedTool == "mouse_get_cursor")
    {
        return m_debugAdapter.MouseGetCursor(arguments.value("player", 0));
    }
    else if (normalizedTool == "get_input_state")
    {
        return m_debugAdapter.GetInputState();
    }
    else if (normalizedTool == "keyboard_key")
    {
        return m_debugAdapter.KeyboardKey(arguments["key"], arguments["action"],
            arguments.contains("modifiers") ? arguments["modifiers"] : json());
    }
    else if (normalizedTool == "get_key_map")
    {
        return m_debugAdapter.GetKeyMap();
    }
    else if (normalizedTool == "set_key_binding")
    {
        return m_debugAdapter.SetKeyBinding(arguments["host"], arguments["keys"]);
    }
    else if (normalizedTool == "set_key_mapping")
    {
        return m_debugAdapter.SetKeyMapping(arguments);
    }
    else if (normalizedTool == "host_key")
    {
        return m_debugAdapter.HostKey(arguments["key"], arguments["action"]);
    }
    else if (normalizedTool == "open_key_mapper")
    {
        return m_debugAdapter.SetKeyMapperOpen(true);
    }
    else if (normalizedTool == "close_key_mapper")
    {
        return m_debugAdapter.SetKeyMapperOpen(false);
    }
    else
        return {{"error", "Unknown tool: " + toolName}};
}

void McpServer::SendResponse(const json& response)
{
    std::string line = response.dump(-1, ' ', false, json::error_handler_t::replace);
    m_transport->send(line);
}

void McpServer::SendError(const json& id, int code, const std::string& message, const json& data)
{
    json error;
    error["jsonrpc"] = "2.0";
    error["id"] = id;
    error["error"] = {
        {"code", code},
        {"message", message}
    };

    if (!data.empty() && !data.is_null())
    {
        error["error"]["data"] = data;
    }

    Log("[MCP] Sending error: %s", error.dump().c_str());

    SendResponse(error);
}

void McpServer::RejectOrSendError(bool notification, const json& id, int code, const std::string& message)
{
    if (notification)
        m_transport->reject_notification();
    else
        SendError(id, code, message);
}

void McpServer::LoadResources()
{
    m_resources.clear();
    m_resourceMap.clear();

    char exe_path[1024];
    get_executable_path(exe_path, sizeof(exe_path));

    if (exe_path[0] == '\0')
        return;

    std::string base_path = exe_path;
    std::string resourcesPath = base_path + "/mcp/resources";

    LoadResourcesFromCategory("hardware", resourcesPath + "/hardware/toc.json");
}

static bool IsValidResourceName(const std::string& name)
{
    if (name.empty() || name == "." || name == "..")
        return false;

    for (size_t i = 0; i < name.size(); i++)
    {
        unsigned char character = (unsigned char)name[i];

        if (character < 0x20 || character == 0x7F || character == '/' || character == '\\')
            return false;
    }

    return true;
}

void McpServer::LoadResourcesFromCategory(const std::string& category, const std::string& tocPath)
{
    std::ifstream file;
    open_ifstream_utf8(file, tocPath.c_str(), std::ios::in);

    if (!file.is_open())
    {
        Log("[MCP] Warning: Resources TOC file not found: %s", tocPath.c_str());
        return;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    bool read_error = file.bad();
    file.close();

    if (read_error)
    {
        Log("[MCP] Warning: Failed to read resources TOC file: %s", tocPath.c_str());
        return;
    }

    if (!json::accept(content))
    {
        Log("[MCP] Warning: Invalid JSON in resources TOC file: %s", tocPath.c_str());
        return;
    }

    json toc = json::parse(content);

    if (!toc.contains("toc") || !toc["toc"].is_array())
    {
        Log("[MCP] Warning: Invalid TOC format in resources TOC file: %s", tocPath.c_str());
        return;
    }

    std::string tocDir = tocPath.substr(0, tocPath.find_last_of("/\\"));

    for (size_t i = 0; i < toc["toc"].size(); i++)
    {
        const json& item = toc["toc"][i];

        if (!item.is_object() || !item.contains("uri") || !item["uri"].is_string() ||
            !item.contains("title") || !item["title"].is_string() ||
            (item.contains("description") && !item["description"].is_string()) ||
            (item.contains("mimeType") && !item["mimeType"].is_string()))
        {
            Log("[MCP] Warning: Invalid resource entry %d in TOC file: %s", (int)i, tocPath.c_str());
            continue;
        }

        std::string name = item["uri"].get<std::string>();

        if (!IsValidResourceName(name))
        {
            Log("[MCP] Warning: Invalid resource name in TOC file: %s", tocPath.c_str());
            continue;
        }

        ResourceInfo resource;
        resource.uri = "geartowns://" + category + "/" + name;
        resource.title = item["title"].get<std::string>();
        resource.description = item.contains("description") ? item["description"].get<std::string>() : "";
        resource.mimeType = item.contains("mimeType") ? item["mimeType"].get<std::string>() : "text/plain";
        resource.filePath = tocDir + "/" + name + ".md";

        if (m_resourceMap.find(resource.uri) != m_resourceMap.end())
        {
            Log("[MCP] Warning: Duplicate resource URI in TOC file: %s", resource.uri.c_str());
            continue;
        }

        m_resources.push_back(resource);
        m_resourceMap[resource.uri] = resource;
    }
}

bool McpServer::ReadFileContents(const std::string& filePath, std::string& content)
{
    content.clear();
    std::ifstream file;
    open_ifstream_utf8(file, filePath.c_str(), std::ios::in | std::ios::binary | std::ios::ate);

    if (!file.is_open())
    {
        Log("[MCP] Warning: Failed to open resource file: %s", filePath.c_str());
        return false;
    }

    std::streamoff file_size = file.tellg();

    if (file_size < 0)
    {
        Log("[MCP] Warning: Failed to read resource file: %s", filePath.c_str());
        return false;
    }

    content.resize((size_t)file_size);
    file.seekg(0, std::ios::beg);

    if (!file || (!content.empty() && !file.read(&content[0], (std::streamsize)content.size())))
    {
        Log("[MCP] Warning: Failed to read resource file: %s", filePath.c_str());
        content.clear();
        return false;
    }

    return true;
}

void McpServer::HandleResourcesList(const json& request)
{
    const json& id = request["id"];

    json resources = json::array();

    for (const ResourceInfo& resource : m_resources)
    {
        json resourceJson;
        resourceJson["uri"] = resource.uri;
        resourceJson["name"] = resource.title;
        resourceJson["title"] = resource.title;
        resourceJson["description"] = resource.description;
        resourceJson["mimeType"] = resource.mimeType;

        resources.push_back(resourceJson);
    }

    json response;
    response["jsonrpc"] = "2.0";
    response["id"] = id;
    response["result"] = {
        {"resources", resources}
    };

    SendResponse(response);
}

void McpServer::HandleResourceTemplatesList(const json& request)
{
    json response;
    response["jsonrpc"] = "2.0";
    response["id"] = request["id"];
    response["result"] = {
        {"resourceTemplates", json::array()}
    };

    SendResponse(response);
}

void McpServer::HandleResourcesRead(const json& request)
{
    const json& id = request["id"];

    if (!request.contains("params") || !request["params"].contains("uri") || !request["params"]["uri"].is_string())
    {
        SendError(id, MCP_ERROR_INVALID_PARAMS, "Invalid params: uri must be a string");
        return;
    }

    std::string uri = request["params"]["uri"];

    std::map<std::string, ResourceInfo>::const_iterator it = m_resourceMap.find(uri);

    if (it == m_resourceMap.end())
    {
        SendError(id, MCP_ERROR_RESOURCE_NOT_FOUND, "Resource not found", {{"uri", uri}});
        return;
    }

    const ResourceInfo& resource = it->second;
    std::string content;

    if (!ReadFileContents(resource.filePath, content))
    {
        SendError(id, MCP_ERROR_INTERNAL, "Failed to read resource", {{"uri", uri}});
        return;
    }

    json response;
    response["jsonrpc"] = "2.0";
    response["id"] = id;
    response["result"] = {
        {"contents", json::array({
            {
                {"uri", resource.uri},
                {"mimeType", resource.mimeType},
                {"text", content}
            }
        })}
    };

    SendResponse(response);
}
