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

#define GUI_DEBUG_AUDIO_IMPORT
#include "gui_debug_audio.h"

#include <math.h>
#include "imgui.h"
#include "implot.h"
#include "fonts/IconsMaterialDesign.h"
#include "geartowns.h"
#include "audio/audio.h"
#include "cdrom/cdrom_audio.h"
#include "cdrom/cdrom_media.h"
#include "../config.h"
#include "../emu.h"
#include "../gui.h"
#include "../gui_colors.h"
#include "../utils.h"
#include "gui_debug_constants.h"
#include "gui_debug_memory.h"

static const double k_ym3438_sample_rate = (double)GT_SOUND_CLOCK_RATE / k_ym3438_native_sample_cycles;
static const double k_rf5c68_sample_rate = (double)GT_SOUND_CLOCK_RATE / k_rf5c68_cycles_per_sample;
static const int k_scope_half_window = 100;

struct AlgorithmLayout
{
    s8 x[4];
    s8 y[4];
    u8 links[4][2];
    int link_count;
    u8 carriers;
};

static const AlgorithmLayout k_algorithm_layouts[8] =
{
    { { 0, 1, 2, 3 }, { 0, 0, 0, 0 }, { { 0, 1 }, { 1, 2 }, { 2, 3 }, { 0, 0 } }, 3, 0x08 },
    { { 0, 0, 1, 2 }, { 0, 1, 0, 0 }, { { 0, 2 }, { 1, 2 }, { 2, 3 }, { 0, 0 } }, 3, 0x08 },
    { { 1, 0, 1, 2 }, { 0, 1, 1, 0 }, { { 0, 3 }, { 1, 2 }, { 2, 3 }, { 0, 0 } }, 3, 0x08 },
    { { 0, 1, 1, 2 }, { 0, 0, 1, 0 }, { { 0, 1 }, { 1, 3 }, { 2, 3 }, { 0, 0 } }, 3, 0x08 },
    { { 0, 1, 0, 1 }, { 0, 0, 1, 1 }, { { 0, 1 }, { 2, 3 }, { 0, 0 }, { 0, 0 } }, 2, 0x0A },
    { { 0, 1, 1, 1 }, { 1, 0, 1, 2 }, { { 0, 1 }, { 0, 2 }, { 0, 3 }, { 0, 0 } }, 3, 0x0E },
    { { 0, 1, 1, 1 }, { 0, 0, 1, 2 }, { { 0, 1 }, { 0, 0 }, { 0, 0 }, { 0, 0 } }, 1, 0x0E },
    { { 0, 1, 2, 3 }, { 0, 0, 0, 0 }, { { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } }, 0, 0x0F }
};

static float* scope_buffer = NULL;
static int fm_solo = -1;
static int pcm_solo = -1;

static void draw_scope(const char* id, const s16* data, int stride, int count, float scale, ImVec2 size);
static void draw_stereo_scope(const char* id, int source, float scale, ImVec2 size);
static bool draw_mute_button(const char* id, bool muted, const char* tooltip);
static bool draw_solo_button(const char* id, bool solo);
static void set_fm_solo(int channel);
static void set_pcm_solo(int channel);
static void draw_fm_global(YM3438::YM3438_State* state);
static void draw_fm_channel(YM3438* ym3438, int channel);
static void draw_algorithm(int algorithm);
static void draw_frequency(u16 f_number, u8 block, bool active);
static void draw_rf5c68_channels(RF5C68* rf5c68, const s16* const* buffers, int count);
static void draw_wave_ram(RF5C68* rf5c68);
static void draw_volume_chip(Audio* audio, int chip);
static void goto_pcm_ram(u32 offset);
static void draw_msf(u32 lba);

void gui_debug_audio_init(void)
{
    scope_buffer = new float[GT_AUDIO_BUFFER_SIZE / 2];
}

void gui_debug_audio_destroy(void)
{
    SafeDeleteArray(scope_buffer);
}

