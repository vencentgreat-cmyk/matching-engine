# Baseline: O(n) cancel

Workload: 100,000 operations, 75% add + 25% cancel (mixed)
Environment: WSL2 Ubuntu 24, g++-14, single thread

## Results
- Operations:  100,000
- Adds:        75,149
- Cancels:     24,851 (hit rate: 22%)
- Trades:      58,229
- Total time:  1,448 ms
- Throughput:  69,047 ops/sec

## Latency distribution
- p50:    574 ns
- p90:    61,286 ns
- p99:    110,210 ns
- p99.9:  192,135 ns
- max:    802,892 ns

## Note
1M operations attempted but did not complete within several minutes
due to O(n) cancel becoming pathological at scale.
