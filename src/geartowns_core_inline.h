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

INLINE void GeartownsCore::Pause(bool paused)
{
    m_paused = paused;
}

INLINE bool GeartownsCore::IsPaused()
{
    return m_paused;
}

INLINE Firmware* GeartownsCore::GetFirmware()
{
    return m_firmware;
}

INLINE Media* GeartownsCore::GetMedia()
{
    return m_media;
}

INLINE Audio* GeartownsCore::GetAudio()
{
    return m_audio;
}

INLINE Input* GeartownsCore::GetInput()
{
    return m_input;
}

INLINE Memory* GeartownsCore::GetMemory()
{
    return m_memory;
}

INLINE I386* GeartownsCore::GetI386()
{
    return m_i386;
}

INLINE IO* GeartownsCore::GetIO()
{
    return m_io;
}

INLINE PIC* GeartownsCore::GetPIC()
{
    return m_pic;
}

INLINE PIT* GeartownsCore::GetPIT()
{
    return m_pit;
}

INLINE SystemControl* GeartownsCore::GetSystemControl()
{
    return m_system_control;
}

INLINE Scheduler* GeartownsCore::GetScheduler()
{
    return m_scheduler;
}

INLINE Video* GeartownsCore::GetVideo()
{
    return m_video;
}

INLINE CdRom* GeartownsCore::GetCDROM()
{
    return m_cdrom;
}

INLINE CdRomMedia* GeartownsCore::GetCDROMMedia()
{
    return m_cdrom_media;
}

INLINE CdRomAudio* GeartownsCore::GetCDROMAudio()
{
    return m_cdrom_audio;
}

INLINE FDCMock* GeartownsCore::GetFDC()
{
    return m_fdc;
}

INLINE Keyboard* GeartownsCore::GetKeyboard()
{
    return m_keyboard;
}

INLINE RTC* GeartownsCore::GetRTC()
{
    return m_rtc;
}

INLINE UPD71071* GeartownsCore::GetDMA()
{
    return m_dma;
}
