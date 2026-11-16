# FM Towns — Input

## 1. Overview

Two identical game connectors share the pad and mouse hardware. Each carries the four directions (FWD, BACK, LEFT,
RIGHT), two trigger lines (TRIG1, TRIG2), a COM line and the supply. Software drives the triggers and COM through one
output register and reads one input byte per connector. Everything is active low. The keyboard is a separate
interface (section 6).

Geartowns plugs one device in each connector:

| Device | MCP name | Buttons |
|---|---|---|
| None | `none` | |
| Original gamepad | `original_gamepad` | Up, Down, Left, Right, Run, Select, A, B |
| Marty gamepad | `marty_gamepad` | The same plus Zoom |
| Six-button gamepad | `six_button_gamepad` | The same plus C, X, Y, Z |
| Mouse | `mouse` | Left (A) and right (B) buttons, X and Y motion |

---

## 2. Ports

| Port | Read | Write |
|---|---|---|
| 04D0h | Connector 1 input | Ignored |
| 04D2h | Connector 2 input | Ignored |
| 04D6h | FFh | Output for both connectors |

Input (04D0h and 04D2h):

| Bit | Line | Meaning |
|---:|---|---|
| 7 | | Not driven, reads 0. The six-button pad reads 1 |
| 6 | COM | The COM output echoed. The Marty pad uses it for Zoom instead |
| 5 | TRIG2 | B button, 0 when pressed |
| 4 | TRIG1 | A button, 0 when pressed |
| 3 | RIGHT | 0 when pressed |
| 2 | LEFT | 0 when pressed |
| 1 | BACK | Down, 0 when pressed |
| 0 | FWD | Up, 0 when pressed |

Output (04D6h write):

| Bit | Line |
|---:|---|
| 7-6 | Zero |
| 5 | COM, connector 2 |
| 4 | COM, connector 1 |
| 3 | TRIG2, connector 2 |
| 2 | TRIG1, connector 2 |
| 1 | TRIG2, connector 1 |
| 0 | TRIG1, connector 1 |

- **Reset:** 04D6h holds 0Fh, so COM is low and every trigger output is high.
- **Trigger outputs gate the buttons.** An input trigger bit (4 or 5) reads 0 whenever its output bit is 0, pressed or
  not. Software sets the trigger outputs high before it reads A and B. The six-button pad is the exception.
- **Reads have no side effects.** Only a COM change written to 04D6h moves a device (the mouse, section 4.4).

---

## 3. Direction Encoding

Run and Select have no lines of their own. They are direction pairs on the wire (bits 3-0, as read):

| Pad | Bits 3-0 |
|---|---|
| Right | 0111 |
| Left | 1011 |
| Down | 1101 |
| Up | 1110 |
| Run | 0011 (Left and Right together) |
| Select | 1100 (Up and Down together) |
| Nothing | 1111 |

Run forces bits 3-2 low and Select bits 1-0 low, so Up and Down held together read as Select. The sound BIOS pad
function 40h returns the same lines in another order (bits 0-5 are Down, Up, Left, Right, A, B, active low) and 41h
adds Run and Select as bits 6 and 7.

---

## 4. Devices

### 4.1 None and Original Gamepad

With nothing plugged in bits 3-0, 4 and 5 read 1, so the byte is 3Fh. The original pad puts section 3 on bits 3-0, A on
bit 4 and B on bit 5. Both echo the COM output on bit 6 and obey the trigger gating of section 2. The original pad
never reads C, X, Y, Z or Zoom, and COM changes nothing else on it.

### 4.2 Marty Gamepad

The same lines as the original pad, with no COM echo. Bit 6 reads 1 only while the COM output is high and Zoom is held,
0 otherwise. A driver that polls the pad with COM low sees the original pad.

### 4.3 Six-Button Gamepad

COM selects what bits 3-0 show:

| COM output | Bits 3-0 | Bits 5-4 | Bit 6 | Bit 7 |
|---|---|---|---|---|
| Low | Directions, Run and Select as section 3 | B, A | 0 | 1 |
| High | Bit 0 Z, bit 1 Y, bit 2 X, bit 3 C (0 when pressed) | B, A | 1 | 1 |

- A and B read the same in both phases and the trigger outputs do not gate them.
- Bit 7 reads 1 in both phases, which is how software tells this pad from the others.
- A driver reads once with COM low, writes COM high to 04D6h (1Fh for connector 1, 2Fh for connector 2, triggers
  high), reads again and writes COM low. Geartowns applies no timing to it.
- The pad is not in the Technical Data Book; this is how Geartowns reads it.

### 4.4 Mouse

A mouse reports motion as two signed 8-bit deltas, X then Y, in two's complement. Right and down are negative, left
and up positive. The driver gets them four bits at a time and strobes COM to move on.

| Input bit | Meaning |
|---:|---|
| 3-0 | The nibble of the current strobe |
| 4 | Left button, 0 when pressed (TRIG1) |
| 5 | Right button, 0 when pressed (TRIG2) |
| 6 | The COM output echoed |
| 7 | 0 |

The buttons do not depend on the strobe. The nibbles come one per COM change, in this order: X bits 7-4, X bits 3-0,
Y bits 7-4, Y bits 3-0.

How Geartowns models it:

- **Every change of a connector's COM output is an edge for it,** rising or falling, including the write that sets COM
  high at the start of a poll if it changes the level. A write to 04D6h that leaves that COM bit alone is not.
