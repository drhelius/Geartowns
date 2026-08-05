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
#include "log.h"

bool g_mcp_router_enabled = false;

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

    const auto reject_or_send_error = [this, is_notification](const json& id, int code, const std::string& message)
    {
        if (is_notification)
            m_transport->reject_notification();
        else
            SendError(id, code, message);
    };

    if (request.contains("id") && !request["id"].is_string() &&
        !request["id"].is_number_integer() && !request["id"].is_number_unsigned())
    {
        reject_or_send_error(json(), MCP_ERROR_INVALID_REQUEST, "Invalid Request: id must be a string or integer");
        return;
    }

    json request_id = request.contains("id") ? request["id"] : json();

    if (!request.contains("jsonrpc") || request["jsonrpc"] != "2.0")
    {
        reject_or_send_error(json(), MCP_ERROR_INVALID_REQUEST, "Invalid Request: missing or invalid jsonrpc version");
        return;
    }

    if (!request.contains("method") || !request["method"].is_string())
    {
        reject_or_send_error(json(), MCP_ERROR_INVALID_REQUEST, "Invalid Request: missing method");
        return;
    }

    method = request["method"];

    if (request.contains("params") && !request["params"].is_object())
    {
        reject_or_send_error(request_id, MCP_ERROR_INVALID_PARAMS, "Invalid params: expected an object");
        return;
    }

    if (method == "initialize" && is_notification)
    {
        reject_or_send_error(json(), MCP_ERROR_INVALID_REQUEST, "Initialize must be a request");
        return;
    }

    if (method == "initialize" && m_initialized)
    {
        reject_or_send_error(request_id, MCP_ERROR_INVALID_REQUEST, "Server already initialized");
        return;
    }

    if (!m_initialized && method != "initialize" && method != "ping")
    {
        reject_or_send_error(request_id, MCP_ERROR_INVALID_REQUEST, "Server not initialized");
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
            {"tools", json::object()}
        }},
        {"serverInfo", {
            {"name", "geartowns-mcp-server"},
            {"title", "Geartowns MCP Server"},
            {"description", "Control the Geartowns FM Towns emulator through generic execution, media, screenshot, fast-forward, and controller tools."},
            {"version", GT_VERSION}
        }}
    };

    response["result"]["instructions"] =
        "Use this server to pause, continue, reset, load media or BIOS files, capture screenshots, "
        "control fast-forward, and inspect or update the two controller ports.";

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

    tools.push_back({
        {"name", "debug_pause"},
        {"title", "Debug Pause"},
        {"description", "Pause emulator execution."},
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
        {"description", "Resume emulator execution."},
        {"annotations", {{"readOnlyHint", false}, {"destructiveHint", true}, {"idempotentHint", true}, {"openWorldHint", false}}},
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
        {"description", "Read emulator state: paused, media loading/ready, and current frame."},
        {"annotations", {{"readOnlyHint", true}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", json::object()},
            {"additionalProperties", false}
        }}
    });

    tools.push_back({
        {"name", "get_media_info"},
        {"title", "Get Media Info"},
        {"description", "Read loaded media metadata and BIOS readiness."},
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
        {"description", "List recent media with file_path values for load_media."},
        {"annotations", {{"readOnlyHint", true}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", json::object()},
            {"additionalProperties", false}
        }}
    });

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
        {"name", "load_bios"},
        {"title", "Load BIOS"},
        {"description", "Load FM Towns BIOS files from a local path."},
        {"annotations", {{"readOnlyHint", false}, {"destructiveHint", true}, {"idempotentHint", false}, {"openWorldHint", true}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"file_path", {{"type", "string"}, {"description", "Absolute BIOS file path."}}}
            }},
            {"required", json::array({"file_path"})},
            {"additionalProperties", false}
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
                {"speed", {{"type", "integer"}, {"description", "Speed index 0-4."}, {"minimum", 0}, {"maximum", 4}}}
            }},
            {"required", json::array({"speed"})},
            {"additionalProperties", false}
        }}
    });

    tools.push_back({
        {"name", "toggle_fast_forward"},
        {"title", "Toggle Fast Forward"},
        {"description", "Enable or disable fast-forward mode at the configured speed."},
        {"annotations", {{"readOnlyHint", false}, {"destructiveHint", true}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"enabled", {{"type", "boolean"}, {"description", "true enables fast forward; false disables it."}}}
            }},
            {"required", json::array({"enabled"})},
            {"additionalProperties", false}
        }}
    });

    tools.push_back({
        {"name", "controller_button"},
        {"title", "Controller Button"},
        {"description", "Press, release, or tap a button on either FM Towns gamepad port."},
        {"annotations", {{"readOnlyHint", false}, {"destructiveHint", true}, {"idempotentHint", false}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"player", {{"type", "integer"}, {"description", "Player number 1-2."}, {"minimum", 1}, {"maximum", 2}}},
                {"button", {{"type", "string"}, {"enum", json::array({"up", "down", "left", "right", "start", "run", "A", "B", "C", "X", "Y", "Z"})}}},
                {"action", {{"type", "string"}, {"description", "press_and_release auto-releases after several frames."}, {"enum", json::array({"press", "release", "press_and_release"})}}}
            }},
            {"required", json::array({"player", "button", "action"})},
            {"additionalProperties", false}
        }}
    });

    tools.push_back({
        {"name", "controller_macro"},
        {"title", "Controller Macro"},
        {"description", "Run a frame-based controller macro. Commands are tap, press, release, and wait; player defaults to 1."},
        {"annotations", {{"readOnlyHint", false}, {"destructiveHint", true}, {"idempotentHint", false}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"player", {{"type", "integer"}, {"description", "Default player number 1-2."}, {"minimum", 1}, {"maximum", 2}}},
                {"commands", {
                    {"type", "array"},
                    {"description", "Ordered macro commands."},
                    {"minItems", 1},
                    {"items", {
                        {"type", "object"},
                        {"properties", {
                            {"tap", {{"type", "string"}, {"enum", json::array({"up", "down", "left", "right", "start", "run", "A", "B", "C", "X", "Y", "Z"})}}},
                            {"press", {{"type", "string"}, {"enum", json::array({"up", "down", "left", "right", "start", "run", "A", "B", "C", "X", "Y", "Z"})}}},
                            {"release", {{"type", "string"}, {"enum", json::array({"up", "down", "left", "right", "start", "run", "A", "B", "C", "X", "Y", "Z"})}}},
                            {"wait", {{"type", "integer"}, {"description", "Frames to wait."}, {"minimum", 1}, {"maximum", 1000}}},
                            {"player", {{"type", "integer"}, {"description", "Player override for this command, 1-2."}, {"minimum", 1}, {"maximum", 2}}}
                        }},
                        {"additionalProperties", false}
                    }}
                }}
            }},
            {"required", json::array({"commands"})},
            {"additionalProperties", false}
        }}
    });

    tools.push_back({
        {"name", "get_input_state"},
        {"title", "Get Input State"},
        {"description", "Get effective pressed buttons and pending tap releases."},
        {"annotations", {{"readOnlyHint", true}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", json::object()},
            {"additionalProperties", false}
        }}
    });

    tools.push_back({
        {"name", "controller_set_type"},
        {"title", "Controller Set Type"},
        {"description", "Set a controller port type: none, original, or 6_button."},
        {"annotations", {{"readOnlyHint", false}, {"destructiveHint", true}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"player", {{"type", "integer"}, {"minimum", 1}, {"maximum", 2}}},
                {"type", {{"type", "string"}, {"enum", json::array({"none", "original", "6_button"})}}}
            }},
            {"required", json::array({"player", "type"})},
            {"additionalProperties", false}
        }}
    });

    tools.push_back({
        {"name", "controller_get_type"},
        {"title", "Controller Get Type"},
        {"description", "Read a controller port type."},
        {"annotations", {{"readOnlyHint", true}, {"destructiveHint", false}, {"idempotentHint", true}, {"openWorldHint", false}}},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"player", {{"type", "integer"}, {"minimum", 1}, {"maximum", 2}}}
            }},
            {"required", json::array({"player"})},
            {"additionalProperties", false}
        }}
    });

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
        {"description", "List routed tools in a category with compact descriptions. Use category names returned by list_tool_categories, then call get_tool_info for one tool's input schema."},
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
        {"description", "Return one MCP tool's title, description, category, direct/routed status, and real input schema. Use this after search_tools or get_category_tools before execute_tool."},
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
        {"description", "Search direct and routed MCP tools by keyword, category, title, description, and aliases. Use this when you know what you want to do but not the tool name."},
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


