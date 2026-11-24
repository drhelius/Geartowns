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

#define EMU_SYNTH_IMPORT
#include "emu_synth.h"
#include <vector>
#include <SDL3/SDL.h>
#include "config.h"
#include "emu.h"
#include "media/media_file.h"
#include "audio/midi_synth_host.h"

static const char* const k_emu_synth_labels[GT_MIDI_ENGINE_COUNT] = { "None", "Built-in MT-32",
    "Built-in General MIDI" };

static MidiSynthHost emu_synth_host;
static char emu_synth_mt32_status[256] = "";
static char emu_synth_soundfont_status[256] = "";
static char emu_synth_playing[GT_MIDI_ENGINE_COUNT][MIDI_SYNTH_NAME_SIZE + 64];

static int get_wanted_mt32(void);
static bool open_mt32(void);
static void add_folder(std::vector<std::string>& folders, const char* base, const char* component);
static void create_folder(int engine, const char* component);
static void update_mt32_status(void);
static void update_soundfont_status(void);

void emu_synth_init(void)
{
    create_folder(GT_MIDI_ENGINE_MT32, "mt32");
    create_folder(GT_MIDI_ENGINE_SOUNDFONT, "soundfonts");
    emu_synth_scan_mt32();
    emu_synth_find_soundfont();
}

void emu_synth_scan_mt32(void)
{
    MidiSynth* synth = emu_synth_get_midi_synth();
    std::vector<std::string> folders;

    if (config_audio.mt32_rom_dir_option == 1)
    {
        if (!config_audio.mt32_rom_path.empty())
            folders.push_back(config_audio.mt32_rom_path);
    }
    else
        emu_synth_get_default_folders(GT_MIDI_ENGINE_MT32, folders);

    emu_synth_host.ScanMT32(folders);
    update_mt32_status();

    if (IsValidPointer(synth) && synth->IsOpen(GT_MIDI_ENGINE_MT32))
        open_mt32();
}

void emu_synth_find_soundfont(void)
{
    MidiSynth* synth = emu_synth_get_midi_synth();
    std::vector<std::string> folders;

    if (config_audio.soundfont_option == 1)
    {
        u64 size = 0;
        s64 modified = 0;
        bool found = MediaFile::GetFileInfo(config_audio.soundfont.c_str(), &size, &modified);
        emu_synth_host.SetSoundFont(found ? config_audio.soundfont : std::string());
    }
    else
    {
        emu_synth_get_default_folders(GT_MIDI_ENGINE_SOUNDFONT, folders);
        emu_synth_host.FindSoundFont(folders);
    }

    update_soundfont_status();

    if (!IsValidPointer(synth) || !synth->IsOpen(GT_MIDI_ENGINE_SOUNDFONT) ||
        (emu_synth_host.GetSoundFont() == synth->GetSoundFontPath()))
        return;

    synth->CloseSoundFont();

    if (!emu_synth_host.GetSoundFont().empty())
        emu_synth_activate(GT_MIDI_ENGINE_SOUNDFONT);
}

void emu_synth_update(void)
{
    MidiSynth* synth = emu_synth_get_midi_synth();

    if (IsValidPointer(synth) && synth->IsOpen(GT_MIDI_ENGINE_MT32) &&
        (get_wanted_mt32() != emu_synth_host.GetMT32Index()))
        open_mt32();
}

void emu_synth_get_default_folders(int engine, std::vector<std::string>& folders)
{
    folders.clear();

    if (engine == GT_MIDI_ENGINE_MT32)
    {
        add_folder(folders, config_root_path, "mt32");
        return;
    }

    add_folder(folders, config_root_path, "soundfonts");
#if defined(__APPLE__)
    add_folder(folders, SDL_GetUserFolder(SDL_FOLDER_HOME), "Library/Audio/Sounds/Banks");
#elif defined(_WIN32)
    add_folder(folders, SDL_GetBasePath(), "soundfonts");
#else
    add_folder(folders, "/usr/share", "soundfonts");
    add_folder(folders, "/usr/share/sounds", "sf2");
    add_folder(folders, "/usr/local/share", "soundfonts");
#endif
}

bool emu_synth_is_compiled(int engine)
{
    return MidiSynth::IsAvailable(engine);
}

bool emu_synth_is_available(int engine)
{
    return emu_synth_host.IsAvailable(engine);
}

bool emu_synth_activate(int engine)
{
    MidiSynth* synth = emu_synth_get_midi_synth();

    if (!IsValidPointer(synth) || !emu_synth_is_available(engine))
        return false;

    if (synth->IsOpen(engine))
        return true;

    if (engine == GT_MIDI_ENGINE_MT32)
        return open_mt32();

    const char* path = emu_synth_host.GetSoundFont().c_str();

    if (emu_synth_host.OpenSoundFont(synth) == MidiSynthHost::MIDI_SYNTH_HOST_OPENED)
    {
        Log("MIDI Out: built-in SoundFont %s", path);
        return true;
    }

    Log("MIDI Out: unable to load the SoundFont %s", path);
    update_soundfont_status();
    return false;
}

int emu_synth_get_engine(int synth)
{
    return synth == GT_MIDI_SYNTH_MT32 ? GT_MIDI_ENGINE_MT32 : GT_MIDI_ENGINE_SOUNDFONT;
}

