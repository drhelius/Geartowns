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

#include "io.h"
#include "../audio/audio.h"
#include "../audio/ym3438.h"
#include "../audio/rf5c68.h"
#include "../cdrom/cdrom.h"
#include "../drive/fdc.h"
#include "../input/keyboard.h"
#include "memory.h"
#include "pic.h"
#include "pit.h"
#include "msm58321.h"
#include "system_control.h"
#include "upd71071.h"
#include "../video/video.h"

IO::IO()
{
    InitPointer(m_audio);
    InitPointer(m_ym3438);
    InitPointer(m_rf5c68);
    InitPointer(m_pic);
    InitPointer(m_pit);
    InitPointer(m_video);
    InitPointer(m_memory);
    InitPointer(m_system_control);
    InitPointer(m_cdrom);
    InitPointer(m_fdc);
    InitPointer(m_keyboard);
    InitPointer(m_rtc);
    InitPointer(m_dma);
}

IO::~IO()
{
}

void IO::Init(Audio* audio, PIC* pic, PIT* pit, Video* video, Memory* memory, SystemControl* system_control,
    CdRom* cdrom, FDC* fdc, Keyboard* keyboard, MSM58321* rtc, UPD71071* dma)
{
    m_audio = audio;
    m_ym3438 = audio->GetYM3438();
    m_rf5c68 = audio->GetRF5C68();
    m_pic = pic;
    m_pit = pit;
    m_video = video;
    m_memory = memory;
    m_system_control = system_control;
    m_cdrom = cdrom;
    m_fdc = fdc;
    m_keyboard = keyboard;
    m_rtc = rtc;
    m_dma = dma;
    Reset();
}

void IO::Reset()
{
}

