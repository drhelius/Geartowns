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

#include <math.h>
#include "audio.h"
#include "ym3438.h"
#include "rf5c68.h"
#include "../cdrom/cdrom_audio.h"
#include "../system/pic.h"
#include "../system/scheduler.h"
#include "../common/trace_logger.h"
#include "../common/state_serializer.h"

// Data Book 5.1 puts an approximately 4 kHz reconstruction filter after the PCM DACs
static const float k_audio_pcm_lowpass_cutoff = 4000.0f;

// MB87078 attenuation in Q15: round(32768 * 10^((data - 63) * 0.5 / 20))
static const s32 k_audio_volume_gain[64] =
{
/* 0x00 */   872,   924,   978,  1036,  1098,  1163,  1232,  1305,
/* 0x08 */  1382,  1464,  1550,  1642,  1740,  1843,  1952,  2068,
/* 0x10 */  2190,  2320,  2457,  2603,  2757,  2920,  3093,  3277,
/* 0x18 */  3471,  3677,  3894,  4125,  4370,  4629,  4903,  5193,
/* 0x20 */  5501,  5827,  6172,  6538,  6925,  7336,  7771,  8231,
/* 0x28 */  8719,  9235,  9783, 10362, 10976, 11627, 12315, 13045,
/* 0x30 */ 13818, 14637, 15504, 16423, 17396, 18427, 19519, 20675,
/* 0x38 */ 21900, 23198, 24573, 26029, 27571, 29205, 30935, 32768
};

static const s32 k_audio_volume_gain_minus_32db = 823;
static const u8 k_audio_volume_enable = 0x04;
static const u8 k_audio_volume_0db = 0x08;
static const u8 k_audio_volume_minus_32db = 0x10;
static const int k_audio_volume_cdda = 1;
static const int k_audio_volume_cdda_left = 0;
static const int k_audio_volume_cdda_right = 1;
static const u8 k_audio_gate_pcm = 0x01;
static const u8 k_audio_gate_fm = 0x02;
static const u8 k_audio_gate_output = 0x40;
static const u8 k_audio_vgm_fm_mode_registers[7] = { 0x22, 0x24, 0x25, 0x26, 0x27, 0x2A, 0x2B };

Audio::Audio()
{
    InitPointer(m_ym3438);
    InitPointer(m_rf5c68);
    InitPointer(m_cdrom_audio);
    InitPointer(m_scheduler);
    InitPointer(m_pic);
    InitPointer(m_pit);
    InitPointer(m_trace_logger);
    m_mute = false;
    m_master_volume = 1.0f;
    m_fm_volume = 1.0f;
    m_pcm_volume = 1.0f;
    m_cdda_volume = 1.0f;
    m_buzzer_volume = 1.0f;
    m_cdda_gain_left = 0;
    m_cdda_gain_right = 0;
    m_fm_enabled = false;
    m_pcm_enabled = false;
    m_cdda_enabled = false;
    m_pcm_irq_clocks = GT_NO_EVENT;
    m_buffer_index = 0;
    m_buffer_overflow = false;
    m_frame_samples = 0;
    m_channel_scopes = false;
    m_vgm_recording_enabled = false;

    for (int i = 0; i < AUDIO_SOURCE_COUNT; i++)
        m_source_mute[i] = false;

    memset(m_fm_buffer, 0, sizeof(m_fm_buffer));
    memset(m_pcm_buffer, 0, sizeof(m_pcm_buffer));
    memset(m_cdda_buffer, 0, sizeof(m_cdda_buffer));
    memset(m_buzzer_buffer, 0, sizeof(m_buzzer_buffer));
    memset(m_fm_channel_buffer, 0, sizeof(m_fm_channel_buffer));
    memset(m_pcm_channel_buffer, 0, sizeof(m_pcm_channel_buffer));
    SetPCMLowpassCutoff(k_audio_pcm_lowpass_cutoff);
}

Audio::~Audio()
{
    SafeDelete(m_rf5c68);
    SafeDelete(m_ym3438);
}

void Audio::Init(Scheduler* scheduler, CdRomAudio* cdrom_audio, PIC* pic, PIT* pit)
{
    m_scheduler = scheduler;
    m_cdrom_audio = cdrom_audio;
    m_pic = pic;
    m_pit = pit;

    if (!IsValidPointer(m_ym3438))
        m_ym3438 = new YM3438();

    if (!IsValidPointer(m_rf5c68))
        m_rf5c68 = new RF5C68();

    m_ym3438->Init();
    m_rf5c68->Init();
    Reset();
}

