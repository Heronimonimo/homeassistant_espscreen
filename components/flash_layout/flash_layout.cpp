#include "flash_layout.h"

#include <cstring>
#include <memory>
#include <esp_flash.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_system.h>
#include <nvs.h>
#include <nvs_flash.h>
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"

namespace esphome::flash_layout {

static const char *const TAG = "flash_layout";
// The copy's name while the old table is in use (in the wide table that place is "nvs" itself), and the old place's
// name once the wide table is.
static const char *const COPY = "nvs_wide";
static const char *const OLD = "nvs_old";
// "These settings are where the wide table keeps them": a value of this component's own in the settings area, so it is
// copied along with everything else.
static const char *const OWN = "flash_layout";
static const char *const DONE = "done";

static FlashLayout *instance = nullptr;

static bool is_table(const Table &t) {
  const auto *nvs = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS, "nvs");
  const auto *a = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, nullptr);
  const auto *b = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, nullptr);
  return nvs && a && b && nvs->address == t.nvs && nvs->size == t.nvs_size && a->address == FIRST_SLOT &&
         a->size == t.slot && b->address == t.second && b->size == t.slot;
}

// Nothing of the table in use may lie where the copy goes.
static bool room_for_copy() {
  bool free = true;
  for (auto it = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, nullptr); it != nullptr;
       it = esp_partition_next(it)) {
    const auto *p = esp_partition_get(it);
    if (p->address < WIDE.nvs + WIDE.nvs_size && p->address + p->size > WIDE.nvs)
      free = false;
  }
  return free;
}

// How many bytes a number of this kind takes; 0 for a text or a blob, whose length NVS tells.
static size_t fixed_size(nvs_type_t type) {
  switch (type) {
    case NVS_TYPE_U8:
    case NVS_TYPE_I8:
      return 1;
    case NVS_TYPE_U16:
    case NVS_TYPE_I16:
      return 2;
    case NVS_TYPE_U32:
    case NVS_TYPE_I32:
      return 4;
    case NVS_TYPE_U64:
    case NVS_TYPE_I64:
      return 8;
    default:
      return 0;
  }
}

// One value of any kind NVS keeps, as bytes. With `data` null, only its length.
static esp_err_t get(nvs_handle_t handle, const nvs_entry_info_t &info, void *data, size_t &size) {
  if (const size_t fixed = fixed_size(info.type)) {
    size = fixed;
    if (data == nullptr)
      return ESP_OK;
  }
  switch (info.type) {
    case NVS_TYPE_U8:
      return nvs_get_u8(handle, info.key, static_cast<uint8_t *>(data));
    case NVS_TYPE_I8:
      return nvs_get_i8(handle, info.key, static_cast<int8_t *>(data));
    case NVS_TYPE_U16:
      return nvs_get_u16(handle, info.key, static_cast<uint16_t *>(data));
    case NVS_TYPE_I16:
      return nvs_get_i16(handle, info.key, static_cast<int16_t *>(data));
    case NVS_TYPE_U32:
      return nvs_get_u32(handle, info.key, static_cast<uint32_t *>(data));
    case NVS_TYPE_I32:
      return nvs_get_i32(handle, info.key, static_cast<int32_t *>(data));
    case NVS_TYPE_U64:
      return nvs_get_u64(handle, info.key, static_cast<uint64_t *>(data));
    case NVS_TYPE_I64:
      return nvs_get_i64(handle, info.key, static_cast<int64_t *>(data));
    case NVS_TYPE_STR:
      return nvs_get_str(handle, info.key, static_cast<char *>(data), &size);
    case NVS_TYPE_BLOB:
      return nvs_get_blob(handle, info.key, data, &size);
    default:
      return ESP_ERR_NOT_SUPPORTED;
  }
}

static esp_err_t put(nvs_handle_t handle, const nvs_entry_info_t &info, const void *data, size_t size) {
  switch (info.type) {
    case NVS_TYPE_U8:
      return nvs_set_u8(handle, info.key, *static_cast<const uint8_t *>(data));
    case NVS_TYPE_I8:
      return nvs_set_i8(handle, info.key, *static_cast<const int8_t *>(data));
    case NVS_TYPE_U16:
      return nvs_set_u16(handle, info.key, *static_cast<const uint16_t *>(data));
    case NVS_TYPE_I16:
      return nvs_set_i16(handle, info.key, *static_cast<const int16_t *>(data));
    case NVS_TYPE_U32:
      return nvs_set_u32(handle, info.key, *static_cast<const uint32_t *>(data));
    case NVS_TYPE_I32:
      return nvs_set_i32(handle, info.key, *static_cast<const int32_t *>(data));
    case NVS_TYPE_U64:
      return nvs_set_u64(handle, info.key, *static_cast<const uint64_t *>(data));
    case NVS_TYPE_I64:
      return nvs_set_i64(handle, info.key, *static_cast<const int64_t *>(data));
    case NVS_TYPE_STR:
      return nvs_set_str(handle, info.key, static_cast<const char *>(data));
    case NVS_TYPE_BLOB:
      return nvs_set_blob(handle, info.key, data, size);
    default:
      return ESP_ERR_NOT_SUPPORTED;
  }
}