void gui_debug_window_ym3438(void)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::SetNextWindowPos(ImVec2(90, 60), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(520, 540), ImGuiCond_FirstUseEver);
    ImGui::Begin("YM3438 FM", &config_debug.show_ym3438);

    Audio* audio = emu_get_core()->GetAudio();
    YM3438* ym3438 = audio->GetYM3438();
    YM3438::YM3438_State* state = ym3438->GetState();
    bool muted = audio->IsSourceMuted(Audio::AUDIO_SOURCE_FM);

    if (draw_mute_button("##fm_mute", muted, "Mute FM"))
        audio->SetSourceMute(Audio::AUDIO_SOURCE_FM, !muted);

    ImGui::SameLine();
    draw_stereo_scope("##fm_mix", Audio::AUDIO_SOURCE_FM, 1.0f / 32768.0f, ImVec2(200, 50));

    ImGui::PushFont(gui_default_font);
    draw_fm_global(state);
    ImGui::PopFont();

    if (ImGui::BeginTabBar("##fm_channels"))
    {
        for (int channel = 0; channel < YM3438_CHANNEL_COUNT; channel++)
        {
            char tab[8];
            snprintf(tab, sizeof(tab), "CH%d", channel + 1);

            if (ImGui::BeginTabItem(tab))
            {
                draw_fm_channel(ym3438, channel);
                ImGui::EndTabItem();
            }
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

void gui_debug_window_ym3438_registers(void)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::SetNextWindowPos(ImVec2(120, 90), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(470, 330), ImGuiCond_FirstUseEver);
    ImGui::Begin("YM3438 Registers", &config_debug.show_ym3438_registers);

    YM3438* ym3438 = emu_get_core()->GetAudio()->GetYM3438();
    u16 latch = ym3438->GetSelectedAddress();

    if (ImGui::BeginTabBar("##fm_register_parts"))
    {
        for (int part = 0; part < 2; part++)
        {
            if (!ImGui::BeginTabItem(part == 0 ? "PART 0" : "PART 1"))
                continue;

            ImGui::PushFont(gui_default_font);
            ImGui::TextColored(violet, "ADDRESS LATCH"); ImGui::SameLine();
            ImGui::TextColored(white, "PART %d $%02X", (latch >> 8) & 1, latch & 0xFF);

            ImGuiTableFlags flags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX;

            if (ImGui::BeginTable("##fm_registers", 17, flags))
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                for (int column = 0; column < 16; column++)
                {
                    ImGui::TableNextColumn();
                    ImGui::TextColored(cyan, " %X", column);
                }

                for (int row = 0; row < 16; row++)
                {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextColored(cyan, "%X0", row);

                    for (int column = 0; column < 16; column++)
                    {
                        u8 address = (u8)((row << 4) | column);
                        char name[64];
                        bool used = gui_debug_ym3438_register_name(part, address, name, sizeof(name));
                        bool latched = (latch >> 8) == part && (latch & 0xFF) == address;

                        ImGui::TableNextColumn();
                        ImGui::TextColored(latched ? yellow : used ? white : dark_gray, "%02X",
                            ym3438->GetRegister((u16)((part << 8) | address)));

                        if (ImGui::IsItemHovered())
                        {
                            ImGui::BeginTooltip();
                            ImGui::TextColored(cyan, "PART %d $%02X", part, address);
                            ImGui::Text("%s", used ? name : "Unused");
                            ImGui::EndTooltip();
                        }
                    }
                }

                ImGui::EndTable();
            }

            ImGui::PopFont();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

void gui_debug_window_rf5c68(void)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::SetNextWindowPos(ImVec2(150, 70), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(540, 560), ImGuiCond_FirstUseEver);
    ImGui::Begin("RF5C68 PCM", &config_debug.show_rf5c68);

    Audio* audio = emu_get_core()->GetAudio();
    RF5C68* rf5c68 = audio->GetRF5C68();
    RF5C68::RF5C68_State* state = rf5c68->GetState();
    bool muted = audio->IsSourceMuted(Audio::AUDIO_SOURCE_PCM);

    if (draw_mute_button("##pcm_mute", muted, "Mute PCM"))
        audio->SetSourceMute(Audio::AUDIO_SOURCE_PCM, !muted);

    ImGui::SameLine();
    draw_stereo_scope("##pcm_mix", Audio::AUDIO_SOURCE_PCM, 1.0f / 32768.0f, ImVec2(200, 50));

    ImGui::PushFont(gui_default_font);

    ImGui::TextColored(cyan, "GLOBAL"); ImGui::Separator();

    u8 enables = 0;

    for (int i = 0; i < RF5C68_CHANNEL_COUNT; i++)
        enables |= state->channels[i].enabled ? (u8)(1 << i) : 0;

    ImGui::TextColored(violet, "SOUND       "); ImGui::SameLine();
    ImGui::TextColored(state->enabled ? green : gray, "%s", state->enabled ? "ON " : "OFF"); ImGui::SameLine();
    ImGui::TextColored(violet, "  CHANNEL BANK"); ImGui::SameLine();
    ImGui::TextColored(white, "%d", state->channel_bank + 1); ImGui::SameLine();
    ImGui::TextColored(violet, "  WAVE BANK"); ImGui::SameLine();
    ImGui::TextColored(white, "%X", state->wave_bank); ImGui::SameLine();
    ImGui::TextColored(gray, "($%04X)", state->wave_bank << 12);

    ImGui::TextColored(violet, "CH ENABLES  "); ImGui::SameLine();
    ImGui::TextColored(white, BYTE_TO_BINARY_PATTERN_SPACED, BYTE_TO_BINARY(enables)); ImGui::SameLine();
    ImGui::TextColored(violet, "  IRQ MASK"); ImGui::SameLine();
    ImGui::TextColored(white, "$%02X", state->irq_mask); ImGui::SameLine();
    ImGui::TextColored(violet, " FLAGS"); ImGui::SameLine();
    ImGui::TextColored(state->irq_flags ? yellow : white, "$%02X", state->irq_flags);

    ImGui::NewLine(); ImGui::TextColored(cyan, "CHANNELS"); ImGui::Separator();

    const s16* buffers[RF5C68_CHANNEL_COUNT];

    for (int i = 0; i < RF5C68_CHANNEL_COUNT; i++)
        buffers[i] = audio->GetPCMChannelBuffer(i);

    draw_rf5c68_channels(rf5c68, buffers, audio->GetFrameSamples() / 2);

    ImGui::NewLine(); ImGui::TextColored(cyan, "WAVE RAM"); ImGui::Separator();
    draw_wave_ram(rf5c68);

    ImGui::PopFont();

    ImGui::End();
    ImGui::PopStyleVar();
}

void gui_debug_window_sound_control(void)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::SetNextWindowPos(ImVec2(180, 100), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 420), ImGuiCond_FirstUseEver);
    ImGui::Begin("Sound Control", &config_debug.show_sound_control);

    ImGui::PushFont(gui_default_font);

    Audio* audio = emu_get_core()->GetAudio();
    YM3438::YM3438_State* fm = audio->GetYM3438()->GetState();
    RF5C68::RF5C68_State* pcm = audio->GetRF5C68()->GetState();

    ImGui::TextColored(cyan, "ELECTRONIC VOLUME"); ImGui::Separator();
    draw_volume_chip(audio, 0);
    draw_volume_chip(audio, 1);

    ImGui::NewLine(); ImGui::TextColored(cyan, "INTERRUPTS"); ImGui::Separator();

    bool fm_cause = fm->timer_a_flag || fm->timer_b_flag;
    ImGui::TextColored(violet, "CAUSE 04E9 "); ImGui::SameLine();
    ImGui::TextColored(fm_cause ? yellow : gray, "FM"); ImGui::SameLine();
    ImGui::TextColored(pcm->irq_flags ? yellow : gray, "PCM");
    ImGui::TextColored(violet, "FM TIMERS  "); ImGui::SameLine();
    ImGui::TextColored(fm->timer_a_flag ? yellow : gray, "A"); ImGui::SameLine();
    ImGui::TextColored(fm->timer_b_flag ? yellow : gray, "B");
    ImGui::TextColored(violet, "PCM MASK   "); ImGui::SameLine();
    ImGui::TextColored(white, BYTE_TO_BINARY_PATTERN_SPACED, BYTE_TO_BINARY(pcm->irq_mask)); ImGui::SameLine();
    ImGui::TextColored(gray, "04EA");
    ImGui::TextColored(violet, "PCM FLAGS  "); ImGui::SameLine();
    ImGui::TextColored(pcm->irq_flags ? yellow : white, BYTE_TO_BINARY_PATTERN_SPACED, BYTE_TO_BINARY(pcm->irq_flags));
    ImGui::SameLine();
    ImGui::TextColored(gray, "04EB");

    ImGui::NewLine(); ImGui::TextColored(cyan, "DEBUGGER MUTES"); ImGui::Separator();

    static const char* sources[Audio::AUDIO_SOURCE_COUNT] = { "FM", "PCM", "CD-DA" };

    for (int i = 0; i < Audio::AUDIO_SOURCE_COUNT; i++)
    {
        bool muted = audio->IsSourceMuted(i);

        if (ImGui::Checkbox(sources[i], &muted))
            audio->SetSourceMute(i, muted);

        if (i < Audio::AUDIO_SOURCE_COUNT - 1)
            ImGui::SameLine(0, 20);
    }

    ImGui::PopFont();

    ImGui::End();
    ImGui::PopStyleVar();
}

