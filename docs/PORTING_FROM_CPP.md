# Porting from C++

## What gets simpler

- No header/source split — modules replace `#include` guards
- No Rule of Three/Five — ownership is explicit
- No template error novels — generics monomorphize to int / text / float / per-call-site struct copies

## Classes → structs + impl

```cpp
// C++
class Server {
    int port;
public:
    void listen();
};
```

```buraaq
struct Server {
    port: i32
    fn listen(self) { ... }
}
```

## RAII

Destructors map to `drop` methods (deterministic destruction ADR). Scope exits run cleanup predictably.

## Templates → generics

Buraaq generics monomorphize. Guest copies today: `min` (int), `min__text`, `min__float`, `min__Struct` with `a.lt(b)` as icmp / `buraaq_text_lt` / fcmp / `Type_lt`. Trait bounds past those four copies are still growing. Expect clearer errors than SFINAE.

## When to keep C++

FFI to existing C++ libraries via `extern c` exported symbols, or C ABI shim.
