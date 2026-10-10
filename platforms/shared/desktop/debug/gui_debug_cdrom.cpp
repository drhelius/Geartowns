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

#define GUI_DEBUG_CDROM_IMPORT
#include "gui_debug_cdrom.h"

#include "imgui.h"
#include "geartowns.h"
#include "cdrom/cdrom.h"
#include "cdrom/cdrom_audio.h"
#include "cdrom/cdrom_media.h"
#include "../config.h"
#include "../emu.h"
#include "../gui.h"
#include "../gui_colors.h"
#include "gui_debug_constants.h"

static void draw_flag(const char* name, bool value);
static void draw_flag_tooltip(const char* text);
static void draw_lba(u32 lba);

void gui_debug_window_cdrom(void)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::SetNextWindowPos(ImVec2(207, 109), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(213, 599), ImGuiCond_FirstUseEver);
    ImGui::Begin("CD-ROM Controller", &config_debug.show_cdrom);

    ImGui::PushFont(gui_default_font);

    GeartownsCore* core = emu_get_core();
    CdRom* cdrom = core->GetCDROM();
    CdRom::CdRom_State* state = cdrom->GetState();
    CdRomMedia* media = core->GetCDROMMedia();
    u8 master = cdrom->Peek(0x04C0);
    u8 command = state->command & k_cdrom_command_mask;

    ImGui::TextColored(cyan, "PORTS"); ImGui::Separator();

    ImGui::TextColored(violet, "MASTER  "); ImGui::SameLine();
    ImGui::TextColored(white, "$%02X", master); ImGui::SameLine();
    ImGui::TextColored(gray, "04C0");
    ImGui::TextColored(violet, "        "); ImGui::SameLine();
    draw_flag("SIRQ", (master & 0x80) != 0);
    draw_flag_tooltip("Interrupt cause: status"); ImGui::SameLine();
    draw_flag("DEI", (master & 0x40) != 0);
    draw_flag_tooltip("Interrupt cause: DMA end"); ImGui::SameLine();
    draw_flag("STSF", (master & 0x20) != 0);
    draw_flag_tooltip("CPU transfer in progress"); ImGui::SameLine();
    draw_flag("DTSF", (master & 0x10) != 0);
    draw_flag_tooltip("DMA transfer in progress");
    ImGui::TextColored(violet, "        "); ImGui::SameLine();
    draw_flag("SRQ", (master & 0x02) != 0);
    draw_flag_tooltip("Status available"); ImGui::SameLine();
    draw_flag("DRY", (master & 0x01) != 0);
    draw_flag_tooltip("Ready for a command");

    ImGui::TextColored(violet, "COMMAND "); ImGui::SameLine();
    ImGui::TextColored(white, "$%02X", state->command); ImGui::SameLine();
    ImGui::TextColored(gray, "04C2");
    ImGui::TextColored(violet, "        "); ImGui::SameLine();
    ImGui::TextColored(blue, "%-11s", gui_debug_cdrom_command_name(command));
    ImGui::TextColored(violet, "        "); ImGui::SameLine();
    draw_flag("+IRQ", (state->command & k_cdrom_flag_irq) != 0); ImGui::SameLine();
    draw_flag("+STATUS", (state->command & k_cdrom_flag_status) != 0);

    ImGui::TextColored(violet, "PARAMS  "); ImGui::SameLine();

    for (int i = 0; i < CDROM_PARAM_COUNT; i++)
    {
        if (i == 4)
        {
            ImGui::TextColored(violet, "        "); ImGui::SameLine();
        }

        ImGui::TextColored(white, "%02X", state->active_params[i]);

        if (i != 3 && i != CDROM_PARAM_COUNT - 1)
            ImGui::SameLine();
    }

    ImGui::TextColored(violet, "PENDING "); ImGui::SameLine();
    ImGui::TextColored(state->param_count ? white : gray, "%d", state->param_count);

    ImGui::TextColored(violet, "STATUS  "); ImGui::SameLine();
    ImGui::TextColored(state->status_count ? white : gray, "%4d QUEUED", state->status_count);
    ImGui::TextColored(violet, "        "); ImGui::SameLine();

    for (int i = 0; i < 4; i++)
    {
        if (i < state->status_count)
            ImGui::TextColored(i == 0 ? yellow : white, "%02X",
                state->status[(state->status_head + i) % CDROM_STATUS_QUEUE_SIZE]);
        else
            ImGui::TextColored(gray, "--");

        if (i < 3)
            ImGui::SameLine();
    }

    static const char* transfer_names[4] = { "OFF  ", "READY", "DMA  ", "CPU  " };
    ImGui::TextColored(violet, "TRANSFER"); ImGui::SameLine();
    ImGui::TextColored(state->transfer != CdRom::CDROM_TRANSFER_NONE ? blue : gray, "%s",
        transfer_names[state->transfer & 3]);

    ImGui::NewLine(); ImGui::TextColored(cyan, "INTERRUPTS"); ImGui::Separator();

    bool irq = (state->sirq && state->sirq_irq && state->enable_sirq) || (state->dei && state->enable_dei);
    ImGui::TextColored(violet, "SIRQ    "); ImGui::SameLine();
    draw_flag("ENABLE", state->enable_sirq); ImGui::SameLine();
    draw_flag("PENDING", state->sirq);
    ImGui::TextColored(violet, "DEI     "); ImGui::SameLine();
    draw_flag("ENABLE", state->enable_dei); ImGui::SameLine();
    draw_flag("PENDING", state->dei);
    ImGui::TextColored(violet, "IRQ9    "); ImGui::SameLine();
    ImGui::TextColored(irq ? yellow : gray, "%s", irq ? "HIGH" : "LOW ");

    ImGui::NewLine(); ImGui::TextColored(cyan, "DRIVE"); ImGui::Separator();

    bool reading = gui_debug_cdrom_reading();
    bool ready = media->IsReady();
    u32 head = gui_debug_cdrom_head();

    ImGui::TextColored(violet, "STATE   "); ImGui::SameLine();
    ImGui::TextColored(ready ? blue : gray, "%-7s", gui_debug_cdrom_drive_state());
    ImGui::TextColored(violet, "HEAD    "); ImGui::SameLine();

    if (ready)
        draw_lba(head);
    else
        ImGui::TextColored(gray, "--");

    ImGui::TextColored(violet, "TRACK   "); ImGui::SameLine();
    s32 track = ready ? media->FindTrackFromLBA(head, true) : -1;

    if (track >= 0)
        ImGui::TextColored(white, "%02d", track + 1);
    else
        ImGui::TextColored(gray, "--");

    ImGui::TextColored(violet, "RANGE   "); ImGui::SameLine();

    if (reading)
        ImGui::TextColored(white, "%u-%u", state->read_lba, state->read_end_lba);
    else
        ImGui::TextColored(gray, "--");

    ImGui::TextColored(violet, "LEFT    "); ImGui::SameLine();

    if (reading && state->read_lba <= state->read_end_lba)
        ImGui::TextColored(white, "%u SECTORS", state->read_end_lba - state->read_lba + 1);
    else
        ImGui::TextColored(gray, "--");

    ImGui::NewLine(); ImGui::TextColored(cyan, "MEDIA"); ImGui::Separator();

    if (ready)
    {
        GT_CdRomMSF length = media->GetCdRomLength();
        ImGui::TextColored(violet, "TYPE    "); ImGui::SameLine();
        ImGui::TextColored(white, "%s", media->GetFileExtension());
        ImGui::TextColored(violet, "TRACKS  "); ImGui::SameLine();
        ImGui::TextColored(white, "%d", media->GetTrackCount());
        ImGui::TextColored(violet, "LENGTH  "); ImGui::SameLine();
        ImGui::TextColored(white, "%02u:%02u:%02u", length.minutes, length.seconds, length.frames);
        ImGui::TextColored(violet, "SECTORS "); ImGui::SameLine();
        ImGui::TextColored(white, "%u", media->GetSectorCount());
    }
    else
    {
        static const char* rows[4] = { "TYPE    ", "TRACKS  ", "LENGTH  ", "SECTORS " };

        for (int i = 0; i < 4; i++)
        {
            ImGui::TextColored(violet, "%s", rows[i]); ImGui::SameLine();
            ImGui::TextColored(gray, "--");
        }
    }

    ImGui::TextColored(violet, "CHANGED "); ImGui::SameLine();
    ImGui::TextColored(state->disc_changed ? yellow : gray, "%s", state->disc_changed ? "YES" : "NO ");

    ImGui::PopFont();

    ImGui::End();
    ImGui::PopStyleVar();
}