void gui_debug_window_cdrom_audio(void)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::SetNextWindowPos(ImVec2(200, 110), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(262, 400), ImGuiCond_FirstUseEver);
    ImGui::Begin("CD Audio", &config_debug.show_cdrom_audio);

    GeartownsCore* core = emu_get_core();
    Audio* audio = core->GetAudio();
    CdRomAudio* cdrom_audio = core->GetCDROMAudio();
    CdRomAudio::CdRomAudio_State* state = cdrom_audio->GetState();
    CdRomMedia* media = core->GetCDROMMedia();
    bool muted = audio->IsSourceMuted(Audio::AUDIO_SOURCE_CDDA);
    bool active = state->play_state != CdRomAudio::CDROM_AUDIO_IDLE;

    if (draw_mute_button("##cdda_mute", muted, "Mute CD-DA"))
        audio->SetSourceMute(Audio::AUDIO_SOURCE_CDDA, !muted);

    ImGui::SameLine();
    draw_stereo_scope("##cdda_mix", Audio::AUDIO_SOURCE_CDDA, 1.0f / 32768.0f, ImVec2(95, 50));

    ImGui::PushFont(gui_default_font);

    static const char* states[3] = { "IDLE   ", "PLAYING", "PAUSED " };
    ImGui::TextColored(cyan, "PLAYBACK"); ImGui::Separator();
    ImGui::TextColored(violet, "STATE    "); ImGui::SameLine();
    ImGui::TextColored(active ? blue : gray, "%s", states[state->play_state % 3]);
    ImGui::TextColored(violet, "END      "); ImGui::SameLine();
    ImGui::TextColored(active ? white : gray, "%s", state->repeat ? "REPEAT" : "STOP  ");

    s32 track = media->IsReady() && active ? media->FindTrackFromLBA(state->current_lba, true) : -1;
    ImGui::TextColored(violet, "TRACK    "); ImGui::SameLine();

    if (track >= 0)
        ImGui::TextColored(orange, "%02d", track + 1);
    else
        ImGui::TextColored(gray, "--");

    ImGui::NewLine(); ImGui::TextColored(cyan, "POSITION"); ImGui::Separator();

    static const char* rows[3] = { "START    ", "STOP     ", "CURRENT  " };
    u32 values[3] = { state->start_lba, state->end_lba, state->current_lba };

    for (int i = 0; i < 3; i++)
    {
        ImGui::TextColored(violet, "%s", rows[i]); ImGui::SameLine();

        if (active)
            draw_msf(values[i]);
        else
            ImGui::TextColored(gray, "--");
    }

    ImGui::TextColored(violet, "TRACK POS"); ImGui::SameLine();

    if (track >= 0)
    {
        GT_CdRomMSF msf;
        LbaToMsf(state->current_lba - MIN(state->current_lba, media->GetTracks()[track].start_lba), &msf);
        ImGui::TextColored(white, "%02u:%02u:%02u", msf.minutes, msf.seconds, msf.frames);
    }
    else
        ImGui::TextColored(gray, "--");

    ImGui::NewLine(); ImGui::TextColored(cyan, "VOLUME"); ImGui::Separator();

    for (int channel = 0; channel < 2; channel++)
    {
        s32 gain = audio->GetVolumeGain(1, channel);
        ImGui::TextColored(violet, "%s", channel == 0 ? "LEFT     " : "RIGHT    "); ImGui::SameLine();

        if (gain > 0)
            ImGui::TextColored(white, "%6.1f dB", 20.0 * log10((double)gain / 32768.0));
        else
            ImGui::TextColored(gray, "MUTE");
    }

    ImGui::PopFont();

    ImGui::End();
    ImGui::PopStyleVar();
}

