# FM Towns — I/O Port Map (Model 1/2)

## 1. Overview

The 80386 I/O space is 64 KB (ports 0000h–FFFFh), accessed with IN, OUT, INS and OUTS. FM Towns devices are
sparsely decoded byte registers, mostly on even addresses (the PIC, for example, has its A0 input wired to CPU
address bit 1).

- A 16-bit or 32-bit access covers consecutive port addresses, one byte each: a word write to 0442h writes the CRTC
  data registers 0442h and 0443h. A byte register does not become a word register because the access is wider.
- Several reads have side effects (status reads that clear flags, FIFOs that advance). The debugger's I/O PORTS
  memory area reads ports without side effects and shows `??` for a port it does not decode or cannot read
  passively.
- The ports differ from the IBM PC: 0020h is the reset register (not a PIC), 00A0h initializes the DMA controller,
  0070h/0080h drive the MSM58321 clock, and the CMOS RAM is at 3000h–3FFEh.

Names below are the ones the debugger uses in the disassembler, the Memory Workspace and MCP tools.

## 2. Port Map

### Interrupt Controllers (8259A x2)

| Port | Name | Function |
|---|---|---|
| 0000h | PIC_M_CMD | PIC master ICW1/OCW2/OCW3, IRR/ISR read |
| 0002h | PIC_M_DATA | PIC master ICW2-ICW4, IMR |
| 0010h | PIC_S_CMD | PIC slave ICW1/OCW2/OCW3, IRR/ISR read |
| 0012h | PIC_S_DATA | PIC slave ICW2-ICW4, IMR |

### System Control

| Port | Name | Function |
|---|---|---|
| 0020h | SYS_RESET | Reset cause read, reset and write protect |
| 0022h | SYS_POWER | Power off |
| 0030h | MACHINE_ID_LO | Machine ID low |
| 0031h | MACHINE_ID_HI | Machine ID high |
| 0032h | SERIAL_ROM | Serial ID ROM |

### Timers (8253 x2)

| Port | Name | Function |
|---|---|---|
| 0040h | PIT0_COUNT0 | PIT counter 0, interval timer |
| 0042h | PIT0_COUNT1 | PIT counter 1, I/O timeout |
| 0044h | PIT0_COUNT2 | PIT counter 2, buzzer |
| 0046h | PIT0_CONTROL | PIT counters 0-2 control |
| 0050h | PIT1_COUNT3 | PIT counter 3 |
| 0052h | PIT1_COUNT4 | PIT counter 4, RS-232C baud rate |
| 0054h | PIT1_COUNT5 | PIT counter 5 |
| 0056h | PIT1_CONTROL | PIT counters 3-5 control |
| 0060h | TIMER_CONTROL | Timer interrupt status and control, buzzer |

### Clock (MSM58321)

| Port | Name | Function |
|---|---|---|
| 0070h | RTC_DATA | RTC data |
| 0080h | RTC_COMMAND | RTC command |

### DMA Controller (uPD71071)

| Port | Name | Function |
|---|---|---|
| 00A0h | DMA_INIT | DMA initialize |
| 00A1h | DMA_CHANNEL | DMA channel select |
| 00A2h | DMA_COUNT_LO | DMA count low |
| 00A3h | DMA_COUNT_HI | DMA count high |
| 00A4h | DMA_ADDRESS_0 | DMA address bits 0-7 |
| 00A5h | DMA_ADDRESS_1 | DMA address bits 8-15 |
| 00A6h | DMA_ADDRESS_2 | DMA address bits 16-23 |
| 00A7h | DMA_ADDRESS_3 | DMA address bits 24-31 |
| 00A8h | DMA_DEVICE_LO | DMA device control low |
| 00A9h | DMA_DEVICE_HI | DMA device control high |
| 00AAh | DMA_MODE | DMA mode control |
| 00ABh | DMA_STATUS | DMA status |
| 00ACh | DMA_TEMP_LO | DMA temporary low |
| 00ADh | DMA_TEMP_HI | DMA temporary high |
| 00AEh | DMA_REQUEST | DMA software request |
| 00AFh | DMA_MASK | DMA mask |

### Floppy Disk Controller (MB8877)

| Port | Name | Function |
|---|---|---|
| 0200h | FDC_STATUS | FDC status read, command write |
| 0202h | FDC_TRACK | FDC track |
| 0204h | FDC_SECTOR | FDC sector |
| 0206h | FDC_DATA | FDC data |
| 0208h | FDC_DRIVE_CONTROL | FDC drive status read, drive control write |
| 020Ch | FDC_DRIVE_SELECT | FDC drive select |
| 020Eh | FDC_DRIVE_SWITCH | FDC drive switch |

### Video and Memory Mapping

