# Scripting mode (optional)

Buraaq’s **core** model is native AOT (`buraaq build` / `buraaq run`). No GC.

For a terminal edit loop, `buraaq`, `-e`, and `script` wrap a snippet in `fn main` and compile it with clang (same path as `run`, not a second interpreter):

## Check install

```bash
buraaq --version
buraaq doctor       # sysroot + clang; scripting line
```

## Interactive REPL

```bash
buraaq              # or: buraaq repl
bq> println("hi")
bq> 1 + 2 * 3
6
bq> :quit
```

## One-liner / file

```bash
buraaq -e "println(\"hi\")"
buraaq script path.bq
buraaq script -e "print_int(40+2)"
```

```buraaq
#!/usr/bin/env -S buraaq script

fn main() {
    println("hello from buraaq script")
}
```

| Command | Role |
|---------|------|
| `buraaq` / `repl` / `shell` | Interactive terminal (clang per line) |
| `buraaq -e` | Eval snippet |
| `buraaq script` | Run a `.bq` file |
| `buraaq run` | Native AOT binary (product path) |

Clang is required for `run` / `build` / the shell / `-e` / `script`.
