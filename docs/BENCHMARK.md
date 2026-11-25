# Benchmark methodology

## Question and controlled work

Does directly loading a small child avoid the cost associated with copying a larger parent's resident user memory in this xv6 version?

`user/spawnbench.c` compares `fork(); exec("spawnchild", ...); wait(...)` with `spawn("spawnchild", ...); wait(...)`. Both launch exactly `spawnchild quiet`, which exits zero without console output. Every child is reaped before the next launch; failures stop the benchmark and prevent a COMPLETE marker. The parent checks both PID and exit status. The source does not insert pause calls or unrelated work into either measured path.

The experiment uses extra eager parent heaps of 0, 1, 4, and 8 MiB. Each page is touched. Zero MiB still includes the benchmark's normal code, data, guard, and stack. The heap is released before moving to the next size. Each method receives one unmeasured warm-up launch per size. There are five trials of 100 completed launches per method per size, totaling 4,000 measured launches and eight warm-ups. Method order alternates across trials; five trials leave one extra first position for fork+exec.

## Timing and interpretation

Timing uses `uptime()` before and after each batch, with unsigned subtraction. All reported values are scheduler-clock ticks, not CPU cycles or host nanoseconds. The kernel arms timer interrupts at 1,000,000 `time` counter units. Converting that to physical time would additionally require verifying the platform timebase and interrupt behavior; the tables retain ticks instead.

For a batch of N launches taking T ticks:

```text
average elapsed ticks per completed launch = T / N
aggregate sequential throughput = sum(N) / sum(T)
```

These averages include argument copying, allocation, executable I/O, scheduling, child execution, exit, wait, and cleanup. They are not isolated creation latencies. The parent's fork return occurs before its child executes exec, whereas spawn returns after loading; comparing syscall return times alone would be unfair for this question.

The default recorded comparison uses one QEMU CPU, 128 MiB RAM, and otherwise idle xv6. SMP correctness is tested separately. QEMU runs in an Ubuntu ARM64 VM; host scheduling, thermal state, emulation, and caches introduce variation. Warm-up reduces cold-cache effects but does not guarantee all inode/file buffers remain cached. File loading still uses xv6's buffer cache normally.

## Memory allocation and copied-page analysis

Source inspection establishes that spawn never calls `uvmcopy()` or `kfork()`. It copies argument data from the parent through kernel buffers and copies those arguments onto a new stack; “no parent address-space copy” does not mean zero copied bytes. ELF bytes must also be loaded.

`uvmcopy()` allocates one 4096-byte physical page and performs a page-sized memmove for each valid mapped parent page, including the guard page. The benchmark's added eager heap therefore contributes the following *analytical incremental counts per fork*:

| Extra heap | Additional copied user pages | Additional copied bytes |
| ---: | ---: | ---: |
| 0 MiB | 0 | 0 |
| 1 MiB | 256 | 1,048,576 |
| 4 MiB | 1,024 | 4,194,304 |
| 8 MiB | 2,048 | 8,388,608 |

These exclude the parent's baseline mappings, page-table pages, trapframe, and new executable allocations. They are derived from code and controlled eager heap sizes, not runtime allocation-counter measurements. Spawn contributes zero copies of those heap pages at every size. Both methods still allocate the new executable. Spawn also briefly owns allocproc's empty page table alongside the loader's replacement table. No measured total/peak allocator count is claimed.

## Reproduction

From the repository on the host, with no other QEMU instance using fs.img:

```sh
make -j4 kernel/kernel fs.img
python3 tools/run-spawn-tests.py --cpus 1 --log results/benchmark-rerun.log spawnbench
python3 tools/summarize-spawn-bench.py results/benchmark-rerun.log
```

The summarizer validates all 40 batches and the COMPLETE marker, writes `results/benchmark.csv`, and regenerates `docs/BENCHMARK_RESULTS.md`. Preserve an earlier CSV/report if needed before rerunning, since these derived files are replaced. The raw log is the source of truth.

Interactively, use `make qemu CPUS=1`, then run `spawnbench` at the xv6 shell. Exit QEMU with Ctrl-A followed by X. The shell itself still uses fork/exec; only the explicit spawn calls in the programs use the new syscall.

## Threats to validity

One boot with five repeated batches is not five independent machine configurations. Coarse ticks especially limit conclusions for the smallest parent. No confidence intervals or precise physical-time speedup are asserted. The experiment does not measure cold boot, parallel throughput, large argument vectors, large child images, storage-limited workloads, or a copy-on-write baseline. A more expensive child could hide launch overhead. Spawn could lose its advantage where loading dominates or implementation costs exceed avoided copying; only observations in the attached run are established. The source-based page analysis explains a mechanism but cannot replace allocation instrumentation.

## Observed behavior

The recorded table shows means of 3.4 versus 3.0 ticks at zero extra heap, 20.4 versus 3.0 at 1 MiB, 59.8 versus 2.6 at 4 MiB, and 101.0 versus 3.0 at 8 MiB, respectively for fork+exec and spawn. At zero extra heap, both methods had batches taking three ticks; this measurement cannot establish a precise small-workload advantage. No spawn batch exceeded the paired fork+exec batch in this run, but that is not a universal guarantee.

The increase in fork+exec time with eager heap size is consistent with its page allocation and copying work. Spawn's results stayed in the two-to-four-tick range and did not grow with this extra parent memory. The 4 MiB spawn mean being smaller than the 1 MiB mean is not evidence that larger parents accelerate spawn: tick phase, scheduling, and host variability can explain differences of this size. These observations support the intended mechanism within this setup while leaving finer latency and allocator effects unmeasured.

## Pre-push replication

The later verification run is recorded separately in [VERIFICATION_BENCHMARK.md](VERIFICATION_BENCHMARK.md) and `results/verification-20261101/benchmark.csv`. Means for fork+exec versus spawn were 3.6/2.8, 20.6/3.0, 61.8/2.4, and 100.0/3.0 batch ticks at 0, 1, 4, and 8 MiB extra parent heap. All 40 batches completed. Earlier raw data and the original table were preserved. The variation between the two runs reinforces the caution about coarse timing, particularly for small parents.

The summarizer now accepts optional output paths to preserve an earlier result set:

```sh
python3 tools/summarize-spawn-bench.py results/verification-20261101/single-cpu.log --csv results/verification-20261101/benchmark.csv --report docs/VERIFICATION_BENCHMARK.md
```