void Audio::SetTraceLogger(TraceLogger* trace_logger)
{
    m_trace_logger = trace_logger;
}

void Audio::Reset()
{
    m_ym3438->Reset();
    m_rf5c68->Reset();

    m_state.sound_clock_remainder = 0;
    m_state.sample_clock_counter = 0;
    m_state.pcm_lowpass_left = 0;
    m_state.pcm_lowpass_right = 0;
    m_state.output_dc_left = 0;
    m_state.output_dc_right = 0;
    m_state.clocks = m_scheduler->GetClocks();

    // Both MB87078 start with every channel enabled at 0 dB
    for (int chip = 0; chip < AUDIO_VOLUME_CHIPS; chip++)
    {
        m_state.volume_channel[chip] = 0;

        for (int channel = 0; channel < AUDIO_VOLUME_CHANNELS; channel++)
        {
            m_state.volume_data[chip][channel] = 0x3F;
            m_state.volume_control[chip][channel] = k_audio_volume_enable;
        }
    }

    m_state.mute_control = 0;
    m_state.output_control = 0;

    UpdateCDDAGain();
    UpdateGates();
    UpdatePCMIRQ();
    m_buffer_index = 0;
    m_buffer_overflow = false;
}

void Audio::SetPCMLowpassCutoff(float cutoff)
{
    // One pole: alpha = 1 - exp(-2 * pi * fc / fs), stored as Q1.15
    float alpha = 1.0f - expf(-2.0f * 3.14159265358979323846f * cutoff / (float)GT_AUDIO_SAMPLE_RATE);
    alpha = CLAMP(alpha, 0.0f, 0.9999f);
    m_pcm_lowpass_alpha_q15 = (u16)(alpha * 32768.0f + 0.5f);
}

// Volume 1 at 04E0h attenuates line in
// Volume 2 at 04E2h attenuates the CD-DA left and right channels, the mic and the modem
// COM selects the channel that DATA writes
// EN, C0 and C32 are in bits 2-4 as on the chip pins
u8 Audio::ReadVolume(u16 port) const
{
    int chip = (port >> 1) & 0x01;
    int channel = m_state.volume_channel[chip];

    if ((port & 0x01) == 0)
        return m_state.volume_data[chip][channel];

    return (u8)(channel | m_state.volume_control[chip][channel]);
}

void Audio::WriteVolume(u16 port, u8 value)
{
    int chip = (port >> 1) & 0x01;

    if ((port & 0x01) == 0)
        m_state.volume_data[chip][m_state.volume_channel[chip]] = value & 0x3F;
    else
    {
        m_state.volume_channel[chip] = value & 0x03;
        m_state.volume_control[chip][value & 0x03] = value & 0x1C;
    }

    UpdateCDDAGain();

    if (unlikely(IsValidPointer(m_trace_logger) && m_trace_logger->IsEventEnabled(TRACE_MIXER, TRACE_MIXER_VOLUME)))
        TraceMixer(TRACE_MIXER_VOLUME, port, value);
}

// 04D5h bit 1 lets FM through and bit 0 PCM
// 04ECh bit 6 permits the final output, CD-DA included, and bit 7 turns the level LEDs off
u8 Audio::ReadGate(u16 port) const
{
    return port == 0x04D5 ? m_state.mute_control : m_state.output_control;
}

void Audio::WriteGate(u16 port, u8 value)
{
    if (port == 0x04D5)
        m_state.mute_control = value & (k_audio_gate_fm | k_audio_gate_pcm);
    else
        m_state.output_control = value;

    UpdateGates();

    if (unlikely(IsValidPointer(m_trace_logger) && m_trace_logger->IsEventEnabled(TRACE_MIXER, TRACE_MIXER_MUTE)))
        TraceMixer(TRACE_MIXER_MUTE, port, value);
}

// The gates only silence the outputs
// FM timers and PCM interrupts keep running behind them
void Audio::UpdateGates()
{
    bool output = (m_state.output_control & k_audio_gate_output) != 0;
    m_fm_enabled = output && (m_state.mute_control & k_audio_gate_fm) != 0;
    m_pcm_enabled = output && (m_state.mute_control & k_audio_gate_pcm) != 0;
    m_cdda_enabled = output;
}