- **The first edge of a packet takes the motion:** the counts pending since the last packet, each axis limited to -127
  to 127 because Towns OS negates the byte. What does not fit stays pending for the next packet. The four nibbles then
  come from that sample; the fifth edge starts the next packet and samples again.
- **A pause restarts the packet.** An edge more than 1 ms after the last one, or more than 150 µs after the second one
  (the T2 timeout), counts as the first edge of a new packet, and nibbles not yet read are lost with their counts.
- **No output latency.** The nibble is readable as soon as the edge is written. A real mouse takes up to 80 µs for the
  first nibble and 40 µs for the others, and a driver samples between 80 and 100 µs and between 40 and 50 µs after a
  strobe. Reading earlier or later gives the same value here.
- **A mouse that is never strobed** keeps its counts pending, up to a limit of 3FFFFFFFh per axis.
- **Host motion:** the host mouse feeds the first connector set to mouse, scaled by the Mouse Sensitivity option
  (1-15, default 5, divided by 6), with the left button as A and the right as B. Changing a connector's device
  discards its pending motion.
- **Who reads it:** the Towns OS mouse BIOS polls from the YM3438 Timer B interrupt, every second tick (about 20 ms)
  when sound is registered and every 23 ms when the mouse runs alone. Games that poll themselves strobe whenever they
  like.

---

## 5. Choosing the Device

Each connector has a setting, `auto` or one of the devices above, and the device in use follows from it:

1. A setting other than `auto` wins. The `source` is `explicit`.
2. With `auto`, the game database entry for the loaded disc or floppy may name the device for that connector (none,
   original pad, Marty pad, six-button pad or mouse). The `source` is `database`. Super Street Fighter II, for example,
   has the six-button pad on both connectors, and a mouse title has it on the connector it uses. A database mouse is
   skipped if another connector already has a mouse.
3. Otherwise connector 1 gets the original pad (`default`) and connector 2 the mouse (`default`), or the original pad
   (`fallback`) if a mouse is already in use.
4. On the Marty model an original pad that was not set explicitly becomes the Marty pad.

`auto` never leaves two mice, though two explicit settings can. The choice is made again when media or settings change.
When the database wants a device other than the one in use, `get_input_state` reports it as `database_type`.

---

## 6. Keyboard

The keyboard has its own ports (0600h-0604h), scan byte pairs and commands; see the system resource, section 7. Host
keys reach it through the key map, which can be changed per game. The mouse and the pads never go through it.

---

## 7. Driving it from MCP

- `get_input_state` shows each connector (`players` 1-2) with `type`, `setting`, `source`, `database_type` when the
  database disagrees, and `pressed` (the button names), and `keys_held` for the keyboard. It does not show the output
  register or the mouse counts: read 04D0h and 04D2h with `read_memory` on the I/O PORTS area (no side effects), and
  watch 04D6h in the trace.
- `controller_set_type` (`player`, `type`: `auto`, `none`, `original_gamepad`, `marty_gamepad`, `six_button_gamepad`,
  `mouse`) changes the setting and the device at once.
- `controller_button` (`player`, `button` up, down, left, right, select, run, A, B, C, X, Y, Z or zoom, `action` `press`,
  `release` or `press_and_release`) is ORed with the host pad's buttons. A button the device does not read changes
  nothing on the lines. On a mouse the directions move 4 counts per frame held or per tap, and A and B are the buttons.
- `controller_macro` runs a timed sequence: `player` (1-2), `device` (`pad`, the default, or `keyboard`), `tap_frames`
  (1-60, default 3) and `commands`, an ordered array of objects each with one of `tap`, `press`, `release` (a button as
  `controller_button` takes it, or a key name as `keyboard_key` takes it) or `wait` (1-1000 frames), optionally with its
  own `player` or `device`. A tap holds `tap_frames` and leaves the same gap after. On a mouse connector it moves and
  clicks as `controller_button` does. Example:
  `[{"tap":"run"},{"wait":30},{"press":"right"},{"wait":60},{"release":"right"},{"tap":"RETURN","device":"keyboard"}]`.
- `mouse_move` goes to a screenshot pixel or by `dx` and `dy` counts, `mouse_click` presses a button (`click`,
  `double_click`, `press`, `release` or `drag`), and `mouse_get_cursor` shows where the pointer was last measured.
  `mouse_move` and `mouse_click` fail with "the software isn't reading the mouse" after 60 frames of counts nobody
  strobes. All three take `player`, by default the first connector set to mouse.
- `keyboard_key`, `keyboard_type` and `host_key` type; `get_key_map`, `set_key_binding`, `set_key_mapping`,
  `open_key_mapper` and `close_key_mapper` manage the host key map; `get_keyboard_status` shows the interface.
- `write_io_port` writes 04D6h as a driver does, so COM edges strobe the mouse and the trace sees them. Break on a
  poll with `set_breakpoint` in the `io` space on 04D0h, 04D2h or 04D6h.
- Trace `input.reads` (each read: connector, device, the byte, the four lines, A, B and COM), `input.writes` (each 04D6h
  write with TRIG A, TRIG B, COM A, COM B and the previous value) and `input.changes` (pad and mouse button changes).
  The keyboard has `keyboard.keys`, `keyboard.reads` and `keyboard.commands`.
