# The flash of a board with 4 MB

A board with 4 MB of flash (the CYD, its ILI9342 variant and the Hosyond 4-inch) builds with a partition table of its
own since firmware 0.33.1 of those boards. It gives each of the two update slots 2,031,616 bytes where ESPHome's own
table gives 1,835,008. This page says what the table is, how a screen that is already out there gets it without
losing anything, and what to keep in mind when you change any of it. For what a screen's owner notices and what to
do when you flash a screen yourself, read [UPDATING_4MB_SCREENS.md](UPDATING_4MB_SCREENS.md).

## The two tables

ESPHome's own table for 4 MB keeps 448 KB at the end of the flash for settings. A screen keeps a few hundred bytes
there, so almost all of it is empty, and the firmware of these boards had filled the slots.

| | ESPHome's table | The wide table |
|---|---|---|
| otadata | 0x9000, 8 KB | the same |
| phy_init | 0xB000, 4 KB | the same |
| 0xC000 to 0x10000 | unused | `nvs`, the settings, 16 KB |
| first slot `app0` | 0x10000, 1,835,008 bytes | 0x10000, 2,031,616 bytes |
| second slot `app1` | 0x1D0000, 1,835,008 bytes | 0x200000, 2,031,616 bytes |
| end of the flash | `nvs`, the settings, 448 KB from 0x390000 | `nvs_more`, 64 KB from 0x3F0000, unused |

The wide table is `components/flash_layout/partitions-4mb-wide.csv`. Three things about it are deliberate:

- The settings go where ESPHome's table has nothing. A copy can be made there while the old table is still in use,
  without touching the settings the screen runs on.
- The first slot starts where it always did. A screen that runs from its first slot keeps running the same bytes when
  the table changes under it.
- 16 KB holds 378 entries of ESP-IDF's settings storage. A screen uses about a hundred: its own settings, the radio's
  calibration and a few lines of ESP-IDF's. `nvs_more` is a second settings area nothing uses yet. A later firmware
  can open it without changing the table again.

## What a screen keeps

Everything in the settings area comes along, whatever wrote it: the touch calibration, the settings of the settings
page, which way the screen hangs, the alarm panel's state, ESPHome's own values and the radio's calibration data. The
Wi-Fi name and password, the API key and the update password are not kept there. They are part of the firmware, from
the screen's own YAML, and a new table does not change them.

## How a screen gets the wide table

A new screen is flashed with it. A screen that is out there has ESPHome's table, and the component
`components/flash_layout` moves it, in one of two ways.

### Over Wi-Fi

1. A screen with ESPHome's table keeps a copy of its whole settings area at 0xC000, where the wide table looks for
   it. The place is wiped before the first copy, because another firmware may have left its own settings there. The
   copy is made when the screen starts and once more when an update begins, and it is compared with the settings in
   both directions each time, so it holds the same values and nothing else.
2. With the copy in place the screen says `widen` in its "Screen flash" sensor, a diagnostic sensor only these boards
   have.