// Every write that can move an FM timer or a PCM pointer looks again at IRQ13
void Audio::WriteFM(u8 port, u8 value)
{
    m_ym3438->Write(port, value);

#ifndef GT_DISABLE_VGMRECORDER
    if (m_vgm_recording_enabled && (port & 0x01) != 0)
        m_vgm_recorder.WriteYM3438(m_ym3438->GetSelectedAddress(), value);
#endif

    if (unlikely((port & 0x01) != 0 && IsValidPointer(m_trace_logger) && m_trace_logger->IsEnabled(TRACE_FM)))
        TraceFM(value);

    UpdateIRQ();
}

void Audio::WritePCM(u16 address, u8 value)
{
    u8 channel = m_rf5c68->GetChannelBank();

    m_rf5c68->Write(address, value);

#ifndef GT_DISABLE_VGMRECORDER
    if (m_vgm_recording_enabled)
        m_vgm_recorder.WriteRF5C68((u8)address, value);
#endif

    if (unlikely(IsValidPointer(m_trace_logger) && m_trace_logger->IsEnabled(TRACE_PCM)))
    {
        u8 event = address < 0x07 ? TRACE_PCM_CHANNEL : address == 0x07 ? TRACE_PCM_CONTROL : TRACE_PCM_KEY;
        TracePCM(event, (u8)address, value, channel);
    }

    UpdatePCMIRQ();
}

void Audio::WritePCMIRQMask(u8 value)
{
    m_rf5c68->WriteIRQMask(value);

    if (unlikely(IsValidPointer(m_trace_logger) && m_trace_logger->IsEnabled(TRACE_PCM)))
        TracePCM(TRACE_PCM_IRQ_MASK, 0, value, 0);

    UpdatePCMIRQ();
}

u8 Audio::ReadPCMIRQFlags()
{
    u8 flags = m_rf5c68->ReadIRQFlags();

    if (unlikely(IsValidPointer(m_trace_logger) && m_trace_logger->IsEnabled(TRACE_PCM)))
        TracePCM(TRACE_PCM_IRQ_READ, 0, flags, 0);

    UpdatePCMIRQ();
    return flags;
}

// IRQ13 combines the FM timer flags and the PCM boundary causes
// While it is low, the first clock either chip can raise it is scheduled
void Audio::UpdateIRQ()
{
    bool asserted = m_ym3438->IsIRQAsserted() || m_rf5c68->IsIRQAsserted();
    u64 next = GT_NO_EVENT;

    if (unlikely(asserted && IsValidPointer(m_trace_logger) &&
        (m_trace_logger->IsEventEnabled(TRACE_FM, TRACE_FM_IRQ) ||
        m_trace_logger->IsEventEnabled(TRACE_PCM, TRACE_PCM_IRQ))))
        TraceIRQ();

    m_pic->SetIRQLine(k_audio_irq, asserted);

    if (!asserted)
    {
        u64 fm_cycles = m_ym3438->GetCyclesToTimerFlag();

        if (fm_cycles != GT_NO_EVENT)
            next = GetEventClocks(fm_cycles - m_ym3438->GetState()->elapsed_cycles);

        next = MIN(next, m_pcm_irq_clocks);
    }

    m_scheduler->Schedule(SCHEDULER_EVENT_AUDIO, next);
}

// The PCM prediction only changes with the PCM state, so it is kept between FM writes
void Audio::UpdatePCMIRQ()
{
    m_rf5c68->Synchronize();
    u64 cycles = m_rf5c68->GetCyclesToBlockIRQ();
    m_pcm_irq_clocks = cycles == GT_NO_EVENT ? GT_NO_EVENT : GetEventClocks(cycles);
    UpdateIRQ();
}

// Machine clock at which the sound chips have run the given cycles past the last catch-up
u64 Audio::GetEventClocks(u64 cycles) const
{
    return m_state.clocks + (cycles * k_audio_clocks_per_sound_clock) - m_state.sound_clock_remainder;
}

