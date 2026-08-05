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

#include "mcp_tool_registry.h"
#include <cctype>
#include <sstream>

static const char* json_type_name(const json& value)
{
    if (value.is_object()) return "object";
    if (value.is_array()) return "array";
    if (value.is_string()) return "string";
    if (value.is_boolean()) return "boolean";
    if (value.is_number_integer() || value.is_number_unsigned()) return "integer";
    if (value.is_number()) return "number";
    if (value.is_null()) return "null";
    return "unknown";
}

static bool json_type_matches(const json& value, const std::string& type)
{
    if (type == "object") return value.is_object();
    if (type == "array") return value.is_array();
    if (type == "string") return value.is_string();
    if (type == "boolean") return value.is_boolean();
    if (type == "integer") return value.is_number_integer() || value.is_number_unsigned();
    if (type == "number") return value.is_number();
    if (type == "null") return value.is_null();
    return true;
}

static std::string json_path_child(const std::string& path, const std::string& child)
{
    return path.empty() ? child : path + "." + child;
}

static bool validate_json_schema(const json& value, const json& schema, const std::string& path, std::string& error)
{
    if (!schema.is_object())
        return true;

    if (schema.contains("type") && schema["type"].is_string())
    {
        std::string type = schema["type"].get<std::string>();
        if (!json_type_matches(value, type))
        {
            error = "Parameter '" + path + "' must be " + type + ", got " + json_type_name(value);
            return false;
        }
    }

    if (schema.contains("enum") && schema["enum"].is_array())
    {
        bool found = false;
        for (json::const_iterator it = schema["enum"].begin(); it != schema["enum"].end(); ++it)
        {
            if (value == *it)
            {
                found = true;
                break;
            }
        }

        if (!found)
        {
            error = "Parameter '" + path + "' has an invalid value";
            return false;
        }
    }

    if (value.is_number())
    {
        double number = value.get<double>();
        if (schema.contains("minimum") && schema["minimum"].is_number() && number < schema["minimum"].get<double>())
        {
            error = "Parameter '" + path + "' is below the minimum";
            return false;
        }
        if (schema.contains("maximum") && schema["maximum"].is_number() && number > schema["maximum"].get<double>())
        {
            error = "Parameter '" + path + "' is above the maximum";
            return false;
        }
    }

    if (value.is_array())
    {
        if (schema.contains("minItems") && schema["minItems"].is_number_integer() && value.size() < schema["minItems"].get<size_t>())
        {
            error = "Parameter '" + path + "' has too few items";
            return false;
        }
        if (schema.contains("maxItems") && schema["maxItems"].is_number_integer() && value.size() > schema["maxItems"].get<size_t>())
        {
            error = "Parameter '" + path + "' has too many items";
            return false;
        }
        if (schema.contains("items") && schema["items"].is_object())
        {
            for (size_t i = 0; i < value.size(); i++)
            {
                std::ostringstream item_path;
                item_path << path << "[" << i << "]";
                if (!validate_json_schema(value[i], schema["items"], item_path.str(), error))
                    return false;
            }
        }
    }

    if (value.is_object())
    {
        if (schema.contains("required") && schema["required"].is_array())
        {
            for (json::const_iterator it = schema["required"].begin(); it != schema["required"].end(); ++it)
            {
                if (it->is_string() && !value.contains(it->get<std::string>()))
                {
                    error = "Missing required parameter '" + json_path_child(path, it->get<std::string>()) + "'";
                    return false;
                }
            }
        }

        const json* properties = NULL;
        if (schema.contains("properties") && schema["properties"].is_object())
            properties = &schema["properties"];

        for (json::const_iterator it = value.begin(); it != value.end(); ++it)
        {
            std::string child_path = json_path_child(path, it.key());
            if (properties && properties->contains(it.key()))
            {
                if (!validate_json_schema(it.value(), (*properties)[it.key()], child_path, error))
                    return false;
            }
            else if (schema.contains("additionalProperties") && schema["additionalProperties"].is_boolean() && !schema["additionalProperties"].get<bool>())
            {
                error = "Unexpected parameter '" + child_path + "'";
                return false;
            }
            else if (schema.contains("additionalProperties") && schema["additionalProperties"].is_object())
            {
                if (!validate_json_schema(it.value(), schema["additionalProperties"], child_path, error))
                    return false;
            }
        }
    }

    return true;
}

struct McpToolCategory
{
    const char* name;
    const char* title;
    const char* description;
};

struct McpToolCategoryTools
{
    const char* category;
    const char* const* tools;
    size_t count;
};