static void draw_scope(const char* id, const s16* data, int stride, int count, float scale, ImVec2 size)
{
    count = MIN(count, GT_AUDIO_BUFFER_SIZE / 2);

    for (int i = 0; i < count; i++)
        scope_buffer[i] = CLAMP((float)data[i * stride] * scale, -1.0f, 1.0f);

    int trigger = 0;

    for (int i = k_scope_half_window; i < count; i++)
    {
        if (scope_buffer[i - 1] < 0.0f && scope_buffer[i] >= 0.0f)
        {
            trigger = i;
            break;
        }
    }

    int x_min = MAX(0, trigger - k_scope_half_window);
    int x_max = MAX(x_min + 1, MIN(count, trigger + k_scope_half_window));
    ImPlotAxisFlags flags = ImPlotAxisFlags_NoGridLines | ImPlotAxisFlags_NoTickLabels | ImPlotAxisFlags_NoLabel |
        ImPlotAxisFlags_NoHighlight | ImPlotAxisFlags_Lock | ImPlotAxisFlags_NoTickMarks;
    ImPlotSpec spec;
    spec.LineColor = white;
    spec.LineWeight = 1.0f;

    ImPlot::PushStyleVar(ImPlotStyleVar_PlotPadding, ImVec2(1, 1));

    if (ImPlot::BeginPlot(id, size, ImPlotFlags_CanvasOnly))
    {
        ImPlot::SetupAxes("x", "y", flags, flags);
        ImPlot::SetupAxesLimits(x_min, x_max, -1.0f, 1.0f, ImPlotCond_Always);

        if (count > 0)
            ImPlot::PlotLine("Wave", scope_buffer, count, 1.0, 0.0, spec);

        ImPlot::EndPlot();
    }

    ImPlot::PopStyleVar();
}

static void draw_stereo_scope(const char* id, int source, float scale, ImVec2 size)
{
    Audio* audio = emu_get_core()->GetAudio();
    const s16* buffer = audio->GetSourceBuffer(source);
    int count = audio->GetFrameSamples() / 2;
    char left_id[32];
    char right_id[32];
    snprintf(left_id, sizeof(left_id), "%s_left", id);
    snprintf(right_id, sizeof(right_id), "%s_right", id);

    draw_scope(left_id, buffer, 2, count, scale, size);
    ImGui::SameLine();
    draw_scope(right_id, buffer + 1, 2, count, scale, size);
}

static bool draw_mute_button(const char* id, bool muted, const char* tooltip)
{
    char label[32];
    snprintf(label, sizeof(label), "%s%s", muted ? ICON_MD_MUSIC_OFF : ICON_MD_MUSIC_NOTE, id);

    ImGui::PushFont(gui_material_icons_font);
    ImGui::PushStyleColor(ImGuiCol_Text, muted ? mid_gray : white);
    bool pressed = ImGui::Button(label);
    ImGui::PopStyleColor();
    ImGui::PopFont();

    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("%s", tooltip);

    return pressed;
}

static bool draw_solo_button(const char* id, bool solo)
{
    char label[32];
    snprintf(label, sizeof(label), "%s%s", ICON_MD_STAR, id);

    ImGui::PushFont(gui_material_icons_font);
    ImGui::PushStyleColor(ImGuiCol_Text, solo ? yellow : white);
    bool pressed = ImGui::Button(label);
    ImGui::PopStyleColor();
    ImGui::PopFont();

    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("Solo Channel");

    return pressed;
}

static void set_fm_solo(int channel)
{
    YM3438* ym3438 = emu_get_core()->GetAudio()->GetYM3438();
    fm_solo = fm_solo == channel ? -1 : channel;

    for (int i = 0; i < YM3438_CHANNEL_COUNT; i++)
        ym3438->SetChannelMute(i, fm_solo >= 0 && i != fm_solo);
}