// Data writes carry the register the address port latched, along with the channel it belongs to
void Audio::TraceFM(u8 value)
{
    const YM3438::YM3438_State* state = m_ym3438->GetState();
    u16 address = state->address;
    int bank = (address >> 8) & 0x01;
    u8 reg = (u8)address;
    u8 event = TRACE_FM_GLOBAL;
    u8 channel = (u8)((reg & 0x03) + bank * 3);
    u16 frequency = 0;

    if (reg == 0x28 && bank == 0)
    {
        event = TRACE_FM_KEY;
        channel = (u8)((value & 0x03) + ((value & 0x04) != 0 ? 3 : 0));
    }
    else if (reg >= 0x24 && reg <= 0x27 && bank == 0)
    {
        event = TRACE_FM_TIMER;
        frequency = reg == 0x26 ? state->timer_b_register : state->timer_a_register;
    }
    else if (reg == 0x2A && bank == 0)
        event = TRACE_FM_DAC;
    else if (reg >= 0x30 && reg <= 0x9F)
        event = TRACE_FM_OPERATOR;
    else if (reg >= 0xA0 && reg <= 0xAE)
    {
        event = TRACE_FM_FREQUENCY;
        frequency = (u16)((state->registers[bank][reg | 0x04] << 8) | state->registers[bank][reg & ~0x04]);
    }
    else if (reg >= 0xB0 && reg <= 0xB6)
        event = TRACE_FM_CHANNEL;

    if (!m_trace_logger->IsEventEnabled(TRACE_FM, event))
        return;

    GT_Trace_Entry entry = {};
    entry.type = TRACE_FM;
    entry.event = event;
    entry.fm.address = address;
    entry.fm.frequency = frequency;
    entry.fm.value = value;
    entry.fm.channel = channel;
    entry.fm.flags = (u8)(state->timer_a_flag | (state->timer_b_flag << 1) | (state->timer_a_enable << 2) |
        (state->timer_b_enable << 3));
    m_trace_logger->TraceLog(entry);
}

void Audio::TracePCM(u8 event, u8 reg, u8 value, u8 channel)
{
    if (!m_trace_logger->IsEventEnabled(TRACE_PCM, event))
        return;

    GT_Trace_Entry entry = {};
    entry.type = TRACE_PCM;
    entry.event = event;
    entry.pcm.reg = reg;
    entry.pcm.value = value;
    entry.pcm.channel = channel;
    entry.pcm.enabled = m_rf5c68->IsEnabled() ? 1 : 0;
    entry.pcm.flags = m_rf5c68->GetIRQFlags();
    entry.pcm.mask = m_rf5c68->GetIRQMask();
    m_trace_logger->TraceLog(entry);
}

void Audio::TraceMixer(u8 event, u16 port, u8 value)
{
    int chip = (port >> 1) & 0x01;
    int channel = m_state.volume_channel[chip];
    GT_Trace_Entry entry = {};
    entry.type = TRACE_MIXER;
    entry.event = event;
    entry.mixer.port = port;
    entry.mixer.value = value;
    entry.mixer.chip = (u8)chip;
    entry.mixer.channel = (u8)channel;
    entry.mixer.data = m_state.volume_data[chip][channel];
    entry.mixer.control = m_state.volume_control[chip][channel];

    if (event == TRACE_MIXER_MUTE)
    {
        entry.mixer.data = m_state.mute_control;
        entry.mixer.control = m_state.output_control;
    }

    m_trace_logger->TraceLog(entry);
}

// Only a rising IRQ13 is logged, with the causes each chip holds at that point
void Audio::TraceIRQ()
{
    if ((m_pic->GetSlave()->GetState()->input_levels & (1 << (k_audio_irq - 8))) != 0)
        return;

    const YM3438::YM3438_State* fm = m_ym3438->GetState();
    u8 fm_flags = (u8)(fm->timer_a_flag | (fm->timer_b_flag << 1));
    u8 pcm_flags = m_rf5c68->GetIRQFlags();

    if (fm_flags != 0 && m_trace_logger->IsEventEnabled(TRACE_FM, TRACE_FM_IRQ))
    {
        GT_Trace_Entry entry = {};
        entry.type = TRACE_FM;
        entry.event = TRACE_FM_IRQ;
        entry.fm.flags = (u8)(fm_flags | (fm->timer_a_enable << 2) | (fm->timer_b_enable << 3));
        m_trace_logger->TraceLog(entry);
    }

    if (pcm_flags != 0)
        TracePCM(TRACE_PCM_IRQ, 0, pcm_flags, 0);
}

