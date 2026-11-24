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

#define EMU_MIDI_IMPORT
#include "emu_midi.h"
#include <SDL3/SDL.h>
#include "config.h"
#include "emu.h"
#include "emu_game.h"
#include "emu_synth.h"
#include "gui_notifications.h"
#include "midi_port.h"
#include "midi_queue.h"

static const u64 k_emu_midi_retry_ns = 2000000000ULL;
static const int k_emu_midi_drain_ms = 500;

static bool emu_midi_stopped = false;
static bool emu_midi_rewinding = false;
static bool emu_midi_notified = false;
static u64 emu_midi_last_try = 0;

static MidiOut* get_midi(void);
static bool is_builtin(void);
static int get_out(int synth);
static const char* get_out_port(int synth);
static bool resolve(int synth, int out, const char* port, int* engine, const char** name);
static void resolve_synth(int synth, int* engine, const char** name);
static void resolve_wanted(int* engine, const char** name);
static void close_port(MidiOut* midi);
static void update_port(MidiOut* midi, bool force);
static void stop(MidiOut* midi);
static void sync(MidiOut* midi);
static void silence(MidiOut* midi);
static void notify(void);

void emu_midi_init(void)
{
    midi_queue_init();
}

void emu_midi_destroy(void)
{
    MidiOut* midi = get_midi();

    if (IsValidPointer(midi) && midi_queue_is_open())
    {
        silence(midi);
        midi_queue_drain(k_emu_midi_drain_ms);
    }

    midi_queue_destroy();
}

// Called just before a frame runs: the port follows the game, and the synth catches up with what changed since the
// last frame, a reset, a stop or a rewind, before any of the frame's own messages
void emu_midi_begin_frame(void)
{
    MidiOut* midi = get_midi();

    if (!IsValidPointer(midi))
        return;

    emu_synth_update();
    update_port(midi, false);
    bool open = midi_queue_is_open();
    bool builtin = is_builtin();

    if (midi->TakeReset())
    {
        emu_midi_notified = false;

        if (builtin)
            emu_synth_get_midi_synth()->Reset();
        else if (open)
            sync(midi);
    }

    if ((emu_midi_stopped && open) || (emu_midi_rewinding && (open || builtin)))
        sync(midi);

    emu_midi_stopped = false;
    emu_midi_rewinding = false;

    bool quiet = config_emulator.ffwd || emu_is_load_ffwd() || !config_audio.enable;
    MidiOut::MidiOut_Capture_Change change = midi->UpdateCapture(open || builtin, quiet, builtin);

    if (change == MidiOut::MIDI_OUT_CAPTURE_SILENCE)
        silence(midi);
    else if (change == MidiOut::MIDI_OUT_CAPTURE_SYNC)
        sync(midi);
}

// Called after the frame's samples are queued, so its messages play with them
void emu_midi_end_frame(int sample_count)
{
    MidiOut* midi = get_midi();

    if (!IsValidPointer(midi))
        return;

    bool builtin = is_builtin();

    if (!builtin)
        midi_queue_frame(midi, sample_count / 2);

    if ((emu_midi_last_try != 0) && midi->HasPlayed() && !midi_queue_is_open() && !builtin && !emu_midi_notified)
        notify();
}

// Paused, stopped in the debugger, powered off: nothing plays until the next frame brings the synth back in step
void emu_midi_idle(void)
{
    if (emu_midi_stopped)
        return;

    emu_midi_stopped = true;
    MidiOut* midi = get_midi();

    if (IsValidPointer(midi) && midi_queue_is_open())
        silence(midi);
}

void emu_midi_rewind(void)
{
    MidiOut* midi = get_midi();

    if (!IsValidPointer(midi))
        return;

    if (!emu_midi_rewinding)
    {
        emu_midi_rewinding = true;

        if (midi_queue_is_open() || is_builtin())
            silence(midi);
    }

    midi->SetCapture(MidiOut::MIDI_OUT_CAPTURE_OFF);
}

void emu_midi_state_loaded(void)
{
    MidiOut* midi = get_midi();

    if (IsValidPointer(midi) && (midi_queue_is_open() || is_builtin()))
        sync(midi);
}

void emu_midi_update_port(void)
{
    MidiOut* midi = get_midi();

    if (IsValidPointer(midi))
        update_port(midi, true);
}