3. Tessera Screen Manager sees the word after an update and sends the wide table with ESPHome's own tools
   (`ota: allow_partition_access: true` in the screen's YAML, `esphome upload --partition-table`). The screen writes
   the table and restarts, about seven seconds in all.
4. The screen starts with wide slots and finds its settings in place. It now says `wide`.

The screen says `widen` only when all of this holds:

- Its table is exactly ESPHome's own for 4 MB. Any other table, such as one a screen's owner wrote, is left alone.
- The copy was read back and matches.
- It runs from its first slot. From the second slot ESPHome would copy the running firmware into the first slot
  before it writes the table, which takes about a quarter of a minute on these boards, close to the limit ESPHome
  gives it. There the word is `widen_next`, and the manager installs the same firmware once more first.
- ESPHome has confirmed the running firmware. After an update the bootloader keeps a firmware on trial for a minute,
  and a restart in that minute goes back to the firmware from before. A table sent in that minute would restart the
  screen in the middle of the trial, so until then the screen says `old`.

The sensor says `old` as well when the copy could not be made or the screen's YAML lacks the line that lets a table be
replaced, and `other` for a table that is not ESPHome's own. The manager acts on `widen`, `widen_next` and `wide` only.

### Over USB

An install over USB writes the wide table straight away. When the screen ran firmware from before this, its
settings still lie in the old place, which is now the end of the second slot. The first time it starts with the wide
table, the screen wipes its new settings area, reads the old place with ESP-IDF's own settings code, opened read
only, takes over what it finds and restarts once. Nothing has written to the old place yet at that moment: an
install over USB writes the first slot only.

A value of the component's own (`flash_layout`, `done`) says that the settings are where the wide table keeps them.
It travels with the copy. A screen that starts with the wide table without it does the above once, and so does a new
screen, which finds nothing in the old place.

## What Tessera Screen Manager does

`screen_manager/app/updates.py` walks every update of a board with 4 MB like this:

1. It builds the firmware. Before a build it adds `allow_partition_access: true` to the `ota:` block of the screen's
   own YAML when it is missing (`Firmware.allow_table_update`). The line has to stand there: in a package it would
   lose against ESPHome's default in the screen's own block.
2. A screen that already says `widen` gets the table first, then the firmware.
3. Otherwise it installs the firmware, waits for the word, installs once more for `widen_next`, and sends the table
   for `widen`.
4. A screen that says `old` or `other`, or that has no such sensor, keeps the table it has. The update itself
   succeeded.

### The bridge

A screen with ESPHome's table cannot take a firmware larger than 1,835,008 bytes, and firmware from before the
component cannot take a new table. When both are true the manager goes over a bridge: `packages/bridge.yaml`, the smallest
firmware that can take the table. It has Wi-Fi, updates over Wi-Fi and the `flash_layout` component, and it is about
a third of the old slot.

The manager writes `<screen>.bridge.yaml` beside the screen's YAML with the screen's name, its `wifi:` and its `ota:`
block word for word, builds it, installs it, sends the table and installs the screen's own firmware over it. Then it
removes the file and its build again. The glass is dark for the minute or two this takes.

The bridge has no `api:` block, on purpose. Home Assistant removes the entities a device stops offering, so a bridge
that talked to Home Assistant would cost a screen all its entities. Without it Home Assistant only sees the screen
away for a moment, as during any update. The manager cannot read the bridge's state either, so it goes by what
ESPHome's upload answers. The screen itself refuses what it cannot take, without writing anything: a firmware too
large for its slot, or a table before the bridge runs.

A note in `updates.json` (`bridging`) remembers a screen that is on its bridge. It stays until the screen is back in
Home Assistant with its own firmware and has been through the minute in which ESPHome confirms that firmware: a
restart in that minute puts the firmware from before back, and from before is the bridge. When a round stops
anywhere, for example because the app restarts or the power goes, the manager picks the screen up five minutes
later from whatever step it is at, with or without the nightly round.

## What can go wrong

- **Power lost while the table is written.** The table is one block of 4 KB, written in a fraction of a second.
  Without power at that moment the screen does not start any more and needs an install over USB. This is the one
  step no software can make safe, and it happens once per screen.
- **The table refused.** ESPHome checks a table before it writes anything. A refusal leaves the screen as it was.
- **The copy fails.** The screen then does not say `widen` and keeps its table. Nothing is lost.

## Changing any of this

- The numbers live in three places that must agree: the CSV, `flash_layout.h` (`NARROW`, `WIDE`) and
  `Firmware.NARROW_SLOT`. `tests/test_flash_layout.py` compares them, and compares `NARROW` with the table ESPHome
  writes when ESPHome is installed.
- Never change the wide table's `nvs` or `app0` once screens have it. A third table would need its own move, with
  its own copy.
- `packages/hardware/flash-4mb.yaml` holds the component and nothing else. A package cannot add to the core's
  `esphome: on_boot:`. ESPHome replaces the core's with it, and the screen then starts without its fonts and its
  settings while the build and the API look fine. `tools/check_packages.py` refuses a package that tries.
- A board with 4 MB that joins includes `flash-4mb.yaml`. `tools/generate_entries.py` then writes the option into its
  checkout entry and `flash_layout` into its components, and `boards.json` gets `wide_slots`.
- Nothing that every board builds knows about the component: its word goes through a sensor of its own in
  `flash-4mb.yaml`, so a change here is a firmware update for the boards with 4 MB and for no other.
- `nvs_more` still holds whatever lay at the end of the flash before. A firmware that starts to use it wipes it first.
- The bridge is built by `tools/check.sh --firmware` whenever a board with 4 MB is, and it has to stay under three
  quarters of the old slot.

## Testing a change on a board

The unit tests cover the manager's side with a screen that follows the rules above. The firmware's side needs a real
board on USB, because it is about what is in the flash afterwards. A test worth trusting does both of these after
every step:

- Read the flash back with `esptool read_flash` (the table at 0x8000, the settings at 0xC000 and at 0x390000) and
  compare every kept value with a dump taken before anything was changed.
- Change a setting over the API, restart the screen and read the setting again. A screen whose start was cut short
  answers on its API and reports its features, but keeps nothing. That is how the first version of this failed.

The states to start from: ESPHome's table with firmware from before the component, with this firmware in the first slot and
in the second, with a setting changed just before the table, with other data in the unused 16 KB (random bytes, and
valid settings pages of another firmware), a table that is not ESPHome's, an erased board, and an install over USB
onto each of the first two. Then the bridge, from firmware before the component, and updates after the table into both
slots. Wait a minute after every update before you reset the board to read its flash: a reset in that minute is a
rollback, and you would be reading the firmware from before.