#define MCP_ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))

static const McpToolCategory kMcpToolCategories[] =
{
    {"execution", "Execution", "Pause, continue, reset, and control fast-forward."},
    {"media", "Media", "Load BIOS or media files and inspect recent or loaded media."},
    {"capture", "Capture", "Capture the current emulator screenshot."},
    {"input", "Input", "Inspect and control the two gamepad ports."},
    {"tools", "Other Tools", "Additional emulator/debugger tools that do not fit another category."}
};

static const char* const kMcpExecutionTools[] =
{
    "debug_pause", "debug_continue", "debug_reset", "debug_get_status",
    "set_fast_forward_speed", "toggle_fast_forward"
};

static const char* const kMcpMediaTools[] =
{
    "load_media", "get_media_info", "list_recent_media", "load_bios"
};

static const char* const kMcpCaptureTools[] =
{
    "get_screenshot"
};

static const char* const kMcpInputTools[] =
{
    "controller_button", "controller_macro", "controller_set_type",
    "controller_get_type", "get_input_state"
};

static const McpToolCategoryTools kMcpToolCategoryTools[] =
{
    {"execution", kMcpExecutionTools, MCP_ARRAY_COUNT(kMcpExecutionTools)},
    {"media", kMcpMediaTools, MCP_ARRAY_COUNT(kMcpMediaTools)},
    {"capture", kMcpCaptureTools, MCP_ARRAY_COUNT(kMcpCaptureTools)},
    {"input", kMcpInputTools, MCP_ARRAY_COUNT(kMcpInputTools)}
};

const size_t kMcpSearchToolLimit = 20;

McpToolRegistry::McpToolRegistry()
{
    m_tools = json::array();
}

void McpToolRegistry::SetTools(const json& tools)
{
    m_tools = json::array();

    if (!tools.is_array())
        return;

    for (json::const_iterator it = tools.begin(); it != tools.end(); ++it)
    {
        if (!it->is_object() || !it->contains("name") || !(*it)["name"].is_string())
            continue;

        if (IsRouterToolName((*it)["name"].get<std::string>()))
            continue;

        m_tools.push_back(*it);
    }
}

bool McpToolRegistry::IsEmpty() const
{
    return !m_tools.is_array() || m_tools.empty();
}

bool McpToolRegistry::HasTool(const std::string& tool_name) const
{
    return FindTool(tool_name) != NULL;
}

bool McpToolRegistry::HasCategory(const std::string& category) const
{
    const size_t category_count = MCP_ARRAY_COUNT(kMcpToolCategories);

    for (size_t i = 0; i < category_count; i++)
    {
        if ((category == kMcpToolCategories[i].name) && HasToolInCategory(category, false))
            return true;
    }

    return false;
}

bool McpToolRegistry::ValidateArguments(const std::string& tool_name, const json& arguments, std::string& error) const
{
    const json* tool = FindTool(tool_name);
    if (!tool)
    {
        error = "Unknown tool '" + tool_name + "'";
        return false;
    }

    if (!tool->contains("inputSchema") || !(*tool)["inputSchema"].is_object())
    {
        error = "Tool has no valid input schema";
        return false;
    }

    return validate_json_schema(arguments, (*tool)["inputSchema"], "", error);
}

json McpToolRegistry::GetStats() const
{
    json categories = json::array();
    const size_t category_count = MCP_ARRAY_COUNT(kMcpToolCategories);
    int routed_count = 0;
    int direct_count = 0;

    for (json::const_iterator it = m_tools.begin(); it != m_tools.end(); ++it)
    {
        std::string name = (*it)["name"].get<std::string>();

        if (IsDirectToolName(name))
            direct_count++;
        else
            routed_count++;
    }

    for (size_t i = 0; i < category_count; i++)
    {
        int tool_count = CountToolsInCategory(kMcpToolCategories[i].name, false);

        if (tool_count == 0)
            continue;

        categories.push_back({
            {"name", kMcpToolCategories[i].name},
            {"tool_count", tool_count}
        });
    }

    return {
        {"total_categories", categories.size()},
        {"total_routed_tools", routed_count},
        {"total_direct_tools", direct_count},
        {"total_tools", routed_count + direct_count},
        {"categories", categories}
    };
}