static void set_pcm_solo(int channel)
{
    RF5C68* rf5c68 = emu_get_core()->GetAudio()->GetRF5C68();
    pcm_solo = pcm_solo == channel ? -1 : channel;

    for (int i = 0; i < RF5C68_CHANNEL_COUNT; i++)
        rf5c68->SetChannelMute(i, pcm_solo >= 0 && i != pcm_solo);
}

static void draw_fm_global(YM3438::YM3438_State* state)
{
    double lfo_rate = k_ym3438_sample_rate / (128.0 * (k_debug_ym3438_lfo_cycles[state->lfo_frequency & 7] + 1));
    double timer_a = (1024 - state->timer_a_register) * 1000.0 / k_ym3438_sample_rate;
    double timer_b = (256 - state->timer_b_register) * 16 * 1000.0 / k_ym3438_sample_rate;

    ImGui::TextColored(cyan, "GLOBAL"); ImGui::Separator();

    ImGui::Columns(2, "##fm_global", false);

    ImGui::TextColored(violet, "LFO     "); ImGui::SameLine();
    ImGui::TextColored(state->lfo_enabled ? green : gray, "%s", state->lfo_enabled ? "ON " : "OFF"); ImGui::SameLine();
    ImGui::TextColored(white, "%5.2f Hz", lfo_rate);
    ImGui::TextColored(violet, "CH3 MODE"); ImGui::SameLine();
    ImGui::TextColored(blue, "%s", k_debug_ym3438_ch3_mode_names[state->channel_3_mode & 3]);
    ImGui::TextColored(violet, "DAC     "); ImGui::SameLine();
    ImGui::TextColored(state->dac_enabled ? green : gray, "%s", state->dac_enabled ? "ON " : "OFF"); ImGui::SameLine();
    ImGui::TextColored(white, "$%02X", (u8)((state->dac_data / 2) + 128));
    ImGui::TextColored(violet, "STATUS  "); ImGui::SameLine();
    ImGui::TextColored(white, "$%02X", state->status); ImGui::SameLine();
    ImGui::TextColored(state->busy_cycles ? yellow : gray, "BUSY");

    ImGui::NextColumn();

    ImGui::TextColored(violet, "TIMER A "); ImGui::SameLine();
    ImGui::TextColored(white, "%4d %7.3f ms", state->timer_a_register, timer_a);
    ImGui::TextColored(violet, "        "); ImGui::SameLine();
    ImGui::TextColored(state->timer_a_load ? green : gray, "LOAD"); ImGui::SameLine();
    ImGui::TextColored(state->timer_a_enable ? green : gray, "ENABLE"); ImGui::SameLine();
    ImGui::TextColored(state->timer_a_flag ? yellow : gray, "FLAG");
    ImGui::TextColored(violet, "TIMER B "); ImGui::SameLine();
    ImGui::TextColored(white, "%4d %7.3f ms", state->timer_b_register, timer_b);
    ImGui::TextColored(violet, "        "); ImGui::SameLine();
    ImGui::TextColored(state->timer_b_load ? green : gray, "LOAD"); ImGui::SameLine();
    ImGui::TextColored(state->timer_b_enable ? green : gray, "ENABLE"); ImGui::SameLine();
    ImGui::TextColored(state->timer_b_flag ? yellow : gray, "FLAG");

    ImGui::Columns(1);
}