void Audio::UpdateCDDAGain()
{
    m_cdda_gain_left = GetVolumeGain(k_audio_volume_cdda, k_audio_volume_cdda_left);
    m_cdda_gain_right = GetVolumeGain(k_audio_volume_cdda, k_audio_volume_cdda_right);
}

// Per the MB87078 truth table EN = 0 mutes and C32 forces -32 dB even with C0 set
s32 Audio::GetVolumeGain(int chip, int channel) const
{
    u8 control = m_state.volume_control[chip][channel];

    if ((control & k_audio_volume_enable) == 0)
        return 0;

    if ((control & k_audio_volume_minus_32db) != 0)
        return k_audio_volume_gain_minus_32db;

    if ((control & k_audio_volume_0db) != 0)
        return k_audio_volume_gain[0x3F];

    return k_audio_volume_gain[m_state.volume_data[chip][channel]];
}

// The CPU window shows the 4 KiB wave RAM bank selected by the PCM control register
// Playback never writes wave RAM, so only writes need the chip caught up
u8 Audio::ReadWaveWindowCallback(void* device, u32 offset)
{
    Audio* audio = (Audio*)device;
    return audio->m_rf5c68->Read((u16)(0x1000 | (offset & 0x0FFF)));
}

u8 Audio::PeekWaveWindowCallback(void* device, u32 offset)
{
    const RF5C68* rf5c68 = ((const Audio*)device)->m_rf5c68;
    return rf5c68->Peek((u16)(0x1000 | (offset & 0x0FFF)));
}

void Audio::WriteWaveWindowCallback(void* device, u32 offset, u8 value)
{
    Audio* audio = (Audio*)device;
    RF5C68* rf5c68 = audio->m_rf5c68;
    bool marker = rf5c68->GetWaveRAM()[((u16)rf5c68->GetWaveBank() << 12) | (offset & 0x0FFF)] == 0xFF;
    audio->Synchronize(audio->m_scheduler->GetClocks());
    rf5c68->Write((u16)(0x1000 | (offset & 0x0FFF)), value);

#ifndef GT_DISABLE_VGMRECORDER
    if (audio->m_vgm_recording_enabled)
        audio->m_vgm_recorder.WriteRF5C68Memory((u16)(offset & 0x0FFF), &value, 1);
#endif

    // Adding or removing a loop marker can change where a playing channel crosses its next block
    if (marker != (value == 0xFF))
        audio->UpdatePCMIRQ();
}

// One value per channel and output sample for the debugger scopes, only while they are enabled
void Audio::CaptureChannels(int index)
{
    YM3438::YM3438_State* fm = m_ym3438->GetState();

    for (int i = 0; i < YM3438_CHANNEL_COUNT; i++)
        m_fm_channel_buffer[i][index] = fm->channels[i].output;

    for (int i = 0; i < RF5C68_CHANNEL_COUNT; i++)
        m_pcm_channel_buffer[i][index] = m_rf5c68->GetChannelOutput(i);
}