// One pass over everything the area `source` keeps, every name space and every kind of value: with `write`, each one
// the area `target` lacks or has differently is written there. Returns how many differed, or -1 when one could not be
// read or written. A second pass without `write` that returns 0 is the proof.
static int pass(const char *source, const char *target, bool write) {
  int differed = 0;
  nvs_iterator_t it = nullptr;
  esp_err_t err = nvs_entry_find(source, nullptr, NVS_TYPE_ANY, &it);
  for (; err == ESP_OK; err = nvs_entry_next(&it)) {
    nvs_entry_info_t info;
    nvs_entry_info(it, &info);
    nvs_handle_t from = 0, to = 0;
    bool ok = nvs_open_from_partition(source, info.namespace_name, NVS_READONLY, &from) == ESP_OK;
    // The target's name space is made when it is written to; when only reading, one that is missing is a difference.
    const esp_err_t opened =
        nvs_open_from_partition(target, info.namespace_name, write ? NVS_READWRITE : NVS_READONLY, &to);
    size_t size = 0, have = 0;
    ok = ok && get(from, info, nullptr, size) == ESP_OK;
    if (ok) {
      // new[] gives memory aligned for any number, which the typed reads and writes above need.
      std::unique_ptr<uint8_t[]> value(new uint8_t[size + 8]()), other(new uint8_t[size + 8]());
      ok = get(from, info, value.get(), size) == ESP_OK;
      have = size;
      const bool same = ok && opened == ESP_OK && get(to, info, nullptr, have) == ESP_OK && have == size &&
                        get(to, info, other.get(), have) == ESP_OK && memcmp(value.get(), other.get(), size) == 0;
      if (ok && !same) {
        differed++;
        if (write)
          ok = opened == ESP_OK && put(to, info, value.get(), size) == ESP_OK && nvs_commit(to) == ESP_OK;
      }
    }
    if (from)
      nvs_close(from);
    if (to)
      nvs_close(to);
    if (!ok) {
      ESP_LOGW(TAG, "%s/%s could not be copied", info.namespace_name, info.key);
      differed = -1;
      break;
    }
  }
  nvs_release_iterator(it);
  if (differed >= 0 && err != ESP_ERR_NVS_NOT_FOUND)
    differed = -1;
  return differed;
}

// Whether the area `part` says its settings are where the wide table keeps them.
static bool done_in(const char *part) {
  nvs_handle_t handle = 0;
  uint8_t done = 0;
  const bool found = nvs_open_from_partition(part, OWN, NVS_READONLY, &handle) == ESP_OK &&
                     nvs_get_u8(handle, DONE, &done) == ESP_OK && done == 1;
  if (handle)
    nvs_close(handle);
  return found;
}

static bool set_done(const char *part) {
  nvs_handle_t handle = 0;
  const bool set = nvs_open_from_partition(part, OWN, NVS_READWRITE, &handle) == ESP_OK &&
                   nvs_set_u8(handle, DONE, 1) == ESP_OK && nvs_commit(handle) == ESP_OK;
  if (handle)
    nvs_close(handle);
  return set;
}

// Everything of the area `source` in the area `target`, and nothing else there: written, read back, and the target
// read against the source as well. `changed` is how many values it wrote.
static bool mirror(const char *source, const char *target, int &changed) {
  changed = pass(source, target, true);
  return changed >= 0 && pass(source, target, false) == 0 && pass(target, source, false) == 0;
}

// Old table: everything the settings area keeps now, where the wide table keeps it, and nothing else there.
//
// That place is unused in the old table, so what lies in it the first time is unknown: empty on a screen that only
// ever had ESPHome's table, another firmware's settings on a board that ran something else before. So unless it is a
// copy this firmware made (it carries DONE), it is wiped first. A copy that holds something the settings no longer
// have is wiped and made again as well: after this the two areas hold the same, value for value.
static bool copy_settings() {
  if (!done_in(NVS_DEFAULT_PART_NAME) && !set_done(NVS_DEFAULT_PART_NAME))
    return false;
  global_preferences->sync();
  const esp_partition_t *part = nullptr;
  if (esp_partition_register_external(nullptr, WIDE.nvs, WIDE.nvs_size, COPY, ESP_PARTITION_TYPE_DATA,
                                      ESP_PARTITION_SUBTYPE_DATA_NVS, &part) != ESP_OK)
    return false;
  bool done = false;
  int changed = -1;
  esp_err_t err = nvs_flash_init_partition_ptr(part);
  bool ours = err == ESP_OK && done_in(COPY);
  for (int attempt = 0; attempt < 2 && !done; attempt++) {
    if (!ours) {
      if (err == ESP_OK)
        nvs_flash_deinit_partition(COPY);
      err = esp_partition_erase_range(part, 0, part->size);
      if (err == ESP_OK)
        err = nvs_flash_init_partition_ptr(part);
    }
    if (err != ESP_OK)
      break;
    done = mirror(NVS_DEFAULT_PART_NAME, COPY, changed);
    ours = false;  // a copy that does not match is made again from an empty area, once
  }
  if (changed != 0 || !done)
    ESP_LOGI(TAG, "Settings copied for the wide table: %d written, %s", changed, done ? "verified" : "FAILED");
  if (err == ESP_OK)
    nvs_flash_deinit_partition(COPY);
  esp_partition_deregister_external(part);
  return done;
}

