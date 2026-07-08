# O(n) → O(1) cancel optimization

## Change
- `std::deque<Order>` → `std::list<Order>`
- Added `unordered_map<int, OrderLocation>` for O(1) order lookup
- Both `match()` and `cancelOrder()` maintain the index

## Workload
75% add + 25% cancel (mixed), simulating real HFT patterns.

## Results at 100K operations

| Metric | Before | After | Speedup |
|---|---|---|---|
| Total time | 1,448 ms | 89 ms | 16× |
| Throughput | 69K ops/s | 1.12M ops/s | 16× |
| p50 | 574 ns | 554 ns | ~1× (fast path) |
| p90 | 61,286 ns | 997 ns | 61× |
| p99 | 110,210 ns | 1,783 ns | 62× |
| p99.9 | 192,135 ns | 8,349 ns | 23× |

## Scalability
1M operations previously did not complete in several minutes.
After optimization: 1.09 seconds (918K ops/sec sustained).

## Remaining tail latency
max at 1M: 12ms — attributed to `std::list` heap allocation
slow paths and/or WSL2 scheduler jitter. Next optimization
candidate: memory pool for order nodes.
