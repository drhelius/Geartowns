# Geartowns BIOS and Firmware

Geartowns needs the ROMs of the FM Towns it emulates: the system ROM (the BIOS), the OS ROM, the font ROM and the dictionary ROM, plus a few optional ones. They are not included with Geartowns; dump them from your own machine.

Each model runs on a set of these ROMs. Some models have a set of their own, others run on the set of a close sibling.

## Quick Start

- **Desktop**: put each set in its own subfolder or MAME zip inside one folder (for example `fmtowns/`, `fmtownsux.zip`, `fmtownshr/`, `fmtmarty.zip`), then choose that folder in **Emulator → Firmware...** as the **Default Folder**.
- **Libretro**: put the same subfolders or zips in a `geartowns` folder inside the frontend's system directory. See the [libretro documentation](https://docs.libretro.com/library/geartowns/).
- **Only the Model 1/2**: the four files `FMT_SYS.ROM`, `FMT_DOS.ROM`, `FMT_FNT.ROM` and `FMT_DIC.ROM` directly in the folder are enough.

## Models and Sets

| Model                    | Runs on                         | Notes                                     |
|--------------------------|---------------------------------|-------------------------------------------|
| FM Towns (Model 1/2)     | Model 1/2 set                   | Or the Free FM Towns Project ROMs         |
| FM Towns 1F/2F           | UX set                          | No dump of its own                        |
| FM Towns 10F/20F         | UX set                          | No dump of its own                        |
| FM Towns II CX           | UX set                          | No dump of its own                        |
| FM Towns II UX           | UX set                          |                                           |
| FM Towns II UG           | UX set                          | No dump of its own                        |
| FM Towns II HG           | HR set, or else the V10 set     | No dump of its own                        |
| FM Towns II HR           | HR set                          |                                           |
| FM Towns Marty           | Marty set or Marty 2 set        |                                           |

The other FM Towns II models (UR, ME, MA, MX, MF/Fresh and HC) are not emulated yet. The V10 set is MAME's FM Towns II FreshTV set, made for the MF/Fresh, and the HG runs on it. MAME's MX set is not recognized.

**How a set is chosen:**

- A set is recognized by its system ROM, whatever folder or zip it is in and whatever it is called.
- A folder or zip with a required ROM missing or of the wrong size is skipped.
- For each model, Geartowns takes the set made for it first, then a set it runs on (as in the table above), then unrecognized ROMs, and last a set made for another model. Those last two load with a warning.
- **Model** on Auto doesn't pick a model whose only firmware is a set made for another model. When a game in the database needs a model with no firmware, Auto picks another one and warns that the game needs that model.
- A folder set for one model alone (**Own Folder** in the desktop) is used as it is, even with unrecognized ROMs.

## Where to Put the Files

### Desktop

Open **Emulator → Firmware...**:

- **Model**: the model whose firmware the window shows. The list shows the status of each model.
- **Default Folder**: searched for every model, together with its subfolders and the MAME zips in it.
- **Own Folder**: a folder for the selected model only, used instead of the default folder. **Use Default** goes back to the default folder.
- **Refresh**: search the folders again after adding or removing files.

The status reads *Ready* with the set in use, or *Own folder*. It is green for the model's own set or folder, amber for a sibling's set (*compatible*), unrecognized ROMs (*unknown ROMs*) or a set made for another model (*for another model*), and red when there is no firmware for the model (*Not found*). Content doesn't start without the required ROMs of the model.

### Libretro

Place the files in a `geartowns` folder inside the frontend's system directory, in a subfolder of it such as `geartowns/fmtowns/`, or as MAME zips such as `geartowns/fmtowns.zip`. The system directory itself is searched the same way when the `geartowns` folder only has a set made for another model, or none for the model a game in the database needs. Content does not load without firmware for the model, and the message names the model.

## Files