static void draw_fm_channel(YM3438* ym3438, int channel)
{
    Audio* audio = emu_get_core()->GetAudio();
    YM3438::YM3438_State* state = ym3438->GetState();
    YM3438::YM3438_Channel& ch = state->channels[channel];
    bool muted = ym3438->IsChannelMuted(channel);
    char id[32];

    snprintf(id, sizeof(id), "##fm_mute%d", channel);

    if (draw_mute_button(id, muted, "Mute Channel"))
    {
        fm_solo = -1;
        ym3438->SetChannelMute(channel, !muted);
    }

    ImGui::SameLine();
    snprintf(id, sizeof(id), "##fm_solo%d", channel);

    if (draw_solo_button(id, fm_solo == channel))
        set_fm_solo(channel);

    ImGui::SameLine();
    snprintf(id, sizeof(id), "##fm_scope%d", channel);
    draw_scope(id, audio->GetFMChannelBuffer(channel), 1, audio->GetFrameSamples() / 2, 1.0f / 256.0f,
        ImVec2(160, 50));

    ImGui::SameLine(0, 20);
    draw_algorithm(ch.algorithm);

    ImGui::PushFont(gui_default_font);

    ImGui::Columns(2, "##fm_channel", false);

    ImGui::TextColored(violet, "KEY ON    "); ImGui::SameLine();

    for (int i = 0; i < YM3438_OPERATOR_COUNT; i++)
    {
        ImGui::TextColored(ch.operators[i].key_on ? green : gray, "OP%d", i + 1);

        if (i < YM3438_OPERATOR_COUNT - 1)
            ImGui::SameLine();
    }

    ImGui::TextColored(violet, "F-NUMBER  "); ImGui::SameLine();
    ImGui::TextColored(white, "$%03X", ch.f_number); ImGui::SameLine();
    ImGui::TextColored(violet, " BLOCK"); ImGui::SameLine();
    ImGui::TextColored(white, "%d", ch.block);
    ImGui::TextColored(violet, "NOTE      "); ImGui::SameLine();
    draw_frequency(ch.f_number, ch.block, true);
    ImGui::TextColored(violet, "ALGORITHM "); ImGui::SameLine();
    ImGui::TextColored(white, "%d", ch.algorithm); ImGui::SameLine();
    ImGui::TextColored(violet, " FEEDBACK"); ImGui::SameLine();
    ImGui::TextColored(white, "%d", ch.feedback);

    ImGui::NextColumn();

    ImGui::TextColored(violet, "PAN       "); ImGui::SameLine();
    ImGui::TextColored(ch.pan_left ? green : gray, "L"); ImGui::SameLine();
    ImGui::TextColored(ch.pan_right ? green : gray, "R");
    ImGui::TextColored(violet, "AMS       "); ImGui::SameLine();
    ImGui::TextColored(white, "%d", ch.amplitude_modulation); ImGui::SameLine();
    ImGui::TextColored(violet, " PMS"); ImGui::SameLine();
    ImGui::TextColored(white, "%d", ch.phase_modulation);

    bool special = channel == 2 && state->channel_3_mode != 0;

    for (int i = 0; i < 3; i++)
    {
        ImGui::TextColored(special ? violet : gray, "CH3 OP%d   ", i + 1); ImGui::SameLine();

        if (channel == 2)
        {
            ImGui::TextColored(special ? white : gray, "$%03X %d ", ch.special_f_number[i], ch.special_block[i]);
            ImGui::SameLine();
            draw_frequency(ch.special_f_number[i], ch.special_block[i], special);
        }
        else
            ImGui::TextColored(gray, "--");
    }

    ImGui::Columns(1);
    ImGui::NewLine();

    ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX;

    if (ImGui::BeginTable("##fm_operators", 14, flags))
    {
        static const char* headers[14] =
        {
            "  ", "DT", "MUL", "TL", "KS", "AR", "D1R", "D2R", "D1L", "RR", "AM", "SSG", "KEY", "LEVEL"
        };

        for (int i = 0; i < 14; i++)
            ImGui::TableSetupColumn(headers[i]);

        ImGui::TableHeadersRow();

        for (int i = 0; i < YM3438_OPERATOR_COUNT; i++)
        {
            YM3438::YM3438_Operator& op = ch.operators[i];
            u32 attenuation = MIN((u32)ym3438->GetEnvelopeOutput(op) + ((u32)op.total_level << 3),
                (u32)k_ym3438_envelope_max);
            float level = 1.0f - (float)attenuation / (float)k_ym3438_envelope_max;

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::TextColored(cyan, "OP%d", i + 1);
            ImGui::TableNextColumn(); ImGui::TextColored(white, "%d", op.detune);
            ImGui::TableNextColumn(); ImGui::TextColored(white, "%2d", op.multiple);
            ImGui::TableNextColumn(); ImGui::TextColored(white, "%3d", op.total_level);
            ImGui::TableNextColumn(); ImGui::TextColored(white, "%d", op.key_scale);
            ImGui::TableNextColumn(); ImGui::TextColored(white, "%2d", op.attack_rate);
            ImGui::TableNextColumn(); ImGui::TextColored(white, "%2d", op.decay_rate);
            ImGui::TableNextColumn(); ImGui::TextColored(white, "%2d", op.sustain_rate);
            ImGui::TableNextColumn(); ImGui::TextColored(white, "%2d", op.sustain_level);
            ImGui::TableNextColumn(); ImGui::TextColored(white, "%2d", op.release_rate);
            ImGui::TableNextColumn(); ImGui::TextColored(op.amplitude_modulation_enabled ? green : gray, "%s",
                op.amplitude_modulation_enabled ? "ON " : "OFF");
            ImGui::TableNextColumn(); ImGui::TextColored((op.ssg_envelope & 0x08) ? white : gray, "$%X", op.ssg_envelope);
            ImGui::TableNextColumn(); ImGui::TextColored(op.key_on ? green : gray, "%s", op.key_on ? "ON " : "OFF");
            ImGui::TableNextColumn();
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, op.key_on ? green : gray);
            ImGui::ProgressBar(level, ImVec2(60, 0), "");
            ImGui::PopStyleColor();

            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s  ENVELOPE $%03X", k_debug_ym3438_envelope_names[op.state & 3], op.envelope);
        }

        ImGui::EndTable();
    }

    ImGui::PopFont();
}

static void draw_algorithm(int algorithm)
{
    const AlgorithmLayout& layout = k_algorithm_layouts[algorithm & 7];
    const float cell = 18.0f;
    const float box = 13.0f;
    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 centers[4];

    for (int i = 0; i < 4; i++)
        centers[i] = ImVec2(origin.x + layout.x[i] * cell + box * 0.5f, origin.y + layout.y[i] * cell + box * 0.5f);

    for (int i = 0; i < layout.link_count; i++)
        draw_list->AddLine(centers[layout.links[i][0]], centers[layout.links[i][1]], ImColor(mid_gray), 1.0f);

    for (int i = 0; i < 4; i++)
    {
        bool carrier = (layout.carriers >> i) & 1;
        ImVec2 min(centers[i].x - box * 0.5f, centers[i].y - box * 0.5f);
        ImVec2 max(centers[i].x + box * 0.5f, centers[i].y + box * 0.5f);
        char text[2] = { (char)('1' + i), 0 };

        draw_list->AddRectFilled(min, max, ImColor(ImGui::GetStyleColorVec4(ImGuiCol_WindowBg)));
        draw_list->AddRect(min, max, ImColor(carrier ? green : orange));
        draw_list->AddText(ImVec2(min.x + 3.0f, min.y - 1.0f), ImColor(white), text);
    }

    ImGui::Dummy(ImVec2(4 * cell, 3 * cell));

    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Algorithm %d\nCarriers in green, modulators in orange", algorithm);
}

