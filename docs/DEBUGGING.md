# Debugging Buraaq programs

## Today

`buraaq build` produces a native executable via clang. Debug builds use `-O0`.

- **Windows:** use LLDB or Visual Studio. `buraaq debug FILE.bq` passes clang `-O0 -g` and prints how to load the pretty-printers.
- **Linux:** GDB / LLDB on the linked ELF.

Source locations in **compiler diagnostics** (not the native debugger) are the supported 1.0-quality path: multi-span errors, LSP hover, go-to-definition.

## `buraaq debug`

```bash
buraaq debug FILE.bq
```

builds with clang `-O0 -g` and prints how to load the pretty-printers:

- `stdlib/debug/lldb_buraaq.py` — `text` (`i8*`), `Option` / `Result` tag+payload
- `stdlib/debug/gdb_buraaq.py` — same summaries for GDB

```text
lldb out.exe
(lldb) command script import stdlib/debug/lldb_buraaq.py
```

## Runtime errors

Unhandled failures should print a message. Stack symbolization is **not** complete; treat traces as best-effort.