void Audio::EndFrame(s16* sample_buffer, int* sample_count)
{
    int samples = m_buffer_index;

    m_buffer_index = 0;
    m_buffer_overflow = false;
    m_frame_samples = samples;

    if (!IsValidPointer(sample_buffer) || !IsValidPointer(sample_count))
    {
        if (IsValidPointer(sample_count))
            *sample_count = 0;

        return;
    }

    *sample_count = samples;

    if (m_mute || (m_master_volume <= 0.0f))
    {
        memset(sample_buffer, 0, sizeof(s16) * samples);
        return;
    }

    float fm_volume = m_source_mute[AUDIO_SOURCE_FM] ? 0.0f : m_fm_volume;
    float pcm_volume = m_source_mute[AUDIO_SOURCE_PCM] ? 0.0f : m_pcm_volume;
    float cdda_volume = m_source_mute[AUDIO_SOURCE_CDDA] ? 0.0f : m_cdda_volume;
    float buzzer_volume = m_source_mute[AUDIO_SOURCE_BUZZER] ? 0.0f : m_buzzer_volume;

    s32 dc_left = m_state.output_dc_left;
    s32 dc_right = m_state.output_dc_right;

    // Relative FM/PCM/CD-DA levels are unmeasured: 
    // FM enters at its full DAC sum, PCM at half scale and CD-DA at full scale
    if ((m_master_volume == 1.0f) && (fm_volume == 1.0f) && (pcm_volume == 1.0f) && (cdda_volume == 1.0f) &&
        (buzzer_volume == 1.0f))
    {
        for (int i = 0; i < samples; i += 2)
        {
            s32 left = (s32)m_fm_buffer[i] + (m_pcm_buffer[i] >> 1) + m_cdda_buffer[i] + m_buzzer_buffer[i];
            s32 right = (s32)m_fm_buffer[i + 1] + (m_pcm_buffer[i + 1] >> 1) + m_cdda_buffer[i + 1] +
                m_buzzer_buffer[i + 1];
            left = HighPass(dc_left, left);
            right = HighPass(dc_right, right);
            sample_buffer[i] = (s16)CLAMP(left, -32768, 32767);
            sample_buffer[i + 1] = (s16)CLAMP(right, -32768, 32767);
        }
    }
    else
    {
        for (int i = 0; i < samples; i += 2)
        {
            float left = (float)m_fm_buffer[i] * fm_volume + (float)(m_pcm_buffer[i] >> 1) * pcm_volume +
                (float)m_cdda_buffer[i] * cdda_volume + (float)m_buzzer_buffer[i] * buzzer_volume;
            float right = (float)m_fm_buffer[i + 1] * fm_volume + (float)(m_pcm_buffer[i + 1] >> 1) * pcm_volume +
                (float)m_cdda_buffer[i + 1] * cdda_volume + (float)m_buzzer_buffer[i + 1] * buzzer_volume;
            s32 out_left = (s32)((float)HighPass(dc_left, (s32)left) * m_master_volume);
            s32 out_right = (s32)((float)HighPass(dc_right, (s32)right) * m_master_volume);
            sample_buffer[i] = (s16)CLAMP(out_left, -32768, 32767);
            sample_buffer[i + 1] = (s16)CLAMP(out_right, -32768, 32767);
        }
    }

    m_state.output_dc_left = dc_left;
    m_state.output_dc_right = dc_right;
}

void Audio::SaveState(std::ostream& stream)
{
    StateSerializer serializer(stream);
    Serialize(serializer, GT_SAVESTATE_VERSION);
    m_ym3438->SaveState(stream);
    m_rf5c68->SaveState(stream);
}

void Audio::LoadState(std::istream& stream, int version)
{
    StateSerializer serializer(stream);
    Serialize(serializer, version);
    m_ym3438->LoadState(stream);
    m_rf5c68->LoadState(stream);
    SanitizeState();
}

bool Audio::StartVgmRecording(const char* file_path, int clock_rate, const GT_VGM_Metadata& metadata)
{
    if (m_vgm_recording_enabled)
        return false;

    m_vgm_recorder.Start(file_path, clock_rate, metadata);
    m_vgm_recording_enabled = m_vgm_recorder.IsRecording();

    if (m_vgm_recording_enabled)
        WriteVgmInitialState();

    return m_vgm_recording_enabled;
}

