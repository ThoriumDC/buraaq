# Roadmap (after 1.0)

Buraaq 1.0 is the language you install and ship. What remains is hardening and ecosystem, not a second language.

## Now

| Item | Why |
|------|-----|
| Gate D 7-day fuzz | Wall-clock evidence; scripts already refuse to lie |

## Next

| Item | Why |
|------|-----|
| Hosted index at packages.buraaq.dev | In-tree `packages/index.json` is fetchable; guest `add` / `index` already serve and resolve it |
| POSIX HTTPS with OpenSSL by default | Guest `build_auto` already passes `-lssl -lcrypto`; Windows WinINet already works |
| Channels cancellation | Send/recv and spawn are real OS primitives; no cancel yet |

## Not the product

- A guest OS inside Ship (that is Docker; we are not)
- Lifetime annotations in source
- A second standard library with overlapping names

Bootstrap truth: [BOOTSTRAP.md](BOOTSTRAP.md). Measured ledger: [STATUS.md](STATUS.md).