void gui_debug_window_cdrom_toc(void)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::SetNextWindowPos(ImVec2(248, 176), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(560, 420), ImGuiCond_FirstUseEver);
    ImGui::Begin("CD-ROM TOC", &config_debug.show_cdrom_toc);

    ImGui::PushFont(gui_default_font);

    GeartownsCore* core = emu_get_core();
    CdRomMedia* media = core->GetCDROMMedia();
    bool ready = media->IsReady();

    if (!ready)
    {
        ImGui::TextColored(gray, "No CD-ROM inserted");
        ImGui::PopFont();
        ImGui::End();
        ImGui::PopStyleVar();
        return;
    }

    const std::vector<CdRomImage::Track>& tracks = media->GetTracks();
    s32 current_track = media->FindTrackFromLBA(gui_debug_cdrom_head(), true);
    int audio_tracks = 0;
    GT_CdRomMSF length = media->GetCdRomLength();

    for (size_t i = 0; i < tracks.size(); i++)
        audio_tracks += tracks[i].type == GT_CDROM_AUDIO_TRACK ? 1 : 0;

    ImGui::TextColored(violet, "MEDIA   "); ImGui::SameLine();
    ImGui::TextColored(white, "%s", media->GetFileName());
    ImGui::TextColored(violet, "TRACKS  "); ImGui::SameLine();
    ImGui::TextColored(white, "%d", (int)tracks.size()); ImGui::SameLine();
    ImGui::TextColored(green, " (AUDIO %d)", audio_tracks); ImGui::SameLine();
    ImGui::TextColored(yellow, " (DATA %d)", (int)tracks.size() - audio_tracks);
    ImGui::TextColored(violet, "LENGTH  "); ImGui::SameLine();
    ImGui::TextColored(white, "%02u:%02u:%02u", length.minutes, length.seconds, length.frames); ImGui::SameLine();
    ImGui::TextColored(violet, "  SECTORS"); ImGui::SameLine();
    ImGui::TextColored(white, "%u", media->GetSectorCount());
    ImGui::Separator();

    ImGuiTableFlags flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter |
        ImGuiTableFlags_BordersV | ImGuiTableFlags_SizingFixedFit;

    if (ImGui::BeginTable("##cdrom_toc", 9, flags))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("#");
        ImGui::TableSetupColumn("TYPE");
        ImGui::TableSetupColumn("START MSF");
        ImGui::TableSetupColumn("END MSF");
        ImGui::TableSetupColumn("LENGTH");
        ImGui::TableSetupColumn("START LBA");
        ImGui::TableSetupColumn("END LBA");
        ImGui::TableSetupColumn("SECTORS");
        ImGui::TableSetupColumn("PREGAP");
        ImGui::TableHeadersRow();

        for (size_t i = 0; i < tracks.size(); i++)
        {
            const CdRomImage::Track& track = tracks[i];
            GT_CdRomMSF start;
            GT_CdRomMSF end;
            GT_CdRomMSF duration;
            LbaToMsf(track.start_lba + 150, &start);
            LbaToMsf(track.end_lba + 150, &end);
            LbaToMsf(track.sector_count, &duration);
            bool current = (s32)i == current_track;

            ImGui::TableNextRow();

            if (current)
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(dark_blue));

            ImGui::TableNextColumn();
            ImGui::TextColored(current ? orange : white, "%02d", (int)i + 1);
            ImGui::TableNextColumn();
            ImGui::TextColored(track.type == GT_CDROM_AUDIO_TRACK ? green : yellow, "%s", TrackTypeName(track.type));
            ImGui::TableNextColumn();
            ImGui::Text("%02u:%02u:%02u", start.minutes, start.seconds, start.frames);
            ImGui::TableNextColumn();
            ImGui::Text("%02u:%02u:%02u", end.minutes, end.seconds, end.frames);
            ImGui::TableNextColumn();
            ImGui::Text("%02u:%02u:%02u", duration.minutes, duration.seconds, duration.frames);
            ImGui::TableNextColumn();
            ImGui::Text("%u", track.start_lba);
            ImGui::TableNextColumn();
            ImGui::Text("%u", track.end_lba);
            ImGui::TableNextColumn();
            ImGui::Text("%u", track.sector_count);
            ImGui::TableNextColumn();

            if (track.has_lead_in)
                ImGui::Text("%u", track.start_lba - track.lead_in_lba);
            else
                ImGui::TextColored(gray, "--");
        }

        ImGui::EndTable();
    }

    ImGui::PopFont();

    ImGui::End();
    ImGui::PopStyleVar();
}

