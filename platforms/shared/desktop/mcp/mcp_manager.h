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
#include "input_macro.h"
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
    int host_key;
    u64 release_at_frame;
};

struct McpInputMacroState
{
    bool active;
    json request_id;
    int command_count;
    int frames_waited;
    bool restore_pause;
    u64 breakpoint_stops;
};

enum McpControllerMacroStepType
{
    MCP_CONTROLLER_MACRO_PRESS = 0,
    MCP_CONTROLLER_MACRO_RELEASE,
    MCP_CONTROLLER_MACRO_WAIT
};

enum McpControllerMacroDevice
{
    MCP_CONTROLLER_MACRO_PAD = 0,
    MCP_CONTROLLER_MACRO_KEYBOARD
};

struct McpControllerMacroStep
{
    McpControllerMacroStepType type;
    McpControllerMacroDevice device;
    int command;
    int player;
    std::string button;
    GT_Keys key;
    int frames;
};

struct McpControllerMacroState
{
    bool active;
    json request_id;
    std::vector<McpControllerMacroStep> steps;
    std::vector<McpControllerMacroStep> held;
    size_t step_index;
    bool waiting;
    u64 wait_target_frame;
    int command_count;
    int frames_waited;
    int dropped;
    bool restore_pause;
    u64 breakpoint_stops;
    u32 generation;
};

struct McpStepFramesState
{
    bool active;
    json request_id;
    int frames;
    u64 start_frame;
    u64 breakpoint_stops;
    u32 generation;
};

static const u64 k_mcp_tap_frames = 10;
static const int k_mcp_macro_tap_frames = 3;
static const int k_mcp_macro_max_wait_frames = 1000;
static const int k_mcp_step_wait_max_frames = 1800;

enum McpMouseMotion
{
    MCP_MOUSE_MOTION_UP = 0,
    MCP_MOUSE_MOTION_DOWN,
    MCP_MOUSE_MOTION_LEFT,
    MCP_MOUSE_MOTION_RIGHT,
    MCP_MOUSE_MOTION_COUNT
};

static const int k_mcp_mouse_wiggle_counts = 32;
static const int k_mcp_mouse_wiggle_max_counts = 1024;
static const int k_mcp_mouse_wiggle_tries = 6;
static const double k_mcp_mouse_wiggle_gain = 4.0;
static const int k_mcp_mouse_unseen_frames = 30;
static const int k_mcp_mouse_settle_frames = 3;
static const int k_mcp_mouse_stall_frames = 60;
static const int k_mcp_mouse_max_moves = 16;
static const int k_mcp_mouse_locate_frames = 10;
static const int k_mcp_mouse_max_misses = 3;
static const int k_mcp_mouse_max_move_counts = 8192;
static const int k_mcp_mouse_button_frames = 4;

enum McpMousePhase
{
    MCP_MOUSE_PHASE_START = 0,
    MCP_MOUSE_PHASE_WIGGLE_OUT,
    MCP_MOUSE_PHASE_WIGGLE_BACK,
    MCP_MOUSE_PHASE_RETRY,
    MCP_MOUSE_PHASE_MOVE,
    MCP_MOUSE_PHASE_DRAG,
    MCP_MOUSE_PHASE_LOCATE,
    MCP_MOUSE_PHASE_BUTTONS
};

struct McpMouseMacroState
{
    bool active;
    json request_id;
    McpMousePhase phase;
    int player;
    bool click;
    bool absolute;
    bool held;
    bool drag;
    bool dragged;
    int target_x;
    int target_y;
    int to_x;
    int to_y;
    int from_x;
    int from_y;
    int delta_x;
    int delta_y;
    int send_x;
    int send_y;
    int settle;
    int stall;
    int step;
    int origin_x;
    int origin_y;
    int sent_x;
    int sent_y;
    int locate_frames;
    int wiggle_counts;
    int wiggle_x;
    int wiggle_y;
    int wiggle_tries;
    bool wiggle_flip;
    int moves;
    int misses;
    int last_distance;
    int screen_width;
    int screen_height;
    std::vector<u32> frame_before;
    std::vector<u32> frame_out;
    std::string button;
    std::string action;
    u16 button_mask;
    int button_steps;
    bool button_press_first;
    int button_step;
    int button_wait;
    int press_frames;
    int release_frames;
    u64 frame;
    u64 start_frame;
    bool restore_pause;
    u64 breakpoint_stops;
    u32 generation;
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
        m_reported_breakpoint_stops = 0;
        reset_input_macro();
        reset_controller_macro();
        reset_mouse_macro();
        reset_step_frames();
        reset_mouse_motion();
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
        reset_controller_macro();
        reset_mouse_macro();
        reset_step_frames();
        reset_mouse_motion();
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

