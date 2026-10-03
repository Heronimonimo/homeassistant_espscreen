// clang++ -std=c++17 -Wall -Wextra -Werror -I. tests/test_kept_pages.cpp -o /tmp/test_kept_pages && /tmp/test_kept_pages
#include "../components/smart_display/kept_pages.h"
#include <cassert>

using namespace kept_pages;

int main() {
  // A board without room keeps nothing: every visit builds the page as before.
  Shelf none;
  assert(none.visit(1, 0, 0).entry == NONE && !none.keeps(0));

  Shelf shelf;
  shelf.capacity = 2;
  Changes changes;
  // The first turns make cards: page 0 leaves the glass into a new entry, page 1 gets that entry's new cards.
  auto v = shelf.visit(1, 0, changes.last);
  assert(v.entry == 0 && v.build && !v.kept && shelf.keeps(0));
  v = shelf.visit(2, 1, changes.last);
  assert(v.entry == 1 && v.build && !v.kept && shelf.keeps(0) && shelf.keeps(1) && shelf.pages() == 2);

  // Back to page 1: its own cards come back, drawn up to what was there when it left; page 2 takes their place.
  v = shelf.visit(1, 2, changes.last);
  assert(v.entry == 1 && v.kept && !v.build && v.synced == 0);
  assert(shelf.keeps(2) && shelf.keeps(0) && !shelf.keeps(1));

  // A tile changes while page 0 is away: only that card is behind on the way back.
  const uint32_t synced0 = shelf.entries[0].synced;
  changes.mark_tile(5);
  v = shelf.visit(0, 1, changes.last);
  assert(v.entry == 0 && v.kept && changes.tile_after(5, v.synced) && !changes.tile_after(6, v.synced) && v.synced == synced0);
  assert(!changes.all_after(v.synced));
  // The page that left takes the glass's number: that change is on its cards.
  assert(shelf.entries[0].page == 1 && !changes.tile_after(5, shelf.entries[0].synced));

  // A drawing asked for before a page was built is not a change that page lacks (the prepared-pages bug of 2026-09-26:
  // "draw everything" was asked for with the layout and ran after the pages had been built).
  const uint32_t asked = changes.mark_all();
  v = shelf.visit(3, 0, changes.last);  // page 3 is built now, after the ask
  assert(!v.kept && v.entry == 1);       // the page shown longest ago (2) gives its cards up
  assert(!changes.all_after(shelf.entries[1].synced) && asked <= shelf.entries[1].synced);

  // Page 1 left before the ask: it lacks everything.
  v = shelf.visit(1, 3, changes.last);
  assert(v.kept && changes.all_after(v.synced));

  // A new layout: nothing is kept, the cards stay for reuse and cards of no page are taken before an old page.
  shelf.forget();
  assert(!shelf.keeps(1) && !shelf.keeps(3) && shelf.pages() == 0);
  v = shelf.visit(2, 1, changes.last);
  assert(!v.kept && !v.build && v.entry < 2 && shelf.keeps(1));

  // The same page on and off the glass, or no page, keeps nothing.
  assert(shelf.visit(1, 1, 0).entry == NONE && shelf.visit(-1, 2, 0).entry == NONE);
  // A tile beyond the screen's ceiling (page_protocol.h, 64 in a build that states none) counts as everything.
  const uint32_t before = changes.all;
  changes.mark_tile(page_protocol::MAX_TILES);
  assert(changes.all > before);
  return 0;
}