bool emu_midi_prepare(void)
{
    bool ready = midi_queue_prepare();
    MidiOut* midi = get_midi();

    if (IsValidPointer(midi))
        update_port(midi, false);

    return ready;
}

int emu_midi_get_synth(void)
{
    int synth = emu_midi_get_known_synth();
    return synth >= 0 ? synth : GT_MIDI_SYNTH_GM;
}

// The user's choice goes first, then the database and what the game has sent
int emu_midi_get_known_synth(void)
{
    GeartownsCore* core = emu_get_core();
    int music = emu_game_get(Emu_Game_Setting_MidiMusic);

    if (music > 0)
        return music - 1;

    int database = IsValidPointer(core) ? core->GetDatabaseMidiSynth() : -1;

    if (database >= 0)
        return database;

    MidiOut* midi = get_midi();
    return IsValidPointer(midi) ? midi->GetDetected() : -1;
}

const char* emu_midi_get_synth_label(int synth)
{
    int engine = GT_MIDI_ENGINE_NONE;
    const char* name = "";
    resolve_synth(synth, &engine, &name);

    if (engine != GT_MIDI_ENGINE_NONE)
        return emu_synth_get_label(engine);

    return name[0] != 0 ? name : k_midi_outs[GT_MIDI_OUT_NONE];
}

int emu_midi_get_wanted_engine(void)
{
    int engine = GT_MIDI_ENGINE_NONE;
    const char* name = "";
    resolve_wanted(&engine, &name);
    return engine;
}

const char* emu_midi_get_wanted_port(void)
{
    int engine = GT_MIDI_ENGINE_NONE;
    const char* name = "";
    resolve_wanted(&engine, &name);
    return name;
}

const char* emu_midi_get_port(void)
{
    if (is_builtin())
        return emu_synth_get_playing_name(emu_synth_get_midi_synth()->GetEngine());

    return midi_queue_is_open() ? midi_queue_get_port() : "";
}

static MidiOut* get_midi(void)
{
    GeartownsCore* core = emu_get_core();
    return IsValidPointer(core) ? core->GetAudio()->GetMidiOut() : NULL;
}

static bool is_builtin(void)
{
    MidiSynth* synth = emu_synth_get_midi_synth();
    return IsValidPointer(synth) && synth->IsActive();
}

static int get_out(int synth)
{
    int out = synth == GT_MIDI_SYNTH_MT32 ? config_audio.midi_mt32_out : config_audio.midi_gm_out;

    if ((out == GT_MIDI_OUT_BUILTIN) && !emu_synth_is_compiled(emu_synth_get_engine(synth)))
        return GT_MIDI_OUT_NONE;

    return CLAMP(out, 0, GT_MIDI_OUT_COUNT - 1);
}

static const char* get_out_port(int synth)
{
    return synth == GT_MIDI_SYNTH_MT32 ? config_audio.midi_mt32_port.c_str() : config_audio.midi_gm_port.c_str();
}

static bool resolve(int synth, int out, const char* port, int* engine, const char** name)
{
    if (out == GT_MIDI_OUT_BUILTIN)
    {
        if (!emu_synth_is_available(emu_synth_get_engine(synth)))
            return false;

        *engine = emu_synth_get_engine(synth);
        *name = "";
        return true;
    }

    if ((out != GT_MIDI_OUT_EXTERNAL) || (port[0] == 0))
        return false;

    *engine = GT_MIDI_ENGINE_NONE;
    *name = port;
    return true;
}

static void resolve_synth(int synth, int* engine, const char** name)
{
    int other = synth == GT_MIDI_SYNTH_MT32 ? GT_MIDI_SYNTH_GM : GT_MIDI_SYNTH_MT32;
    *engine = GT_MIDI_ENGINE_NONE;
    *name = "";

    if (resolve(synth, get_out(synth), get_out_port(synth), engine, name) ||
        resolve(other, get_out(other), get_out_port(other), engine, name))
        return;

#if defined(_WIN32)
    if (get_out(GT_MIDI_SYNTH_GM) == GT_MIDI_OUT_BUILTIN)
        *name = "Microsoft GS Wavetable Synth";
#endif
}

static void resolve_wanted(int* engine, const char** name)
{
    resolve_synth(emu_midi_get_synth(), engine, name);
}

