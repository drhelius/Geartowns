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

#ifndef MCP_MANAGER_H
#define MCP_MANAGER_H

#include <string>
#include <vector>
#include "mcp_server.h"
#include "mcp_transport.h"
#include "mcp_debug_adapter.h"
#include "emu.h"

extern bool g_mcp_stdio_mode;

enum McpTransportMode
{
    MCP_TRANSPORT_STDIO,
    MCP_TRANSPORT_TCP
};

struct DelayedButtonRelease
{
    int player;
    std::string button;
    u64 release_at_pump;
};

class McpManager
{
public:
    McpManager()
    {
        m_debug_adapter = NULL;
        m_server = NULL;
        m_transport_mode = MCP_TRANSPORT_STDIO;
        m_tcp_port = 7777;
        m_tcp_address = "127.0.0.1";
        m_pending_media_load = false;
        m_pending_media_load_request_id = json();
        m_pump_count = 0;
    }

    ~McpManager()
    {
        Stop();
        SafeDelete(m_debug_adapter);
    }

    void Init(GeartownsCore* core)
    {
        m_debug_adapter = new DebugAdapter(core);
    }

    void SetTransportMode(McpTransportMode mode, int tcp_port = 7777,
        const char* tcp_address = "127.0.0.1")
    {
        m_transport_mode = mode;
        m_tcp_port = tcp_port;
        m_tcp_address = IsValidPointer(tcp_address) && tcp_address[0] ?
            tcp_address : "127.0.0.1";
    }

    void Start()
    {
        if (IsValidPointer(m_server))
        {
            if (m_server->IsRunning())
                return;
            SafeDelete(m_server);
        }

        m_command_queue.Clear();
        m_response_queue.Reset();
        m_delayed_releases.clear();
        m_pump_count = 0;
        m_pending_media_load = false;
        m_pending_media_load_file_path.clear();
        m_debug_adapter->ClearControllerState();

        McpTransportInterface* transport = NULL;
        if (m_transport_mode == MCP_TRANSPORT_TCP)
        {
            g_mcp_stdio_mode = false;
            Log("[MCP] Starting HTTP transport on %s:%d", m_tcp_address.c_str(),
                m_tcp_port);
            transport = new HttpTransport(m_tcp_address, m_tcp_port);
        }
        else
        {
            g_mcp_stdio_mode = true;
            transport = new StdioTransport();
        }

        m_server = new McpServer(transport, *m_debug_adapter, m_command_queue,
            m_response_queue);
        m_server->Start();
    }

    void Stop()
    {
        SafeDelete(m_server);
        m_command_queue.Clear();
        m_delayed_releases.clear();
        m_pending_media_load = false;
        m_pending_media_load_file_path.clear();

        if (IsValidPointer(m_debug_adapter))
            m_debug_adapter->ClearControllerState();
    }

    bool IsRunning() const
    {
        return IsValidPointer(m_server) && m_server->IsRunning();
    }

    int GetTransportMode() const
    {
        return (int)m_transport_mode;
    }

    const char* GetTcpAddress() const
    {
        return m_tcp_address.c_str();
    }

    int GetTcpPort() const
    {
        return m_tcp_port;
    }

    void PumpCommands(GeartownsCore* core)
    {
        UNUSED(core);
        m_pump_count++;

        for (size_t i = 0; i < m_delayed_releases.size();)
        {
            if (m_pump_count >= m_delayed_releases[i].release_at_pump)
            {
                m_debug_adapter->ControllerButton(m_delayed_releases[i].player,
                    m_delayed_releases[i].button, "release");
                m_delayed_releases.erase(m_delayed_releases.begin() + i);
            }
            else
                i++;
        }

        if (m_pending_media_load)
        {
            if (m_debug_adapter->IsMediaLoading())
                return;

            DebugResponse* response = new DebugResponse();
            response->requestId = m_pending_media_load_request_id;
            response->result =
                m_debug_adapter->FinishLoadMedia(m_pending_media_load_file_path);
            UpdateResponseError(response);

            m_pending_media_load = false;
            m_pending_media_load_file_path.clear();
            m_response_queue.Push(response);
        }

        DebugCommand* command = NULL;
        while ((command = m_command_queue.Pop()) != NULL)
        {
            if (NormalizeToolName(command->toolName) == "load_media")
            {
                DebugResponse* response = new DebugResponse();
                response->requestId = command->requestId;

                std::string file_path = command->arguments.value("file_path", "");
                response->result = m_debug_adapter->StartLoadMedia(file_path);

                if (response->result.contains("error"))
                {
                    UpdateResponseError(response);
                    m_response_queue.Push(response);
                }
                else
                {
                    m_pending_media_load = true;
                    m_pending_media_load_request_id = response->requestId;
                    m_pending_media_load_file_path = file_path;
                    SafeDelete(response);
                }

                SafeDelete(command);
                break;
            }

            DebugResponse* response = new DebugResponse();
            response->requestId = command->requestId;
            response->result = m_server->ExecuteCommand(command->toolName,
                command->arguments);
            UpdateResponseError(response);
            HandleControllerSideEffects(response->result);

            m_response_queue.Push(response);
            SafeDelete(command);
        }
    }

private:
    std::string NormalizeToolName(std::string tool_name) const
    {
        size_t position = 0;
        while ((position = tool_name.find('.', position)) != std::string::npos)
        {
            tool_name[position] = '_';
            position++;
        }

        return tool_name;
    }

    void UpdateResponseError(DebugResponse* response)
    {
        if (!response->result.contains("error"))
            return;

        response->isToolError = true;
        response->errorMessage = response->result["error"];
    }

    void HandleControllerSideEffects(json& result)
    {
        if (!result.contains("__delayed_release") ||
            result["__delayed_release"] != true)
        {
            return;
        }

        DelayedButtonRelease release;
        release.player = result["player"];
        release.button = result["button"];
        release.release_at_pump = m_pump_count + 10;
        m_delayed_releases.push_back(release);
        result.erase("__delayed_release");
    }

private:
    DebugAdapter* m_debug_adapter;
    McpServer* m_server;
    CommandQueue m_command_queue;
    ResponseQueue m_response_queue;
    McpTransportMode m_transport_mode;
    int m_tcp_port;
    std::string m_tcp_address;
    bool m_pending_media_load;
    json m_pending_media_load_request_id;
    std::string m_pending_media_load_file_path;
    std::vector<DelayedButtonRelease> m_delayed_releases;
    u64 m_pump_count;
};

#endif /* MCP_MANAGER_H */