json McpServer::ExecuteCommand(const std::string& toolName, const json& arguments)
{
    std::string normalizedTool = toolName;
    size_t pos = 0;
    while ((pos = normalizedTool.find('.', pos)) != std::string::npos)
    {
        normalizedTool[pos] = '_';
        pos++;
    }

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
    else if (normalizedTool == "debug_reset")
    {
        m_debugAdapter.Reset();
        return {{"success", true}};
    }
    else if (normalizedTool == "debug_get_status")
    {
        return m_debugAdapter.GetDebugStatus();
    }
    else if (normalizedTool == "get_media_info")
        return m_debugAdapter.GetMediaInfo();
    else if (normalizedTool == "list_recent_media")
        return m_debugAdapter.ListRecentMedia();
    else if (normalizedTool == "get_screenshot")
    {
        return m_debugAdapter.GetScreenshot();
    }
    else if (normalizedTool == "load_media")
        return {{"error", "load_media must be handled by the MCP manager"}};
    else if (normalizedTool == "load_bios")
        return m_debugAdapter.LoadBios(arguments["file_path"]);
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
    else if (normalizedTool == "controller_button")
        return m_debugAdapter.ControllerButton(arguments["player"], arguments["button"], arguments["action"]);
    else if (normalizedTool == "controller_macro")
        return {{"error", "controller_macro must be handled by the MCP manager"}};
    else if (normalizedTool == "get_input_state")
        return m_debugAdapter.GetInputState();
    else if (normalizedTool == "controller_set_type")
        return m_debugAdapter.ControllerSetType(arguments["player"], arguments["type"]);
    else if (normalizedTool == "controller_get_type")
        return m_debugAdapter.ControllerGetType(arguments["player"]);
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

void McpServer::LoadResources()
{
    m_resources.clear();
    m_resourceMap.clear();
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