const char* emu_synth_get_label(int engine)
{
    return k_emu_synth_labels[CLAMP(engine, 0, GT_MIDI_ENGINE_COUNT - 1)];
}

const char* emu_synth_get_playing_name(int engine)
{
    MidiSynth* synth = emu_synth_get_midi_synth();
    engine = CLAMP(engine, 0, GT_MIDI_ENGINE_COUNT - 1);
    char* name = emu_synth_playing[engine];

    if (engine == GT_MIDI_ENGINE_MT32 && IsValidPointer(synth))
        snprintf(name, sizeof(emu_synth_playing[engine]), "%s (%s)", k_emu_synth_labels[engine],
            synth->GetMT32Name());
    else if (engine == GT_MIDI_ENGINE_SOUNDFONT && IsValidPointer(synth))
        snprintf(name, sizeof(emu_synth_playing[engine]), "%s (%s)", k_emu_synth_labels[engine],
            get_filename(synth->GetSoundFontPath()));
    else
        strncpy_fit(name, k_emu_synth_labels[engine], sizeof(emu_synth_playing[engine]));

    return name;
}

const char* emu_synth_get_mt32_folder(void)
{
    return emu_synth_host.GetMT32Folder().c_str();
}

const char* emu_synth_get_mt32_status(void)
{
    return emu_synth_mt32_status;
}

const char* emu_synth_get_soundfont(void)
{
    return emu_synth_host.GetSoundFont().c_str();
}

const char* emu_synth_get_soundfont_status(void)
{
    return emu_synth_soundfont_status;
}

MidiSynth* emu_synth_get_midi_synth(void)
{
    GeartownsCore* core = emu_get_core();
    return IsValidPointer(core) ? core->GetAudio()->GetMidiSynth() : NULL;
}

static int get_wanted_mt32(void)
{
    return emu_synth_host.FindMT32(CLAMP(config_audio.mt32_model, 0, GT_MT32_MODEL_COUNT - 1));
}

static bool open_mt32(void)
{
    MidiSynth* synth = emu_synth_get_midi_synth();
    int index = get_wanted_mt32();

    if (!IsValidPointer(synth))
        return false;

    MidiSynthHost::MidiSynthHost_Open result = emu_synth_host.OpenMT32(synth, index);

    if (result == MidiSynthHost::MIDI_SYNTH_HOST_OPENED)
    {
        Log("MIDI Out: built-in MT-32 is %s", emu_synth_host.GetMT32(index).name);
        return true;
    }

    if (result == MidiSynthHost::MIDI_SYNTH_HOST_NONE)
    {
        synth->CloseMT32();
        return false;
    }

    Log("MIDI Out: unable to open the %s ROMs in %s", emu_synth_host.GetMT32(index).name,
        emu_synth_host.GetMT32Folder().c_str());
    update_mt32_status();
    return false;
}

static void add_folder(std::vector<std::string>& folders, const char* base, const char* component)
{
    if (!IsValidPointer(base) || (base[0] == 0))
        return;

    std::string path(base);
    append_path_component(path, component);
    folders.push_back(path);
}

static void create_folder(int engine, const char* component)
{
    if (!MidiSynth::IsAvailable(engine) || !IsValidPointer(config_root_path) || (config_root_path[0] == 0))
        return;

    std::string path(config_root_path);
    append_path_component(path, component);
    MediaFile::MakeDirectory(path.c_str());
}

static void update_mt32_status(void)
{
    char* status = emu_synth_mt32_status;
    size_t size = sizeof(emu_synth_mt32_status);

    if (!MidiSynth::IsAvailable(GT_MIDI_ENGINE_MT32))
    {
        strncpy_fit(status, "Not in this build", size);
        return;
    }

    int count = emu_synth_host.GetMT32Count();

    if (count == 0)
    {
        strncpy_fit(status, "No MT-32 ROMs", size);
        return;
    }

    if (emu_synth_host.HasMT32Failed())
    {
        strncpy_fit(status, "Unable to open the MT-32 ROMs", size);
        return;
    }

    status[0] = 0;

    for (int i = 0; i < count; i++)
    {
        if (i > 0)
            strncat_fit(status, i == count - 1 ? " and " : ", ", size);

        strncat_fit(status, emu_synth_host.GetMT32(i).name, size);
    }

    strncat_fit(status, " found", size);
}

static void update_soundfont_status(void)
{
    char* status = emu_synth_soundfont_status;
    size_t size = sizeof(emu_synth_soundfont_status);
    const char* path = emu_synth_host.GetSoundFont().c_str();
    u64 bytes = 0;
    s64 modified = 0;

    if (!MidiSynth::IsAvailable(GT_MIDI_ENGINE_SOUNDFONT))
        strncpy_fit(status, "Not in this build", size);
    else if (path[0] == 0)
        strncpy_fit(status, "No sound bank", size);
    else if (emu_synth_host.HasSoundFontFailed())
        snprintf(status, size, "Unable to load %s", get_filename(path));
    else if (MediaFile::GetFileInfo(path, &bytes, &modified))
        snprintf(status, size, "%s, %u MB", get_filename(path), (unsigned)((bytes + 0x80000) >> 20));
    else
        strncpy_fit(status, get_filename(path), size);
}