u8 IO::Read8(u16 port, GT_Bus_Access_Context& context)
{
    switch (port)
    {
        case 0x0000:
            // PIC master status
        case 0x0002:
            // PIC master mask
        case 0x0010:
            // PIC slave status
        case 0x0012:
            // PIC slave mask
            return m_pic->Read(port);
        case 0x0020:
            // Reset reason
            return m_system_control->Read(port);
        case 0x0022:
            // Power control
            break;
        case 0x0030:
            // Machine ID low
            return 0x01;
        case 0x0031:
            // Machine ID high
            return 0x01;
        case 0x0032:
            // Serial ID ROM
            return m_system_control->Read(port);
        case 0x0040:
            // PIT counter 0
        case 0x0042:
            // PIT counter 1
        case 0x0044:
            // PIT counter 2
        case 0x0046:
            // PIT 0-2 control
        case 0x0050:
            // PIT counter 3
        case 0x0052:
            // PIT counter 4
        case 0x0054:
            // PIT counter 5
        case 0x0056:
            // PIT 3-5 control
        case 0x0060:
            // Timer interrupt status
            return m_pit->Read(port, context.clocks);
        case 0x0070:
            // RTC data
        case 0x0080:
            // RTC command
            return m_rtc->Read(port, context.clocks);
        case 0x00A0:
            // DMA initialize
        case 0x00A1:
            // DMA channel select
        case 0x00A2:
            // DMA count low
        case 0x00A3:
            // DMA count high
        case 0x00A4:
            // DMA address low
        case 0x00A5:
            // DMA address mid low
        case 0x00A6:
            // DMA address mid high
        case 0x00A7:
            // DMA address high
        case 0x00A8:
            // DMA device control low
        case 0x00A9:
            // DMA device control high
        case 0x00AA:
            // DMA mode control
        case 0x00AB:
            // DMA status
        case 0x00AC:
            // DMA temporary low
        case 0x00AD:
            // DMA temporary high
        case 0x00AE:
            // DMA request
        case 0x00AF:
            // DMA mask
            return m_dma->Read(port);
        case 0x0200:
            // FDC status
        case 0x0202:
            // FDC track
        case 0x0204:
            // FDC sector
        case 0x0206:
            // FDC data
        case 0x0208:
            // FDC drive status
        case 0x020C:
            // FDC drive select
        case 0x020D:
            // FDC drive type extension
        case 0x020E:
            // FDC drive switch
            return m_fdc->Read(port, context.clocks);
        case 0x0400:
            // Resolution status
            return 0xFE;
        case 0x0404:
            // FM-R VRAM mapping
            return m_memory->ReadMappingControl(port);
        case 0x0440:
            // CRTC address
        case 0x0442:
            // CRTC data low
        case 0x0443:
            // CRTC data high
        case 0x0448:
            // Video output address
        case 0x044A:
            // Video output data
        case 0x044C:
            // Palette and sprite status
        case 0x0450:
            // Sprite address
        case 0x0452:
            // Sprite data
        case 0x0458:
            // VRAM mask address
        case 0x045A:
            // VRAM mask low
        case 0x045B:
            // VRAM mask high
            return m_video->Read(port, context.clocks);
        case 0x0480:
            // System ROM mapping
        case 0x0484:
            // Dictionary ROM bank
            return m_memory->ReadMappingControl(port);
        case 0x048A:
            // Memory card status
            return 0x06;
        case 0x04C0:
            // CD-ROM master status
        case 0x04C2:
            // CD-ROM status
        case 0x04C4:
            // CD-ROM data
        case 0x04C6:
            // CD-ROM transfer control
        case 0x04CC:
            // CD-ROM subcode status
        case 0x04CD:
            // CD-ROM subcode data
            return m_cdrom->Read(port, context.clocks);
        case 0x04D0:
            // Game port A
        case 0x04D2:
            // Game port B
            break;
        case 0x04D5:
            // Sound mute
            return m_audio->ReadGate(port);
        case 0x04D6:
            // Game port output
            break;
        case 0x04D8:
            // FM status
        case 0x04DA:
            // FM data bank 0
        case 0x04DC:
            // FM address bank 1
        case 0x04DE:
            // FM data bank 1
            m_audio->Synchronize(context.clocks);
            return m_ym3438->Read((u8)((port - 0x04D8) >> 1));
        case 0x04E0:
            // Volume 1 data
        case 0x04E1:
            // Volume 1 command
        case 0x04E2:
            // Volume 2 data
        case 0x04E3:
            // Volume 2 command
            return m_audio->ReadVolume(port);
        case 0x04E7:
            // ADC sample data
        case 0x04E8:
            // ADC sample ready
            break;
        case 0x04E9:
            // Sound interrupt reason
            m_audio->Synchronize(context.clocks);
            return (m_rf5c68->IsIRQAsserted() ? 0x08 : 0x00) | (m_ym3438->IsIRQAsserted() ? 0x01 : 0x00);
        case 0x04EA:
            // PCM interrupt mask
            return m_rf5c68->GetIRQMask();
        case 0x04EB:
            // PCM interrupt reason
            m_audio->Synchronize(context.clocks);
            return m_audio->ReadPCMIRQFlags();
        case 0x04EC:
            // LED and output mute
            return m_audio->ReadGate(port);
        case 0x04F0:
            // PCM envelope
        case 0x04F1:
            // PCM pan
        case 0x04F2:
            // PCM step low
        case 0x04F3:
            // PCM step high
        case 0x04F4:
            // PCM loop start low
        case 0x04F5:
            // PCM loop start high
        case 0x04F6:
            // PCM start address
        case 0x04F7:
            // PCM control
        case 0x04F8:
            // PCM channel enable
            break;
        case 0x05C0:
            // Expansion NMI mask
        case 0x05C2:
            // Expansion NMI status
            break;
        case 0x05C8:
            // Text VRAM written
            return m_video->Read(port, context.clocks);
        case 0x05CA:
            // VSYNC interrupt clear
            break;
        case 0x05E0:
            // Undocumented, BIOS writes 01h
            return m_system_control->Read(port);
        case 0x0600:
            // Keyboard data
        case 0x0602:
            // Keyboard status
        case 0x0604:
            // Keyboard interrupt
            return m_keyboard->Read(port, context.clocks);
        case 0x0800:
            // Printer status 1
        case 0x0802:
            // Printer status 2
        case 0x0804:
            // Printer interrupt enable
            break;
        case 0x0A00:
            // Serial receive data
        case 0x0A02:
            // Serial status
        case 0x0A04:
            // Serial modem status
        case 0x0A06:
            // Serial interrupt reason
        case 0x0A08:
            // Serial interrupt control
        case 0x0A0A:
            // Serial modem control
            break;
        case 0x0C30:
            // SCSI data
        case 0x0C32:
            // SCSI status
            break;
        case 0xFD90:
            // Palette index
        case 0xFD92:
            // Palette blue
        case 0xFD94:
            // Palette red
        case 0xFD96:
            // Palette green
        case 0xFD98:
            // FM-R digital palette 0
        case 0xFD99:
            // FM-R digital palette 1
        case 0xFD9A:
            // FM-R digital palette 2
        case 0xFD9B:
            // FM-R digital palette 3
        case 0xFD9C:
            // FM-R digital palette 4
        case 0xFD9D:
            // FM-R digital palette 5
        case 0xFD9E:
            // FM-R digital palette 6
        case 0xFD9F:
            // FM-R digital palette 7
        case 0xFDA0:
            // Sync status
            return m_video->Read(port, context.clocks);
        case 0xFF81:
            // FM-R plane mask
        case 0xFF83:
            // FM-R page select
        case 0xFF84:
            // FM-R light pen status
        case 0xFF86:
            // FM-R sync status
        case 0xFF94:
            // Kanji ROM status
        case 0xFF96:
            // Kanji ROM pattern left
        case 0xFF97:
            // Kanji ROM pattern right
        case 0xFF98:
            // Buzzer on
        case 0xFFA0:
            // FM-R logical operation status
            return m_video->ReadFMRRegister(port & 0x0FFF, false);
        default:
            // CMOS RAM, even ports
            if ((port & 0xF001) == 0x3000)
                return m_memory->ReadCMOS((port & 0x0FFF) >> 1);

            Debug("Unknown IO read at %04X", port);
            break;
    }

    return 0xFF;
}