static void draw_frequency(u16 f_number, u8 block, bool active)
{
    double hz = (double)f_number * (double)(1 << block) * k_ym3438_sample_rate / (double)(1 << 21);

    if (f_number == 0 || hz < 8.0)
    {
        ImGui::TextColored(gray, "--          ");
        return;
    }

    int midi = (int)floor(69.0 + 12.0 * log2(hz / 440.0) + 0.5);
    int note = ((midi % 12) + 12) % 12;
    char name[8];
    snprintf(name, sizeof(name), "%s%d", k_debug_note_names[note], midi / 12 - 1);

    ImGui::TextColored(active ? orange : gray, "%-4s", name); ImGui::SameLine();
    ImGui::TextColored(active ? white : gray, "%8.2f Hz", hz);
}

static void draw_rf5c68_channels(RF5C68* rf5c68, const s16* const* buffers, int count)
{
    RF5C68::RF5C68_State* state = rf5c68->GetState();
    ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX;

    if (!ImGui::BeginTable("##pcm_channels", 10, flags))
        return;

    static const char* headers[10] = { "CH", " ", "SCOPE", "ON", "ENV", "PAN", "FD", "LS", "ST", "ADDRESS" };

    for (int i = 0; i < 10; i++)
        ImGui::TableSetupColumn(headers[i]);

    ImGui::TableHeadersRow();

    for (int i = 0; i < RF5C68_CHANNEL_COUNT; i++)
    {
        RF5C68::RF5C68_Channel& ch = state->channels[i];
        bool muted = rf5c68->IsChannelMuted(i);
        bool sounding = state->enabled && ch.enabled;
        u32 address = ch.address >> k_rf5c68_address_fraction_bits;
        char id[32];

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(orange, "%d", i + 1);

        ImGui::TableNextColumn();
        snprintf(id, sizeof(id), "##pcm_mute%d", i);

        if (draw_mute_button(id, muted, "Mute Channel"))
        {
            pcm_solo = -1;
            rf5c68->SetChannelMute(i, !muted);
        }

        ImGui::SameLine(0, 2);
        snprintf(id, sizeof(id), "##pcm_solo%d", i);

        if (draw_solo_button(id, pcm_solo == i))
            set_pcm_solo(i);

        ImGui::TableNextColumn();
        snprintf(id, sizeof(id), "##pcm_scope%d", i);
        draw_scope(id, buffers[i], 1, count, 1.0f / 16384.0f, ImVec2(60, 22));

        ImGui::TableNextColumn();
        ImGui::TextColored(sounding ? green : gray, "%s", ch.enabled ? "ON " : "OFF");
        ImGui::TableNextColumn();
        ImGui::TextColored(white, "$%02X", ch.envelope);
        ImGui::TableNextColumn();
        ImGui::TextColored(white, "%X %X", ch.pan & 0x0F, ch.pan >> 4);
        ImGui::TableNextColumn();
        ImGui::TextColored(white, "$%04X", ch.step);

        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%.1f Hz playback rate", k_rf5c68_sample_rate * ch.step / 2048.0);

        ImGui::TableNextColumn();
        ImGui::TextColored(white, "$%04X", ch.loop_start);
        ImGui::TableNextColumn();
        ImGui::TextColored(white, "$%02X", ch.start);
        ImGui::TableNextColumn();
        ImGui::TextColored(sounding ? cyan : gray, "$%04X", address);

        if (ImGui::IsItemClicked())
            goto_pcm_ram(address);
    }

    ImGui::EndTable();
}

