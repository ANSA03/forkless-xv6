# Recorded benchmark results

Source: `results/verification-20261101/single-cpu.log`. Five batches of 100 launches per cell. All times are xv6 uptime ticks.

| Extra parent heap (MiB) | Method | Mean batch ticks | Median | Range | Launches/tick (aggregate) |
| ---: | --- | ---: | ---: | --- | ---: |
| 0 | fork_exec | 3.6 | 4 | 3–4 | 27.78 |
| 0 | spawn | 2.8 | 3 | 2–3 | 35.71 |
| 1 | fork_exec | 20.6 | 21 | 20–21 | 4.85 |
| 1 | spawn | 3.0 | 3 | 3–3 | 33.33 |
| 4 | fork_exec | 61.8 | 62 | 58–64 | 1.62 |
| 4 | spawn | 2.4 | 2 | 2–3 | 41.67 |
| 8 | fork_exec | 100.0 | 100 | 100–100 | 1.00 |
| 8 | spawn | 3.0 | 3 | 3–3 | 33.33 |

These are end-to-end launch/execute/wait measurements, including scheduling and cleanup.
A tick is coarse: small differences at the smallest heap size should not be treated as precise speedups.
Throughput is sequential completed launches per tick, not concurrent capacity.
See BENCHMARK.md for controls, memory analysis, and limitations.
