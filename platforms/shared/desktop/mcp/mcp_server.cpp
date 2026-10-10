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
    for (int i = 0; i < k_mcp_trace_filter_count; i++)
    {
        const McpTraceFilter& trace_filter = k_mcp_trace_filters[i];

        if (filter != trace_filter.name)
            continue;

        *flags |= 1U << trace_filter.type;
        event_filters[trace_filter.type] |= trace_filter.mask;
        return true;
    }

    return false;
}

#define MCP_ADDRESS_DESCRIPTION "Linear hex (1234ABCD, 0x1234ABCD, $1234ABCD), SR:offset with a segment register " \
    "(CS:1234), or SSSS:offset with a selector or real-mode segment (0008:00001234)."

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
        "system hardware, save states, rewind, pad and keyboard input, screenshots, and video recording. Addresses take "
        "linear hex, SR:offset or SSSS:offset forms.";

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

json McpServer::BuildToolList()
{
    json tools = json::array();

    // Execution control tools
    tools.push_back({
        {"name", "debug_pause"},
        {"title", "Debug Pause"},
        {"description", "Pause execution at the current Intel 80386 instruction; enter the debugger."},
        {"annotations", {{"readOnlyHint", false}, {"destructiveHint", true}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", json::object()},
            {"additionalProperties", false}
        }}
    });

    tools.push_back({
        {"name", "debug_continue"},
        {"title", "Debug Continue"},
        {"description", "Resume emulator execution from pause or breakpoint."},
        {"annotations", {{"readOnlyHint", false}, {"destructiveHint", true}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", json::object()},
            {"additionalProperties", false}
        }}
    });

    tools.push_back({
        {"name", "debug_step_into"},
        {"title", "Debug Step Into"},
        {"description", "Step the next Intel 80386 instruction, entering calls and interrupts; a REP string instruction runs all its elements."},
        {"annotations", {{"readOnlyHint", false}, {"destructiveHint", true}, {"idempotentHint", false}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", json::object()},
            {"additionalProperties", false}
        }}
    });

    tools.push_back({
        {"name", "debug_step_over"},
        {"title", "Debug Step Over"},
        {"description", "Step the next Intel 80386 instruction, running through CALL subroutines, INT n handlers and IRQs."},
        {"annotations", {{"readOnlyHint", false}, {"destructiveHint", true}, {"idempotentHint", false}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", json::object()},
            {"additionalProperties", false}
        }}
    });

    tools.push_back({
        {"name", "debug_step_out"},
        {"title", "Debug Step Out"},
        {"description", "Run until the current subroutine or interrupt handler returns to its caller; with an empty call stack it steps one instruction and adds a note."},
        {"annotations", {{"readOnlyHint", false}, {"destructiveHint", true}, {"idempotentHint", false}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", json::object()},
            {"additionalProperties", false}
        }}
    });

    tools.push_back({
        {"name", "debug_step_frame"},
        {"title", "Debug Step Frame"},
        {"description", "Run one complete frame with breakpoints active, then pause."},
        {"annotations", {{"readOnlyHint", false}, {"destructiveHint", true}, {"idempotentHint", false}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", json::object()},
            {"additionalProperties", false}
        }}
    });

    tools.push_back({
        {"name", "debug_reset"},
        {"title", "Debug Reset"},
        {"description", "Reset the emulated FM Towns system."},
        {"annotations", {{"readOnlyHint", false}, {"destructiveHint", true}, {"idempotentHint", false}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", json::object()},
            {"additionalProperties", false}
        }}
    });

    tools.push_back({
        {"name", "debug_get_status"},
        {"title", "Debug Get Status"},
        {"description", "Read debugger state: paused, breakpoint hit, linear and logical PC, CPU mode, halted, media and frame."},
        {"annotations", {{"readOnlyHint", true}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", json::object()},
            {"additionalProperties", false}
        }}
    });

    tools.push_back({
        {"name", "debug_run_to_cursor"},
        {"title", "Debug Run To Cursor"},
        {"description", "Continue execution until the Intel 80386 reaches an address."},
        {"annotations", {{"readOnlyHint", false}, {"destructiveHint", true}, {"idempotentHint", false}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"address", {
                    {"type", "string"},
                    {"description", MCP_ADDRESS_DESCRIPTION}
                }}
            }},
            {"required", json::array({"address"})}
        }}
    });

    tools.push_back({
        {"name", "set_fast_forward_speed"},
        {"title", "Set Fast Forward Speed"},
        {"description", "Set fast-forward speed index: 0=1.5x, 1=2x, 2=2.5x, 3=3x, 4=unlimited."},
        {"annotations", {{"readOnlyHint", false}, {"destructiveHint", true}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"speed", {
                    {"type", "integer"},
                    {"description", "Speed index 0-4."},
                    {"minimum", 0},
                    {"maximum", 4}
                }}
            }},
            {"required", json::array({"speed"})}
        }}
    });

    tools.push_back({
        {"name", "toggle_fast_forward"},
        {"title", "Toggle Fast Forward"},
        {"description", "Enable/disable fast-forward mode at configured speed."},
        {"annotations", {{"readOnlyHint", false}, {"destructiveHint", true}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"enabled", {
                    {"type", "boolean"},
                    {"description", "true enables fast forward; false disables."}
                }}
            }},
            {"required", json::array({"enabled"})}
        }}
    });

    // Breakpoint tools
    tools.push_back({
        {"name", "set_breakpoint"},
        {"title", "Set Breakpoint"},
        {"description", "Add an Intel 80386 breakpoint. Execute breakpoints stop before the instruction runs, in linear space or in physical space through the current page mapping. Read, write and access breakpoints stop after the instruction that made the access: linear and physical match CPU data accesses by address, physical (and linear while paging is off) also DMA transfers, io matches IN/OUT/INS/OUTS ports. CPU accesses to descriptor tables, the TSS and page tables don't trigger them."},
        {"annotations", {{"readOnlyHint", false}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"address", {
                    {"type", "string"},
                    {"description", "Linear space: " MCP_ADDRESS_DESCRIPTION " Physical and io spaces: hex address or port."}
                }},
                {"type", {
                    {"type", "string"},
                    {"description", "Breakpoint type: execute (default for linear), read (default for physical and io), write, or access (read or write)."},
                    {"enum", json::array({"execute", "read", "write", "access"})}
                }},
                {"space", {
                    {"type", "string"},
                    {"description", "Address space: linear (default), physical, or io (I/O port)."},
                    {"enum", json::array({"linear", "physical", "io"})}
                }}
            }},
            {"required", json::array({"address"})}
        }}
    });

    tools.push_back({
        {"name", "set_breakpoint_range"},
        {"title", "Set Breakpoint Range"},
        {"description", "Add an Intel 80386 breakpoint over an inclusive address range."},
        {"annotations", {{"readOnlyHint", false}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"start_address", {
                    {"type", "string"},
                    {"description", "Range start. " MCP_ADDRESS_DESCRIPTION}
                }},
                {"end_address", {
                    {"type", "string"},
                    {"description", "Range end, inclusive, in the same form as start_address."}
                }},
                {"type", {
                    {"type", "string"},
                    {"description", "Breakpoint type: execute (default for linear), read (default for physical and io), write, or access (read or write)."},
                    {"enum", json::array({"execute", "read", "write", "access"})}
                }},
                {"space", {
                    {"type", "string"},
                    {"description", "Address space: linear (default), physical, or io (I/O port)."},
                    {"enum", json::array({"linear", "physical", "io"})}
                }}
            }},
            {"required", json::array({"start_address", "end_address"})}
        }}
    });

    tools.push_back({
        {"name", "remove_breakpoint"},
        {"title", "Remove Breakpoint"},
        {"description", "Remove a matching single or range breakpoint by address, end_address, type, and space."},
        {"annotations", {{"readOnlyHint", false}, {"destructiveHint", true}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"address", {
                    {"type", "string"},
                    {"description", "Breakpoint address; range removals use this as the start. " MCP_ADDRESS_DESCRIPTION}
                }},
                {"end_address", {
                    {"type", "string"},
                    {"description", "Range end; required only for range breakpoints."}
                }},
                {"type", {
                    {"type", "string"},
                    {"description", "Breakpoint type: execute (default for linear), read (default for physical and io), write, or access."},
                    {"enum", json::array({"execute", "read", "write", "access"})}
                }},
                {"space", {
                    {"type", "string"},
                    {"description", "Address space: linear (default), physical, or io."},
                    {"enum", json::array({"linear", "physical", "io"})}
                }}
            }},
            {"required", json::array({"address"})}
        }}
    });

    tools.push_back({
        {"name", "enable_breakpoint"},
        {"title", "Enable Breakpoint"},
        {"description", "Enable or disable a matching single or range breakpoint by address, end_address, type, and space, keeping it in the list."},
        {"annotations", {{"readOnlyHint", false}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"address", {
                    {"type", "string"},
                    {"description", "Breakpoint address; range breakpoints use this as the start. " MCP_ADDRESS_DESCRIPTION}
                }},
                {"end_address", {
                    {"type", "string"},
                    {"description", "Range end; required only for range breakpoints."}
                }},
                {"type", {
                    {"type", "string"},
                    {"description", "Breakpoint type: execute (default for linear), read (default for physical and io), write, or access."},
                    {"enum", json::array({"execute", "read", "write", "access"})}
                }},
                {"space", {
                    {"type", "string"},
                    {"description", "Address space: linear (default), physical, or io."},
                    {"enum", json::array({"linear", "physical", "io"})}
                }},
                {"enabled", {
                    {"type", "boolean"},
                    {"description", "true to enable, false to disable."}
                }}
            }},
            {"required", json::array({"address", "enabled"})}
        }}
    });

    tools.push_back({
        {"name", "set_breakpoints_active"},
        {"title", "Set Breakpoints Active"},
        {"description", "Turn every breakpoint on or off at once, like Disable All in the debugger; each one keeps its own enabled state."},
        {"annotations", {{"readOnlyHint", false}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"active", {
                    {"type", "boolean"},
                    {"description", "false stops no breakpoint, true stops at the enabled ones."}
                }}
            }},
            {"required", json::array({"active"})},
            {"additionalProperties", false}
        }}
    });

    tools.push_back({
        {"name", "list_breakpoints"},
        {"title", "List Breakpoints"},
        {"description", "List all breakpoints: type, space, address or range, and enabled state, and whether all are disabled."},
        {"annotations", {{"readOnlyHint", true}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", json::object()},
            {"additionalProperties", false}
        }}
    });

    json interrupt_properties = {
        {"vector", {
            {"type", "integer"},
            {"description", "Interrupt vector 0-255 (exceptions 0-31, FM Towns IRQs at 40h-4Fh with the BIOS setup)."},
            {"minimum", 0},
            {"maximum", 255}
        }},
        {"source", {
            {"type", "string"},
            {"enum", json::array({"any", "exception", "hardware", "software"})},
            {"description", "What raised it: any (default), exception, hardware (an IRQ through the PIC), or software (INT n)."}
        }}
    };

    tools.push_back({
        {"name", "set_breakpoint_on_interrupt"},
        {"title", "Set Breakpoint On Interrupt"},
        {"description", "Break when the CPU enters an interrupt or exception vector, before the handler's first instruction."},
        {"annotations", {{"readOnlyHint", false}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", interrupt_properties},
            {"required", json::array({"vector"})}
        }}
    });

    tools.push_back({
        {"name", "clear_breakpoint_on_interrupt"},
        {"title", "Clear Breakpoint On Interrupt"},
        {"description", "Remove an interrupt breakpoint by vector and source."},
        {"annotations", {{"readOnlyHint", false}, {"destructiveHint", true}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", interrupt_properties},
            {"required", json::array({"vector"})}
        }}
    });

    json enable_interrupt_properties = interrupt_properties;
    enable_interrupt_properties["enabled"] = {{"type", "boolean"}, {"description", "true to enable, false to disable."}};

    tools.push_back({
        {"name", "enable_breakpoint_on_interrupt"},
        {"title", "Enable Breakpoint On Interrupt"},
        {"description", "Enable or disable an interrupt breakpoint by vector and source, keeping it in the list."},
        {"annotations", {{"readOnlyHint", false}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", enable_interrupt_properties},
            {"required", json::array({"vector", "enabled"})}
        }}
    });

    tools.push_back({
        {"name", "list_breakpoints_on_interrupt"},
        {"title", "List Breakpoints On Interrupt"},
        {"description", "List interrupt breakpoints with vector names and descriptions, sources and enabled state."},
        {"annotations", {{"readOnlyHint", true}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", json::object()},
            {"additionalProperties", false}
        }}
    });

    json irq_properties = {
        {"irq", {
            {"type", "integer"},
            {"minimum", 0},
            {"maximum", 15},
            {"description", "IRQ line 0-15 of the two 8259A PICs: 0 timer, 1 keyboard, 6 floppy, 9 CD-ROM, 11 VSYNC, 13 sound."}
        }}
    };

    tools.push_back({
        {"name", "set_breakpoint_on_irq"},
        {"title", "Set Breakpoint On IRQ"},
        {"description", "Break when the PIC delivers this IRQ line, before the handler's first instruction. Follows the line, "
            "not the vector, so it keeps working if the program moves the PIC vector bases."},
        {"annotations", {{"readOnlyHint", false}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", irq_properties},
            {"required", json::array({"irq"})}
        }}
    });

    tools.push_back({
        {"name", "clear_breakpoint_on_irq"},
        {"title", "Clear Breakpoint On IRQ"},
        {"description", "Remove the breakpoint on an IRQ line."},
        {"annotations", {{"readOnlyHint", false}, {"destructiveHint", true}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", irq_properties},
            {"required", json::array({"irq"})}
        }}
    });

    json enable_irq_properties = irq_properties;
    enable_irq_properties["enabled"] = {{"type", "boolean"}, {"description", "true to enable, false to disable."}};

    tools.push_back({
        {"name", "enable_breakpoint_on_irq"},
        {"title", "Enable Breakpoint On IRQ"},
        {"description", "Enable or disable the breakpoint on an IRQ line, keeping it in the list."},
        {"annotations", {{"readOnlyHint", false}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", enable_irq_properties},
            {"required", json::array({"irq", "enabled"})}
        }}
    });

    tools.push_back({
        {"name", "list_breakpoints_on_irq"},
        {"title", "List Breakpoints On IRQ"},
        {"description", "List the IRQ lines with a breakpoint, their sources and enabled state."},
        {"annotations", {{"readOnlyHint", true}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", json::object()},
            {"additionalProperties", false}
        }}
    });

    // Memory tools
    tools.push_back({
        {"name", "list_memory_areas"},
        {"title", "List Memory Areas"},
        {"description", "List memory areas: LINEAR and PHYSICAL 4 GB spaces, I/O PORTS (side-effect free reads), and every "
            "memory region (Main RAM, ROMs, CMOS, VRAM, Sprite RAM, PCM RAM, media images, and CPU windows that show a chip "
            "through another mapping); returns IDs, sizes, flags, group and a description."},
        {"annotations", {{"readOnlyHint", true}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", json::object()},
            {"additionalProperties", false}
        }}
    });

    tools.push_back({
        {"name", "read_memory"},
        {"title", "Read Memory"},
        {"description", "Read bytes from a memory area by 0-based offset. Unmapped or unreadable bytes read as ??."},
        {"annotations", {{"readOnlyHint", true}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"area", {
                    {"type", "integer"},
                    {"description", "Memory area ID from list_memory_areas."}
                }},
                {"offset", {
                    {"type", "string"},
                    {"description", "0-based hex offset, e.g. '0100'. The LINEAR area also takes SR:offset or SSSS:offset."}
                }},
                {"size", {
                    {"type", "integer"},
                    {"description", "Number of bytes to read, 1-65536."}
                }}
            }},
            {"required", json::array({"area", "offset", "size"})}
        }}
    });

    tools.push_back({
        {"name", "write_memory"},
        {"title", "Write Memory"},
        {"description", "Write hex bytes to a memory area by 0-based offset. ROMs and I/O PORTS are read-only."},
        {"annotations", {{"readOnlyHint", false}, {"destructiveHint", true}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"area", {
                    {"type", "integer"},
                    {"description", "Memory area ID from list_memory_areas."}
                }},
                {"offset", {
                    {"type", "string"},
                    {"description", "0-based hex offset, e.g. '0100'. The LINEAR area also takes SR:offset or SSSS:offset."}
                }},
                {"bytes", {
                    {"type", "string"},
                    {"description", "Hex bytes, spaces optional, e.g. 'B8 00 4C CD 21'."}
                }}
            }},
            {"required", json::array({"area", "offset", "bytes"})}
        }}
    });

    tools.push_back({
        {"name", "translate_address"},
        {"title", "Translate Address"},
        {"description", "Translate a logical or linear address through segmentation and paging: logical, linear, "
            "PDE and PTE, physical, bus, memory region and offset, or the reason the translation fails."},
        {"annotations", {{"readOnlyHint", true}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"address", {
                    {"type", "string"},
                    {"description", MCP_ADDRESS_DESCRIPTION}
                }}
            }},
            {"required", json::array({"address"})}
        }}
    });

    tools.push_back({
        {"name", "select_memory_range"},
        {"title", "Select Memory Range"},
        {"description", "Select a range in the Memory Workspace by area and 0-based offsets."},
        {"annotations", {{"readOnlyHint", false}, {"destructiveHint", true}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"area", {
                    {"type", "integer"},
                    {"description", "Memory area ID from list_memory_areas."}
                }},
                {"start_address", {
                    {"type", "string"},
                    {"description", "Start 0-based hex offset, e.g. '0100'."}
                }},
                {"end_address", {
                    {"type", "string"},
                    {"description", "End 0-based hex offset, e.g. '01FF'."}
                }}
            }},
            {"required", json::array({"area", "start_address", "end_address"})}
        }}
    });

    tools.push_back({
        {"name", "set_memory_selection_value"},
        {"title", "Set Memory Selection Value"},
        {"description", "Fill the current memory selection with a byte value; use select_memory_range first."},
        {"annotations", {{"readOnlyHint", false}, {"destructiveHint", true}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"area", {
                    {"type", "integer"},
                    {"description", "Memory area ID from list_memory_areas."}
                }},
                {"value", {
                    {"type", "string"},
                    {"description", "Byte hex value, e.g. 'FF' or '00'."}
                }}
            }},
            {"required", json::array({"area", "value"})}
        }}
    });

    tools.push_back({
        {"name", "get_memory_selection"},
        {"title", "Get Memory Selection"},
        {"description", "Read the current Memory Workspace selection range for an area."},
        {"annotations", {{"readOnlyHint", true}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"area", {
                    {"type", "integer"},
                    {"description", "Memory area ID from list_memory_areas."}
                }}
            }},
            {"required", json::array({"area"})}
        }}
    });

    tools.push_back({
        {"name", "add_memory_bookmark"},
        {"title", "Add Memory Bookmark"},
        {"description", "Add a memory bookmark at an area offset."},
        {"annotations", {{"readOnlyHint", false}, {"destructiveHint", false}, {"idempotentHint", false}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"area", {
                    {"type", "integer"},
                    {"description", "Memory area ID from list_memory_areas."}
                }},
                {"address", {
                    {"type", "string"},
                    {"description", "0-based hex offset in the memory area."}
                }},
                {"name", {
                    {"type", "string"},
                    {"description", "Bookmark name; optional."}
                }}
            }},
            {"required", json::array({"area", "address"})}
        }}
    });

    tools.push_back({
        {"name", "remove_memory_bookmark"},
        {"title", "Remove Memory Bookmark"},
        {"description", "Remove the memory bookmark at an area offset."},
        {"annotations", {{"readOnlyHint", false}, {"destructiveHint", true}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"area", {
                    {"type", "integer"},
                    {"description", "Memory area ID from list_memory_areas."}
                }},
                {"address", {
                    {"type", "string"},
                    {"description", "0-based hex offset in the memory area."}
                }}
            }},
            {"required", json::array({"area", "address"})}
        }}
    });

    tools.push_back({
        {"name", "list_memory_bookmarks"},
        {"title", "List Memory Bookmarks"},
        {"description", "List the bookmarks of a memory area."},
        {"annotations", {{"readOnlyHint", true}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"area", {
                    {"type", "integer"},
                    {"description", "Memory area ID from list_memory_areas."}
                }}
            }},
            {"required", json::array({"area"})}
        }}
    });

    tools.push_back({
        {"name", "add_memory_watch"},
        {"title", "Add Memory Watch"},
        {"description", "Add a memory watch at an area offset with optional notes and bit size."},
        {"annotations", {{"readOnlyHint", false}, {"destructiveHint", false}, {"idempotentHint", false}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"area", {
                    {"type", "integer"},
                    {"description", "Memory area ID from list_memory_areas."}
                }},
                {"address", {
                    {"type", "string"},
                    {"description", "0-based hex offset in the memory area."}
                }},
                {"notes", {
                    {"type", "string"},
                    {"description", "Watch notes; optional."}
                }},
                {"size", {
                    {"type", "integer"},
                    {"description", "Watch bit size: 8, 16, 32, or 64; default 8. Values are little endian."},
                    {"enum", {8, 16, 32, 64}}
                }}
            }},
            {"required", json::array({"area", "address"})}
        }}
    });

    tools.push_back({
        {"name", "remove_memory_watch"},
        {"title", "Remove Memory Watch"},
        {"description", "Remove the memory watch at an area offset."},
        {"annotations", {{"readOnlyHint", false}, {"destructiveHint", true}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"area", {
                    {"type", "integer"},
                    {"description", "Memory area ID from list_memory_areas."}
                }},
                {"address", {
                    {"type", "string"},
                    {"description", "0-based hex offset in the memory area."}
                }}
            }},
            {"required", json::array({"area", "address"})}
        }}
    });

    tools.push_back({
        {"name", "list_memory_watches"},
        {"title", "List Memory Watches"},
        {"description", "List the watches of a memory area with their current values."},
        {"annotations", {{"readOnlyHint", true}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"area", {
                    {"type", "integer"},
                    {"description", "Memory area ID from list_memory_areas."}
                }}
            }},
            {"required", json::array({"area"})}
        }}
    });

    tools.push_back({
        {"name", "memory_search_capture"},
        {"title", "Memory Search Capture"},
        {"description", "Snapshot a memory area, or a range of it, for later value-change search."},
        {"annotations", {{"readOnlyHint", false}, {"destructiveHint", true}, {"idempotentHint", false}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"area", {
                    {"type", "integer"},
                    {"description", "Memory area ID from list_memory_areas."}
                }},
                {"start", {
                    {"type", "string"},
                    {"description", "Hex start offset; optional, default 0. Needed with size for areas over 64 MB."}
                }},
                {"size", {
                    {"type", "string"},
                    {"description", "Hex byte count; optional, default to the end of the area, at most 4000000."}
                }},
                {"width", {
                    {"type", "integer"},
                    {"description", "Value width in bits: 8 (default), 16, or 32; aligned little-endian values."},
                    {"enum", {8, 16, 32}}
                }}
            }},
            {"required", json::array({"area"})}
        }}
    });

    tools.push_back({
        {"name", "memory_search"},
        {"title", "Memory Search"},
        {"description", "Filter the captured values by comparison against the previous snapshot, a constant value, or the value at an address."},
        {"annotations", {{"readOnlyHint", false}, {"destructiveHint", true}, {"idempotentHint", false}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"area", {
                    {"type", "integer"},
                    {"description", "Memory area ID from list_memory_areas"}
                }},
                {"operator", {
                    {"type", "string"},
                    {"description", "Comparison operator: <, >, ==, !=, <=, >=."},
                    {"enum", json::array({"<", ">", "==", "!=", "<=", ">="})}
                }},
                {"compare_type", {
                    {"type", "string"},
                    {"description", "Compare against previous snapshot, constant value, or value at address."},
                    {"enum", json::array({"previous", "value", "address"})}
                }},
                {"compare_value", {
                    {"type", "integer"},
                    {"description", "Search value or area offset used for compare_type value/address."}
                }},
                {"data_type", {
                    {"type", "string"},
                    {"description", "Value type: unsigned default, signed, or hex."},
                    {"enum", json::array({"unsigned", "signed", "hex"})}
                }}
            }},
            {"required", json::array({"area", "operator", "compare_type"})}
        }}
    });

    tools.push_back({
        {"name", "memory_find"},
        {"title", "Find Bytes or Text in Memory"},
        {"description", "Find consecutive hex bytes or text in a memory area; return offsets."},
        {"annotations", {{"readOnlyHint", true}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"area", {
                    {"type", "integer"},
                    {"description", "Memory area ID from list_memory_areas."}
                }},
                {"hex_bytes", {
                    {"type", "string"},
                    {"description", "Hex byte pairs to find, e.g. '66B8FF00' (spaces optional). "
                        "Use either hex_bytes or text."},
                    {"minLength", 1}
                }},
                {"text", {
                    {"type", "string"},
                    {"description", "Text to find, byte for byte. Use either text or hex_bytes."},
                    {"minLength", 1}
                }},
                {"case_sensitive", {
                    {"type", "boolean"},
                    {"description", "Match text case. Default true; false folds ASCII letters. "
                        "Ignored for hex_bytes."}
                }},
                {"start", {
                    {"type", "string"},
                    {"description", "Hex start offset; optional. Needed with size for areas over 64 MB."}
                }},
                {"size", {
                    {"type", "string"},
                    {"description", "Hex byte count; optional, default to the end of the area."}
                }}
            }},
            {"required", json::array({"area"})},
            {"oneOf", json::array({
                {{"required", json::array({"hex_bytes"})}},
                {{"required", json::array({"text"})}}
            })},
            {"additionalProperties", false}
        }}
    });

    // CPU tools
    tools.push_back({
        {"name", "get_i386_status"},
        {"title", "Get Intel 80386 Status"},
        {"description", "Read Intel 80386 state: general registers, EIP, EFLAGS bits, CS:EIP with linear and physical PC, "
            "CR0/CR2/CR3, mode, CPL, IOPL, code and stack size, last exception, segment descriptor caches, "
            "GDTR/IDTR/LDTR/TR, debug and test registers."},
        {"annotations", {{"readOnlyHint", true}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", json::object()},
            {"additionalProperties", false}
        }}
    });

    tools.push_back({
        {"name", "get_i386_descriptors"},
        {"title", "Get i386 Descriptors"},
        {"description", "Decode GDT, LDT or IDT entries: selector, base, limit, type, DPL and flags, or gate targets with symbols. In real and VM86 mode the IDT is the real-mode IVT."},
        {"annotations", {{"readOnlyHint", true}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"table", {
                    {"type", "string"},
                    {"enum", json::array({"gdt", "ldt", "idt"})},
                    {"description", "Descriptor table."}
                }},
                {"start", {
                    {"type", "integer"},
                    {"description", "First entry index. Default 0."},
                    {"minimum", 0}
                }},
                {"count", {
                    {"type", "integer"},
                    {"description", "Maximum entries, 1-256. Default 64."},
                    {"minimum", 1},
                    {"maximum", 256}
                }}
            }},
            {"required", json::array({"table"})}
        }}
    });

    tools.push_back({
        {"name", "get_page_directory"},
        {"title", "Get Page Directory"},
        {"description", "List the present page-directory entries, or the present pages of one page table (index), with linear and physical addresses and P/W/U/A/D flags."},
        {"annotations", {{"readOnlyHint", true}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"index", {
                    {"type", "integer"},
                    {"description", "Page-directory entry 0-1023; omit to list the directory."},
                    {"minimum", 0},
                    {"maximum", 1023}
                }}
            }}
        }}
    });

    tools.push_back({
        {"name", "write_i386_register"},
        {"title", "Write Intel 80386 Register"},
        {"description", "Write an Intel 80386 register. Segment writes reload base and limit in real and VM86 mode and "
            "load the descriptor in protected mode, failing if it is invalid. LDTR and TR load their LDT or TSS "
            "descriptor from the GDT, like LLDT and LTR."},
        {"annotations", {{"readOnlyHint", false}, {"destructiveHint", true}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"name", {
                    {"type", "string"},
                    {"description", "Register: EAX, EBX, ECX, EDX, ESI, EDI, EBP, ESP, EIP, EFLAGS, CR0, CR2, CR3, "
                        "DR0-DR7, TR6, TR7, CS, DS, ES, FS, GS, SS, LDTR, TR, GDTR_BASE, GDTR_LIMIT, IDTR_BASE or "
                        "IDTR_LIMIT."}
                }},
                {"value", {
                    {"type", "string"},
                    {"description", "Hex value."}
                }}
            }},
            {"required", json::array({"name", "value"})}
        }}
    });

    // Disassembly tools
    tools.push_back({
        {"name", "get_disassembly"},
        {"title", "Get Disassembly"},
        {"description", "Decode Intel 80386 instructions from current memory in Intel syntax: linear and logical address, "
            "bytes, mnemonic, operands and length. Give end_address or count."},
        {"annotations", {{"readOnlyHint", true}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"start_address", {
                    {"type", "string"},
                    {"description", "First instruction. " MCP_ADDRESS_DESCRIPTION}
                }},
                {"end_address", {
                    {"type", "string"},
                    {"description", "Last byte to decode, inclusive; at most 0x10000 bytes after start_address."}
                }},
                {"count", {
                    {"type", "integer"},
                    {"description", "Number of instructions to decode instead of end_address, 1-1000."},
                    {"minimum", 1},
                    {"maximum", 1000}
                }},
                {"code_size", {
                    {"type", "string"},
                    {"description", "Default operand and address size: auto (from the current CS, default), 16, or 32."},
                    {"enum", json::array({"auto", "16", "32"})}
                }},
                {"resolve_symbols", {
                    {"type", "boolean"},
                    {"description", "Include symbols for instructions and targets. Default false."}
                }},
                {"detailed", {
                    {"type", "boolean"},
                    {"description", "Include control flow (call, jump, conditional, return, int, iret), targets, "
                        "I/O port names for IN/OUT, vector names for INT and, on the current instruction, the BIOS "
                        "or DOS function picked by AX. Default false."}
                }}
            }},
            {"required", json::array({"start_address"})}
        }}
    });

    tools.push_back({
        {"name", "get_call_stack"},
        {"title", "Get Call Stack"},
        {"description", "List the Intel 80386 call stack: calls, interrupts and exceptions with vector, "
            "source, destination and return addresses (logical and linear) and symbols."},
        {"annotations", {{"readOnlyHint", true}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", json::object()},
            {"additionalProperties", false}
        }}
    });

    tools.push_back({
        {"name", "add_disassembler_bookmark"},
        {"title", "Add Disassembler Bookmark"},
        {"description", "Add a disassembler bookmark at an address."},
        {"annotations", {{"readOnlyHint", false}, {"destructiveHint", false}, {"idempotentHint", false}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"address", {
                    {"type", "string"},
                    {"description", MCP_ADDRESS_DESCRIPTION}
                }},
                {"name", {
                    {"type", "string"},
                    {"description", "Bookmark name; optional, auto-generated if omitted."}
                }}
            }},
            {"required", json::array({"address"})}
        }}
    });

    tools.push_back({
        {"name", "remove_disassembler_bookmark"},
        {"title", "Remove Disassembler Bookmark"},
        {"description", "Remove the disassembler bookmark at an address."},
        {"annotations", {{"readOnlyHint", false}, {"destructiveHint", true}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"address", {
                    {"type", "string"},
                    {"description", MCP_ADDRESS_DESCRIPTION}
                }}
            }},
            {"required", json::array({"address"})}
        }}
    });

    tools.push_back({
        {"name", "list_disassembler_bookmarks"},
        {"title", "List Disassembler Bookmarks"},
        {"description", "List disassembler bookmarks."},
        {"annotations", {{"readOnlyHint", true}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", json::object()},
            {"additionalProperties", false}
        }}
    });

    // Symbol tools
    tools.push_back({
        {"name", "add_symbol"},
        {"title", "Add Symbol"},
        {"description", "Add or rename a user symbol; user symbols take precedence over automatic labels in disassembly, call stack and breakpoints."},
        {"annotations", {{"readOnlyHint", false}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"address", {
                    {"type", "string"},
                    {"description", MCP_ADDRESS_DESCRIPTION}
                }},
                {"name", {
                    {"type", "string"},
                    {"description", "Symbol name: letters, digits, _ . @ ? $, not starting with a digit."}
                }}
            }},
            {"required", json::array({"address", "name"})}
        }}
    });

    tools.push_back({
        {"name", "remove_symbol"},
        {"title", "Remove Symbol"},
        {"description", "Remove the user symbol at an address."},
        {"annotations", {{"readOnlyHint", false}, {"destructiveHint", true}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"address", {
                    {"type", "string"},
                    {"description", MCP_ADDRESS_DESCRIPTION}
                }}
            }},
            {"required", json::array({"address"})}
        }}
    });

    tools.push_back({
        {"name", "load_symbols"},
        {"title", "Load Symbols"},
        {"description", "Load user symbols from a text file: one per line as 'ADDRESS NAME' or 'NAME = ADDRESS' (or EQU), comments after ; or #. Addresses are linear hex or SSSS:OOOOOOOO resolved with the current descriptor tables."},
        {"annotations", {{"readOnlyHint", false}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", true}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"file_path", {
                    {"type", "string"},
                    {"description", "Absolute path to the symbol file."}
                }}
            }},
            {"required", json::array({"file_path"})}
        }}
    });

    tools.push_back({
        {"name", "list_symbols"},
        {"title", "List Symbols"},
        {"description", "List user symbols and automatic labels, with an optional case-insensitive name filter, a page at a time: start and count select the page, total and has_more tell what is left."},
        {"annotations", {{"readOnlyHint", true}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"filter", {
                    {"type", "string"},
                    {"description", "Substring the symbol name must contain; optional."}
                }},
                {"start", {
                    {"type", "integer"},
                    {"minimum", 0},
                    {"description", "Index of the first symbol to return; default 0."}
                }},
                {"count", {
                    {"type", "integer"},
                    {"minimum", 1},
                    {"maximum", 1000},
                    {"description", "Symbols to return; default 200, at most 1000."}
                }}
            }},
            {"additionalProperties", false}
        }}
    });

    tools.push_back({
        {"name", "lookup_symbol_by_name"},
        {"title", "Lookup Symbol by Name"},
        {"description", "Find a symbol by its exact name, or with partial by a case-insensitive part of it; return all matches, up to 1000."},
        {"annotations", {{"readOnlyHint", true}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"name", {
                    {"type", "string"},
                    {"description", "Symbol name, or part of it with partial."}
                }},
                {"partial", {
                    {"type", "boolean"},
                    {"description", "Match a case-insensitive part of the name instead of all of it; default false."}
                }}
            }},
            {"required", json::array({"name"})},
            {"additionalProperties", false}
        }}
    });

    tools.push_back({
        {"name", "lookup_symbol_at_address"},
        {"title", "Lookup Symbol at Address"},
        {"description", "Find the symbol at an address."},
        {"annotations", {{"readOnlyHint", true}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"address", {
                    {"type", "string"},
                    {"description", MCP_ADDRESS_DESCRIPTION}
                }}
            }},
            {"required", json::array({"address"})},
            {"additionalProperties", false}
        }}
    });

    // System hardware tools
    tools.push_back({
        {"name", "get_pic_status"},
        {"title", "Get PIC Status"},
        {"description", "Read the two 8259A interrupt controllers: per-IRQ source, input level, request, in-service, mask and vector, and each PIC's ICW1-ICW4 decoded, init state, read register, special mask, poll and priority."},
        {"annotations", {{"readOnlyHint", true}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", json::object()},
            {"additionalProperties", false}
        }}
    });

    tools.push_back({
        {"name", "get_pit_status"},
        {"title", "Get PIT Status"},
        {"description", "Read the timers: board register 0060 (timer enables, latches, SOUND, IRQ0) and the six 8253 counters with use, clock, mode, access, BCD, reload, live count, OUT and period."},
        {"annotations", {{"readOnlyHint", true}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", json::object()},
            {"additionalProperties", false}
        }}
    });

    tools.push_back({
        {"name", "get_dma_status"},
        {"title", "Get DMA Status"},
        {"description", "Read the uPD71071 DMA controller: device control bits, mask, software requests, request levels, terminal count, selected channel, and the four channels with device, mode, current and base address and count."},
        {"annotations", {{"readOnlyHint", true}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", json::object()},
            {"additionalProperties", false}
        }}
    });

    tools.push_back({
        {"name", "get_rtc_status"},
        {"title", "Get RTC Status"},
        {"description", "Read the MSM58321 clock: date, time, weekday, 12/24 hour, PM, leap phase, the 0070/0080 interface and the 16 registers."},
        {"annotations", {{"readOnlyHint", true}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", json::object()},
            {"additionalProperties", false}
        }}
    });

    tools.push_back({
        {"name", "get_system_status"},
        {"title", "Get System Status"},
        {"description", "Read system control: machine model, CPU, clock, RAM and floppy drives, reset cause and power-off request, low memory and boot ROM windows, dictionary bank, CMOS write protect, RAM wait and the serial ID ROM."},
        {"annotations", {{"readOnlyHint", true}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", json::object()},
            {"additionalProperties", false}
        }}
    });

    tools.push_back({
        {"name", "get_keyboard_status"},
        {"title", "Get Keyboard Status"},
        {"description", "Read the keyboard interface: data, status, IRQ enable, KBINT, last command, the queued scan bytes with key names, and held keys."},
        {"annotations", {{"readOnlyHint", true}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", json::object()},
            {"additionalProperties", false}
        }}
    });

    // Video tools
    tools.push_back({
        {"name", "get_crtc_status"},
        {"title", "Get CRTC Status"},
        {"description", "Read the CRTC: dot clock, line and frame timing, interlace, sync widths, the beam line, dot, field and H/V state, the VSYNC IRQ, and per layer format, windows, VRAM start, stride, HAJ, field offset, zoom and visible size."},
        {"annotations", {{"readOnlyHint", true}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", json::object()},
            {"additionalProperties", false}
        }}
    });

    tools.push_back({
        {"name", "get_crtc_registers"},
        {"title", "Get CRTC Registers"},
        {"description", "Read the 32 CRTC registers R00-R1F with their names and the selected index (0440)."},
        {"annotations", {{"readOnlyHint", true}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", json::object()},
            {"additionalProperties", false}
        }}
    });

    tools.push_back({
        {"name", "write_crtc_register"},
        {"title", "Write CRTC Register"},
        {"description", "Write a CRTC register through the 0440/0442 port path like a CPU write, so timing updates and the trace logger records it, then restore the index."},
        {"annotations", {{"readOnlyHint", false}, {"destructiveHint", true}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"register", {
                    {"type", "integer"},
                    {"description", "Register number 0-31 (R00-R1F)."},
                    {"minimum", 0},
                    {"maximum", 31}
                }},
                {"value", {
                    {"type", "string"},
                    {"description", "16-bit hex value: '1234', '0x1234', or '$1234'."}
                }}
            }},
            {"required", json::array({"register", "value"})}
        }}
    });

    tools.push_back({
        {"name", "get_video_output_status"},
        {"title", "Get Video Output Status"},
        {"description", "Read the output controller (0448/044A) mode, layer formats, front layer and palette select, the FDA0 layer enables, the 044C status, the VRAM write mask and the FM-R display state."},
        {"annotations", {{"readOnlyHint", true}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", json::object()},
            {"additionalProperties", false}
        }}
    });

    tools.push_back({
        {"name", "get_palettes"},
        {"title", "Get Palettes"},
        {"description", "Read the palettes: the two 16 color banks (4-bit B, R, G) and the 256 color palette (8-bit B, R, G), each entry with its RGB888 color, and the FM-R digital palette (IGRB) with its DPMD flag. in_use tells which palettes the display layers read; 32K color layers read none."},
        {"annotations", {{"readOnlyHint", true}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"palette", {
                    {"type", "string"},
                    {"enum", json::array({"all", "layer0", "layer1", "256", "digital"})},
                    {"description", "Palette to read. Default all."}
                }}
            }}
        }}
    });

    tools.push_back({
        {"name", "get_frame_buffer"},
        {"title", "Get Frame Buffer"},
        {"description", "Decode a VRAM buffer as PNG: a layer's whole page with its format and stride, a sprite page (transparent pixels as a checkerboard), or a custom VRAM view to find off-screen graphics. get_crtc_status gives each layer's start, stride and visible size."},
        {"annotations", {{"readOnlyHint", true}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"buffer", {
                    {"type", "string"},
                    {"enum", json::array({"layer0", "layer1", "sprite_display", "sprite_draw", "custom"})},
                    {"description", "Buffer to decode."}
                }},
                {"offset", {
                    {"type", "string"},
                    {"description", "custom: hex byte offset in the two-page VRAM view, 0-7FFFF."}
                }},
                {"format", {
                    {"type", "string"},
                    {"enum", json::array({"16_colors", "256_colors", "32k_colors"})},
                    {"description", "custom: pixel format. Default 16_colors."}
                }},
                {"width", {
                    {"type", "integer"},
                    {"description", "custom: width in pixels, 1-1024. Default 512."},
                    {"minimum", 1},
                    {"maximum", 1024}
                }},
                {"height", {
                    {"type", "integer"},
                    {"description", "custom: height in pixels, 1-512. Default 256."},
                    {"minimum", 1},
                    {"maximum", 512}
                }},
                {"palette", {
                    {"type", "string"},
                    {"enum", json::array({"layer0", "layer1", "256"})},
                    {"description", "custom: palette for 16 color data. Default layer0."}
                }}
            }},
            {"required", json::array({"buffer"})}
        }}
    });

    tools.push_back({
        {"name", "list_sprites"},
        {"title", "List Sprites"},
        {"description", "Read the sprite engine state (enable, busy, first drawn entry, offsets, pages) and list sprite entries with screen position, pattern, colors and flags."},
        {"annotations", {{"readOnlyHint", true}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"start", {
                    {"type", "integer"},
                    {"description", "First entry, 0-1023. Default 0."},
                    {"minimum", 0},
                    {"maximum", 1023}
                }},
                {"count", {
                    {"type", "integer"},
                    {"description", "Maximum entries to return, 1-1024. Default 64."},
                    {"minimum", 1},
                    {"maximum", 1024}
                }},
                {"filter", {
                    {"type", "string"},
                    {"enum", json::array({"all", "drawn", "visible"})},
                    {"description", "all, drawn (from the first drawn entry on) or visible (drawn, not hidden and inside the 256x256 page). Default all."}
                }}
            }}
        }}
    });

    tools.push_back({
        {"name", "get_sprite"},
        {"title", "Get Sprite"},
        {"description", "Read one sprite entry: its 16x16 pattern as an 8x PNG with transparent pixels as a checkerboard, or its details: position, raw words, pattern and color table addresses, and flags."},
        {"annotations", {{"readOnlyHint", true}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"index", {
                    {"type", "integer"},
                    {"description", "Sprite entry, 0-1023."},
                    {"minimum", 0},
                    {"maximum", 1023}
                }},
                {"format", {
                    {"type", "string"},
                    {"enum", json::array({"image", "info"})},
                    {"description", "image (PNG) or info (entry details). Default image."}
                }}
            }},
            {"required", json::array({"index"})}
        }}
    });

    // Audio tools
    tools.push_back({
        {"name", "get_ym3438_status"},
        {"title", "Get YM3438 Status"},
        {"description", "Read the YM3438 FM chip: LFO, channel 3 mode, DAC, timers A and B, status, and per channel the frequency with note, algorithm, feedback, pan, AMS, PMS, channel 3 special frequencies and the four operators with their envelope state and level."},
        {"annotations", {{"readOnlyHint", true}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"channel", {
                    {"type", "integer"},
                    {"description", "Channel 1-6. Default all."},
                    {"minimum", 1},
                    {"maximum", 6}
                }}
            }}
        }}
    });

    tools.push_back({
        {"name", "get_ym3438_registers"},
        {"title", "Get YM3438 Registers"},
        {"description", "Read the YM3438 register file of one part as last written, with register names, channels and operators, and the address latch."},
        {"annotations", {{"readOnlyHint", true}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"part", {
                    {"type", "integer"},
                    {"description", "Part 0 (04D8/04DA, channels 1-3 and mode registers) or 1 (04DC/04DE, channels 4-6). Default 0."},
                    {"minimum", 0},
                    {"maximum", 1}
                }}
            }}
        }}
    });

    tools.push_back({
        {"name", "get_rf5c68_status"},
        {"title", "Get RF5C68 Status"},
        {"description", "Read the RF5C68 PCM chip: sound enable, channel and wave banks, IRQ mask and flags, and the eight channels with envelope, pan, step and playback rate, loop start, start and play address."},
        {"annotations", {{"readOnlyHint", true}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", json::object()},
            {"additionalProperties", false}
        }}
    });

    tools.push_back({
        {"name", "get_sound_status"},
        {"title", "Get Sound Status"},
        {"description", "Read the sound board: both MB87078 electronic volumes (04E0/04E2) with gain, the FM and PCM mutes (04D5) and the output gate (04EC), the 04E9-04EB interrupt causes, mask and flags, and the debugger mutes."},
        {"annotations", {{"readOnlyHint", true}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", json::object()},
            {"additionalProperties", false}
        }}
    });

    tools.push_back({
        {"name", "set_audio_mute"},
        {"title", "Set Audio Mute"},
        {"description", "Mute or unmute an audio source or one channel in the debugger. Emulated state, registers and IRQs are unchanged."},
        {"annotations", {{"readOnlyHint", false}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"source", {
                    {"type", "string"},
                    {"enum", json::array({"fm", "pcm", "cdda"})},
                    {"description", "Audio source."}
                }},
                {"channel", {
                    {"type", "integer"},
                    {"description", "Optional channel: 1-6 for fm, 1-8 for pcm. Omit to mute the whole source."},
                    {"minimum", 1},
                    {"maximum", 8}
                }},
                {"mute", {
                    {"type", "boolean"},
                    {"description", "true to mute, false to unmute."}
                }}
            }},
            {"required", json::array({"source", "mute"})}
        }}
    });

    // CD-ROM tools
    tools.push_back({
        {"name", "get_cdrom_status"},
        {"title", "Get CD-ROM Status"},
        {"description", "Read the CD-ROM controller: 04C0 master status, last command with flags and parameters, status queue, transfer mode, SIRQ/DEI interrupts and IRQ9, drive state, head position, read range, and the disc."},
        {"annotations", {{"readOnlyHint", true}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", json::object()},
            {"additionalProperties", false}
        }}
    });

    tools.push_back({
        {"name", "list_cdrom_tracks"},
        {"title", "List CD-ROM Tracks"},
        {"description", "List the disc TOC: track type, start and end MSF (absolute) and LBA, length, sectors, pregap, and the track under the head."},
        {"annotations", {{"readOnlyHint", true}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", json::object()},
            {"additionalProperties", false}
        }}
    });

    tools.push_back({
        {"name", "get_cdrom_audio_status"},
        {"title", "Get CD Audio Status"},
        {"description", "Read CD audio playback: state, end action, track, start, stop and current position, track position, and the CD-DA electronic volume."},
        {"annotations", {{"readOnlyHint", true}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", json::object()},
            {"additionalProperties", false}
        }}
    });

    tools.push_back({
        {"name", "read_cdrom_sector"},
        {"title", "Read CD-ROM Sector"},
        {"description", "Read one sector of a CD image as hex: user data (2048 bytes) or the raw 2352-byte sector. Does not move the drive head. Not available for physical CD drives."},
        {"annotations", {{"readOnlyHint", true}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"lba", {
                    {"type", "integer"},
                    {"description", "Sector LBA from the start of the program area (MSF minus 00:02:00)."},
                    {"minimum", 0}
                }},
                {"mode", {
                    {"type", "string"},
                    {"enum", json::array({"user", "raw"})},
                    {"description", "user (2048-byte data) or raw (2352 bytes). Default user."}
                }}
            }},
            {"required", json::array({"lba"})}
        }}
    });

    // Floppy tools
    tools.push_back({
        {"name", "get_fdc_status"},
        {"title", "Get FDC Status"},
        {"description", "Read the MB8877 floppy controller: status with its bit names, last command decoded, track, sector and data registers, BUSY/DRQ/INTRQ and IRQ6, drive control, drive status, drive select and switch."},
        {"annotations", {{"readOnlyHint", true}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", json::object()},
            {"additionalProperties", false}
        }}
    });

    tools.push_back({
        {"name", "list_floppy_drives"},
        {"title", "List Floppy Drives"},
        {"description", "List the floppy drives: image and disk name, media, geometry, RPM, write protect, modified, head cylinder, motor, ready and selected."},
        {"annotations", {{"readOnlyHint", true}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", json::object()},
            {"additionalProperties", false}
        }}
    });

    tools.push_back({
        {"name", "list_floppy_sectors"},
        {"title", "List Floppy Sectors"},
        {"description", "List the sectors of one track in disk order: C, H, R, N IDs, size, density, deleted mark, status and image offset."},
        {"annotations", {{"readOnlyHint", true}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"drive", {
                    {"type", "integer"},
                    {"description", "Drive 0 or 1."},
                    {"minimum", 0},
                    {"maximum", 1}
                }},
                {"cylinder", {
                    {"type", "integer"},
                    {"description", "Cylinder 0-81."},
                    {"minimum", 0},
                    {"maximum", 81}
                }},
                {"head", {
                    {"type", "integer"},
                    {"description", "Head 0 or 1."},
                    {"minimum", 0},
                    {"maximum", 1}
                }}
            }},
            {"required", json::array({"drive", "cylinder", "head"})}
        }}
    });

    tools.push_back({
        {"name", "read_floppy_sector"},
        {"title", "Read Floppy Sector"},
        {"description", "Read the first sector with ID R on a track: its IDs, density, deleted mark, status and data as hex."},
        {"annotations", {{"readOnlyHint", true}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"drive", {
                    {"type", "integer"},
                    {"description", "Drive 0 or 1."},
                    {"minimum", 0},
                    {"maximum", 1}
                }},
                {"cylinder", {
                    {"type", "integer"},
                    {"description", "Cylinder 0-81."},
                    {"minimum", 0},
                    {"maximum", 81}
                }},
                {"head", {
                    {"type", "integer"},
                    {"description", "Head 0 or 1."},
                    {"minimum", 0},
                    {"maximum", 1}
                }},
                {"sector", {
                    {"type", "integer"},
                    {"description", "Sector ID R, usually 1 to the sectors per track."},
                    {"minimum", 0},
                    {"maximum", 255}
                }}
            }},
            {"required", json::array({"drive", "cylinder", "head", "sector"})}
        }}
    });

    // Media tools
    tools.push_back({
        {"name", "get_media_info"},
        {"title", "Get Media Info"},
        {"description", "Read loaded media metadata, firmware status and the floppy drives."},
        {"annotations", {{"readOnlyHint", true}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", json::object()},
            {"additionalProperties", false}
        }}
    });

    tools.push_back({
        {"name", "list_recent_media"},
        {"title", "List Recent Media"},
        {"description", "List recent CD images with file_path values for load_media, and recent floppies."},
        {"annotations", {{"readOnlyHint", true}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", json::object()},
            {"additionalProperties", false}
        }}
    });

    tools.push_back({
        {"name", "load_media"},
        {"title", "Load Media"},
        {"description", "Load FM Towns media from a local file path."},
        {"annotations", {{"readOnlyHint", false}, {"destructiveHint", true}, {"idempotentHint", false}, {"openWorldHint", true}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"file_path", {{"type", "string"}, {"description", "Absolute media file path."}}}
            }},
            {"required", json::array({"file_path"})},
            {"additionalProperties", false}
        }}
    });

    tools.push_back({
        {"name", "insert_floppy"},
        {"title", "Insert Floppy"},
        {"description", "Insert a floppy image (.d77, .d88, .hdm, .img, .xdf, an .m3u list or a .zip) into a drive. Changes to the previous disk are kept in its working copy."},
        {"annotations", {{"readOnlyHint", false}, {"destructiveHint", false}, {"idempotentHint", false}, {"openWorldHint", true}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"drive", {
                    {"type", "integer"},
                    {"description", "Drive 0 or 1."},
                    {"minimum", 0},
                    {"maximum", 1}
                }},
                {"file_path", {
                    {"type", "string"},
                    {"description", "Absolute path to the floppy image."}
                }},
                {"write_protected", {
                    {"type", "boolean"},
                    {"description", "Optional write protect tab; default keeps the drive setting."}
                }}
            }},
            {"required", json::array({"drive", "file_path"})}
        }}
    });

    tools.push_back({
        {"name", "eject_floppy"},
        {"title", "Eject Floppy"},
        {"description", "Eject the disk in a drive; its changes are kept in the working copy."},
        {"annotations", {{"readOnlyHint", false}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"drive", {
                    {"type", "integer"},
                    {"description", "Drive 0 or 1."},
                    {"minimum", 0},
                    {"maximum", 1}
                }}
            }},
            {"required", json::array({"drive"})}
        }}
    });

    tools.push_back({
        {"name", "swap_floppies"},
        {"title", "Swap Floppies"},
        {"description", "Swap the disks in drives 0 and 1."},
        {"annotations", {{"readOnlyHint", false}, {"destructiveHint", false}, {"idempotentHint", false}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", json::object()},
            {"additionalProperties", false}
        }}
    });

    tools.push_back({
        {"name", "set_floppy_write_protect"},
        {"title", "Set Floppy Write Protect"},
        {"description", "Set or clear the write protect tab of the disk in a drive."},
        {"annotations", {{"readOnlyHint", false}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"drive", {
                    {"type", "integer"},
                    {"description", "Drive 0 or 1."},
                    {"minimum", 0},
                    {"maximum", 1}
                }},
                {"write_protected", {
                    {"type", "boolean"},
                    {"description", "true to protect, false to allow writes."}
                }}
            }},
            {"required", json::array({"drive", "write_protected"})}
        }}
    });

    tools.push_back({
        {"name", "load_bios"},
        {"title", "Load BIOS"},
        {"description", "Load the FM Towns firmware set from a local directory, reset, and stop at the reset vector."},
        {"annotations", {{"readOnlyHint", false}, {"destructiveHint", true}, {"idempotentHint", false}, {"openWorldHint", true}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"directory_path", {{"type", "string"}, {"description", "Absolute directory containing FMT_SYS.ROM and companion firmware files."}}}
            }},
            {"required", json::array({"directory_path"})},
            {"additionalProperties", false}
        }}
    });

    // Capture tools
    tools.push_back({
        {"name", "get_screenshot"},
        {"title", "Get Screenshot"},
        {"description", "Capture current screen/frame/video output as PNG screenshot image."},
        {"annotations", {{"readOnlyHint", true}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", json::object()},
            {"additionalProperties", false}
        }}
    });

    tools.push_back({
        {"name", "start_video_recording"},
        {"title", "Start Video Recording"},
        {"description", "Start recording emulated video and audio to an AVI file (MJPEG or uncompressed video, PCM audio). The file is written to disk only; its path is returned. Options given here update the recording settings, same as the GUI menu."},
        {"annotations", {{"readOnlyHint", false}, {"destructiveHint", true}, {"idempotentHint", false}, {"openWorldHint", true}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"file_path", {
                    {"type", "string"},
                    {"description", "Absolute destination .avi file path. If omitted, an automatic name is used in the configured video recordings directory."}
                }},
                {"scale", {
                    {"type", "integer"},
                    {"description", "Output height multiplier (1-20)."},
                    {"minimum", 1},
                    {"maximum", 20}
                }},
                {"aspect_ratio", {
                    {"type", "string"},
                    {"description", "Output aspect ratio. screen follows the current display settings (debug output screen settings while debugging)."},
                    {"enum", json::array({"screen", "square", "4:3", "16:9"})}
                }},
                {"quality", {
                    {"type", "string"},
                    {"description", "low and medium halve color resolution; lossless writes uncompressed video with very large files."},
                    {"enum", json::array({"low", "medium", "high", "lossless"})}
                }}
            }},
            {"additionalProperties", false}
        }}
    });

    tools.push_back({
        {"name", "stop_video_recording"},
        {"title", "Stop Video Recording"},
        {"description", "Stop the active video recording and finalize the AVI file. Returns the file path and recorded frame count."},
        {"annotations", {{"readOnlyHint", false}, {"destructiveHint", false}, {"idempotentHint", false}, {"openWorldHint", true}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", json::object()},
            {"additionalProperties", false}
        }}
    });

    // Save state tools
    tools.push_back({
        {"name", "list_save_state_slots"},
        {"title", "List Save State Slots"},
        {"description", "List save-state slots: slot, media name, timestamp, screenshot flag."},
        {"annotations", {{"readOnlyHint", true}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", true}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", json::object()},
            {"additionalProperties", false}
        }}
    });

    tools.push_back({
        {"name", "select_save_state_slot"},
        {"title", "Select Save State Slot"},
        {"description", "Select active save-state slot 1-5 for save_state/load_state."},
        {"annotations", {{"readOnlyHint", false}, {"destructiveHint", true}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"slot", {
                    {"type", "integer"},
                    {"description", "Slot number 1-5."},
                    {"minimum", 1},
                    {"maximum", 5}
                }}
            }},
            {"required", json::array({"slot"})}
        }}
    });

    tools.push_back({
        {"name", "save_state"},
        {"title", "Save State"},
        {"description", "Save emulator state to active save-state slot."},
        {"annotations", {{"readOnlyHint", false}, {"destructiveHint", true}, {"idempotentHint", false}, {"openWorldHint", true}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", json::object()},
            {"additionalProperties", false}
        }}
    });

    tools.push_back({
        {"name", "load_state"},
        {"title", "Load State"},
        {"description", "Load emulator state from active save-state slot."},
        {"annotations", {{"readOnlyHint", false}, {"destructiveHint", true}, {"idempotentHint", true}, {"openWorldHint", true}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", json::object()},
            {"additionalProperties", false}
        }}
    });

    tools.push_back({
        {"name", "save_state_file"},
        {"title", "Save State File"},
        {"description", "Save emulator state to an explicit file path."},
        {"annotations", {{"readOnlyHint", false}, {"destructiveHint", true}, {"idempotentHint", false}, {"openWorldHint", true}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"file_path", {
                    {"type", "string"},
                    {"description", "Absolute destination file path."}
                }}
            }},
            {"required", json::array({"file_path"})},
            {"additionalProperties", false}
        }}
    });

    tools.push_back({
        {"name", "load_state_file"},
        {"title", "Load State File"},
        {"description", "Load emulator state from an explicit file path."},
        {"annotations", {{"readOnlyHint", false}, {"destructiveHint", true}, {"idempotentHint", true}, {"openWorldHint", true}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"file_path", {
                    {"type", "string"},
                    {"description", "Absolute save-state file path."}
                }}
            }},
            {"required", json::array({"file_path"})},
            {"additionalProperties", false}
        }}
    });

    // Rewind tools
    tools.push_back({
        {"name", "get_rewind_status"},
        {"title", "Get Rewind Status"},
        {"description", "Read rewind buffer: snapshot count, capacity, buffered seconds, memory usage, settings."},
        {"annotations", {{"readOnlyHint", true}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", json::object()},
            {"additionalProperties", false}
        }}
    });

    tools.push_back({
        {"name", "rewind_seek"},
        {"title", "Rewind Seek"},
        {"description", "Load rewind snapshot by number (1 oldest, snapshot_count newest); refresh screen; non-consuming seek."},
        {"annotations", {{"readOnlyHint", false}, {"destructiveHint", true}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"snapshot", {
                    {"type", "integer"},
                    {"description", "Snapshot number: 1 oldest, snapshot_count newest."},
                    {"minimum", 1}
                }}
            }},
            {"required", json::array({"snapshot"})},
            {"additionalProperties", false}
        }}
    });

    // Controller input tools
    tools.push_back({
        {"name", "controller_button"},
        {"title", "Controller Button"},
        {"description", "Press, release, or tap a button on either FM Towns game port. On a mouse port the directions move the mouse (held: 4 counts per frame, tap: 4 counts) and A/B are the left/right buttons."},
        {"annotations", {{"readOnlyHint", false}, {"destructiveHint", true}, {"idempotentHint", false}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"player", {
                    {"type", "integer"},
                    {"description", "Player number 1-2."},
                    {"minimum", 1},
                    {"maximum", 2}
                }},
                {"button", {
                    {"type", "string"},
                    {"description", "Button: up, down, left, right, select, run, A, B, C, X, Y, Z."},
                    {"enum", json::array({"up", "down", "left", "right", "select", "run", "A", "B", "C", "X", "Y", "Z"})}
                }},
                {"action", {
                    {"type", "string"},
                    {"description", "Action; press_and_release auto-releases after several frames."},
                    {"enum", json::array({"press", "release", "press_and_release"})}
                }}
            }},
            {"required", json::array({"player", "button", "action"})}
        }}
    });

    tools.push_back({
        {"name", "controller_set_type"},
        {"title", "Set Controller Type"},
        {"description", "Set the device on a game port: none, original_gamepad, marty_gamepad, six_button_gamepad or mouse."},
        {"annotations", {{"readOnlyHint", false}, {"destructiveHint", true}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"player", {
                    {"type", "integer"},
                    {"description", "Player number 1-2."},
                    {"minimum", 1},
                    {"maximum", 2}
                }},
                {"type", {
                    {"type", "string"},
                    {"description", "Device type."},
                    {"enum", json::array({"none", "original_gamepad", "marty_gamepad", "six_button_gamepad", "mouse"})}
                }}
            }},
            {"required", json::array({"player", "type"})}
        }}
    });

    tools.push_back({
        {"name", "controller_get_type"},
        {"title", "Get Controller Type"},
        {"description", "Read the device on a game port: none, original_gamepad, marty_gamepad, six_button_gamepad or mouse."},
        {"annotations", {{"readOnlyHint", true}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"player", {
                    {"type", "integer"},
                    {"description", "Player number 1-2."},
                    {"minimum", 1},
                    {"maximum", 2}
                }}
            }},
            {"required", json::array({"player"})}
        }}
    });

    tools.push_back({
        {"name", "get_input_state"},
        {"title", "Get Input State"},
        {"description", "Get each port's device type and effective pressed buttons, held keyboard keys and pending tap releases."},
        {"annotations", {{"readOnlyHint", true}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", json::object()},
            {"additionalProperties", false}
        }}
    });

    tools.push_back({
        {"name", "keyboard_key"},
        {"title", "Keyboard Key"},
        {"description", "Press, release or tap a key of the FM Towns JIS keyboard by name, e.g. RETURN, SPACE, A, 1, PF1, "
            "SHIFT, CTRL, ESCAPE, UP, KP_ENTER, HIRAGANA, EXECUTE."},
        {"annotations", {{"readOnlyHint", false}, {"destructiveHint", true}, {"idempotentHint", false}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"key", {
                    {"type", "string"},
                    {"description", "Key name as in get_input_state keys_held (case-insensitive); ENTER and ESC are accepted."}
                }},
                {"action", {
                    {"type", "string"},
                    {"description", "Action; press_and_release auto-releases after several frames."},
                    {"enum", json::array({"press", "release", "press_and_release"})}
                }}
            }},
            {"required", json::array({"key", "action"})}
        }}
    });

    tools.push_back({
        {"name", "keyboard_type"},
        {"title", "Keyboard Type"},
        {"description", "Type ASCII text on the FM Towns keyboard through a frame macro (Shift is pressed for "
            "uppercase and shifted symbols on the JIS layout; newline is RETURN). For Towns OS and DOS prompts."},
        {"annotations", {{"readOnlyHint", false}, {"destructiveHint", true}, {"idempotentHint", false}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"text", {
                    {"type", "string"},
                    {"description", "ASCII text to type, at most 256 characters."},
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
        }}
    });

    // Trace and profiler tools
    json trace_filter_names = json::array();

    for (int i = 0; i < k_mcp_trace_filter_count; i++)
        trace_filter_names.push_back(k_mcp_trace_filters[i].name);

    tools.push_back({
        {"name", "get_trace_log"},
        {"title", "Get Trace Log"},
        {"description", "Read trace log entries: CPU instructions and hardware events. Lines start with the CPU clock."},
        {"annotations", {{"readOnlyHint", true}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"start", {
                    {"type", "integer"},
                    {"description", "Absolute trace sequence, or a negative value to read that many entries from the retained tail (omit for latest 100)"}
                }},
                {"count", {
                    {"type", "integer"},
                    {"description", "Entries to return (default 100, max 1000)"},
                    {"minimum", 1},
                    {"maximum", 1000}
                }}
            }},
            {"additionalProperties", false}
        }}
    });

    tools.push_back({
        {"name", "set_trace_log"},
        {"title", "Set Trace Logger"},
        {"description", "Enable/disable trace logging to memory or disk; configure capacity, file limit, output directory, and event filters. It records while the debugger runs the machine, so enabling it turns the debugger on, like the profiler."},
        {"annotations", {{"readOnlyHint", false}, {"destructiveHint", true}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"enabled", {
                    {"type", "boolean"},
                    {"description", "true starts logging and opens the Trace Logger window, false stops; preserves entries."}
                }},
                {"output", {
                    {"type", "string"},
                    {"description", "Trace destination. Defaults to memory when starting a stopped logger."},
                    {"enum", json::array({"memory", "disk"})}
                }},
                {"memory_size", {
                    {"type", "string"},
                    {"description", "Maximum entries retained in memory mode."},
                    {"enum", json::array({"100K", "500K", "1M", "2M", "5M"})}
                }},
                {"disk_size", {
                    {"type", "string"},
                    {"description", "Maximum disk trace file size."},
                    {"enum", json::array({"10MB", "50MB", "100MB", "250MB", "500MB", "1GB", "unbounded"})}
                }},
                {"output_path", {
                    {"type", "string"},
                    {"description", "Directory for the automatically named disk trace file."}
                }},
                {"vblank_watch_address", {
                    {"type", "string"},
                    {"description", "Linear address hex watched by video.missed_vblank: '1234ABCD', '0x1234ABCD', or '$1234ABCD'. Omit to keep current."}
                }},
                {"vblank_watch_operation", {
                    {"type", "string"},
                    {"description", "Access that marks a frame as on time for video.missed_vblank. Omit to keep current."},
                    {"enum", json::array({"read", "write", "read_write"})}
                }},
                {"filters", {
                    {"type", "array"},
                    {"description", "Exact event streams to record. Defaults to CPU instructions, IRQs and exceptions."},
                    {"items", {
                        {"type", "string"},
                        {"enum", trace_filter_names}
                    }},
                    {"minItems", 1},
                    {"uniqueItems", true}
                }}
            }},
            {"required", json::array({"enabled"})},
            {"additionalProperties", false}
        }}
    });

    tools.push_back({
        {"name", "set_profiler"},
        {"title", "Set Profiler"},
        {"description", "Start (opens the Profiler window), stop (closes it), or reset the function profiler. Collects only while the window is visible and the debugger runs the machine."},
        {"annotations", {{"readOnlyHint", false}, {"destructiveHint", true}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"action", {
                    {"type", "string"},
                    {"description", "start opens the window and resumes collection; stop closes the window; reset clears collected data."},
                    {"enum", json::array({"start", "stop", "reset"})}
                }}
            }},
            {"required", json::array({"action"})},
            {"additionalProperties", false}
        }}
    });

    tools.push_back({
        {"name", "get_profiler_data"},
        {"title", "Get Profiler Data"},
        {"description", "Read function profiler results: totals plus per-function symbol, address, interrupt vector, calls, calls per frame, inclusive/exclusive cycles and percentages, and avg/min/max cycles per call."},
        {"annotations", {{"readOnlyHint", true}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"sort", {
                    {"type", "string"},
                    {"description", "Sort order, highest first (default inclusive)."},
                    {"enum", json::array({"inclusive", "exclusive", "calls", "average", "max"})}
                }},
                {"count", {
                    {"type", "integer"},
                    {"description", "Functions to return (default 50, max 1000)"},
                    {"minimum", 1},
                    {"maximum", 1000}
                }},
                {"filter", {
                    {"type", "string"},
                    {"description", "Case-insensitive substring matched against function name or hex address."}
                }}
            }},
            {"additionalProperties", false}
        }}
    });

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

    if (!value.is_string() || !parse_hex_with_prefix(value.get<std::string>(), &address))
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
    std::string compact;

    for (size_t i = 0; i < text.size(); i++)
    {
        if (!isspace((unsigned char)text[i]))
            compact += text[i];
    }

    if (compact.empty() || (compact.size() & 1) != 0)
        return false;

    for (size_t i = 0; i < compact.size(); i += 2)
    {
        u8 byte = 0;

        if (!parse_hex_string(compact.c_str() + i, 2, &byte))
            return false;

        data.push_back(byte);
    }

    return true;
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
    else if (normalizedTool == "debug_step_frame")
    {
        m_debugAdapter.StepFrame();
        return {{"success", true}, {"pending", true}};
    }
    else if (normalizedTool == "debug_reset")
    {
        m_debugAdapter.Reset();
        return {{"success", true}};
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
    else if (normalizedTool == "set_fast_forward_speed")
    {
        int speed = arguments["speed"];
        return m_debugAdapter.SetFastForwardSpeed(speed);
    }
    else if (normalizedTool == "toggle_fast_forward")
    {
        bool enabled = arguments["enabled"];
        return m_debugAdapter.ToggleFastForward(enabled);
    }
    // Breakpoints
    else if (normalizedTool == "set_breakpoint")
    {
        u8 type = 0;
        u8 space = 0;
        u32 start = 0;

        if (!ParseBreakpointType(arguments, type, space, error) ||
            !ParseBreakpointAddress(m_debugAdapter, arguments.value("address", json()), space, start, error))
            return {{"error", error}};

        return m_debugAdapter.SetBreakpoint(start, start, false, type, space);
    }
    else if (normalizedTool == "set_breakpoint_range")
    {
        u8 type = 0;
        u8 space = 0;
        u32 start = 0;
        u32 end = 0;

        if (!ParseBreakpointType(arguments, type, space, error) ||
            !ParseBreakpointAddress(m_debugAdapter, arguments.value("start_address", json()), space, start, error) ||
            !ParseBreakpointAddress(m_debugAdapter, arguments.value("end_address", json()), space, end, error))
            return {{"error", error}};

        if (start > end)
            return {{"error", "start_address must be <= end_address"}};

        return m_debugAdapter.SetBreakpoint(start, end, start != end, type, space);
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
    else if (normalizedTool == "set_breakpoint_on_interrupt")
    {
        return m_debugAdapter.SetInterruptBreakpoint(arguments.value("vector", -1), arguments.value("source", "any"));
    }
    else if (normalizedTool == "clear_breakpoint_on_interrupt")
    {
        return m_debugAdapter.ClearInterruptBreakpoint(arguments.value("vector", -1), arguments.value("source", "any"));
    }
    else if (normalizedTool == "list_breakpoints_on_interrupt")
    {
        return m_debugAdapter.ListInterruptBreakpoints();
    }
    else if (normalizedTool == "set_breakpoint_on_irq")
    {
        return m_debugAdapter.SetIRQBreakpoint(arguments.value("irq", -1));
    }
    else if (normalizedTool == "clear_breakpoint_on_irq")
    {
        return m_debugAdapter.ClearIRQBreakpoint(arguments.value("irq", -1));
    }
    else if (normalizedTool == "list_breakpoints_on_irq")
    {
        return m_debugAdapter.ListIRQBreakpoints();
    }
    else if (normalizedTool == "list_breakpoints")
    {
        return m_debugAdapter.ListBreakpoints();
    }
    else if (normalizedTool == "enable_breakpoint")
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

        return m_debugAdapter.EnableBreakpoint(start, end, range, type, space, arguments.value("enabled", true));
    }
    else if (normalizedTool == "set_breakpoints_active")
    {
        return m_debugAdapter.SetBreakpointsActive(arguments.value("active", true));
    }
    else if (normalizedTool == "enable_breakpoint_on_interrupt")
    {
        return m_debugAdapter.EnableInterruptBreakpoint(arguments.value("vector", -1), arguments.value("source", "any"),
            arguments.value("enabled", true));
    }
    else if (normalizedTool == "enable_breakpoint_on_irq")
    {
        return m_debugAdapter.EnableIRQBreakpoint(arguments.value("irq", -1), arguments.value("enabled", true));
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

        if (!ParseHexBytes(arguments["bytes"], data))
            return {{"error", "Invalid byte format"}};

        return m_debugAdapter.WriteMemory(arguments["area"], arguments["offset"], data);
    }
    else if (normalizedTool == "translate_address")
    {
        return m_debugAdapter.TranslateAddress(arguments["address"]);
    }
    else if (normalizedTool == "select_memory_range")
    {
        u32 start_address = 0;
        u32 end_address = 0;

        if (!parse_hex_with_prefix(arguments["start_address"].get<std::string>(), &start_address))
            return {{"error", "Invalid start_address format"}};

        if (!parse_hex_with_prefix(arguments["end_address"].get<std::string>(), &end_address))
            return {{"error", "Invalid end_address format"}};

        return m_debugAdapter.SelectMemoryRange(arguments["area"], start_address, end_address);
    }
    else if (normalizedTool == "set_memory_selection_value")
    {
        u8 value = 0;

        if (!parse_hex_with_prefix(arguments["value"].get<std::string>(), &value))
            return {{"error", "Invalid value format"}};

        return m_debugAdapter.SetMemorySelectionValue(arguments["area"], value);
    }
    else if (normalizedTool == "get_memory_selection")
    {
        return m_debugAdapter.GetMemorySelection(arguments["area"]);
    }
    else if (normalizedTool == "add_memory_bookmark")
    {
        u32 offset = 0;

        if (!parse_hex_with_prefix(arguments["address"].get<std::string>(), &offset))
            return {{"error", "Invalid address format"}};

        return m_debugAdapter.AddMemoryBookmark(arguments["area"], offset, arguments.value("name", ""));
    }
    else if (normalizedTool == "remove_memory_bookmark")
    {
        u32 offset = 0;

        if (!parse_hex_with_prefix(arguments["address"].get<std::string>(), &offset))
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

        if (!parse_hex_with_prefix(arguments["address"].get<std::string>(), &offset))
            return {{"error", "Invalid address format"}};

        return m_debugAdapter.AddMemoryWatch(arguments["area"], offset, arguments.value("notes", ""),
            arguments.value("size", 8));
    }
    else if (normalizedTool == "remove_memory_watch")
    {
        u32 offset = 0;

        if (!parse_hex_with_prefix(arguments["address"].get<std::string>(), &offset))
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

        if (!parse_hex_with_prefix(arguments["value"].get<std::string>(), &value))
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
        return m_debugAdapter.ListSymbols(arguments.value("filter", ""), arguments.value("start", 0),
            arguments.value("count", 200));
    }
    else if (normalizedTool == "lookup_symbol_by_name")
    {
        return m_debugAdapter.LookupSymbolByName(arguments["name"], arguments.value("partial", false));
    }
    else if (normalizedTool == "lookup_symbol_at_address")
    {
        if (!m_debugAdapter.ParseAddress(arguments["address"], address, error))
            return {{"error", error}};

        return m_debugAdapter.LookupSymbolAtAddress(address.linear);
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
        return m_debugAdapter.GetCDROMStatus();
    }
    else if (normalizedTool == "list_cdrom_tracks")
    {
        return m_debugAdapter.ListCDROMTracks();
    }
    else if (normalizedTool == "get_cdrom_audio_status")
    {
        return m_debugAdapter.GetCDROMAudioStatus();
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
    else if (normalizedTool == "list_floppy_sectors")
    {
        return m_debugAdapter.ListFloppySectors(arguments.value("drive", -1), arguments.value("cylinder", -1),
            arguments.value("head", -1));
    }
    else if (normalizedTool == "read_floppy_sector")
    {
        return m_debugAdapter.ReadFloppySector(arguments.value("drive", -1), arguments.value("cylinder", -1),
            arguments.value("head", -1), arguments.value("sector", -1));
    }
    else if (normalizedTool == "insert_floppy")
    {
        json write_protected = arguments.contains("write_protected") ? arguments["write_protected"] : json();
        return m_debugAdapter.InsertFloppy(arguments.value("drive", -1), arguments.value("file_path", ""), write_protected);
    }
    else if (normalizedTool == "eject_floppy")
    {
        return m_debugAdapter.EjectFloppy(arguments.value("drive", -1));
    }
    else if (normalizedTool == "swap_floppies")
    {
        return m_debugAdapter.SwapFloppies();
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
    else if (normalizedTool == "load_bios")
    {
        return m_debugAdapter.LoadBios(arguments["directory_path"]);
    }
    // Trace and profiler
    else if (normalizedTool == "get_trace_log")
    {
        return m_debugAdapter.GetTraceLog(arguments.value("start", (s64)-100), arguments.value("count", 100));
    }
    else if (normalizedTool == "set_trace_log")
    {
        bool enabled = arguments["enabled"];
        u32 flags = TRACE_FLAG_CPU | TRACE_FLAG_CPU_INTERRUPT;
        u32 event_filters[TRACE_TYPE_COUNT] = {};
        event_filters[TRACE_CPU_INTERRUPT] = TRACE_CPU_INTERRUPT_EVENT_DEFAULT;
        event_filters[TRACE_IO] = TRACE_IO_EVENT_ALL;
        event_filters[TRACE_PIC] = TRACE_PIC_EVENT_ALL;
        event_filters[TRACE_TIMER] = TRACE_TIMER_EVENT_ALL;
        event_filters[TRACE_DMA] = TRACE_DMA_EVENT_ALL;
        event_filters[TRACE_VIDEO] = TRACE_VIDEO_EVENT_DEFAULT;
        event_filters[TRACE_SPRITE] = TRACE_SPRITE_EVENT_ALL;
        event_filters[TRACE_FM] = TRACE_FM_EVENT_DEFAULT;
        event_filters[TRACE_PCM] = TRACE_PCM_EVENT_ALL;
        event_filters[TRACE_MIXER] = TRACE_MIXER_EVENT_ALL;
        event_filters[TRACE_CDROM] = TRACE_CDROM_EVENT_ALL;
        event_filters[TRACE_FDC] = TRACE_FDC_EVENT_ALL;
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
                    return {{"error", "Unknown trace filter: " + filter}};
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
    else if (normalizedTool == "select_save_state_slot")
    {
        int slot = arguments["slot"];
        return m_debugAdapter.SelectSaveStateSlot(slot);
    }
    else if (normalizedTool == "save_state")
    {
        return m_debugAdapter.SaveState();
    }
    else if (normalizedTool == "load_state")
    {
        return m_debugAdapter.LoadState();
    }
    else if (normalizedTool == "save_state_file")
    {
        return m_debugAdapter.SaveStateFile(arguments["file_path"]);
    }
    else if (normalizedTool == "load_state_file")
    {
        return m_debugAdapter.LoadStateFile(arguments["file_path"]);
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
    // Input
    else if (normalizedTool == "controller_button")
    {
        return m_debugAdapter.ControllerButton(arguments["player"], arguments["button"], arguments["action"]);
    }
    else if (normalizedTool == "controller_set_type")
    {
        return m_debugAdapter.ControllerSetType(arguments["player"], arguments["type"]);
    }
    else if (normalizedTool == "controller_get_type")
    {
        return m_debugAdapter.ControllerGetType(arguments["player"]);
    }
    else if (normalizedTool == "keyboard_type")
    {
        return {{"error", normalizedTool + " must be handled by the MCP manager"}};
    }
    else if (normalizedTool == "get_input_state")
    {
        return m_debugAdapter.GetInputState();
    }
    else if (normalizedTool == "keyboard_key")
    {
        return m_debugAdapter.KeyboardKey(arguments["key"], arguments["action"]);
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
    std::ifstream file(tocPath);

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
        resource.category = category;
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
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);

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