void Audio::WriteVgmInitialState()
{
    YM3438::YM3438_State* fm = m_ym3438->GetState();
    RF5C68::RF5C68_State* pcm = m_rf5c68->GetState();

    for (int i = 0; i < 7; i++)
    {
        u8 reg = k_audio_vgm_fm_mode_registers[i];
        m_vgm_recorder.WriteYM3438(reg, fm->registers[0][reg]);
    }

    for (int bank = 0; bank < 2; bank++)
    {
        u16 base = (u16)(bank << 8);

        for (int reg = 0x30; reg <= 0xB6; reg++)
        {
            int group = reg & 0xFC;

            if (((reg & 0x03) == 0x03) || (group == 0xA4) || (group == 0xAC))
                continue;

            // A frequency high byte only latches until its low byte is written
            if ((group == 0xA0) || (group == 0xA8))
                m_vgm_recorder.WriteYM3438((u16)(base | (reg + 4)), fm->registers[bank][reg + 4]);

            m_vgm_recorder.WriteYM3438((u16)(base | reg), fm->registers[bank][reg]);
        }
    }

    for (int i = 0; i < YM3438_CHANNEL_COUNT; i++)
    {
        YM3438::YM3438_Channel& channel = fm->channels[i];
        u8 slots = (u8)(channel.s1_key_written | (channel.operators[1].manual_key_on << 1) |
            (channel.operators[2].manual_key_on << 2) | (channel.operators[3].manual_key_on << 3));
        m_vgm_recorder.WriteYM3438(0x28, (u8)((slots << 4) | (i < 3 ? i : i + 1)));
    }

    m_vgm_recorder.WriteRF5C68Memory(0, pcm->wave_ram, RF5C68_WAVE_RAM_SIZE);

    u8 enable = pcm->enabled ? 0x80 : 0x00;
    u8 channel_mask = 0;

    for (int i = 0; i < RF5C68_CHANNEL_COUNT; i++)
    {
        RF5C68::RF5C68_Channel& channel = pcm->channels[i];
        m_vgm_recorder.WriteRF5C68(0x07, (u8)(0x40 | i));
        m_vgm_recorder.WriteRF5C68(0x00, channel.envelope);
        m_vgm_recorder.WriteRF5C68(0x01, channel.pan);
        m_vgm_recorder.WriteRF5C68(0x02, (u8)channel.step);
        m_vgm_recorder.WriteRF5C68(0x03, (u8)(channel.step >> 8));
        m_vgm_recorder.WriteRF5C68(0x04, (u8)channel.loop_start);
        m_vgm_recorder.WriteRF5C68(0x05, (u8)(channel.loop_start >> 8));
        m_vgm_recorder.WriteRF5C68(0x06, channel.start);

        if (!channel.enabled)
            channel_mask |= (u8)(1 << i);
    }

    m_vgm_recorder.WriteRF5C68(0x08, channel_mask);
    m_vgm_recorder.WriteRF5C68(0x07, (u8)(enable | 0x40 | pcm->channel_bank));
    m_vgm_recorder.WriteRF5C68(0x07, (u8)(enable | pcm->wave_bank));
}

void Audio::StopVgmRecording()
{
    if (m_vgm_recording_enabled)
    {
        m_vgm_recorder.Stop();
        m_vgm_recording_enabled = false;
    }
}

bool Audio::IsVgmRecording() const
{
    return m_vgm_recording_enabled;
}

void Audio::Serialize(StateSerializer& serializer, int version)
{
    UNUSED(version);
    G_SERIALIZE(serializer, m_state.sound_clock_remainder);
    G_SERIALIZE(serializer, m_state.sample_clock_counter);
    G_SERIALIZE(serializer, m_state.pcm_lowpass_left);
    G_SERIALIZE(serializer, m_state.pcm_lowpass_right);
    G_SERIALIZE(serializer, m_state.clocks);
    G_SERIALIZE_ARRAY(serializer, m_state.volume_channel, AUDIO_VOLUME_CHIPS);
    G_SERIALIZE_ARRAY(serializer, &m_state.volume_data[0][0], AUDIO_VOLUME_CHIPS * AUDIO_VOLUME_CHANNELS);
    G_SERIALIZE_ARRAY(serializer, &m_state.volume_control[0][0], AUDIO_VOLUME_CHIPS * AUDIO_VOLUME_CHANNELS);
    G_SERIALIZE(serializer, m_state.mute_control);
    G_SERIALIZE(serializer, m_state.output_control);
    G_SERIALIZE(serializer, m_state.output_dc_left);
    G_SERIALIZE(serializer, m_state.output_dc_right);
}

void Audio::SanitizeState()
{
    m_state.sound_clock_remainder %= k_audio_clocks_per_sound_clock;
    m_state.sample_clock_counter %= GT_MACHINE_CLOCK_RATE;
    m_state.pcm_lowpass_left = CLAMP(m_state.pcm_lowpass_left, -32768, 32767);
    m_state.pcm_lowpass_right = CLAMP(m_state.pcm_lowpass_right, -32768, 32767);
    m_state.output_dc_left = CLAMP(m_state.output_dc_left, -k_audio_dc_limit, k_audio_dc_limit);
    m_state.output_dc_right = CLAMP(m_state.output_dc_right, -k_audio_dc_limit, k_audio_dc_limit);

    for (int chip = 0; chip < AUDIO_VOLUME_CHIPS; chip++)
    {
        m_state.volume_channel[chip] &= 0x03;

        for (int channel = 0; channel < AUDIO_VOLUME_CHANNELS; channel++)
        {
            m_state.volume_data[chip][channel] &= 0x3F;
            m_state.volume_control[chip][channel] &= 0x1C;
        }
    }

    m_state.mute_control &= k_audio_gate_fm | k_audio_gate_pcm;
    UpdateCDDAGain();
    UpdateGates();
    UpdatePCMIRQ();
}