// What a read would return, without its side effects, for the debugger
// Returns false for ports the board does not decode
bool IO::Peek(u16 port, u64 clocks, u8& value) const
{
    value = 0xFF;

    switch (port)
    {
        case 0x0000:
        case 0x0002:
        case 0x0010:
        case 0x0012:
            value = m_pic->Peek(port);
            break;
        case 0x0020:
        case 0x0032:
            value = m_system_control->Peek(port);
            break;
        case 0x0022:
            break;
        case 0x0030:
        case 0x0031:
            value = 0x01;
            break;
        case 0x0040:
        case 0x0042:
        case 0x0044:
        case 0x0046:
        case 0x0050:
        case 0x0052:
        case 0x0054:
        case 0x0056:
        case 0x0060:
            value = m_pit->Peek(port, clocks);
            break;
        case 0x0070:
        case 0x0080:
            value = m_rtc->Peek(port, clocks);
            break;
        case 0x00A0:
        case 0x00A1:
        case 0x00A2:
        case 0x00A3:
        case 0x00A4:
        case 0x00A5:
        case 0x00A6:
        case 0x00A7:
        case 0x00A8:
        case 0x00A9:
        case 0x00AA:
        case 0x00AB:
        case 0x00AC:
        case 0x00AD:
        case 0x00AE:
        case 0x00AF:
            value = m_dma->Peek(port);
            break;
        case 0x0200:
        case 0x0202:
        case 0x0204:
        case 0x0206:
        case 0x0208:
        case 0x020C:
        case 0x020D:
        case 0x020E:
            value = m_fdc->Peek(port, clocks);
            break;
        case 0x0400:
            value = 0xFE;
            break;
        case 0x0404:
        case 0x0480:
        case 0x0484:
            value = m_memory->ReadMappingControl(port);
            break;
        case 0x0440:
        case 0x0442:
        case 0x0443:
        case 0x0448:
        case 0x044A:
        case 0x044C:
        case 0x0450:
        case 0x0452:
        case 0x0458:
        case 0x045A:
        case 0x045B:
        case 0x05C8:
        case 0xFD90:
        case 0xFD92:
        case 0xFD94:
        case 0xFD96:
        case 0xFD98:
        case 0xFD99:
        case 0xFD9A:
        case 0xFD9B:
        case 0xFD9C:
        case 0xFD9D:
        case 0xFD9E:
        case 0xFD9F:
        case 0xFDA0:
            value = m_video->Peek(port, clocks);
            break;
        case 0x048A:
            value = 0x06;
            break;
        case 0x04C0:
        case 0x04C2:
        case 0x04C4:
        case 0x04C6:
        case 0x04CC:
        case 0x04CD:
            value = m_cdrom->Peek(port);
            break;
        case 0x04D8:
        case 0x04DA:
        case 0x04DC:
        case 0x04DE:
            value = m_ym3438->Peek((u8)((port - 0x04D8) >> 1));
            break;
        case 0x04E0:
        case 0x04E1:
        case 0x04E2:
        case 0x04E3:
            value = m_audio->ReadVolume(port);
            break;
        case 0x04E9:
            value = (m_rf5c68->GetIRQFlags() != 0 ? 0x08 : 0x00) | ((m_ym3438->Peek(0) & 0x03) != 0 ? 0x01 : 0x00);
            break;
        case 0x04EA:
            value = m_rf5c68->GetIRQMask();
            break;
        case 0x04EB:
            value = m_rf5c68->GetIRQFlags();
            break;
        case 0x04D5:
        case 0x04EC:
            value = m_audio->ReadGate(port);
            break;
        case 0x04D0:
        case 0x04D2:
        case 0x04D6:
        case 0x04E7:
        case 0x04E8:
        case 0x04F0:
        case 0x04F1:
        case 0x04F2:
        case 0x04F3:
        case 0x04F4:
        case 0x04F5:
        case 0x04F6:
        case 0x04F7:
        case 0x04F8:
        case 0x05C0:
        case 0x05C2:
        case 0x05CA:
        case 0x0800:
        case 0x0802:
        case 0x0804:
        case 0x0A00:
        case 0x0A02:
        case 0x0A04:
        case 0x0A06:
        case 0x0A08:
        case 0x0A0A:
        case 0x0C30:
        case 0x0C32:
            break;
        case 0x05E0:
            value = m_system_control->Peek(port);
            break;
        case 0x0600:
        case 0x0602:
        case 0x0604:
            value = m_keyboard->Peek(port);
            break;
        case 0xFF81:
        case 0xFF83:
        case 0xFF84:
        case 0xFF86:
        case 0xFF94:
        case 0xFF96:
        case 0xFF97:
        case 0xFF98:
        case 0xFFA0:
            value = m_video->ReadFMRRegister(port & 0x0FFF, true);
            break;
        default:
            if ((port & 0xF001) != 0x3000)
                return false;

            value = m_memory->ReadCMOS((port & 0x0FFF) >> 1);
            break;
    }

    return true;
}