static void draw_wave_ram(RF5C68* rf5c68)
{
    static float wave[4096];
    RF5C68::RF5C68_State* state = rf5c68->GetState();
    int bank = CLAMP(config_debug.rf5c68_wave_bank, 0, 15);
    u32 base = (u32)bank << 12;

    ImGui::PushItemWidth(60.0f);
    ImGui::Combo("BANK##pcm_wave_bank", &config_debug.rf5c68_wave_bank,
        "0\0" "1\0" "2\0" "3\0" "4\0" "5\0" "6\0" "7\0" "8\0" "9\0" "A\0" "B\0" "C\0" "D\0" "E\0" "F\0\0");
    ImGui::PopItemWidth();
    ImGui::SameLine();
    ImGui::TextColored(gray, "$%04X-$%04X", base, base + 0x0FFF);

    for (int i = 0; i < 4096; i++)
    {
        u8 sample = state->wave_ram[base + i];
        wave[i] = sample == 0xFF ? 0.0f : ((sample & 0x80) ? 1.0f : -1.0f) * (float)(sample & 0x7F) / 127.0f;
    }

    ImPlotAxisFlags flags = ImPlotAxisFlags_NoGridLines | ImPlotAxisFlags_NoTickLabels | ImPlotAxisFlags_NoLabel |
        ImPlotAxisFlags_NoHighlight | ImPlotAxisFlags_Lock | ImPlotAxisFlags_NoTickMarks;
    ImPlotSpec spec;
    spec.LineColor = orange;
    spec.LineWeight = 1.0f;

    ImPlot::PushStyleVar(ImPlotStyleVar_PlotPadding, ImVec2(1, 1));

    if (ImPlot::BeginPlot("##pcm_wave_ram", ImVec2(-1, 110), ImPlotFlags_CanvasOnly))
    {
        ImPlot::SetupAxes("x", "y", flags, flags);
        ImPlot::SetupAxesLimits(0, 4096, -1.0f, 1.0f, ImPlotCond_Always);
        ImPlot::PlotLine("Wave", wave, 4096, 1.0, 0.0, spec);

        ImPlotSpec position_spec;
        position_spec.LineColor = green;
        ImPlotSpec loop_spec;
        loop_spec.LineColor = yellow;

        for (int i = 0; i < RF5C68_CHANNEL_COUNT; i++)
        {
            RF5C68::RF5C68_Channel& ch = state->channels[i];
            u32 position = ch.address >> k_rf5c68_address_fraction_bits;
            char label[16];

            if (state->enabled && ch.enabled && (position & 0xF000) == base)
            {
                double x = (double)(position & 0x0FFF);
                snprintf(label, sizeof(label), "##position%d", i);
                ImPlot::TagX(x, green, "%d", i + 1);
                ImPlot::PlotInfLines(label, &x, 1, position_spec);
            }

            if ((u32)(ch.loop_start & 0xF000) == base)
            {
                double x = (double)(ch.loop_start & 0x0FFF);
                snprintf(label, sizeof(label), "##loop%d", i);
                ImPlot::PlotInfLines(label, &x, 1, loop_spec);
            }
        }

        if (ImPlot::IsPlotHovered())
        {
            ImPlotPoint mouse = ImPlot::GetPlotMousePos();
            u32 offset = base + (u32)CLAMP((int)mouse.x, 0, 4095);
            ImGui::SetTooltip("$%04X: $%02X\nClick to open PCM RAM", offset, state->wave_ram[offset]);

            if (ImGui::IsMouseClicked(0))
                goto_pcm_ram(offset);
        }

        ImPlot::EndPlot();
    }

    ImPlot::PopStyleVar();
    ImGui::TextColored(green, "PLAY POSITION"); ImGui::SameLine();
    ImGui::TextColored(yellow, " LOOP START");
}

static void draw_volume_chip(Audio* audio, int chip)
{
    Audio::Audio_State* state = audio->GetState();
    char id[32];
    snprintf(id, sizeof(id), "##volume%d", chip);

    ImGui::TextColored(violet, "VOLUME %d   ", chip + 1); ImGui::SameLine();
    ImGui::TextColored(gray, "%04X", 0x04E0 + chip * 2); ImGui::SameLine();
    ImGui::TextColored(violet, " COM"); ImGui::SameLine();
    ImGui::TextColored(white, "%d", state->volume_channel[chip]);

    ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX;

    if (!ImGui::BeginTable(id, 5, flags))
        return;

    ImGui::TableSetupColumn("CH");
    ImGui::TableSetupColumn("DATA");
    ImGui::TableSetupColumn("EN");
    ImGui::TableSetupColumn("FIXED");
    ImGui::TableSetupColumn("GAIN");
    ImGui::TableHeadersRow();

    for (int channel = 0; channel < AUDIO_VOLUME_CHANNELS; channel++)
    {
        u8 control = state->volume_control[chip][channel];
        s32 gain = audio->GetVolumeGain(chip, channel);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(orange, "%d", channel);

        if (chip == 1 && channel < 2)
        {
            ImGui::SameLine();
            ImGui::TextColored(gray, "%s", channel == 0 ? "CD L" : "CD R");
        }

        ImGui::TableNextColumn();
        ImGui::TextColored(white, "$%02X", state->volume_data[chip][channel]);
        ImGui::TableNextColumn();
        ImGui::TextColored((control & 0x04) ? green : gray, "%s", (control & 0x04) ? "ON " : "OFF");
        ImGui::TableNextColumn();

        if (control & 0x10)
            ImGui::TextColored(white, "-32 dB");
        else if (control & 0x08)
            ImGui::TextColored(white, "0 dB  ");
        else
            ImGui::TextColored(gray, "--    ");

        ImGui::TableNextColumn();

        if (gain > 0)
            ImGui::TextColored(white, "%6.1f dB", 20.0 * log10((double)gain / 32768.0));
        else
            ImGui::TextColored(gray, "MUTE     ");
    }

    ImGui::EndTable();
}

static void draw_msf(u32 lba)
{
    GT_CdRomMSF msf;
    LbaToMsf(lba + 150, &msf);
    ImGui::TextColored(white, "%6u", lba); ImGui::SameLine();
    ImGui::TextColored(orange, "%02u:%02u:%02u", msf.minutes, msf.seconds, msf.frames);
}

static void goto_pcm_ram(u32 offset)
{
    GT_Debug_Memory_Address target = { };
    target.space = GT_DEBUG_MEMORY_REGION;
    target.region = GT_DEBUG_REGION_PCM_RAM;
    target.address = offset;
    target.segment_register = -1;
    gui_debug_memory_goto(target);
}