json McpToolRegistry::GetCategories() const
{
    json categories = json::array();
    const size_t category_count = MCP_ARRAY_COUNT(kMcpToolCategories);

    for (size_t i = 0; i < category_count; i++)
    {
        if (!HasRoutedToolInCategory(kMcpToolCategories[i].name))
            continue;

        categories.push_back({
            {"name", kMcpToolCategories[i].name},
            {"title", kMcpToolCategories[i].title},
            {"description", kMcpToolCategories[i].description},
            {"tool_count", CountToolsInCategory(kMcpToolCategories[i].name, false)}
        });
    }

    return categories;
}

json McpToolRegistry::GetCategoryNames() const
{
    json categories = json::array();
    const size_t category_count = MCP_ARRAY_COUNT(kMcpToolCategories);

    for (size_t i = 0; i < category_count; i++)
    {
        if (HasRoutedToolInCategory(kMcpToolCategories[i].name))
            categories.push_back(kMcpToolCategories[i].name);
    }

    return categories;
}

json McpToolRegistry::GetDirectTools() const
{
    json tools = json::array();

    for (json::const_iterator it = m_tools.begin(); it != m_tools.end(); ++it)
    {
        std::string name = (*it)["name"].get<std::string>();
        if (IsDirectToolName(name))
            tools.push_back(*it);
    }

    return tools;
}

json McpToolRegistry::GetToolsInCategory(const std::string& category) const
{
    json tools = json::array();

    for (json::const_iterator it = m_tools.begin(); it != m_tools.end(); ++it)
    {
        std::string name = (*it)["name"].get<std::string>();

        if (IsDirectToolName(name))
            continue;

        if (ToolCategoryForName(name) == category)
            tools.push_back(ToolToSummaryJson(*it));
    }

    return tools;
}

json McpToolRegistry::GetToolInfo(const std::string& tool_name) const
{
    const json* tool = FindTool(tool_name);

    if (tool == NULL)
        return json::object();

    return ToolToInfoJson(*tool);
}

std::string McpToolRegistry::GetCategoryTitle(const std::string& category) const
{
    const size_t category_count = MCP_ARRAY_COUNT(kMcpToolCategories);

    for (size_t i = 0; i < category_count; i++)
    {
        if (category == kMcpToolCategories[i].name)
            return kMcpToolCategories[i].title;
    }

    return "";
}

std::string McpToolRegistry::GetCategoryDescription(const std::string& category) const
{
    const size_t category_count = MCP_ARRAY_COUNT(kMcpToolCategories);

    for (size_t i = 0; i < category_count; i++)
    {
        if (category == kMcpToolCategories[i].name)
            return kMcpToolCategories[i].description;
    }

    return "";
}

int McpToolRegistry::GetCategoryToolCount(const std::string& category) const
{
    return CountToolsInCategory(category, false);
}

json McpToolRegistry::SearchTools(const std::string& query) const
{
    json tools = json::array();
    std::string query_lower = ToLower(query);

    if (query_lower.empty())
        return tools;

    for (json::const_iterator it = m_tools.begin(); it != m_tools.end(); ++it)
    {
        std::string name = (*it)["name"].get<std::string>();

        std::string haystack = name + " ";
        haystack += it->value("title", "");
        haystack += " ";
        haystack += it->value("description", "");
        haystack += " ";
        haystack += ToolCategoryForName(name);
        haystack += " ";
        haystack += AliasesForTool(name);

        std::string haystack_lower = ToLower(haystack);
        std::istringstream terms(query_lower);
        std::string term;
        bool has_terms = false;
        bool matches = true;

        while (terms >> term)
        {
            has_terms = true;

            if (haystack_lower.find(term) == std::string::npos)
            {
                matches = false;
                break;
            }
        }

        if (has_terms && matches)
        {
            tools.push_back(ToolToSearchJson(*it));

            if (tools.size() >= kMcpSearchToolLimit)
                return tools;
        }
    }

    return tools;
}

bool McpToolRegistry::IsRouterTool(const std::string& tool_name) const
{
    return IsRouterToolName(tool_name);
}

bool McpToolRegistry::IsRouterTool(const std::string& tool_name, const std::string& router_tool_name) const
{
    return NormalizeToolName(tool_name) == router_tool_name;
}

size_t McpToolRegistry::GetSearchToolLimit() const
{
    return kMcpSearchToolLimit;
}

bool McpToolRegistry::IsRouterToolName(const std::string& tool_name) const
{
    std::string name = NormalizeToolName(tool_name);

    return (name == "list_tool_categories") ||
           (name == "get_category_tools") ||
           (name == "get_tool_info") ||
           (name == "search_tools") ||
           (name == "execute_tool");
}