    // Keys and buttons still waiting for their release are released, so nothing stays held
    void Stop()
    {
        SafeDelete(m_server);
        m_command_queue.Clear();

        for (size_t i = 0; i < m_delayed_releases.size(); i++)
        {
            if (m_delayed_releases[i].host_key >= 0)
                emu_host_key_released(m_delayed_releases[i].host_key);
            else if (!IsValidPointer(m_debug_adapter))
                continue;
            else if (m_delayed_releases[i].key != GT_KEY_NONE)
                m_debug_adapter->ReleaseKey(m_delayed_releases[i].key);
            else
                m_debug_adapter->ControllerButton(m_delayed_releases[i].player, m_delayed_releases[i].button, "release");
        }

        m_delayed_releases.clear();
        m_pending_media_load = false;
        m_pending_media_load_file_path.clear();
        release_input_macro_keys();
        reset_input_macro();
        release_controller_macro_held();
        reset_controller_macro();
        reset_mouse_macro();
        reset_step_frames();
        reset_mouse_motion();

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

    void PumpCommands()
    {
        pump_mouse_motion();

        for (size_t i = 0; i < m_delayed_releases.size();)
        {
            if (emu_frame_counter >= m_delayed_releases[i].release_at_frame)
            {
                if (m_delayed_releases[i].host_key >= 0)
                    emu_host_key_released(m_delayed_releases[i].host_key);
                else if (m_delayed_releases[i].key != GT_KEY_NONE)
                    m_debug_adapter->ReleaseKey(m_delayed_releases[i].key);
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
            pump_input_macro();
            return;
        }

        if (m_controller_macro.active)
        {
            pump_controller_macro();
            return;
        }

        if (m_mouse_macro.active)
        {
            pump_mouse_macro();
            return;
        }

        if (m_step_frames.active)
        {
            pump_step_frames();
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

            if ((tool == "load_media") || (tool == "insert_playlist_disc"))
            {
                DebugResponse* response = new DebugResponse();
                response->requestId = command->requestId;

                if (tool == "load_media")
                    response->result = m_debug_adapter->StartLoadMedia(command->arguments.value("file_path", ""));
                else
                    response->result = m_debug_adapter->StartInsertPlaylistDisc(command->arguments.value("index", -1));

                if (response->result.contains("error"))
                {
                    UpdateResponseError(response);
                    m_response_queue.Push(response);
                }
                else
                {
                    m_pending_media_load = true;
                    m_pending_media_load_request_id = response->requestId;
                    m_pending_media_load_file_path = response->result.value("file_path", "");
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
                    pump_input_macro();
                }

                SafeDelete(command);
                break;
            }

            if (tool == "controller_macro")
            {
                DebugResponse* response = new DebugResponse();
                response->requestId = command->requestId;
                response->result = start_controller_macro(command->arguments, command->requestId);

                if (response->result.contains("error"))
                {
                    UpdateResponseError(response);
                    m_response_queue.Push(response);
                }
                else
                {
                    SafeDelete(response);
                    pump_controller_macro();
                }

                SafeDelete(command);
                break;
            }

            if (tool == "debug_step_frame")
            {
                json result = start_step_frames(command->arguments, command->requestId);

                if (!result.is_null())
                {
                    DebugResponse* response = new DebugResponse();
                    response->requestId = command->requestId;
                    response->result = result;
                    UpdateResponseError(response);
                    m_response_queue.Push(response);
                }

                SafeDelete(command);
                break;
            }

            if ((tool == "mouse_move") || (tool == "mouse_click"))
            {
                DebugResponse* response = new DebugResponse();
                response->requestId = command->requestId;
                response->result = start_mouse_macro(tool, command->arguments, command->requestId);

                if (response->result.contains("error"))
                {
                    UpdateResponseError(response);
                    m_response_queue.Push(response);
                }
                else
                {
                    SafeDelete(response);
                    pump_mouse_macro();
                }

                SafeDelete(command);
                break;
            }

            if (tool.compare(0, 6, "debug_") == 0)
                m_reported_breakpoint_stops = emu_debug_breakpoint_stops;

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

    bool unreported_breakpoint_stop()
    {
        if (!emu_is_debug_idle() || emu_debug_breakpoint_stops == m_reported_breakpoint_stops)
            return false;

        m_reported_breakpoint_stops = emu_debug_breakpoint_stops;
        return true;
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
            release.host_key = -1;
            release.release_at_frame = emu_frame_counter + k_mcp_tap_frames;
            m_delayed_releases.push_back(release);
            result.erase("__delayed_release");
        }

        if (result.contains("__delayed_keys"))
        {
            for (size_t i = 0; i < result["__delayed_keys"].size(); i++)
            {
                DelayedButtonRelease release;
                GT_Keys key = GT_KEY_NONE;
                m_debug_adapter->GetKeyCode(result["__delayed_keys"][i], key);
                release.player = 0;
                release.key = key;
                release.host_key = -1;
                release.release_at_frame = emu_frame_counter + k_mcp_tap_frames;
                m_delayed_releases.push_back(release);
            }

            result.erase("__delayed_keys");
        }

        if (result.contains("__delayed_host_release"))
        {
            DelayedButtonRelease release;
            release.player = 0;
            release.key = GT_KEY_NONE;
            release.host_key = result["__delayed_host_release"];
            release.release_at_frame = emu_frame_counter + k_mcp_tap_frames;
            m_delayed_releases.push_back(release);
            result.erase("__delayed_host_release");
        }

        if (result.contains("__mouse_motion") && result["__mouse_motion"] == true)
        {
            int player = result["player"];
            std::string button = result["button"];
            int direction = -1;

            if (button == "up")
                direction = MCP_MOUSE_MOTION_UP;
            else if (button == "down")
                direction = MCP_MOUSE_MOTION_DOWN;
            else if (button == "left")
                direction = MCP_MOUSE_MOTION_LEFT;
            else if (button == "right")
                direction = MCP_MOUSE_MOTION_RIGHT;

            if (direction >= 0 && player >= 1 && player <= GT_MAX_GAMEPADS)
                m_mouse_motion_held[player - 1][direction] = result["action"] == "press";

            result.erase("__mouse_motion");
        }
    }

    void reset_mouse_motion()
    {
        memset(m_mouse_motion_held, 0, sizeof(m_mouse_motion_held));
        m_mouse_motion_frame = emu_frame_counter;
    }

    // Held directions move a mouse one step per emulated frame, so nothing piles up while paused
    void pump_mouse_motion()
    {
        if (!IsValidPointer(m_debug_adapter) || emu_frame_counter == m_mouse_motion_frame)
            return;

        m_mouse_motion_frame = emu_frame_counter;

        for (int i = 0; i < GT_MAX_GAMEPADS; i++)
        {
            int player = i + 1;

            if (!m_debug_adapter->IsMouseController(player))
            {
                for (int j = 0; j < MCP_MOUSE_MOTION_COUNT; j++)
                    m_mouse_motion_held[i][j] = false;

                continue;
            }

            int delta_x = 0;
            int delta_y = 0;

            if (m_mouse_motion_held[i][MCP_MOUSE_MOTION_UP])
                delta_y -= k_mcp_mouse_motion_step;

            if (m_mouse_motion_held[i][MCP_MOUSE_MOTION_DOWN])
                delta_y += k_mcp_mouse_motion_step;

            if (m_mouse_motion_held[i][MCP_MOUSE_MOTION_LEFT])
                delta_x -= k_mcp_mouse_motion_step;

            if (m_mouse_motion_held[i][MCP_MOUSE_MOTION_RIGHT])
                delta_x += k_mcp_mouse_motion_step;

            m_debug_adapter->ApplyMouseMotion(player, delta_x, delta_y);
        }
    }

    DebugResponse* macro_response(const json& request_id, const json& result)
    {
        m_reported_breakpoint_stops = emu_debug_breakpoint_stops;
        DebugResponse* response = new DebugResponse();
        response->requestId = request_id;
        response->result = result;
        return response;
    }

    DebugResponse* macro_error_response(const json& request_id, const std::string& error)
    {
        DebugResponse* response = macro_response(request_id, {{"error", error}});
        response->isToolError = true;
        response->errorMessage = error;
        return response;
    }

    void push_macro_response(DebugResponse* response, bool restore_pause)
    {
        if (restore_pause)
            emu_pause();

        m_response_queue.Push(response);
    }

    const char* continue_macro_wait(u64 breakpoint_stops, bool restore_pause, u64 frames)
    {
        if (emu_is_debug_idle())
        {
            if (emu_debug_breakpoint_stops != breakpoint_stops)
                return "interrupted by breakpoint";

            if (frames > (u64)k_mcp_macro_max_wait_frames)
                frames = k_mcp_macro_max_wait_frames;

            if (frames > 0)
                emu_debug_step_frames((int)frames);
        }
        else if (restore_pause && emu_is_paused())
            emu_resume();
        else if (emu_is_paused())
            return "interrupted by a pause";

        return NULL;
    }

    void reset_input_macro()
    {
        m_input_macro.active = false;
        m_input_macro.request_id = json();
        m_input_macro.command_count = 0;
        m_input_macro.frames_waited = 0;
        m_input_macro.restore_pause = false;
        m_input_macro.breakpoint_stops = 0;
    }

    void release_input_macro_keys()
    {
        if (input_macro_get_owner() == Input_Macro_Owner_MCP)
            input_macro_stop();
    }

    json start_keyboard_macro(const json& arguments, const json& request_id)
    {
        json result;

        if (emu_is_empty())
        {
            result["error"] = "Emulator is powered off";
            return result;
        }

        if (unreported_breakpoint_stop())
        {
            result["error"] = "Stopped at a breakpoint not reported yet, see debug_get_status";
            return result;
        }

        std::string text = arguments.value("text", "");
        int frames = arguments.value("frames_per_key", k_input_macro_frames_per_key);

        if (text.empty() || text.size() > 256)
        {
            result["error"] = "text must be 1-256 characters";
            return result;
        }

        if (input_macro_get_owner() == Input_Macro_Owner_GUI)
        {
            result["error"] = "The keyboard is typing text pasted from the GUI";
            return result;
        }

        int untypeable = input_macro_find_untypeable(text.c_str(), NULL);

        if (untypeable >= 0)
        {
            result["error"] = "Character " + std::to_string(untypeable) + " cannot be typed on the JIS keyboard";
            return result;
        }

        reset_input_macro();
        m_input_macro.request_id = request_id;
        m_input_macro.command_count = input_macro_type_text(text.c_str(), frames, Input_Macro_Owner_MCP);
        m_input_macro.frames_waited = m_input_macro.command_count * frames * 2;
        m_input_macro.restore_pause = emu_is_paused() && !emu_is_debug_idle();
        m_input_macro.breakpoint_stops = emu_debug_breakpoint_stops;
        m_input_macro.active = true;

        result["success"] = true;
        result["pending"] = true;
        result["characters"] = m_input_macro.command_count;
        return result;
    }

    void pump_input_macro()
    {
        if (input_macro_get_owner() != Input_Macro_Owner_MCP)
            finish_input_macro_error("Input macro stopped");
        else if (input_macro_update(Input_Macro_Owner_MCP))
            continue_input_macro_wait();
        else
            finish_input_macro_success();
    }

    // Only a breakpoint that stopped the frames this macro ran interrupts it, not the stop it started from
    void continue_input_macro_wait()
    {
        const char* stop = continue_macro_wait(m_input_macro.breakpoint_stops, m_input_macro.restore_pause,
            input_macro_get_frames_left());

        if (IsValidPointer(stop))
            finish_input_macro_error(std::string("Input macro ") + stop);
    }

    void finish_input_macro_success()
    {
        if (emu_debug_breakpoint_stops != m_input_macro.breakpoint_stops)
        {
            finish_input_macro_error("Input macro interrupted by breakpoint");
            return;
        }

        bool restore_pause = m_input_macro.restore_pause;
        DebugResponse* response = macro_response(m_input_macro.request_id, {
            {"success", true},
            {"characters", m_input_macro.command_count},
            {"frames_waited", m_input_macro.frames_waited},
            {"dropped", input_macro_get_dropped()}
        });

        reset_input_macro();
        push_macro_response(response, restore_pause);
    }

    void finish_input_macro_error(const std::string& error)
    {
        bool restore_pause = m_input_macro.restore_pause;
        release_input_macro_keys();
        DebugResponse* response = macro_error_response(m_input_macro.request_id, error);
        reset_input_macro();
        push_macro_response(response, restore_pause);
    }

    void reset_controller_macro()
    {
        McpControllerMacroState& macro = m_controller_macro;
        macro.active = false;
        macro.request_id = json();
        macro.steps.clear();
        macro.held.clear();
        macro.step_index = 0;
        macro.waiting = false;
        macro.wait_target_frame = 0;
        macro.command_count = 0;
        macro.frames_waited = 0;
        macro.dropped = 0;
        macro.restore_pause = false;
        macro.breakpoint_stops = 0;
        macro.generation = 0;
    }

    bool parse_controller_macro_device(const json& value, McpControllerMacroDevice& device, std::string& error) const
    {
        std::string name = value.is_string() ? value.get<std::string>() : "";

        if (name == "pad")
            device = MCP_CONTROLLER_MACRO_PAD;
        else if (name == "keyboard")
            device = MCP_CONTROLLER_MACRO_KEYBOARD;
        else
        {
            error = "device must be pad or keyboard";
            return false;
        }

        return true;
    }

    bool parse_controller_macro_player(const json& value, int& player, std::string& error) const
    {
        if (!value.is_number_integer())
        {
            error = "player must be an integer";
            return false;
        }

        player = value.get<int>();

        if ((player < 1) || (player > GT_MAX_GAMEPADS))
        {
            error = "player must be 1-2";
            return false;
        }

        return true;
    }

    void append_controller_macro_step(McpControllerMacroStepType type, McpControllerMacroDevice device, int player,
        const std::string& button, GT_Keys key, int frames)
    {
        McpControllerMacroStep step;
        step.type = type;
        step.device = device;
        step.command = m_controller_macro.command_count;
        step.player = player;
        step.button = button;
        step.key = key;
        step.frames = frames;
        m_controller_macro.steps.push_back(step);
    }

    bool append_controller_macro_command(const json& command, McpControllerMacroDevice default_device,
        int default_player, int tap_frames, std::string& error)
    {
        if (!command.is_object())
        {
            error = "command must be an object";
            return false;
        }

        int actions = 0;

        for (json::const_iterator it = command.begin(); it != command.end(); ++it)
        {
            if ((it.key() == "tap") || (it.key() == "press") || (it.key() == "release") || (it.key() == "wait"))
                actions++;
            else if ((it.key() != "device") && (it.key() != "player"))
            {
                error = "unknown property: " + it.key();
                return false;
            }
        }

        if (actions != 1)
        {
            error = "command must contain exactly one of tap, press, release, or wait";
            return false;
        }

        McpControllerMacroDevice device = default_device;
        int player = default_player;

        if (command.contains("device") && !parse_controller_macro_device(command["device"], device, error))
            return false;

        if (command.contains("player") && !parse_controller_macro_player(command["player"], player, error))
            return false;

        if (command.contains("wait"))
        {
            if (!command["wait"].is_number_integer())
            {
                error = "wait must be an integer frame count";
                return false;
            }

            int frames = command["wait"].get<int>();

            if ((frames < 1) || (frames > k_mcp_macro_max_wait_frames))
            {
                error = "wait must be 1-" + std::to_string(k_mcp_macro_max_wait_frames) + " frames";
                return false;
            }

            append_controller_macro_step(MCP_CONTROLLER_MACRO_WAIT, device, player, "", GT_KEY_NONE, frames);
            return true;
        }

        std::string action = command.contains("tap") ? "tap" : (command.contains("press") ? "press" : "release");

        if (!command[action].is_string())
        {
            error = action + " must be a string";
            return false;
        }

        std::string name = command[action].get<std::string>();
        GT_Keys key = GT_KEY_NONE;

        if (device == MCP_CONTROLLER_MACRO_KEYBOARD)
        {
            if (!m_debug_adapter->GetKeyCode(name, key))
            {
                error = "Invalid key name '" + name + "'";
                return false;
            }
        }
        else if (!m_debug_adapter->IsControllerButton(name))
        {
            error = "Invalid button name, pad buttons are up, down, left, right, select, run, A, B, C, X, Y, Z and zoom";
            return false;
        }

        if (action == "release")
            append_controller_macro_step(MCP_CONTROLLER_MACRO_RELEASE, device, player, name, key, 0);
        else
            append_controller_macro_step(MCP_CONTROLLER_MACRO_PRESS, device, player, name, key, 0);

        if (action == "tap")
        {
            append_controller_macro_step(MCP_CONTROLLER_MACRO_WAIT, device, player, "", GT_KEY_NONE, tap_frames);
            append_controller_macro_step(MCP_CONTROLLER_MACRO_RELEASE, device, player, name, key, 0);
            append_controller_macro_step(MCP_CONTROLLER_MACRO_WAIT, device, player, "", GT_KEY_NONE, tap_frames);
        }

        return true;
    }

    json start_controller_macro(const json& arguments, const json& request_id)
    {
        json result;

        if (emu_is_empty())
        {
            result["error"] = "Emulator is powered off";
            return result;
        }

        if (unreported_breakpoint_stop())
        {
            result["error"] = "Stopped at a breakpoint not reported yet, see debug_get_status";
            return result;
        }

        McpControllerMacroDevice device = MCP_CONTROLLER_MACRO_PAD;
        int player = 1;
        std::string error;

        if ((arguments.contains("device") && !parse_controller_macro_device(arguments["device"], device, error)) ||
            (arguments.contains("player") && !parse_controller_macro_player(arguments["player"], player, error)))
        {
            result["error"] = error;
            return result;
        }

        int tap_frames = arguments.value("tap_frames", k_mcp_macro_tap_frames);
        const json& commands = arguments["commands"];

        reset_controller_macro();

        for (size_t i = 0; i < commands.size(); i++)
        {
            m_controller_macro.command_count = (int)i;

            if (!append_controller_macro_command(commands[i], device, player, tap_frames, error))
            {
                reset_controller_macro();
                result["error"] = "Invalid macro command " + std::to_string((int)i) + ": " + error;
                return result;
            }
        }

        McpControllerMacroState& macro = m_controller_macro;
        macro.request_id = request_id;
        macro.command_count = (int)commands.size();
        macro.restore_pause = emu_is_paused() && !emu_is_debug_idle();
        macro.breakpoint_stops = emu_debug_breakpoint_stops;
        macro.generation = emu_machine_generation;
        macro.active = true;

        result["success"] = true;
        result["pending"] = true;
        return result;
    }

    bool apply_controller_macro_step(const McpControllerMacroStep& step, bool pressed, std::string& error)
    {
        if (step.device == MCP_CONTROLLER_MACRO_KEYBOARD)
        {
            if (!pressed)
                m_debug_adapter->ReleaseKey(step.key);
            else if (!m_debug_adapter->PressKey(step.key))
                m_controller_macro.dropped++;

            return true;
        }

        json result = m_debug_adapter->ControllerButton(step.player, step.button, pressed ? "press" : "release");

        if (result.contains("error"))
        {
            error = result["error"].get<std::string>();
            return false;
        }

        HandleControllerSideEffects(result);
        return true;
    }

    void update_controller_macro_held(const McpControllerMacroStep& step)
    {
        std::vector<McpControllerMacroStep>& held = m_controller_macro.held;

        for (size_t i = 0; i < held.size(); i++)
        {
            if ((held[i].device == step.device) && (held[i].player == step.player) && (held[i].button == step.button) &&
                (held[i].key == step.key))
            {
                held.erase(held.begin() + i);
                break;
            }
        }

        if (step.type == MCP_CONTROLLER_MACRO_PRESS)
            held.push_back(step);
    }

    void release_controller_macro_held()
    {
        std::vector<McpControllerMacroStep>& held = m_controller_macro.held;
        std::string error;

        for (size_t i = 0; IsValidPointer(m_debug_adapter) && (i < held.size()); i++)
            apply_controller_macro_step(held[i], false, error);

        held.clear();
    }

    void pump_controller_macro()
    {
        McpControllerMacroState& macro = m_controller_macro;

        if (emu_is_empty())
        {
            finish_controller_macro_error("Emulator is powered off");
            return;
        }

        if (emu_machine_generation != macro.generation)
        {
            finish_controller_macro_error("Controller macro interrupted by a reset, state load or rewind");
            return;
        }

        if (macro.waiting)
        {
            if (emu_frame_counter < macro.wait_target_frame)
            {
                continue_controller_macro_wait();
                return;
            }

            macro.waiting = false;
        }

        while (macro.step_index < macro.steps.size())
        {
            const McpControllerMacroStep& step = macro.steps[macro.step_index];
            macro.step_index++;

            if (step.type == MCP_CONTROLLER_MACRO_WAIT)
            {
                macro.frames_waited += step.frames;
                macro.wait_target_frame = emu_frame_counter + (u64)step.frames;
                macro.waiting = true;
                continue_controller_macro_wait();
                return;
            }

            std::string error;
            int dropped = macro.dropped;

            if (!apply_controller_macro_step(step, step.type == MCP_CONTROLLER_MACRO_PRESS, error))
            {
                finish_controller_macro_error("Macro command " + std::to_string(step.command) + ": " + error);
                return;
            }

            if (macro.dropped == dropped)
                update_controller_macro_held(step);
        }

        finish_controller_macro_success();
    }

    void continue_controller_macro_wait()
    {
        McpControllerMacroState& macro = m_controller_macro;
        const char* stop = continue_macro_wait(macro.breakpoint_stops, macro.restore_pause,
            macro.wait_target_frame - emu_frame_counter);

        if (IsValidPointer(stop))
            finish_controller_macro_error(std::string("Controller macro ") + stop);
    }

    void finish_controller_macro_success()
    {
        McpControllerMacroState& macro = m_controller_macro;

        if (emu_debug_breakpoint_stops != macro.breakpoint_stops)
        {
            finish_controller_macro_error("Controller macro interrupted by breakpoint");
            return;
        }

        bool restore_pause = macro.restore_pause;
        DebugResponse* response = macro_response(macro.request_id, {
            {"success", true},
            {"commands", macro.command_count},
            {"steps", (int)macro.steps.size()},
            {"frames_waited", macro.frames_waited},
            {"held", (int)macro.held.size()},
            {"dropped", macro.dropped}
        });

        reset_controller_macro();
        push_macro_response(response, restore_pause);
    }

    void finish_controller_macro_error(const std::string& error)
    {
        bool restore_pause = m_controller_macro.restore_pause;
        release_controller_macro_held();
        DebugResponse* response = macro_error_response(m_controller_macro.request_id, error);
        reset_controller_macro();
        push_macro_response(response, restore_pause);
    }

    void reset_mouse_macro()
    {
        McpMouseMacroState& macro = m_mouse_macro;
        macro.active = false;
        macro.request_id = json();
        macro.phase = MCP_MOUSE_PHASE_START;
        macro.player = 0;
        macro.click = false;
        macro.absolute = false;
        macro.held = false;
        macro.drag = false;
        macro.dragged = false;
        macro.target_x = 0;
        macro.target_y = 0;
        macro.to_x = 0;
        macro.to_y = 0;
        macro.from_x = 0;
        macro.from_y = 0;
        macro.delta_x = 0;
        macro.delta_y = 0;
        macro.send_x = 0;
        macro.send_y = 0;
        macro.settle = 0;
        macro.stall = 0;
        macro.step = k_mcp_mouse_motion_step;
        macro.origin_x = 0;
        macro.origin_y = 0;
        macro.sent_x = 0;
        macro.sent_y = 0;
        macro.locate_frames = 0;
        macro.wiggle_counts = k_mcp_mouse_wiggle_counts;
        macro.wiggle_x = 0;
        macro.wiggle_y = 0;
        macro.wiggle_tries = 0;
        macro.wiggle_flip = false;
        macro.moves = 0;
        macro.misses = 0;
        macro.last_distance = 0;
        macro.screen_width = 0;
        macro.screen_height = 0;
        macro.frame_before.clear();
        macro.frame_out.clear();
        macro.button.clear();
        macro.action.clear();
        macro.button_mask = 0;
        macro.button_steps = 0;
        macro.button_press_first = false;
        macro.button_step = 0;
        macro.button_wait = 0;
        macro.press_frames = 0;
        macro.release_frames = 0;
        macro.frame = 0;
        macro.start_frame = 0;
        macro.restore_pause = false;
        macro.breakpoint_stops = 0;
        macro.generation = 0;
    }

    json start_mouse_macro(const std::string& tool, const json& arguments, const json& request_id)
    {
        json result;

        if (emu_is_empty())
        {
            result["error"] = "Emulator is powered off";
            return result;
        }

        if (unreported_breakpoint_stop())
        {
            result["error"] = "Stopped at a breakpoint not reported yet, see debug_get_status";
            return result;
        }

        int player = arguments.value("player", 0);

        if (player == 0)
            player = m_debug_adapter->FindMousePlayer();

        if (player == 0)
        {
            result["error"] = "No game port is set to mouse, use controller_set_type";
            return result;
        }

        if (!m_debug_adapter->IsMouseController(player))
        {
            result["error"] = "Port " + std::to_string(player) + " is not a mouse";
            return result;
        }

        bool click = tool == "mouse_click";
        bool absolute = arguments.contains("x") || arguments.contains("y");
        bool relative = arguments.contains("dx") || arguments.contains("dy");

        if (absolute && (!arguments.contains("x") || !arguments.contains("y")))
        {
            result["error"] = "x and y go together";
            return result;
        }

        if (!click && (absolute == relative))
        {
            result["error"] = "Give x and y to move to a screen pixel, or dx and dy to move by mouse counts";
            return result;
        }

        GT_Runtime_Info runtime;
        emu_get_runtime(runtime);
        int x = arguments.value("x", 0);
        int y = arguments.value("y", 0);

        if (absolute && ((x < 0) || (y < 0) || (x >= runtime.screen_width) || (y >= runtime.screen_height)))
        {
            result["error"] = "x and y must be inside the " + std::to_string(runtime.screen_width) + "x" +
                std::to_string(runtime.screen_height) + " screen";
            return result;
        }

        std::string button = arguments.value("button", "left");
        std::string action = arguments.value("action", "click");
        bool drag = click && (action == "drag");
        bool to = arguments.contains("to_x") || arguments.contains("to_y");
        int to_x = arguments.value("to_x", 0);
        int to_y = arguments.value("to_y", 0);

        if ((drag != to) || (to && (!arguments.contains("to_x") || !arguments.contains("to_y"))) ||
            (drag && !absolute))
        {
            result["error"] = "drag needs x, y to start and to_x, to_y to end, which only go with drag";
            return result;
        }

        if (drag && ((to_x < 0) || (to_y < 0) || (to_x >= runtime.screen_width) || (to_y >= runtime.screen_height)))
        {
            result["error"] = "to_x and to_y must be inside the " + std::to_string(runtime.screen_width) + "x" +
                std::to_string(runtime.screen_height) + " screen";
            return result;
        }

        bool held = m_debug_adapter->IsMouseButtonHeld(player);

        if (absolute && held && !m_debug_adapter->IsMousePointerKnown(player))
        {
            result["error"] = "A button is held and the pointer position is unknown: release it and move to x, y "
                "before pressing, or move by dx, dy";
            return result;
        }

        reset_mouse_macro();
        McpMouseMacroState& macro = m_mouse_macro;
        macro.request_id = request_id;
        macro.player = player;
        macro.click = click;
        macro.absolute = absolute;
        macro.held = held;
        macro.drag = drag;
        macro.target_x = x;
        macro.target_y = y;
        macro.to_x = to_x;
        macro.to_y = to_y;
        macro.delta_x = arguments.value("dx", 0);
        macro.delta_y = arguments.value("dy", 0);
        macro.button = button;
        macro.action = action;
        macro.button_mask = button == "right" ? GT_GAMEPAD_B : GT_GAMEPAD_A;
        macro.button_steps = action == "double_click" ? 4 : ((action == "click") || drag ? 2 : 1);
        macro.button_press_first = action != "release";
        macro.press_frames = arguments.value("press_frames", k_mcp_mouse_button_frames);
        macro.release_frames = arguments.value("release_frames", k_mcp_mouse_button_frames);
        macro.frame = emu_frame_counter;
        macro.start_frame = emu_frame_counter;
        macro.restore_pause = emu_is_paused() && !emu_is_debug_idle();
        macro.breakpoint_stops = emu_debug_breakpoint_stops;
        macro.generation = emu_machine_generation;
        macro.active = true;

        result["success"] = true;
        result["pending"] = true;
        return result;
    }

    void pump_mouse_macro()
    {
        if (emu_is_empty())
        {
            finish_mouse_macro_error("Emulator is powered off");
            return;
        }

        if (emu_machine_generation != m_mouse_macro.generation)
        {
            finish_mouse_macro_error("Mouse interrupted by a reset, state load or rewind");
            return;
        }

        if (emu_frame_counter != m_mouse_macro.frame)
        {
            m_mouse_macro.frame = emu_frame_counter;
            step_mouse_macro();

            if (!m_mouse_macro.active)
                return;
        }

        if (emu_is_debug_idle())
        {
            if (emu_debug_breakpoint_stops != m_mouse_macro.breakpoint_stops)
            {
                finish_mouse_macro_error("Mouse interrupted by breakpoint");
                return;
            }

            emu_debug_step_frames(1);
        }
        else if (m_mouse_macro.restore_pause && emu_is_paused())
            emu_resume();
        else if (emu_is_paused())
            finish_mouse_macro_error("Mouse interrupted by a pause");
    }

    void step_mouse_macro()
    {
        McpMouseMacroState& macro = m_mouse_macro;

        if (!m_debug_adapter->IsMouseController(macro.player))
        {
            finish_mouse_macro_error("Port " + std::to_string(macro.player) + " is no longer a mouse");
            return;
        }

        switch (macro.phase)
        {
            case MCP_MOUSE_PHASE_START:
                if (macro.absolute && macro.held)
                    begin_mouse_drag(macro.target_x, macro.target_y);
                else if (macro.absolute)
                    begin_mouse_locate(false);
                else if (!macro.click)
                {
                    m_debug_adapter->MoveMousePointerEstimate(macro.player, macro.delta_x, macro.delta_y);
                    begin_mouse_move(macro.delta_x, macro.delta_y);
                }
                else
                {
                    macro.phase = MCP_MOUSE_PHASE_BUTTONS;
                    step_mouse_buttons();
                }
                break;
            case MCP_MOUSE_PHASE_WIGGLE_OUT:
                if (!feed_mouse_motion())
                    break;

                capture_mouse_frame(macro.frame_out);
                macro.send_x = -macro.wiggle_x;
                macro.send_y = -macro.wiggle_y;
                macro.phase = MCP_MOUSE_PHASE_WIGGLE_BACK;
                feed_mouse_motion();
                break;
            case MCP_MOUSE_PHASE_WIGGLE_BACK:
                if (feed_mouse_motion())
                    finish_mouse_wiggle();
                break;
            case MCP_MOUSE_PHASE_RETRY:
                if (macro.settle > 0)
                    macro.settle--;
                else
                    begin_mouse_wiggle();
                break;
            case MCP_MOUSE_PHASE_MOVE:
                if (!feed_mouse_motion())
                    break;

                if (macro.absolute)
                    begin_mouse_locate(true);
                else
                    finish_mouse_macro_success();
                break;
            case MCP_MOUSE_PHASE_LOCATE:
                step_mouse_locate();
                break;
            case MCP_MOUSE_PHASE_DRAG:
                if (!feed_mouse_motion())
                    break;

                if (macro.click)
                {
                    macro.phase = MCP_MOUSE_PHASE_BUTTONS;
                    step_mouse_buttons();
                }
                else
                    finish_mouse_macro_success();
                break;
            case MCP_MOUSE_PHASE_BUTTONS:
                step_mouse_buttons();
                break;
        }
    }

    bool feed_mouse_motion()
    {
        McpMouseMacroState& macro = m_mouse_macro;

        if (m_debug_adapter->IsMouseMotionPending(macro.player))
        {
            macro.stall++;

            if (macro.stall >= k_mcp_mouse_stall_frames)
                finish_mouse_macro_error("The software isn't reading the mouse on port " +
                    std::to_string(macro.player));

            return false;
        }

        macro.stall = 0;

        if ((macro.send_x != 0) || (macro.send_y != 0))
        {
            int steps = (MAX(abs(macro.send_x), abs(macro.send_y)) + macro.step - 1) / macro.step;
            int step_x = ((macro.send_x * 2) + (macro.send_x < 0 ? -steps : steps)) / (steps * 2);
            int step_y = ((macro.send_y * 2) + (macro.send_y < 0 ? -steps : steps)) / (steps * 2);
            m_debug_adapter->ApplyMouseMotion(macro.player, step_x, step_y);
            macro.send_x -= step_x;
            macro.send_y -= step_y;
            macro.settle = k_mcp_mouse_settle_frames;
            return false;
        }

        if (macro.settle > 0)
        {
            macro.settle--;
            return false;
        }

        return true;
    }

    void capture_mouse_frame(std::vector<u32>& frame)
    {
        GT_Runtime_Info runtime;
        emu_get_runtime(runtime);
        const u32* source = (const u32*)emu_frame_buffer;
        frame.assign(source, source + (runtime.screen_width * runtime.screen_height));
    }

    void begin_mouse_wiggle()
    {
        McpMouseMacroState& macro = m_mouse_macro;
        const McpMousePointer& pointer = m_debug_adapter->GetMousePointer(macro.player);
        GT_Runtime_Info runtime;
        emu_get_runtime(runtime);
        bool known = m_debug_adapter->IsMousePointerKnown(macro.player);
        int x = macro.moves > 0 ? macro.target_x : (known ? pointer.x : 0);
        int y = macro.moves > 0 ? macro.target_y : (known ? pointer.y : 0);
        double gain_x = known ? MAX(pointer.pixels_per_count_x, k_mcp_mouse_wiggle_gain) : k_mcp_mouse_wiggle_gain;
        double gain_y = known ? MAX(pointer.pixels_per_count_y, k_mcp_mouse_wiggle_gain) : k_mcp_mouse_wiggle_gain;
        int reach_x = (int)(macro.wiggle_counts * gain_x) + 32;
        int reach_y = (int)(macro.wiggle_counts * gain_y) + 32;
        int sign = macro.wiggle_flip ? -1 : 1;

        macro.wiggle_x = ((x + reach_x) < runtime.screen_width ? macro.wiggle_counts : -macro.wiggle_counts) * sign;
        macro.wiggle_y = ((y + reach_y) < runtime.screen_height ? macro.wiggle_counts : -macro.wiggle_counts) * sign;
        macro.screen_width = runtime.screen_width;
        macro.screen_height = runtime.screen_height;
        macro.step = known ? pointer_motion_step() :
            CLAMP(macro.wiggle_counts / 8, k_mcp_mouse_motion_step, k_mcp_mouse_max_motion_step);
        capture_mouse_frame(macro.frame_before);
        macro.send_x = macro.wiggle_x;
        macro.send_y = macro.wiggle_y;
        macro.phase = MCP_MOUSE_PHASE_WIGGLE_OUT;
        feed_mouse_motion();
    }

    void finish_mouse_wiggle()
    {
        McpMouseMacroState& macro = m_mouse_macro;
        GT_Runtime_Info runtime;
        emu_get_runtime(runtime);
        McpMouseMiss miss = MCP_MOUSE_MISS_REJECTED;
        std::string reason = "the screen size changed";
        bool same_screen = (runtime.screen_width == macro.screen_width) &&
            (runtime.screen_height == macro.screen_height) && (macro.frame_out.size() == macro.frame_before.size());
        bool found = same_screen && m_debug_adapter->FindMousePointer(macro.player, macro.frame_before.data(),
            macro.frame_out.data(), (const u32*)emu_frame_buffer, macro.screen_width, macro.screen_height,
            macro.wiggle_x, macro.wiggle_y, miss, reason);

        if (!found)
        {
            macro.wiggle_tries++;

            if (miss != MCP_MOUSE_MISS_OVERLAP)
                macro.wiggle_flip = !macro.wiggle_flip;
            else if (macro.wiggle_counts < k_mcp_mouse_wiggle_max_counts)
                macro.wiggle_counts *= 2;

            if (macro.wiggle_tries >= k_mcp_mouse_wiggle_tries)
                finish_mouse_macro_error("No mouse pointer found after " + std::to_string(macro.wiggle_tries) +
                    " tries, the last one because " + reason + ". The software may hide it, draw none or not "
                    "read this port; try again or move it with dx and dy");
            else if (miss == MCP_MOUSE_MISS_UNSEEN)
            {
                macro.settle = k_mcp_mouse_unseen_frames;
                macro.phase = MCP_MOUSE_PHASE_RETRY;
            }
            else
                begin_mouse_wiggle();

            return;
        }

        macro.wiggle_tries = 0;
        steer_mouse_pointer();
    }

    void begin_mouse_locate(bool moved)
    {
        McpMouseMacroState& macro = m_mouse_macro;

        if (!m_debug_adapter->IsMousePointerKnown(macro.player))
        {
            begin_mouse_wiggle();
            return;
        }

        if (!moved)
        {
            const McpMousePointer& pointer = m_debug_adapter->GetMousePointer(macro.player);
            macro.origin_x = pointer.x;
            macro.origin_y = pointer.y;
            macro.sent_x = 0;
            macro.sent_y = 0;
        }

        macro.locate_frames = k_mcp_mouse_locate_frames;
        macro.phase = MCP_MOUSE_PHASE_LOCATE;
        step_mouse_locate();
    }

    void step_mouse_locate()
    {
        McpMouseMacroState& macro = m_mouse_macro;
        GT_Runtime_Info runtime;
        emu_get_runtime(runtime);

        if (m_debug_adapter->LocateMousePointer(macro.player, (const u32*)emu_frame_buffer, runtime.screen_width,
            runtime.screen_height, macro.origin_x, macro.origin_y, macro.sent_x, macro.sent_y))
        {
            steer_mouse_pointer();
            return;
        }

        if (--macro.locate_frames <= 0)
            begin_mouse_wiggle();
    }

    void steer_mouse_pointer()
    {
        McpMouseMacroState& macro = m_mouse_macro;
        const McpMousePointer& pointer = m_debug_adapter->GetMousePointer(macro.player);
        int error_x = macro.target_x - pointer.x;
        int error_y = macro.target_y - pointer.y;
        int counts_x = pixels_to_counts(error_x, pointer.pixels_per_count_x);
        int counts_y = pixels_to_counts(error_y, pointer.pixels_per_count_y);
        int largest = MAX(abs(counts_x), abs(counts_y));

        if (largest > k_mcp_mouse_max_move_counts)
        {
            counts_x = (counts_x * k_mcp_mouse_max_move_counts) / largest;
            counts_y = (counts_y * k_mcp_mouse_max_move_counts) / largest;
        }

        int distance = abs(error_x) + abs(error_y);
        bool closest = (counts_x == 0) && (counts_y == 0);
        if ((macro.moves > 0) && (distance >= macro.last_distance))
            macro.misses++;
        else
            macro.misses = 0;

        bool stuck = (macro.moves >= k_mcp_mouse_max_moves) || (macro.misses >= k_mcp_mouse_max_misses);

        if (closest || stuck)
        {
            bool covered = (abs(error_x) <= (pointer.width / 2)) && (abs(error_y) <= (pointer.height / 2));

            if (!macro.click)
                finish_mouse_macro_success();
            else if (closest || covered)
            {
                macro.phase = MCP_MOUSE_PHASE_BUTTONS;
                step_mouse_buttons();
            }
            else
                finish_mouse_macro_error("The pointer stopped at " + std::to_string(pointer.x) + "," +
                    std::to_string(pointer.y) + " and can't reach " + std::to_string(macro.target_x) + "," +
                    std::to_string(macro.target_y));

            return;
        }

        macro.last_distance = distance;
        macro.moves++;
        macro.origin_x = pointer.x;
        macro.origin_y = pointer.y;
        macro.sent_x = counts_x;
        macro.sent_y = counts_y;
        m_debug_adapter->MoveMousePointerEstimate(macro.player, counts_x, counts_y);
        begin_mouse_move(counts_x, counts_y);
    }

    // Software that turns many counts into a pixel gets bigger steps, so the pointer still crosses the screen in a
    // couple of seconds; a pointer near a pixel per count keeps the small steps drivers don't speed up. Software that
    // drops what's left of a division between its reads moves less with small steps, which the next measure raises
    int pointer_motion_step()
    {
        if (!m_debug_adapter->IsMousePointerKnown(m_mouse_macro.player))
            return k_mcp_mouse_motion_step;

        const McpMousePointer& pointer = m_debug_adapter->GetMousePointer(m_mouse_macro.player);
        double gain = MAX(MIN(fabs(pointer.pixels_per_count_x), fabs(pointer.pixels_per_count_y)), 1.0 / 1024.0);
        int step = (int)(k_mcp_mouse_pixels_per_frame / gain);
        return CLAMP(step, k_mcp_mouse_motion_step, k_mcp_mouse_max_motion_step);
    }

    int pixels_to_counts(int pixels, double pixels_per_count)
    {
        double counts = pixels / pixels_per_count;
        return (int)(counts + (counts < 0.0 ? -0.5 : 0.5));
    }

    void begin_mouse_move(int counts_x, int counts_y)
    {
        m_mouse_macro.step = pointer_motion_step();
        m_mouse_macro.send_x = counts_x;
        m_mouse_macro.send_y = counts_y;
        m_mouse_macro.phase = MCP_MOUSE_PHASE_MOVE;
        feed_mouse_motion();
    }

    void begin_mouse_drag(int x, int y)
    {
        McpMouseMacroState& macro = m_mouse_macro;

        if (!m_debug_adapter->IsMousePointerKnown(macro.player))
        {
            finish_mouse_macro_error("The pointer position was lost before the drag");
            return;
        }

        const McpMousePointer& pointer = m_debug_adapter->GetMousePointer(macro.player);
        int counts_x = pixels_to_counts(x - pointer.x, pointer.pixels_per_count_x);
        int counts_y = pixels_to_counts(y - pointer.y, pointer.pixels_per_count_y);

        m_debug_adapter->MoveMousePointerEstimate(macro.player, counts_x, counts_y);
        macro.step = pointer_motion_step();
        macro.send_x = counts_x;
        macro.send_y = counts_y;
        macro.phase = MCP_MOUSE_PHASE_DRAG;
        feed_mouse_motion();
    }

    void step_mouse_buttons()
    {
        McpMouseMacroState& macro = m_mouse_macro;

        if (macro.button_wait > 0)
        {
            macro.button_wait--;
            return;
        }

        if (macro.drag && !macro.dragged && (macro.button_step == 1))
        {
            const McpMousePointer& pointer = m_debug_adapter->GetMousePointer(macro.player);
            macro.from_x = pointer.x;
            macro.from_y = pointer.y;
            macro.dragged = true;
            begin_mouse_drag(macro.to_x, macro.to_y);
            return;
        }

        if (macro.button_step >= macro.button_steps)
        {
            finish_mouse_macro_success();
            return;
        }

        bool pressed = macro.button_press_first && ((macro.button_step % 2) == 0);
        m_debug_adapter->SetMouseButton(macro.player, macro.button_mask, pressed);
        macro.button_step++;
        macro.button_wait = (pressed ? macro.press_frames : macro.release_frames) - 1;
    }

    void finish_mouse_macro_success()
    {
        McpMouseMacroState& macro = m_mouse_macro;

        if (emu_debug_breakpoint_stops != macro.breakpoint_stops)
        {
            finish_mouse_macro_error("Mouse interrupted by breakpoint");
            return;
        }

        bool restore_pause = macro.restore_pause;
        json result = {{"success", true}, {"player", macro.player}, {"frames", emu_frame_counter - macro.start_frame}};

        const McpMousePointer& pointer = m_debug_adapter->GetMousePointer(macro.player);

        if (macro.drag)
        {
            result["from"] = {{"x", macro.from_x}, {"y", macro.from_y}};
            result["to"] = {{"x", pointer.x}, {"y", pointer.y}};
        }
        else if (macro.absolute)
        {
            result["x"] = pointer.x;
            result["y"] = pointer.y;
            result["reached"] = (pointer.x == macro.target_x) && (pointer.y == macro.target_y);
            result["box"] = {{"x", pointer.x}, {"y", pointer.y}, {"width", pointer.width}, {"height", pointer.height}};
        }
        else if (!macro.click)
        {
            result["dx"] = macro.delta_x;
            result["dy"] = macro.delta_y;
        }

        if (macro.absolute && pointer.estimated)
            result["estimated"] = true;

        if (macro.click)
        {
            result["button"] = macro.button;
            result["action"] = macro.action;
        }

        DebugResponse* response = macro_response(macro.request_id, result);
        reset_mouse_macro();
        push_macro_response(response, restore_pause);
    }

    void finish_mouse_macro_error(const std::string& error)
    {
        McpMouseMacroState& macro = m_mouse_macro;
        bool restore_pause = macro.restore_pause;
        m_reported_breakpoint_stops = emu_debug_breakpoint_stops;

        if (macro.click && (macro.button_steps > 1) && (macro.button_step > 0))
            m_debug_adapter->SetMouseButton(macro.player, macro.button_mask, false);

        if (macro.player > 0)
            m_debug_adapter->ForgetMousePointer(macro.player);

        DebugResponse* response = new DebugResponse();
        response->requestId = macro.request_id;
        response->isToolError = true;
        response->errorMessage = error;
        response->result = {{"error", error}};

        reset_mouse_macro();

        if (restore_pause)
            emu_pause();

        m_response_queue.Push(response);
    }

    void reset_step_frames()
    {
        m_step_frames.active = false;
        m_step_frames.request_id = json();
        m_step_frames.frames = 0;
        m_step_frames.start_frame = 0;
        m_step_frames.breakpoint_stops = 0;
        m_step_frames.generation = 0;
    }

    json start_step_frames(const json& arguments, const json& request_id)
    {
        int frames = arguments.value("frames", 1);
        bool wait = arguments.value("wait", true);

        if (emu_is_empty())
            return {{"error", "Emulator is powered off"}};

        if (unreported_breakpoint_stop())
            return {{"error", "Stopped at a breakpoint not reported yet, see debug_get_status"}};

        if (wait && (frames > k_mcp_step_wait_max_frames))
            return {{"error", "Waiting covers up to " + std::to_string(k_mcp_step_wait_max_frames) +
                " frames, pass wait false for more and poll debug_get_status"}};

        if (!wait)
        {
            m_debug_adapter->StepFrames(frames);
            return {{"success", true}, {"pending", true}, {"requested_frames", frames}};
        }

        reset_step_frames();
        m_step_frames.request_id = request_id;
        m_step_frames.frames = frames;
        m_step_frames.start_frame = emu_frame_counter;
        m_step_frames.breakpoint_stops = emu_debug_breakpoint_stops;
        m_step_frames.generation = emu_machine_generation;
        m_step_frames.active = true;
        m_debug_adapter->StepFrames(frames);
        return json();
    }

    void pump_step_frames()
    {
        DebugResponse* response = NULL;

        if (emu_is_empty())
        {
            response = new DebugResponse();
            response->result = {{"error", "Emulator is powered off"}};
        }
        else if (emu_machine_generation != m_step_frames.generation)
        {
            response = new DebugResponse();
            response->result = {{"error", "Frames interrupted by a reset, state load or rewind"}};
        }
        else if (emu_is_debug_idle())
        {
            response = new DebugResponse();
            response->result = m_debug_adapter->GetDebugStatus();
            response->result["success"] = true;
            response->result["frames"] = emu_frame_counter - m_step_frames.start_frame;
            response->result["requested_frames"] = m_step_frames.frames;
            response->result["breakpoint_hit"] = emu_debug_breakpoint_stops != m_step_frames.breakpoint_stops;
        }
        else if (emu_is_paused())
        {
            response = new DebugResponse();
            response->result = {{"error", "Frames interrupted by a pause"}};
        }

        if (!IsValidPointer(response))
            return;

        response->requestId = m_step_frames.request_id;
        UpdateResponseError(response);
        reset_step_frames();
        m_reported_breakpoint_stops = emu_debug_breakpoint_stops;
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
    McpControllerMacroState m_controller_macro;
    McpMouseMacroState m_mouse_macro;
    McpStepFramesState m_step_frames;
    bool m_mouse_motion_held[GT_MAX_GAMEPADS][MCP_MOUSE_MOTION_COUNT];
    u64 m_mouse_motion_frame;
    u64 m_reported_breakpoint_stops;
};

#endif /* MCP_MANAGER_H */