| File                                    | Contents                                     | Size           | Needed                                   |
|-----------------------------------------|----------------------------------------------|----------------|------------------------------------------|
| `FMT_SYS.ROM`                           | System ROM: boot code and BIOS               | 256 KB         | Required                                 |
| `FMT_DOS.ROM`                           | OS ROM: the DOS in ROM                       | 512 KB         | Required                                 |
| `FMT_FNT.ROM`                           | Font ROM: the 16-dot kanji font              | 256 KB         | Required                                 |
| `FMT_DIC.ROM`                           | Dictionary ROM: the kana-kanji dictionary    | 512 KB         | Required                                 |
| `FMT_F20.ROM`                           | 20-dot font ROM                              | 512 KB         | Optional, a blank ROM is used without it |
| `MYTOWNS.ROM`                           | Serial ROM: the machine's ID and serial      | 32 bytes       | Optional, generated without it           |
| `MAR_EX0.ROM` to `MAR_EX3.ROM`          | Marty extension ROM                          | 4 x 512 KB     | Marty only, needed to boot               |

- File names are accepted in any case.
- MAME's names are accepted too: `fmt_sys_a.rom` and `fmthr_sys.rom` for the system ROM, `fmt_dos_a.rom` for the OS ROM, `fmt_f20_a.rom` for the 20-dot font, and `mytownsux.rom`, `mytownsm2.rom`, `mytownsftv.rom` or `mytownshr.rom` for the serial ROM. MAME's Marty `mrom.m36` and `mrom.m37` hold the whole Marty firmware in two files.
- Without its extension ROM the Marty stops at its logo, although the firmware loads.

## Sets

CRC32 and md5 of each file. Files with no md5 here are known only by their CRC32.

### Model 1/2 set (MAME `fmtowns`)

| File        | Description               | Size   | CRC32    | md5                              |
|-------------|---------------------------|--------|----------|----------------------------------|
| fmt_sys.rom | System ROM - Required     | 256 KB | 53319E23 | feaf8c5675151e00cfe3ad27673bff29 |
| fmt_dos.rom | OS ROM - Required         | 512 KB | 112872EE | 0585b19930d4a7f4c71bcc8a33746588 |
| fmt_fnt.rom | Font ROM - Required       | 256 KB | 955C6B75 | eb44f2093f51eb7159f03e170b13af76 |
| fmt_dic.rom | Dictionary ROM - Required | 512 KB | B314C659 | 34847786d7de94b5d1133c956ab1d75d |

The set has no 20-dot font or serial ROM; a blank font and a generated serial ROM are used. The Free FM Towns Project's replacement ROMs are recognized too, and run the Model 1/2.

### UX set (MAME `fmtownsux`)

Also used by the 1F/2F, 10F/20F, CX and UG.

| File          | Description               | Size     | CRC32    | md5                              |
|---------------|---------------------------|----------|----------|----------------------------------|
| fmt_sys_a.rom | System ROM - Required     | 256 KB   | 92F3FA67 | 90b5e01d42aaa93e8f4503a5e94e120b |
| fmt_dos_a.rom | OS ROM - Required         | 512 KB   | 22270E9F | 03c8fac9a5f5f5f35fb5de5a5d0d018f |
| fmt_fnt.rom   | Font ROM - Required       | 256 KB   | DD6FD544 | b91300e55b70227ce98b59c5f02fa8dd |
| fmt_dic.rom   | Dictionary ROM - Required | 512 KB   | 82D1DAA2 | 8fa4e553f28cfc0c30a0a1e589799942 |
| mytownsux.rom | Serial ROM - Optional     | 32 bytes | 5CC7E6BC | 1a15f6c1b58ec7e5f850118610a787a7 |

### HR set (MAME `fmtownshr`)

Also used by the HG.

