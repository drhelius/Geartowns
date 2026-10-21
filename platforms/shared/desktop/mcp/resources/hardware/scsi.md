# FM Towns — SCSI Hard Disks

## 1. Overview

The SCSI host interface is built into the 1F/2F and every later desktop model. The Model 1/2 takes it on an optional
card, which Geartowns fits while a hard disk is attached, and the Marty has none. The host is ID 7 and the targets,
hard disks here, use IDs 0-6. The interface interrupts on IRQ8 (slave IR0, vector 48h with the BIOS setup) and
moves data through DMA channel 1.

At power-on the BIOS pulses `RST` and stores "BUSY clear" in CMOS byte 562h. With no interface 0C32h reads FFh, BUSY
looks set and 562h stays 0, and the boot device scan then skips SCSI.

---

## 2. Ports

| Port | Read | Write |
|---|---|---|
| 0C30h | Data: in a phase with REQ set, takes the byte and acknowledges it | Data: same |
| 0C32h | Status | Control |

| Bit | Control (write) | Status (read) |
|---:|---|---|
| 7 | WEN: enables the host's drivers, needed to send data and control the bus | REQ |
| 6 | IMSK: 1 lets INT reach IRQ8 | I/O |
| 5 | RMSK (UX, CX and later, ignored) | MSG |
| 4 | ATN: asks for a Message Out phase | C/D |
| 3 | W/B word DMA (CX, ignored) | BUSY |
| 2 | SEL: selection, with the IDs on the data bus | EX (reads 0) |
| 1 | DMAE: data phases request DMA on channel 1 | INT |
| 0 | RST: resets the bus and every target | PERR (reads 0) |

INT is set while REQ is up in the Command, Status and Message phases; IRQ8 follows INT and IMSK. Towns OS writes
C2h and C6h (IMSK set) and serves IRQ8, while the BIOS polls with IMSK clear.

| C/D | MSG | I/O | Phase |
|---:|---:|---:|---|
| 0 | 0 | 0 | Data Out |
| 0 | 0 | 1 | Data In |
| 1 | 0 | 0 | Command |
| 1 | 0 | 1 | Status |
| 1 | 1 | 0 | Message Out |
| 1 | 1 | 1 | Message In |

---

## 3. A Command

The BIOS disk service does:

1. Wait for BUSY to clear. Reset the DMAC (00A0h = 03h), select channel 1 and write 20h to device control 00A8h.
2. Write 80h plus the target's ID bit to 0C30h and 86h (WEN, SEL, DMAE) to 0C32h. Wait for BUSY (a target with that
   ID answers in about 5 µs; none never does), then write 82h to drop SEL.
3. While BUSY, on REQ dispatch on status bits 6-4: Command sends the next CDB byte, Status and Message In read one
   byte each. A data phase programs channel 1 (mode 44h for Data In, 48h for Data Out, the count up to the next
   64 KB boundary) and waits for terminal count or a phase change.
4. Status 00h GOOD or 02h CHECK CONDITION, then Message In 00h COMMAND COMPLETE and the bus goes free.

Without DMAE, data moves one byte per 0C30h access while REQ is set. With ATN set during selection the target goes
to Message Out first: IDENTIFY (80h + LUN) selects the LUN, ABORT and BUS DEVICE RESET free the bus, NO OPERATION is
taken, and other messages get MESSAGE REJECT.

---

## 4. Hard Disk Commands

| Opcode | Command | Notes |
|---|---|---|
| 00h | TEST UNIT READY | |
| 01h | REZERO UNIT | |
| 03h | REQUEST SENSE | 18-byte fixed format (70h), cut to the allocation length; 0 returns 4 bytes |
| 04h | FORMAT UNIT | Accepted, the data is kept |
| 08h, 28h | READ(6), READ(10) | READ(6): 21-bit LBA, 0 blocks means 256 |
| 0Ah, 2Ah | WRITE(6), WRITE(10) | DATA PROTECT on a write-protected disk |
| 0Bh, 2Bh | SEEK(6), SEEK(10) | |
| 12h | INQUIRY | Direct-access device, SCSI-1 CCS, "GEARTOWN" "HARD DISK" "1.00" |
| 15h | MODE SELECT(6) | Parameters taken and dropped |
| 16h, 17h | RESERVE, RELEASE | |
| 1Ah | MODE SENSE(6) | Pages 00h and 3Fh: header with the write protect bit, 8-byte block descriptor |
| 1Bh | START STOP UNIT | |
| 1Dh | SEND DIAGNOSTIC | Parameters taken and dropped |
| 1Eh | PREVENT ALLOW MEDIUM REMOVAL | |
| 25h | READ CAPACITY | Last LBA and 512 |
| 2Fh | VERIFY(10) | With BytChk the host's data is compared |

Blocks are 512 bytes. A LUN other than 0 and an unknown opcode end with CHECK CONDITION.

| Sense key | Code | Cause |
|---|---|---|
| 03h MEDIUM ERROR | 11h | The image could not be read |
| 05h ILLEGAL REQUEST | 20h | Unknown opcode |
| 05h ILLEGAL REQUEST | 21h | LBA out of range |
| 05h ILLEGAL REQUEST | 24h | Invalid field in the CDB |
| 05h ILLEGAL REQUEST | 25h | LUN not supported |
| 07h DATA PROTECT | 27h | Write protected |
| 0Eh MISCOMPARE | 1Dh | VERIFY found different data |

---

## 5. Booting and Disk Layout

- LBA 0 holds an IPL4 boot sector. LBA 1 holds the partition table: up to 10 entries of 30h bytes from offset 20h,
  FFh in an entry's first byte marks the boot partition, and the dword at +2 is its start LBA.
- The BIOS enters the IPL with BL = 1 (SCSI) and BH = the unit. A bootable CD boots before the hard disk.
- Towns OS registers its hard disk drives in the CMOS, not on the disk. A disk installed on one model stops on
  another until it is registered there (Towns OS V2.1: TownsMENU Settings menu, Drive Configuration, Add Drive).
- Towns OS V2.1 formats its partitions with 2048-byte logical sectors.

## 6. Images

Hard disk images are raw 512-byte sectors with no header (`.hdd`, `.h0`-`.h6`). `read_hard_disk_sector` reads one
as the machine sees it, `list_hard_disks` shows the attached images, and `get_scsi_status` the interface and each
disk's sense data.