| Port | Name | Function |
|---|---|---|
| 0400h | VIDEO_RESOLUTION | Resolution status |
| 0404h | FMR_VRAM_MAP | FM-R VRAM mapping |
| 0440h | CRTC_ADDRESS | CRTC register index |
| 0442h | CRTC_DATA_LO | CRTC data low |
| 0443h | CRTC_DATA_HI | CRTC data high |
| 0448h | VIDEO_OUT_ADDRESS | Video output control register index |
| 044Ah | VIDEO_OUT_DATA | Video output control data |
| 044Ch | VIDEO_STATUS | Digital palette and sprite status |
| 0450h | SPRITE_ADDRESS | Sprite controller register index |
| 0452h | SPRITE_DATA | Sprite controller data |
| 0458h | VRAM_MASK_ADDRESS | VRAM write mask register index |
| 045Ah | VRAM_MASK_LO | VRAM write mask low |
| 045Bh | VRAM_MASK_HI | VRAM write mask high |
| 0480h | SYS_ROM_MAP | Boot ROM and dictionary windows |
| 0484h | DIC_ROM_BANK | Dictionary ROM bank |
| 048Ah | MEMCARD_STATUS | Memory card status |

### CD-ROM Controller

| Port | Name | Function |
|---|---|---|
| 04C0h | CDROM_MASTER | CD-ROM master status and control |
| 04C2h | CDROM_COMMAND | CD-ROM status read, command write |
| 04C4h | CDROM_DATA | CD-ROM data read, parameter write |
| 04C6h | CDROM_TRANSFER | CD-ROM transfer control |
| 04CCh | CDROM_SUBCODE | CD-ROM subcode status |
| 04CDh | CDROM_SUBCODE_DATA | CD-ROM subcode data |

### Pads and Sound

| Port | Name | Function |
|---|---|---|
| 04D0h | PAD_A | Game port A |
| 04D2h | PAD_B | Game port B |
| 04D5h | SOUND_MUTE | Sound mute |
| 04D6h | PAD_OUTPUT | Game port output control |
| 04D8h | FM_ADDRESS_0 | YM3438 status read, part 0 address write |
| 04DAh | FM_DATA_0 | YM3438 part 0 data |
| 04DCh | FM_ADDRESS_1 | YM3438 part 1 address |
| 04DEh | FM_DATA_1 | YM3438 part 1 data |
| 04E0h | EVOL1_DATA | Electronic volume 1 data |
| 04E1h | EVOL1_COMMAND | Electronic volume 1 command |
| 04E2h | EVOL2_DATA | Electronic volume 2 data |
| 04E3h | EVOL2_COMMAND | Electronic volume 2 command |
| 04E7h | ADC_DATA | ADC sample data |
| 04E8h | ADC_READY | ADC sample ready |
| 04E9h | SOUND_IRQ_CAUSE | Sound interrupt cause |
| 04EAh | PCM_IRQ_MASK | PCM interrupt mask |
| 04EBh | PCM_IRQ_CAUSE | PCM interrupt cause |
| 04ECh | SOUND_LED_MUTE | LED and output mute |
| 04F0h | PCM_ENV | RF5C68 envelope |
| 04F1h | PCM_PAN | RF5C68 pan |
| 04F2h | PCM_FD_LO | RF5C68 step low |
| 04F3h | PCM_FD_HI | RF5C68 step high |
| 04F4h | PCM_LS_LO | RF5C68 loop start low |
| 04F5h | PCM_LS_HI | RF5C68 loop start high |
| 04F6h | PCM_ST | RF5C68 start address |
| 04F7h | PCM_CONTROL | RF5C68 control |
| 04F8h | PCM_CHANNEL_ON | RF5C68 channel enables |

### Miscellaneous

| Port | Name | Function |
|---|---|---|
| 05C0h | EXP_NMI_MASK | Expansion NMI mask |
| 05C2h | EXP_NMI_STATUS | Expansion NMI status |
| 05C8h | TVRAM_WRITTEN | Text VRAM written |
| 05CAh | VSYNC_IRQ_CLEAR | VSYNC interrupt clear |
| 05E0h | RAM_WAIT | Written 01h by the Model 1/2 BIOS at boot |

### Keyboard

| Port | Name | Function |
|---|---|---|
| 0600h | KB_DATA | Keyboard data |
| 0602h | KB_STATUS | Keyboard status read, command write |
| 0604h | KB_IRQ | Keyboard interrupt |

### Printer

| Port | Name | Function |
|---|---|---|
| 0800h | PRN_DATA | Printer data, status 1 |
| 0802h | PRN_CONTROL | Printer control, status 2 |
| 0804h | PRN_IRQ | Printer interrupt enable |

### RS-232C

| Port | Name | Function |
|---|---|---|
| 0A00h | SIO_DATA | RS-232C data |
| 0A02h | SIO_STATUS | RS-232C status read, command write |
| 0A04h | SIO_MODEM_STATUS | RS-232C modem status |
| 0A06h | SIO_IRQ_CAUSE | RS-232C interrupt cause |
| 0A08h | SIO_IRQ_CONTROL | RS-232C interrupt control |
| 0A0Ah | SIO_MODEM_CONTROL | RS-232C modem control |

### SCSI