| File          | Description               | Size     | CRC32    | md5                              |
|---------------|---------------------------|----------|----------|----------------------------------|
| fmthr_sys.rom | System ROM - Required     | 256 KB   | 8AEFF982 | ecc8c37735e591a878d750d5976d7f52 |
| fmt_dos.rom   | OS ROM - Required         | 512 KB   | 112872EE | 0585b19930d4a7f4c71bcc8a33746588 |
| fmt_fnt.rom   | Font ROM - Required       | 256 KB   | DD6FD544 | b91300e55b70227ce98b59c5f02fa8dd |
| fmt_dic.rom   | Dictionary ROM - Required | 512 KB   | 82D1DAA2 | 8fa4e553f28cfc0c30a0a1e589799942 |
| mytownshr.rom | Serial ROM - Optional     | 32 bytes | C52F0E89 | 72a96dabf0f9e5fe0bb47b08fa456815 |

### V10 set (MAME `fmtownsftv`)

MAME's FM Towns II FreshTV set. The HG's second choice, when there is no HR set.

| File           | Description                 | Size     | CRC32    | md5                              |
|----------------|-----------------------------|----------|----------|----------------------------------|
| fmt_sys.rom    | System ROM - Required       | 256 KB   | AFE4EBCF | 86fb6f7280689259f0ca839dd3dd6cde |
| fmt_dos.rom    | OS ROM - Required           | 512 KB   | 112872EE | 0585b19930d4a7f4c71bcc8a33746588 |
| fmt_fnt.rom    | Font ROM - Required         | 256 KB   | DD6FD544 | b91300e55b70227ce98b59c5f02fa8dd |
| fmt_dic.rom    | Dictionary ROM - Required   | 512 KB   | 82D1DAA2 | 8fa4e553f28cfc0c30a0a1e589799942 |
| fmt_f20.rom    | 20-dot Font ROM - Optional  | 512 KB   | 9F55A20C | ac0c7021e9bf48ca84b51ab651169a88 |
| mytownsftv.rom | Serial ROM - Optional       | 32 bytes | A961AAE7 |                                  |

### Marty set (MAME `fmtmarty`)

| File     | Description                                      | Size | CRC32    | md5                              |
|----------|--------------------------------------------------|------|----------|----------------------------------|
| mrom.m36 | OS ROM and extension ROM 0-2 - Required          | 2 MB | 9C0C060C | 6618519b2c104cf9b7e71a48381b44a9 |
| mrom.m37 | Extension ROM 3, fonts, dictionary and system ROM - Required | 2 MB | FB66BB56 | 75a5c7afb4bc8221bab8cf24db417950 |

### Marty 2 set (MAME `fmtmarty2`)

The same ROMs as the Marty set, as separate files.

| File          | Description                 | Size     | CRC32    | md5                              |
|---------------|-----------------------------|----------|----------|----------------------------------|
| fmt_sys.rom   | System ROM - Required       | 256 KB   | 937311F6 | 8053958d759487f7765d740ef3fd529f |
| fmt_dos.rom   | OS ROM - Required           | 512 KB   | 2BC2AF96 | 3f3dde5dd766b4921e5c7265df14f93d |
| fmt_fnt.rom   | Font ROM - Required         | 256 KB   | DD6FD544 | b91300e55b70227ce98b59c5f02fa8dd |
| fmt_dic.rom   | Dictionary ROM - Required   | 512 KB   | 82D1DAA2 | 8fa4e553f28cfc0c30a0a1e589799942 |
| mar_ex0.rom   | Extension ROM 0 - Required  | 512 KB   | F67540F6 | 4f43e0a04fa5eb609ad93d6498fc3ee1 |
| mar_ex1.rom   | Extension ROM 1 - Required  | 512 KB   | 99938A4B | 62c7f3fe1827af995d0c82c664611954 |
| mar_ex2.rom   | Extension ROM 2 - Required  | 512 KB   | C6783422 | a260bc140b96ee43445ca026d0cb452b |
| mar_ex3.rom   | Extension ROM 3 - Required  | 512 KB   | 4AA43E16 | 655fc5f6f661c6b28c8b877dd2e26ad1 |
| mytownsm2.rom | Serial ROM - Optional       | 32 bytes | 44F2F076 |                                  |

Unlike the Marty set, it has no 20-dot font; a blank one is used.