// Wide table, and nothing says the settings are here: a screen that starts with it for the first time, new or
// flashed over USB from firmware older than this. Its settings area is wiped first, so nothing that lay there before
// (another firmware's) is taken for a setting. Then what the old table's place still holds is fetched: that place
// lies in the second slot now, which is why it is in no table: a partition made by hand, read only, so ESP-IDF's
// settings code reads it and its partition code refuses every write. With DONE set this happens once, and the screen
// starts again, because wiping the area took ESPHome's hold of it away.
static void fetch_settings() {
  nvs_flash_deinit();
  if (nvs_flash_erase() != ESP_OK || nvs_flash_init() != ESP_OK) {
    ESP_LOGE(TAG, "The settings area could not be prepared");
    return;
  }
  static esp_partition_t old = {};
  old.flash_chip = esp_flash_default_chip;
  old.type = ESP_PARTITION_TYPE_DATA;
  old.subtype = ESP_PARTITION_SUBTYPE_DATA_NVS;
  old.address = NARROW.nvs;
  old.size = NARROW.nvs_size;
  old.erase_size = 4096;
  old.readonly = true;
  strcpy(old.label, OLD);
  int fetched = 0;
  bool read = false;
  if (nvs_flash_init_partition_ptr(&old) == ESP_OK) {
    fetched = pass(OLD, NVS_DEFAULT_PART_NAME, true);
    read = fetched >= 0 && pass(OLD, NVS_DEFAULT_PART_NAME, false) == 0;
    nvs_flash_deinit_partition(OLD);
  }
  if (!set_done(NVS_DEFAULT_PART_NAME)) {
    ESP_LOGE(TAG, "The settings area could not be written");
    return;
  }
  ESP_LOGI(TAG, "First start with the wide table: %d settings fetched from the old table's place%s; restarting", fetched,
           fetched > 0 && !read ? " (not all)" : "");
  esp_restart();
}

// Whether ESPHome has confirmed the firmware that runs: after an update the bootloader holds it on trial until the
// screen has run for a minute, and a restart before that goes back to the firmware from before.
static bool confirmed() {
  esp_ota_img_states_t trial;
  const auto *running = esp_ota_get_running_partition();
  if (running == nullptr || esp_ota_get_state_partition(running, &trial) != ESP_OK)
    return true;  // a bootloader that keeps no such state has nothing on trial
  return trial != ESP_OTA_IMG_NEW && trial != ESP_OTA_IMG_PENDING_VERIFY;
}

void FlashLayout::setup() {
  instance = this;
  if (is_table(WIDE)) {
    this->state_ = State::WIDE;
    if (!done_in(NVS_DEFAULT_PART_NAME))
      fetch_settings();
  }
#ifdef USE_OTA_STATE_LISTENER
  ota::get_global_ota_callback()->add_global_state_listener(this);
#endif
}

void FlashLayout::loop() {
  this->look_();
  this->disable_loop();
}

#ifdef USE_OTA_STATE_LISTENER
void FlashLayout::on_ota_global_state(ota::OTAState state, float progress, uint8_t error,
                                      ota::OTAComponent *component) {
  if (state == ota::OTA_STARTED)
    this->look_();
}
#endif

void FlashLayout::look_() {
  if (is_table(WIDE)) {
    this->state_ = State::WIDE;
    return;
  }
  if (!is_table(NARROW) || !room_for_copy()) {
    this->state_ = State::OTHER;
    return;
  }
  this->state_ = State::NARROW;
  const auto *running = esp_ota_get_running_partition();
  this->first_slot_ = running != nullptr && running->address == FIRST_SLOT;
  this->copied_ = copy_settings();
}

const char *FlashLayout::word() const {
  if (this->state_ == State::WIDE)
    return "wide";
  if (this->state_ != State::NARROW)
    return "other";
#ifdef USE_OTA_PARTITIONS
  // Only a screen built to take a table over Wi-Fi (`ota: allow_partition_access: true` in its own YAML) asks for one.
  if (this->copied_ && confirmed())
    return this->first_slot_ ? "widen" : "widen_next";
#endif
  return "old";
}

const char *word() { return instance != nullptr ? instance->word() : "other"; }

}  // namespace esphome::flash_layout