std::string McpToolRegistry::NormalizeToolName(std::string tool_name) const
{
    size_t pos = 0;
    while ((pos = tool_name.find('.', pos)) != std::string::npos)
    {
        tool_name[pos] = '_';
        pos++;
    }

    return tool_name;
}

std::string McpToolRegistry::ToLower(const std::string& text) const
{
    std::string result = text;

    for (size_t i = 0; i < result.size(); i++)
        result[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(result[i])));

    return result;
}

bool McpToolRegistry::StringContains(const std::string& text, const std::string& needle) const
{
    return text.find(needle) != std::string::npos;
}

bool McpToolRegistry::ToolNameInList(const std::string& name, const char* const* tools, size_t count) const
{
    for (size_t i = 0; i < count; i++)
    {
        if (name == tools[i])
            return true;
    }

    return false;
}

bool McpToolRegistry::IsDirectToolName(const std::string& tool_name) const
{
    std::string name = NormalizeToolName(tool_name);

    return (name == "load_media") ||
           (name == "get_media_info") ||
           (name == "debug_pause") ||
           (name == "debug_continue") ||
           (name == "get_screenshot") ||
           (name == "controller_button");
}

std::string McpToolRegistry::ToolCategoryForName(const std::string& tool_name) const
{
    std::string name = NormalizeToolName(tool_name);
    const size_t category_count = MCP_ARRAY_COUNT(kMcpToolCategoryTools);

    for (size_t i = 0; i < category_count; i++)
    {
        if (ToolNameInList(name, kMcpToolCategoryTools[i].tools, kMcpToolCategoryTools[i].count))
            return kMcpToolCategoryTools[i].category;
    }

    return "tools";
}

std::string McpToolRegistry::AliasesForTool(const std::string& tool_name) const
{
    std::string name = NormalizeToolName(tool_name);
    std::string aliases = ToolCategoryForName(name);

    if (StringContains(name, "controller"))
        aliases += " input joypad gamepad button macro tap press release";

    return aliases;
}

const json* McpToolRegistry::FindTool(const std::string& tool_name) const
{
    std::string normalized_name = NormalizeToolName(tool_name);

    for (json::const_iterator it = m_tools.begin(); it != m_tools.end(); ++it)
    {
        std::string name = (*it)["name"].get<std::string>();
        if (NormalizeToolName(name) == normalized_name)
            return &(*it);
    }

    return NULL;
}

bool McpToolRegistry::HasToolInCategory(const std::string& category, bool include_direct) const
{
    for (json::const_iterator it = m_tools.begin(); it != m_tools.end(); ++it)
    {
        std::string name = (*it)["name"].get<std::string>();

        if (!include_direct && IsDirectToolName(name))
            continue;

        if (ToolCategoryForName(name) == category)
            return true;
    }

    return false;
}

bool McpToolRegistry::HasRoutedToolInCategory(const std::string& category) const
{
    return HasToolInCategory(category, false);
}

int McpToolRegistry::CountToolsInCategory(const std::string& category, bool include_direct) const
{
    int count = 0;

    for (json::const_iterator it = m_tools.begin(); it != m_tools.end(); ++it)
    {
        std::string name = (*it)["name"].get<std::string>();

        if (!include_direct && IsDirectToolName(name))
            continue;

        if (ToolCategoryForName(name) == category)
            count++;
    }

    return count;
}

json McpToolRegistry::ToolToSummaryJson(const json& tool) const
{
    json result;
    std::string name = tool.value("name", "");

    result["name"] = name;
    result["description"] = tool.value("description", "");

    return result;
}

json McpToolRegistry::ToolToSearchJson(const json& tool) const
{
    json result;
    std::string name = tool.value("name", "");

    result["category"] = ToolCategoryForName(name);
    result["tool"] = name;
    result["description"] = tool.value("description", "");

    if (IsDirectToolName(name))
        result["category"] = "direct";

    return result;
}

json McpToolRegistry::ToolToInfoJson(const json& tool) const
{
    json result;
    std::string name = tool.value("name", "");

    result["name"] = name;
    result["title"] = tool.value("title", name);
    result["description"] = tool.value("description", "");
    result["category"] = ToolCategoryForName(name);
    result["direct"] = IsDirectToolName(name);

    if (IsDirectToolName(name))
        result["category"] = "direct";

    if (tool.contains("inputSchema"))
        result["inputSchema"] = tool["inputSchema"];
    else
        result["inputSchema"] = json::object();

    if (tool.contains("annotations"))
        result["annotations"] = tool["annotations"];

    return result;
}