static void close_port(MidiOut* midi)
{
    if (midi_queue_is_lost())
        Log("MIDI Out: %s stopped taking messages", midi_queue_get_port());
    else
    {
        silence(midi);
        midi_queue_drain(k_emu_midi_drain_ms);
    }

    midi_queue_close();
    midi->ForgetSynth();
    midi->SetCapture(MidiOut::MIDI_OUT_CAPTURE_OFF);
}

// A port that went away, or a synth started after Geartowns, is tried again every two seconds. A newly opened synth
// knows nothing, so it gets everything the machine has sent
static void update_port(MidiOut* midi, bool force)
{
    if (!emu_get_core()->GetAudio()->GetMidiCard()->IsFitted())
    {
        stop(midi);
        return;
    }

    int engine = GT_MIDI_ENGINE_NONE;
    const char* wanted = "";
    resolve_wanted(&engine, &wanted);
    MidiSynth* synth = emu_synth_get_midi_synth();

    if (engine != GT_MIDI_ENGINE_NONE)
    {
        if (midi_queue_is_open())
            close_port(midi);

        synth->SetEngine(emu_synth_activate(engine) ? engine : GT_MIDI_ENGINE_NONE);
        return;
    }

    synth->SetEngine(GT_MIDI_ENGINE_NONE);

    if ((wanted[0] != 0) && !midi_queue_prepare())
        return;

    u64 now = SDL_GetTicksNS();
    bool due = force || (emu_midi_last_try == 0) || ((now - emu_midi_last_try) >= k_emu_midi_retry_ns);

    if (due)
        emu_midi_last_try = now;

    if (due && midi_queue_is_open())
        midi_queue_check();

    if (midi_queue_is_open())
    {
        bool lost = midi_queue_is_lost();

        if (!lost && (wanted[0] != 0) && MidiPort::IsSamePort(wanted, midi_queue_get_port()))
            return;

        close_port(midi);
    }

    if ((wanted[0] == 0) || !due || !midi_queue_open(wanted))
        return;

    midi->ForgetSynth();
    sync(midi);
}

static void stop(MidiOut* midi)
{
    emu_synth_get_midi_synth()->SetEngine(GT_MIDI_ENGINE_NONE);

    if (midi_queue_is_open())
        close_port(midi);

    midi->SetCapture(MidiOut::MIDI_OUT_CAPTURE_OFF);
    midi->Clear();
    emu_midi_last_try = 0;
}

static void sync(MidiOut* midi)
{
    midi->Sync();

    if (!is_builtin())
        midi_queue_flush(midi);
}

static void silence(MidiOut* midi)
{
    midi->Silence();

    if (!is_builtin())
        midi_queue_flush(midi);
}

static void notify(void)
{
    emu_midi_notified = true;
    const char* wanted = emu_midi_get_wanted_port();
    int kind = emu_midi_get_synth();
    const char* synth = k_midi_synths[kind].name;
    bool builtin = get_out(kind) == GT_MIDI_OUT_BUILTIN;
    char message[256];

    if (!MidiPort::IsSupported() && !builtin)
        return;

    if ((wanted[0] == 0) && builtin && (kind == GT_MIDI_SYNTH_MT32))
    {
        snprintf(message, sizeof(message), "This game is playing %s music, and no MT-32 ROMs were found", synth);
        gui_notify(gui_NotificationWarning, ICON_MD_MUSIC_NOTE, message,
            "Choose their folder in Audio > Built-in Synths > MT-32.", "midi");
    }
    else if ((wanted[0] == 0) && builtin)
    {
        snprintf(message, sizeof(message), "This game is playing %s music, and no sound bank was found", synth);
        gui_notify(gui_NotificationWarning, ICON_MD_MUSIC_NOTE, message,
            "Choose one in Audio > Built-in Synths > General MIDI.", "midi");
    }
    else if (wanted[0] == 0)
    {
        snprintf(message, sizeof(message), "This game is playing %s music, and no MIDI output is set", synth);
        gui_notify(gui_NotificationWarning, ICON_MD_MUSIC_NOTE, message, "Choose one in Audio > MIDI Out.", "midi");
    }
    else
    {
        snprintf(message, sizeof(message), "This game is playing %s music, and %s isn't there", synth, wanted);
        gui_notify(gui_NotificationWarning, ICON_MD_MUSIC_NOTE, message,
            "Start the synth, or choose another output in Audio > MIDI Out.", "midi");
    }
}
