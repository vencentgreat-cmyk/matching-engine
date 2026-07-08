# Benchmark Methodology

## Workload

Single-thread simulation (`SIM`) uses a deterministic mixed workload:

- 75% add operations
- 25% cancel operations
- RNG seed: `42`

Command shape:

```bash
./build/engine
SIM AMD <count>
```

In each loop iteration:

- If cancel path is selected and there are candidate IDs, one ID is chosen and `cancelOrder(id)` is timed.
- Otherwise an add order is generated and `addOrder(order)` is timed.

## Why this mix

Cancel-heavy traffic is representative of exchange-like and HFT-style behavior, where order updates/cancels are frequent and often dominate tail behavior.

## Why split cancel hit vs miss

Cancel-hit and cancel-miss exercise different paths:

- Cancel hit: `unordered_map::find` + `list::erase` + price-level cleanup + index erase.
- Cancel miss: `unordered_map::find` returns not found and exits quickly.

Mixing these into one cancel bucket can hide regressions and make percentile movement ambiguous. Reporting add / cancel-hit / cancel-miss separately gives clearer diagnostics.

## Environment

- Platform: WSL2 Ubuntu 24
- Compiler: g++-14
- Threading mode: single-thread (`SIM`)
- CPU pinning: not used

## Known limitations

- WSL2 scheduler jitter can introduce outliers.
- Allocator effects are not isolated (no dedicated allocator instrumentation in this round).
- CPU frequency scaling is not controlled.

## Warmup policy

No warmup phase is used in this round.

Reason:

- Warmup mutates book state and changes measured-phase distribution.
- Warmup may consume RNG draws and break strict seed-42 comparability with historical runs.

For comparability, measurements start directly from the deterministic seed-driven sequence.