bool gui_debug_cdrom_reading(void)
{
    CdRom::CdRom_State* state = emu_get_core()->GetCDROM()->GetState();
    u8 active = state->active_command & k_cdrom_command_mask;
    bool read_command = active == CdRom::CDROM_COMMAND_MODE1_READ || active == CdRom::CDROM_COMMAND_MODE2_READ ||
        active == CdRom::CDROM_COMMAND_RAW_READ;
    bool transferring = state->transfer != CdRom::CDROM_TRANSFER_NONE;
    bool pending = state->event == CdRom::CDROM_EVENT_SECTOR || state->event == CdRom::CDROM_EVENT_LOST_DATA;

    return read_command && (transferring || (pending && state->read_lba <= state->read_end_lba));
}

const char* gui_debug_cdrom_drive_state(void)
{
    CdRomAudio* audio = emu_get_core()->GetCDROMAudio();

    if (emu_get_core()->GetCDROM()->GetState()->event == CdRom::CDROM_EVENT_SEEK_DONE)
        return "SEEKING";

    if (gui_debug_cdrom_reading())
        return "READING";

    if (audio->IsPlaying())
        return "PLAYING";

    if (audio->IsPaused())
        return "PAUSED";

    return "IDLE";
}

u32 gui_debug_cdrom_head(void)
{
    CdRomAudio* audio = emu_get_core()->GetCDROMAudio();

    if (audio->IsPlaying() || audio->IsPaused())
        return audio->GetCurrentLBA();

    return emu_get_core()->GetCDROMMedia()->GetCurrentSector();
}

static void draw_flag(const char* name, bool value)
{
    ImGui::TextColored(value ? green : gray, "%s", name);
}

static void draw_flag_tooltip(const char* text)
{
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", text);
}

static void draw_lba(u32 lba)
{
    GT_CdRomMSF msf;
    LbaToMsf(lba + 150, &msf);
    ImGui::TextColored(white, "%6u", lba); ImGui::SameLine();
    ImGui::TextColored(orange, "%02u:%02u:%02u", msf.minutes, msf.seconds, msf.frames);
}
