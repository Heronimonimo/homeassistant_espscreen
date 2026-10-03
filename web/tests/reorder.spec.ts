// The rows of a dragged list (app 0.4.55): the screensaver's steps and the players of its music step.
import { describe, expect, it } from "vitest";
import { dropIndex, moved } from "../src/model/reorder";

const rows = [0, 1, 2, 3].map((i) => ({ top: i * 40, height: 40 }));

describe("a row dragged up or down", () => {
  it("takes a neighbour's place once the pointer passed its middle", () => {
    expect(dropIndex(rows, 0, 10)).toBe(0);
    expect(dropIndex(rows, 0, 59)).toBe(0);
    expect(dropIndex(rows, 0, 61)).toBe(1);
    expect(dropIndex(rows, 0, 500)).toBe(3);
    expect(dropIndex(rows, 3, 101)).toBe(3);
    expect(dropIndex(rows, 3, 99)).toBe(2);
    expect(dropIndex(rows, 3, -50)).toBe(0);
    expect(dropIndex(rows, 1, 70)).toBe(1);
  });
  it("moves one row and keeps the others in their order", () => {
    expect(moved(["sonos", "tv", "kitchen"], 1, 0)).toEqual(["tv", "sonos", "kitchen"]);
    expect(moved(["sonos", "tv", "kitchen"], 0, 2)).toEqual(["tv", "kitchen", "sonos"]);
    expect(moved(["sonos", "tv"], 0, 2)).toEqual(["sonos", "tv"]);
    expect(moved(["sonos", "tv"], -1, 0)).toEqual(["sonos", "tv"]);
  });
});
