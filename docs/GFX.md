# std.gfx — game canvas and sound

**Gfx** is the one obvious way to draw pixels and play sound. Not Lumen (operator console). Not an engine you rent. A framebuffer, a window, keys, and a tone.

```buraaq
fn main() {
    canvas(320, 180, "dot")
    ink(40, 200, 90)
    wipe()
    box(20, 40, 24, 24)
    flip()
    hush()
}
```

Unique names import themselves. You do not write `use gfx`. Stack: [STACK.md](STACK.md). API map: [STDLIB.md](STDLIB.md). Live files on a wire: [Stream](STDLIB.md#stdstream) `clip` / `shot`.

---

## API

| Function | Role |
|----------|------|
| `canvas(w, h, title)` | Open the framebuffer (and a desktop window) |
| `ink(r, g, b)` | Draw color |
| `wipe()` | Fill the frame with the current ink |
| `plot(x, y)` | One pixel |
| `box(x, y, w, h)` | Filled rectangle |
| `dash(x0, y0, x1, y1)` | Line |
| `flip()` | Present the frame and pump input |
| `held(name)` | 1 while that key is down |
| `pulse()` | Frames presented since `canvas` |
| `play(path)` | Play a sound file (Windows) |
| `tone(hz, ms)` | Beep (blocking) |
| `hush()` | Stop sound |

`held` names: `left`, `right`, `up`, `down`, `escape`, `quit`, `space`, `enter`, `shift`, `ctrl`, `a`–`z`, `0`–`9`.

`held` is 1 only while the Gfx window is focused. Closing the window is `held("quit")` / `held("escape")`. The game is that window — not the terminal.

`BURAAQ_GFX_HEADLESS=1` keeps the pixels and skips the window.

`tone` blocks the thread. Prefer it for a hit sting, not every frame. `wait(16)` is about 60 Hz.

Do not name your own function `off` — that is `std.led.off`.

---

## A loop

```buraaq
fn main() {
    canvas(320, 180, "dot")
    x = 20
    while held("escape") == 0 {
        ink(12, 18, 32)
        wipe()
        ink(40, 200, 90)
        box(x, 60, 24, 24)
        flip()
        if held("right") {
            x = x + 2
        }
        if held("left") {
            x = x - 2
        }
        wait(16)
    }
    hush()
}
```

Click the window, then play. Guest today: keep moving state in lists (`p[0] = p[0] + 6`) and pass slots into helpers (`ship(p[0], p[1], p[11])`). Bare locals as helper arguments are not a reliable path on the guest emitter yet.

---

## Examples

| File | What it is |
|------|------------|
| `stdlib/examples/nova.bq` | **NOVA** — neon arena shooter (the game to run) |
| `stdlib/examples/gfx.bq` | One frame, then exit |
| `stdlib/examples/snake.bq` | Grid snake |

```text
buraaq build stdlib/examples/nova.bq dist/nova.exe
dist\nova.exe
```

NOVA: click the **NOVA** window. Space starts and fires. Arrows or WASD move. Escape quits. Cyan pips are lives.

---

## Platforms

| OS | Status |
|----|--------|
| Windows | GDI window + `play` / `tone` / `hush` (winmm) |
| POSIX | Framebuffer only; `play` / `tone` are no-ops |

Guest `build_auto` links `buraaq_gfx.c` with `-lgdi32 -luser32 -lwinmm` on Windows.
