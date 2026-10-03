#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include "screen_text.h"

namespace header_bar {
// The top bar right of the screen name (firmware 0.2.32+). ESP Screen Manager decides what shows
// and writes an entity's text; the screen draws it, ticks its clocks and counts "5 min ago"
// itself. Everything here is free of LVGL, so tests/test_header_bar.cpp covers it on a PC.
// The items one page's bar holds (firmware 0.34.0+): six, or what a board with room for more states as the build flag
// SCREEN_MAX_BAR_ITEMS (packages/core.yaml, its board file). The glass's width still decides how many show (place()).
#ifndef SCREEN_MAX_BAR_ITEMS
#define SCREEN_MAX_BAR_ITEMS 6
#endif
constexpr size_t MAX_ITEMS = SCREEN_MAX_BAR_ITEMS;
static_assert(MAX_ITEMS >= 6 && MAX_ITEMS <= 16, "a page's bar holds 6 to 16 items");
// The items the app numbered a bar_value's targets by when it says nothing (page * 6 + index): every app before 0.4.57.
constexpr size_t WIRE_ITEMS = 6;
constexpr size_t TEXT_BYTES = 48;
enum class Kind : uint8_t { none, clock, analog, date, text, ago };

inline Kind kind(const std::string &name) {
  if (name == "clock") return Kind::clock;
  if (name == "analog") return Kind::analog;
  if (name == "date") return Kind::date;
  if (name == "text") return Kind::text;
  if (name == "ago") return Kind::ago;
  return Kind::none;
}

struct Item {
  Kind kind = Kind::none;
  uint32_t icon = 0;   // Material Design Icons codepoint, 0 without an icon
  std::string text;    // Kind::text
  int64_t epoch = 0;   // Kind::ago: the moment, past or future
  uint32_t color = 0;  // accent of the icon while `has_color`
  bool has_color = false;
  bool operator==(const Item &o) const {
    return kind == o.kind && icon == o.icon && text == o.text && epoch == o.epoch && color == o.color && has_color == o.has_color;
  }
};

struct Bar {
  std::array<Item, MAX_ITEMS> items{};
  size_t count = 0;
  bool received = false;  // false until the manager sent one: the clock of show_clock then
};

// "RRGGBB" as 0xRRGGBB; false for anything else.
inline bool color(const std::string &hex, uint32_t &out) {
  if (hex.size() != 6) return false;
  uint32_t value = 0;
  for (char c : hex) {
    int digit = c >= '0' && c <= '9' ? c - '0' : c >= 'A' && c <= 'F' ? c - 'A' + 10 : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
    if (digit < 0) return false;
    value = value << 4 | static_cast<uint32_t>(digit);
  }
  out = value;
  return true;
}

// Next codepoint of UTF-8 text at `i` (advanced past it); 0 at the end. Broken bytes count as one.
inline uint32_t next_codepoint(const std::string &s, size_t &i) {
  if (i >= s.size()) return 0;
  unsigned char c = static_cast<unsigned char>(s[i]);
  int extra = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
  uint32_t cp = extra == 3 ? c & 0x07 : extra == 2 ? c & 0x0F : extra == 1 ? c & 0x1F : c;
  ++i;
  for (int k = 0; k < extra && i < s.size() && (static_cast<unsigned char>(s[i]) & 0xC0) == 0x80; ++k, ++i)
    cp = cp << 6 | (static_cast<unsigned char>(s[i]) & 0x3F);
  return cp;
}

// Relative time in the editor's words (app.js agoText): "Just now", "5 min ago", "Yesterday",
// "In 2 hours", in the screen's language (screen.time, app 0.2.90). `now` 0 means the clock is not set yet.
inline std::string ago_text(int64_t then, int64_t now) {
  using namespace screen_text;
  if (now <= 0 || then <= 0) return "—";
  int64_t seconds = now - then, span = seconds < 0 ? -seconds : seconds;
  auto n = [&](int64_t unit) { return static_cast<int>(span / unit); };
  if (seconds < 0) {
    if (span < 3600) return fill(txt::time_in_minutes, "n", std::max(1, n(60)));
    if (span < 86400) return plural(txt::time_in_hours, n(3600));
    if (span < 172800) return tr(txt::time_tomorrow);
    return plural(txt::time_in_days, n(86400));
  }
  if (span < 60) return tr(txt::time_just_now);
  if (span < 3600) return fill(txt::time_minutes_ago, "n", n(60));
  if (span < 86400) return plural(txt::time_hours_ago, n(3600));
  if (span < 172800) return tr(txt::time_yesterday);
  if (span < 604800) return plural(txt::time_days_ago, n(86400));
  if (span < 2592000) return plural(txt::time_weeks_ago, n(604800));
  if (span < 31536000) return plural(txt::time_months_ago, n(2592000));
  return plural(txt::time_years_ago, n(31536000));
}

// "Mo 14 Sep" in English, "za 19 sep" in Dutch, "sam. 19 sept." in French (screen.date.top_bar, with the language's
// abbreviation as {weekday} or its two letters as {weekday_min}); day_of_week 1 is Sunday, as ESPHome counts.
inline std::string date_text(int day_of_week, int day_of_month, int month) {
  using namespace screen_text;
  if (day_of_week < 1 || day_of_week > 7 || month < 1 || month > 12) return "—";
  std::string text = fill(txt::date_top_bar, "weekday", tr(txt::date_weekdays_short + day_of_week - 1));
  text = fill(text, "weekday_min", tr(txt::date_weekdays_min + day_of_week - 1));
  text = fill(text, "day", std::to_string(day_of_month));
  return fill(text, "month", tr(txt::date_months_short + month - 1));
}

// What you see between icon and value, between two items and after the name, from the height of
// the digits; the editor's barGaps() uses the same factors.
struct Gaps { int icon, item, name; };
inline int scaled(int cap, int tenths) { return (cap * tenths + 5) / 10; }
inline Gaps gaps(int cap) {
  return {std::max(2, scaled(cap, 4)), std::max(6, (cap * 125 + 50) / 100), std::max(8, scaled(cap, 16))};
}

// Right-aligned placement of item widths (ink, gaps included) in `width` pixels beside a name of
// `name_natural` pixels. The name keeps at least min(natural, 35% of the width); items leave from
// the front until the rest fits. `x` is each item's left ink edge; `name_room` what the name may use.
struct Placement {
  size_t first = 0;
  std::array<int, MAX_ITEMS> x{};
  int name_room = 0;
};
inline Placement place(const int *widths, size_t count, const Gaps &g, int width, int name_natural) {
  Placement p;
  count = std::min(count, MAX_ITEMS);
  int min_name = std::min(name_natural, width * 35 / 100);
  auto total = [&](size_t from) {
    int sum = 0;
    for (size_t i = from; i < count; ++i) sum += widths[i] + (i > from ? g.item : 0);
    return sum;
  };
  while (p.first < count && total(p.first) + g.name + min_name > width) ++p.first;
  int x = width - total(p.first);
  for (size_t i = p.first; i < count; ++i) { p.x[i] = x; x += widths[i] + g.item; }
  p.name_room = p.first < count ? p.x[p.first] - g.name : width;
  return p;
}
}  // namespace header_bar
