#pragma once
// The flash of a board with 4 MB (firmware 0.33.1+ of those boards): wider update slots, and the way a screen gets them.
// docs/FLASH_LAYOUT.md has the whole story, with the part ESP Screens plays.
//
// ESPHome's table for 4 MB keeps 448 KB for settings and two update slots of 1,835,008 bytes, which the firmware of
// these boards had filled. Their table (partitions-4mb-wide.csv, beside this file) keeps the settings in the 16 KB that
// were never used in front of the first slot, and gives each slot 2,031,616 bytes. A new screen is flashed with it.
//
// A screen that is out there has the old table, and everything it kept must come along: the touch calibration, the
// brightness, which way the screen hangs, the radio's calibration. Two things see to that, and each covers the other.
//
// - Over Wi-Fi. A screen with the old table keeps a copy of everything in its settings area where the wide table will
//   look for it. That place is free in the old table, so the copy is made with the settings in use and nothing of
//   the old ones is touched; it is made when the screen starts and once more when an update begins, and read back
//   each time. With the copy in place the screen says "widen" in its "Screen flash" sensor, and ESP Screens sends the
//   wide table after its next update (ESPHome's `ota: allow_partition_access` and `esphome upload --partition-table`). The
//   screen restarts with wide slots and everything it had.
// - Over USB. A screen flashed with the wide table while its settings still lie in the old place (an install over
//   USB from firmware older than this) fetches them from there the first time it starts. Nothing has written there
//   yet, and the old place is opened read only, so ESP-IDF's own settings code reads it and cannot change it.
//
// The screen says "widen" only when all of this holds: its table is exactly ESPHome's own for 4 MB, the copy was read
// back, it runs from its first slot, and ESPHome has confirmed the firmware it runs. From its second slot ESPHome
// would copy the firmware first, a quarter of a minute in which a slow flash chip runs into its watchdog, so there
// the word is "widen_next": one more update, then the table. And a firmware ESPHome has not confirmed yet (the first
// minute after an update) would be rolled back by a restart, so until then the screen says "old", as it does when the
// copy could not be made or its YAML does not let a table be replaced.
//
// A value of its own, DONE, says that this screen's settings are where the wide table keeps them. It travels with
// the copy. A screen that starts with the wide table without it wipes its settings area, looks in the old place
// once, and starts again; so does a new screen, which finds nothing there. Either way the area then holds what this
// firmware put in it and nothing a firmware before it left behind, and the copy over Wi-Fi is held to the same: it
// is made in a wiped place and compared both ways.
//
// Any table that is not exactly ESPHome's own for 4 MB (someone's own) is left alone: no copy, and the word is "other".
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#ifdef USE_OTA_STATE_LISTENER
#include "esphome/components/ota/ota_backend.h"
#endif

namespace esphome::flash_layout {

// The two tables this firmware knows: ESPHome's own for 4 MB, and partitions-4mb-wide.csv. tests/test_flash_layout.py
// compares these numbers with that file and with the table ESPHome writes.
struct Table {
  uint32_t nvs, nvs_size, slot, second;
};
constexpr Table NARROW{0x390000, 0x70000, 0x1C0000, 0x1D0000};
constexpr Table WIDE{0x00C000, 0x04000, 0x1F0000, 0x200000};
constexpr uint32_t FIRST_SLOT = 0x10000;

enum class State : uint8_t { OTHER, NARROW, WIDE };

class FlashLayout final : public Component
#ifdef USE_OTA_STATE_LISTENER
    ,
                          public ota::OTAGlobalStateListener
#endif
{
 public:
  // Before anything reads a setting: a screen flashed with the wide table over USB fetches what the old table kept.
  float get_setup_priority() const override { return setup_priority::BUS + 50.0f; }
  void setup() override;
  // Once, when everything has started: which table this screen has, and for the old one the copy.
  void loop() override;
#ifdef USE_OTA_STATE_LISTENER
  // When an update of any kind begins: the copy once more, with the settings as they are at that moment.
  void on_ota_global_state(ota::OTAState state, float progress, uint8_t error, ota::OTAComponent *component) override;
#endif
  // The word for the "Screen flash" sensor: "wide", "widen", "widen_next", "old" or "other".
  const char *word() const;

 protected:
  void look_();
  State state_{State::OTHER};
  bool copied_{false};
  bool first_slot_{false};
};

// The screen's word about its table, for the sensor's lambda (packages/hardware/flash-4mb.yaml).
const char *word();

}  // namespace esphome::flash_layout
