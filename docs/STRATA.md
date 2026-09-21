# Strata — layered model memory

**Strata** is how Mind runs models larger than VRAM: one model appears fully available while live tensors occupy three tiers.

```text
Hot   → GPU VRAM     (active layers / KV)
Warm  → System RAM   (pinned / pageable weights)
Cold  → NVMe cache   (mmap / staged blocks under ~/.buraaq/strata)
```

You type intent (`buraaq ai serve BIG-MODEL`). Strata decides the split and **prints why**. Not a hub. Not “fit entirely in VRAM or fail.”

## CLI

```bash
buraaq ai strata Qwen/Qwen3-32B          # residency plan only
buraaq ai serve MODEL --strata auto      # default when model > usable VRAM
buraaq ai serve MODEL --strata off
buraaq ai serve MODEL --strata gpu-ram
buraaq ai serve MODEL --strata gpu-ram-disk
buraaq ai doctor                         # + Strata cache path / free disk
```

Flags: `--strata auto|off|gpu-ram|gpu-ram-disk`.

Cache root: `~/.buraaq/strata` (override with `BURAAQ_STRATA_CACHE`). Separate from raw HF pulls in `~/.buraaq/models`.

## Planner heuristics (estimates)

- Keep ~15% VRAM free for KV + activations
- Fill GPU first, then RAM (leave ~20% OS headroom), remainder → disk mmap
- Prefer fewer GPU layers over OOM
- Fit verdict: `fits-vram` | `fits-vram+ram` | `needs-disk` | `impossible`
- Speed class: `gpu-bound` | `ram-bound` | `disk-bound` (cold tier slows decode)

Every plan prints an explicit residency map, e.g. `layers 0–12 GPU · 13–28 RAM · 29–40 disk`.

## Phase 1 honesty

Phase 1 **orchestrates** peer engines:

| Backend | Strata behavior |
|---------|-----------------|
| **llama.cpp** | `-ngl` + mmap / `--no-mmap`; conservative threads when disk-bound |
| **vLLM** | CPU offload (`--cpu-offload-gb`) for gpu-ram; **no disk tier** — use llamacpp |
| **OpenAI remote** | Strata reports off (remote owns memory) |

There is **no** Buraaq-native tensor pager yet. Measured speed drops as the cold tier grows — Strata prints the expected class. Do not claim parity with in-VRAM throughput when disk-heavy.

## Out of Phase 1

- Custom CUDA/HIP paging runtime
- Multi-host Strata (model sharded across machines)
- Training through Strata tiers

## Related

- Mind surface: [AI.md](AI.md)
- Stack names: [STACK.md](STACK.md)
