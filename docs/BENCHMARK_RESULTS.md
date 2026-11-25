# Recorded benchmark results

This table preserves the initial implementation run. See [the fresh pre-push run](VERIFICATION_BENCHMARK.md) for the later verification results.

Source: `results/single-cpu.log`. Five batches of 100 launches per cell. All times are xv6 uptime ticks.

| Extra parent heap (MiB) | Method | Mean batch ticks | Median | Range | Launches/tick (aggregate) |
| ---: | --- | ---: | ---: | --- | ---: |
| 0 | fork_exec | 3.4 | 3 | 3–4 | 29.41 |
| 0 | spawn | 3.0 | 3 | 3–3 | 33.33 |
| 1 | fork_exec | 20.4 | 20 | 20–21 | 4.90 |
| 1 | spawn | 3.0 | 3 | 3–3 | 33.33 |
| 4 | fork_exec | 59.8 | 61 | 54–64 | 1.67 |
| 4 | spawn | 2.6 | 2 | 2–4 | 38.46 |
| 8 | fork_exec | 101.0 | 100 | 100–103 | 0.99 |
| 8 | spawn | 3.0 | 3 | 3–3 | 33.33 |

These are end-to-end launch/execute/wait measurements, including scheduling and cleanup.
A tick is coarse: small differences at the smallest heap size should not be treated as precise speedups.
Throughput is sequential completed launches per tick, not concurrent capacity.
See BENCHMARK.md for controls, memory analysis, and limitations.