void IO::Write8(u16 port, u8 value, GT_Bus_Access_Context& context)
{
    switch (port)
    {
        case 0x0000:
            // PIC master command
        case 0x0002:
            // PIC master data
        case 0x0010:
            // PIC slave command
        case 0x0012:
            // PIC slave data
            m_pic->Write(port, value);
            break;
        case 0x0020:
            // Reset and power control
        case 0x0022:
            // Power off
            m_system_control->Write(port, value);
            break;
        case 0x0030:
            // Machine ID low
        case 0x0031:
            // Machine ID high
            break;
        case 0x0032:
            // Serial ID ROM
            m_system_control->Write(port, value);
            break;
        case 0x0040:
            // PIT counter 0
        case 0x0042:
            // PIT counter 1
        case 0x0044:
            // PIT counter 2
        case 0x0046:
            // PIT 0-2 control
        case 0x0050:
            // PIT counter 3
        case 0x0052:
            // PIT counter 4
        case 0x0054:
            // PIT counter 5
        case 0x0056:
            // PIT 3-5 control
        case 0x0060:
            // Timer interrupt control
            m_pit->Write(port, value, context.clocks);
            break;
        case 0x0070:
            // RTC data
        case 0x0080:
            // RTC command
            m_rtc->Write(port, value, context.clocks);
            break;
        case 0x00A0:
            // DMA initialize
        case 0x00A1:
            // DMA channel select
        case 0x00A2:
            // DMA count low
        case 0x00A3:
            // DMA count high
        case 0x00A4:
            // DMA address low
        case 0x00A5:
            // DMA address mid low
        case 0x00A6:
            // DMA address mid high
        case 0x00A7:
            // DMA address high
        case 0x00A8:
            // DMA device control low
        case 0x00A9:
            // DMA device control high
        case 0x00AA:
            // DMA mode control
        case 0x00AB:
            // DMA status
        case 0x00AC:
            // DMA temporary low
        case 0x00AD:
            // DMA temporary high
        case 0x00AE:
            // DMA request
        case 0x00AF:
            // DMA mask
            m_dma->Write(port, value);
            break;
        case 0x0200:
            // FDC command
        case 0x0202:
            // FDC track
        case 0x0204:
            // FDC sector
        case 0x0206:
            // FDC data
        case 0x0208:
            // FDC drive control
        case 0x020C:
            // FDC drive select
        case 0x020E:
            // FDC drive switch
            m_fdc->Write(port, value, context.clocks);
            break;
        case 0x0400:
            // Resolution status
            break;
        case 0x0404:
            // FM-R VRAM mapping
            m_memory->WriteMappingControl(port, value);
            break;
        case 0x0440:
            // CRTC address
        case 0x0442:
            // CRTC data low
        case 0x0443:
            // CRTC data high
        case 0x0448:
            // Video output address
        case 0x044A:
            // Video output data
        case 0x044C:
            // Palette and sprite status
        case 0x0450:
            // Sprite address
        case 0x0452:
            // Sprite data
        case 0x0458:
            // VRAM mask address
        case 0x045A:
            // VRAM mask low
        case 0x045B:
            // VRAM mask high
            m_video->Write(port, value, context.clocks);
            break;
        case 0x0480:
            // System ROM mapping
        case 0x0484:
            // Dictionary ROM bank
            m_memory->WriteMappingControl(port, value);
            break;
        case 0x048A:
            // Memory card status
            break;
        case 0x04C0:
            // CD-ROM master control
        case 0x04C2:
            // CD-ROM command
        case 0x04C4:
            // CD-ROM parameter
        case 0x04C6:
            // CD-ROM transfer control
            m_cdrom->Write(port, value, context.clocks);
            break;
        case 0x04CC:
            // CD-ROM subcode status
        case 0x04CD:
            // CD-ROM subcode data
            break;
        case 0x04D0:
            // Game port A
        case 0x04D2:
            // Game port B
            break;
        case 0x04D5:
            // Sound mute
            m_audio->Synchronize(context.clocks);
            m_audio->WriteGate(port, value);
            break;
        case 0x04D6:
            // Game port output
            break;
        case 0x04D8:
            // FM address bank 0
        case 0x04DA:
            // FM data bank 0
        case 0x04DC:
            // FM address bank 1
        case 0x04DE:
            // FM data bank 1
            m_audio->Synchronize(context.clocks);
            m_audio->WriteFM((u8)((port - 0x04D8) >> 1), value);
            break;
        case 0x04E0:
            // Volume 1 data
        case 0x04E1:
            // Volume 1 command
        case 0x04E2:
            // Volume 2 data
        case 0x04E3:
            // Volume 2 command
            m_audio->Synchronize(context.clocks);
            m_audio->WriteVolume(port, value);
            break;
        case 0x04E7:
            // ADC sample data
        case 0x04E8:
            // ADC sample clear
        case 0x04E9:
            // Sound interrupt reason
            break;
        case 0x04EA:
            // PCM interrupt mask
            m_audio->Synchronize(context.clocks);
            m_audio->WritePCMIRQMask(value);
            break;
        case 0x04EB:
            // PCM interrupt reason
            break;
        case 0x04EC:
            // LED and output mute
            m_audio->Synchronize(context.clocks);
            m_audio->WriteGate(port, value);
            break;
        case 0x04F0:
            // PCM envelope
        case 0x04F1:
            // PCM pan
        case 0x04F2:
            // PCM step low
        case 0x04F3:
            // PCM step high
        case 0x04F4:
            // PCM loop start low
        case 0x04F5:
            // PCM loop start high
        case 0x04F6:
            // PCM start address
        case 0x04F7:
            // PCM control
        case 0x04F8:
            // PCM channel enable
            m_audio->Synchronize(context.clocks);
            m_audio->WritePCM((u16)(port - 0x04F0), value);
            break;
        case 0x05C0:
            // Expansion NMI mask
        case 0x05C2:
            // Expansion NMI status
            break;
        case 0x05C8:
            // Text VRAM written
            break;
        case 0x05CA:
            // VSYNC interrupt clear
            m_video->Write(port, value, context.clocks);
            break;
        case 0x05E0:
            // Undocumented, BIOS writes 01h
            m_system_control->Write(port, value);
            break;
        case 0x0600:
            // Keyboard data
        case 0x0602:
            // Keyboard command
        case 0x0604:
            // Keyboard interrupt
            m_keyboard->Write(port, value, context.clocks);
            break;
        case 0x0800:
            // Printer data
        case 0x0802:
            // Printer control
        case 0x0804:
            // Printer interrupt enable
            break;
        case 0x0A00:
            // Serial transmit data
        case 0x0A02:
            // Serial command
        case 0x0A04:
            // Serial modem status
        case 0x0A06:
            // Serial interrupt reason
        case 0x0A08:
            // Serial interrupt control
        case 0x0A0A:
            // Serial modem control
            break;
        case 0x0C30:
            // SCSI data
        case 0x0C32:
            // SCSI control
            break;
        case 0xFD90:
            // Palette index
        case 0xFD92:
            // Palette blue
        case 0xFD94:
            // Palette red
        case 0xFD96:
            // Palette green
        case 0xFD98:
            // FM-R digital palette 0
        case 0xFD99:
            // FM-R digital palette 1
        case 0xFD9A:
            // FM-R digital palette 2
        case 0xFD9B:
            // FM-R digital palette 3
        case 0xFD9C:
            // FM-R digital palette 4
        case 0xFD9D:
            // FM-R digital palette 5
        case 0xFD9E:
            // FM-R digital palette 6
        case 0xFD9F:
            // FM-R digital palette 7
        case 0xFDA0:
            // CRT output control
            m_video->Write(port, value, context.clocks);
            break;
        case 0xFF81:
            // FM-R plane mask
        case 0xFF82:
            // FM-R display mode
        case 0xFF83:
            // FM-R page select
        case 0xFF94:
            // Kanji ROM code high
        case 0xFF95:
            // Kanji ROM code low
        case 0xFF97:
            // Kanji ROM row advance
        case 0xFF98:
            // Buzzer off
        case 0xFF99:
            // ANK font window
            m_video->WriteFMRRegister(port & 0x0FFF, value);
            break;
        default:
            // CMOS RAM, even ports
            if ((port & 0xF001) == 0x3000)
            {
                m_memory->WriteCMOS((port & 0x0FFF) >> 1, value);
                break;
            }

            Debug("Unknown IO write at %04X, value=%02X", port, value);
            break;
    }
}
