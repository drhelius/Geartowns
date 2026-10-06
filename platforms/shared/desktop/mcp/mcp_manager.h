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
#include "debug/gui_debug_constants.h"

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
    GT_Keys key;
    u64 release_at_frame;
};

enum McpInputMacroStepType
{
    MCP_INPUT_MACRO_STEP_KEY_PRESS,
    MCP_INPUT_MACRO_STEP_KEY_RELEASE,
    MCP_INPUT_MACRO_STEP_WAIT
};

struct McpInputMacroStep
{
    McpInputMacroStepType type;
    GT_Keys key;
    int frames;
};

struct McpInputMacroState
{
    bool active;
    json request_id;
    std::vector<McpInputMacroStep> steps;
    size_t step_index;
    bool waiting;
    u64 wait_target_frame;
    int command_count;
    int frames_waited;
    bool restore_pause;
};

static const u64 k_mcp_tap_frames = 10;

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
        reset_input_macro();
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

    void SetTransportMode(McpTransportMode mode, int tcp_port = 7777, const char* tcp_address = "127.0.0.1")
    {
        m_transport_mode = mode;
        m_tcp_port = tcp_port;
        m_tcp_address = IsValidPointer(tcp_address) && tcp_address[0] ? tcp_address : "127.0.0.1";
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
        m_pending_media_load = false;
        m_pending_media_load_file_path.clear();
        reset_input_macro();
        m_debug_adapter->ClearControllerState();

        McpTransportInterface* transport = NULL;

        if (m_transport_mode == MCP_TRANSPORT_TCP)
        {
            g_mcp_stdio_mode = false;
            Log("[MCP] Starting HTTP transport on %s:%d", m_tcp_address.c_str(), m_tcp_port);
            transport = new HttpTransport(m_tcp_address, m_tcp_port);
        }
        else
        {
            g_mcp_stdio_mode = true;
            transport = new StdioTransport();
        }

        m_server = new McpServer(transport, *m_debug_adapter, m_command_queue, m_response_queue);
        m_server->Start();
    }

    void Stop()
    {
        SafeDelete(m_server);
        m_command_queue.Clear();
        m_delayed_releases.clear();
        m_pending_media_load = false;
        m_pending_media_load_file_path.clear();
        reset_input_macro();

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
        for (size_t i = 0; i < m_delayed_releases.size();)
        {
            if (emu_frame_counter >= m_delayed_releases[i].release_at_frame)
            {
                if (m_delayed_releases[i].key != GT_KEY_NONE)
                    emu_key_released(m_delayed_releases[i].key);
                else
                    m_debug_adapter->ControllerButton(m_delayed_releases[i].player, m_delayed_releases[i].button,
                        "release");

                m_delayed_releases.erase(m_delayed_releases.begin() + i);
            }
            else
                i++;
        }

        if (m_input_macro.active)
        {
            pump_input_macro(core);
            return;
        }

        if (m_pending_media_load)
        {
            if (m_debug_adapter->IsMediaLoading())
                return;

            DebugResponse* response = new DebugResponse();
            response->requestId = m_pending_media_load_request_id;
            response->result = m_debug_adapter->FinishLoadMedia(m_pending_media_load_file_path);
            UpdateResponseError(response);

            m_pending_media_load = false;
            m_pending_media_load_file_path.clear();
            m_response_queue.Push(response);
        }

        DebugCommand* command = NULL;

        while ((command = m_command_queue.Pop()) != NULL)
        {
            std::string tool = NormalizeToolName(command->toolName);

            if (tool == "load_media")
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

            if (tool == "keyboard_type")
            {
                DebugResponse* response = new DebugResponse();
                response->requestId = command->requestId;
                response->result = start_keyboard_macro(command->arguments, command->requestId);

                if (response->result.contains("error"))
                {
                    UpdateResponseError(response);
                    m_response_queue.Push(response);
                }
                else
                {
                    SafeDelete(response);
                    pump_input_macro(core);
                }

                SafeDelete(command);
                break;
            }

            DebugResponse* response = new DebugResponse();
            response->requestId = command->requestId;
            response->result = m_server->ExecuteCommand(command->toolName, command->arguments);

            if (tool == "get_input_state")
                AppendInputRuntimeState(response->result);

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

    void AppendInputRuntimeState(json& result) const
    {
        if (result.contains("error"))
            return;

        json pending_releases = json::array();

        for (size_t i = 0; i < m_delayed_releases.size(); i++)
        {
            if (m_delayed_releases[i].key != GT_KEY_NONE)
                pending_releases.push_back({{"key", gui_debug_key_name(m_delayed_releases[i].key)}});
            else
                pending_releases.push_back({{"player", m_delayed_releases[i].player},
                    {"button", m_delayed_releases[i].button}});
        }

        result["pending_releases"] = pending_releases;
    }

    void HandleControllerSideEffects(json& result)
    {
        if (result.contains("__delayed_release") && result["__delayed_release"] == true)
        {
            DelayedButtonRelease release;
            release.player = result["player"];
            release.button = result["button"];
            release.key = GT_KEY_NONE;
            release.release_at_frame = emu_frame_counter + k_mcp_tap_frames;
            m_delayed_releases.push_back(release);
            result.erase("__delayed_release");
        }

        if (result.contains("__delayed_key_release") && result["__delayed_key_release"] == true)
        {
            DelayedButtonRelease release;
            GT_Keys key = GT_KEY_NONE;
            m_debug_adapter->GetKeyCode(result["key"], key);
            release.player = 0;
            release.key = key;
            release.release_at_frame = emu_frame_counter + k_mcp_tap_frames;
            m_delayed_releases.push_back(release);
            result.erase("__delayed_key_release");
        }
    }

    void reset_input_macro()
    {
        m_input_macro.active = false;
        m_input_macro.request_id = json();
        m_input_macro.steps.clear();
        m_input_macro.step_index = 0;
        m_input_macro.waiting = false;
        m_input_macro.wait_target_frame = 0;
        m_input_macro.command_count = 0;
        m_input_macro.frames_waited = 0;
        m_input_macro.restore_pause = false;
    }

    json start_keyboard_macro(const json& arguments, const json& request_id)
    {
        json result;

        if (emu_is_empty())
        {
            result["error"] = "Emulator is powered off";
            return result;
        }

        std::string text = arguments.value("text", "");
        int frames = arguments.value("frames_per_key", 3);

        if (text.empty() || text.size() > 256)
        {
            result["error"] = "text must be 1-256 characters";
            return result;
        }

        reset_input_macro();
        m_input_macro.request_id = request_id;
        m_input_macro.command_count = (int)text.size();
        m_input_macro.restore_pause = emu_is_paused() && !emu_is_debug_idle();

        for (size_t i = 0; i < text.size(); i++)
        {
            GT_Keys key = GT_KEY_NONE;
            bool shift = false;

            if (!m_debug_adapter->GetTypedKey(text[i], key, shift))
            {
                reset_input_macro();
                result["error"] = "Character " + std::to_string((int)i) + " cannot be typed on the JIS keyboard";
                return result;
            }

            if (shift)
                append_key_step(MCP_INPUT_MACRO_STEP_KEY_PRESS, GT_KEY_SHIFT);

            append_key_step(MCP_INPUT_MACRO_STEP_KEY_PRESS, key);
            append_wait_step(frames);
            append_key_step(MCP_INPUT_MACRO_STEP_KEY_RELEASE, key);

            if (shift)
                append_key_step(MCP_INPUT_MACRO_STEP_KEY_RELEASE, GT_KEY_SHIFT);

            append_wait_step(frames);
        }

        m_input_macro.active = true;

        result["success"] = true;
        result["pending"] = true;
        result["characters"] = (int)text.size();
        return result;
    }

    void append_key_step(McpInputMacroStepType type, GT_Keys key)
    {
        McpInputMacroStep step;
        step.type = type;
        step.key = key;
        step.frames = 0;
        m_input_macro.steps.push_back(step);
    }

    void append_wait_step(int frames)
    {
        McpInputMacroStep step;
        step.type = MCP_INPUT_MACRO_STEP_WAIT;
        step.key = GT_KEY_NONE;
        step.frames = frames;
        m_input_macro.steps.push_back(step);
    }

    void pump_input_macro(GeartownsCore* core)
    {
        if (m_input_macro.waiting)
        {
            if (emu_frame_counter < m_input_macro.wait_target_frame)
            {
                continue_input_macro_wait(core);
                return;
            }

            m_input_macro.waiting = false;
        }

        while (m_input_macro.step_index < m_input_macro.steps.size())
        {
            const McpInputMacroStep& step = m_input_macro.steps[m_input_macro.step_index];

            if (step.type == MCP_INPUT_MACRO_STEP_WAIT)
            {
                m_input_macro.frames_waited += step.frames;
                m_input_macro.wait_target_frame = emu_frame_counter + (u64)step.frames;
                m_input_macro.waiting = true;
                m_input_macro.step_index++;

                continue_input_macro_wait(core);
                return;
            }

            if (step.type == MCP_INPUT_MACRO_STEP_KEY_PRESS)
                emu_key_pressed(step.key);
            else
                emu_key_released(step.key);

            m_input_macro.step_index++;
        }

        finish_input_macro_success();
    }

    void continue_input_macro_wait(GeartownsCore* core)
    {
        if (emu_is_debug_idle())
        {
            u32 address = 0;

            if (core->GetI386()->GetBreakpointHitAddress(address))
            {
                finish_input_macro_error("Input macro interrupted by breakpoint");
                return;
            }

            u64 frames = m_input_macro.wait_target_frame - emu_frame_counter;

            if (frames > 1000)
                frames = 1000;

            if (frames > 0)
                emu_debug_step_frames((int)frames);
        }
        else if (m_input_macro.restore_pause && emu_is_paused())
            emu_resume();
    }

    void finish_input_macro_success()
    {
        bool restore_pause = m_input_macro.restore_pause;

        DebugResponse* response = new DebugResponse();
        response->requestId = m_input_macro.request_id;
        response->result = {
            {"success", true},
            {"characters", m_input_macro.command_count},
            {"frames_waited", m_input_macro.frames_waited}
        };

        reset_input_macro();

        if (restore_pause)
            emu_pause();

        m_response_queue.Push(response);
    }

    void finish_input_macro_error(const std::string& error)
    {
        bool restore_pause = m_input_macro.restore_pause;

        DebugResponse* response = new DebugResponse();
        response->requestId = m_input_macro.request_id;
        response->isToolError = true;
        response->errorMessage = error;
        response->result = {{"error", error}};

        reset_input_macro();

        if (restore_pause)
            emu_pause();

        m_response_queue.Push(response);
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
    McpInputMacroState m_input_macro;
};

#endif /* MCP_MANAGER_H */
