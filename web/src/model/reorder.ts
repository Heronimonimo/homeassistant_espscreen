// A list reordered by dragging one of its rows up or down (the screensaver's steps and its players, app 0.4.55): which
// place the dragged row takes for a pointer at height `y`. A row gives way once the pointer has passed its middle, so
// the dragged row follows the finger and never jumps back and forth on a boundary.
export type RowSpan = { top: number; height: number };
export function dropIndex(rows: RowSpan[], from: number, y: number) {
  let target = from;
  rows.forEach((row, i) => {
    const middle = row.top + row.height / 2;
    if (i < from && y < middle) target = Math.min(target, i);
    if (i > from && y > middle) target = Math.max(target, i);
  });
  return target;
}
// The list with the row at `from` moved to `to`; the same list when either is outside it.
export function moved<T>(list: readonly T[], from: number, to: number): T[] {
  const next = [...list];
  if (from < 0 || to < 0 || from >= next.length || to >= next.length || from === to) return next;
  next.splice(to, 0, ...next.splice(from, 1));
  return next;
}