| Port | Name | Function |
|---|---|---|
| 0C30h | SCSI_DATA | SCSI data |
| 0C32h | SCSI_STATUS | SCSI status read, control write |

### Palettes and CRT Output

| Port | Name | Function |
|---|---|---|
| FD90h | PAL_INDEX | Palette index |
| FD92h | PAL_BLUE | Palette blue |
| FD94h | PAL_RED | Palette red |
| FD96h | PAL_GREEN | Palette green |
| FD98h | DPAL_0 | FM-R digital palette 0 |
| FD99h | DPAL_1 | FM-R digital palette 1 |
| FD9Ah | DPAL_2 | FM-R digital palette 2 |
| FD9Bh | DPAL_3 | FM-R digital palette 3 |
| FD9Ch | DPAL_4 | FM-R digital palette 4 |
| FD9Dh | DPAL_5 | FM-R digital palette 5 |
| FD9Eh | DPAL_6 | FM-R digital palette 6 |
| FD9Fh | DPAL_7 | FM-R digital palette 7 |
| FDA0h | CRT_OUTPUT | Sync status read, CRT output control write |

### FM-R Compatible Registers

| Port | Name | Function |
|---|---|---|
| FF81h | FMR_PLANE_MASK | FM-R plane access mask |
| FF82h | FMR_DISPLAY | FM-R display planes and page |
| FF83h | FMR_PAGE | FM-R access page |
| FF84h | FMR_LIGHT_PEN | FM-R light pen status |
| FF86h | FMR_SYNC | FM-R sync status |
| FF94h | KANJI_HI | Kanji ROM code high, status read |
| FF95h | KANJI_LO | Kanji ROM code low |
| FF96h | KANJI_LEFT | Kanji ROM pattern left |
| FF97h | KANJI_RIGHT | Kanji ROM pattern right, row advance |
| FF98h | BUZZER | Buzzer on read, off write |
| FF99h | FMR_ANK | ANK font window |
| FFA0h | FMR_LOGIC | FM-R logical operation status |

### CMOS RAM

| Port | Name | Function |
|---|---|---|
| 3000h–3FFEh, even | CMOS | First 2 KB of the 8 KB CMOS RAM, index = (port - 3000h) / 2 |

## 3. Interrupt Lines

| IRQ | PIC input | Source |
|---:|---|---|
| 0 | Master IR0 | Timer (PIT counters 0 and 1, enabled through 0060h) |
| 1 | Master IR1 | Keyboard |
| 2 | Master IR2 | RS-232C |
| 3 | Master IR3 | Expansion RS-232C |
| 4 | Master IR4 | I/O expansion |
| 5 | Master IR5 | I/O expansion |
| 6 | Master IR6 | Floppy disk controller |
| 7 | Master IR7 | Slave PIC |
| 8 | Slave IR0 | SCSI |
| 9 | Slave IR1 | CD-ROM controller |
| 10 | Slave IR2 | I/O expansion |
| 11 | Slave IR3 | VSYNC (cleared by a write to 05CAh) |
| 12 | Slave IR4 | Printer |
| 13 | Slave IR5 | Sound: YM3438 timers and RF5C68 (cause in 04E9h) |
| 14 | Slave IR6 | I/O expansion |
| 15 | Slave IR7 | Reserved |

Master IR7 carries the slave, so there is no device on IRQ7. NMI comes from the keyboard (RAS) and from the I/O
expansion bus, which 05C0h can mask.

The Model 1/2 BIOS programs the master to vectors 40h–47h and the slave to 48h–4Fh (ICW2), so IRQ9 arrives as
vector 49h. Software may reprogram both.

## 4. DMA Channels

| Channel | Device |
|---:|---|
| 0 | Floppy disk controller |
| 1 | SCSI |
| 2 | Printer |
| 3 | CD-ROM controller |

An optional second DMA controller at 00B0h–00BFh mirrors the 00A0h–00AFh layout for the expansion slot.

## 5. Timer Counters

| Counter | Port | Clock | Use |
|---:|---|---|---|
| 0 | 0040h | 307.2 kHz | Interval timer, IRQ0 when enabled in 0060h bit 0 |
| 1 | 0042h | 307.2 kHz | I/O timeout, IRQ0 when enabled in 0060h bit 1 |
| 2 | 0044h | 307.2 kHz | Buzzer tone, heard while 0060h bit 2 (SOUND) is set |
| 3 | 0050h | — | Reserved |
| 4 | 0052h | 1.2288 MHz | RS-232C baud rate |
| 5 | 0054h | — | Reserved |

0060h read: bit 0 counter 0 timeout latched, bit 1 counter 1 timeout latched, bit 2 counter 0 enable, bit 3
counter 1 enable, bit 4 SOUND.
0060h write: bit 7 clears the counter 0 latch, bit 0 counter 0 enable, bit 1 counter 1 enable, bit 2 SOUND.
Loading counter 1 clears its latch. Counters 3 and 5 have no assigned clock, and no counter has a gate input.
