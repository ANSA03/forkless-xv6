# Pre-push verification

## Scope and repository state

Verification was performed in the existing `/home/osproject/xv6-riscv` working tree on `spawn-development`, based on commit `e93dabeb98431235c6e1a101cb96c73c923fdbc4`. The nine tracked modifications implementing spawn and all untracked project deliverables were inspected. No commit, push, reset, branch switch, or Git deletion was performed.

The tracked changes are Makefile, kernel/{proc.c,exec.c,defs.h,sysfile.c,syscall.c,syscall.h}, and user/{user.h,usys.pl}. New source includes spawnchild.c, spawntest.c, and spawnbench.c. Tools, documentation, and recorded results are also untracked and must be included in a future reviewed commit; pushing the existing base commit alone would not publish this work.

Fresh evidence is stored under `results/verification-20261101/`. Earlier transcripts, CSV, and the initial benchmark table were preserved. Environment and Git-state records accompany the new logs.

## Implementation audit

The syscall number, dispatch entry, user declaration, and generated stub agree on spawn syscall 23. Regenerating usys.S from usys.pl produced identical content. kspawn contains no kfork or uvmcopy call.

The construction path reserves a USED slot through allocproc, releases its spinlock before filesystem operations that can sleep, zeroes the child's trapframe, and calls the shared execimage loader with the child as its explicit target. Argument copying and pathname resolution use the calling parent's context. The ELF image, stack, argument vector, and entry/register state are complete before reference-counted file/cwd inheritance and publication.

The final parent attachment/publication uses wait_lock then the child lock. Scheduler selection requires RUNNABLE, so it cannot select the reserved slot during loading. Loader failure frees the temporary image and filesystem references; kspawn then frees the reserved trapframe and initial page table. Inherited resources are acquired only after fallible construction succeeds. Normal exit closes file/cwd references, and wait performs process reclamation.

No kernel correctness defect was found by this review or the completed execution checks. This is an evidence-based review, not a formal proof over all malformed inputs or scheduling interleavings.

## Improvements made during verification

The original marker demonstration used private ordinary static variables. Compiler optimization could remove the actual memory checks. Both markers are now volatile; rebuilt disassembly contains real accesses. The parent prints the returned PID after waiting to keep demonstration lines together.

Tests were extended to cover an empty argument vector, exactly 31 arguments, a spawned child's actual stack-guard fault, killing/reaping a spawned child, and a parent exiting without waiting for its spawned child. The orphan case is followed by normal init adoption and later process/memory recovery checks; the test does not claim to observe an orphan's exit status directly.

The benchmark summarizer accepts optional CSV/report destinations, allowing this verification's measurements to be saved without replacing earlier results. No kernel behavior needed correction during this pass.

## Build and execution record

| Check | Status | Evidence |
| --- | --- | --- |
| Forced full kernel and filesystem build | PASS, no compiler errors | `build.log` |
| Rebuild after final lifecycle test additions | PASS | `lifecycle-build.log` |
| One-CPU spawntest with argument/guard additions | ALL PASSED; 32,471 pages before/after | `single-cpu.log` |
| One-CPU benchmark | COMPLETE, 40 batches / 4,000 measured launches | `single-cpu.log`, `benchmark.csv` |
| One-CPU usertests -q and standalone forktest | ALL TESTS PASSED; fork test OK | `single-cpu.log` |
| Final three-CPU spawntest including kill/orphan tests | ALL PASSED; 61 slots recovered; 32,471 pages before/after | `three-cpu.log` |
| Three-CPU full usertests and standalone forktest | ALL TESTS PASSED, including all six slow tests; fork test OK | `three-cpu.log` |
| Final one-CPU spawntest with kill/orphan tests | ALL PASSED; 61 slots recovered; 32,471 pages before/after | `final-single-spawn.log` |
| Generated-stub consistency, Python syntax, whitespace | PASS | Host checks described below |

Evidence filenames in this table are relative to `results/verification-20261101/`. The first single-CPU run preceded only the final spawntest kill/orphan additions; the benchmark executable, child executable, and kernel were unchanged by those additions.

The full rebuild reports RWX LOAD-segment linker warnings for the kernel and forktest. These correspond to existing linker/build arrangements; no new linker script or permission layout was introduced. There were no compilation errors. Expected usertrap diagnostics occur in the deliberate guard test and several existing protection tests; the guard test requires child status -1. These are distinct from a kernel panic.

## Fresh performance results

Five trials of 100 completed launches per method and size, measured with xv6 uptime:

| Extra eager parent heap | fork+exec mean batch ticks | spawn mean batch ticks |
| ---: | ---: | ---: |
| 0 MiB | 3.6 | 2.8 |
| 1 MiB | 20.6 | 3.0 |
| 4 MiB | 61.8 | 2.4 |
| 8 MiB | 100.0 | 3.0 |

[Full ranges and throughput](VERIFICATION_BENCHMARK.md) are generated from the new CSV. Both paths launch spawnchild quiet and check PID, successful exit, and wait. These measure end-to-end loading, scheduling, execution, exit, and cleanup, not isolated syscall latency. Coarse ticks limit small differences. The large-parent behavior is consistent with avoiding eager parent-page copying; no Linux/COW performance claim follows.

## Reproduction

Run one QEMU instance against fs.img at a time. From the repository:

```sh
make -B -j4 kernel/kernel fs.img
python3 tools/run-spawn-tests.py --cpus 1 --log results/verification-rerun-single.log spawntest spawnbench 'usertests -q' forktest
python3 tools/run-spawn-tests.py --cpus 3 --log results/verification-rerun-smp.log spawntest usertests forktest
python3 tools/summarize-spawn-bench.py results/verification-rerun-single.log --csv results/verification-rerun.csv --report docs/VERIFICATION_RERUN_BENCHMARK.md
python3 -m py_compile tools/run-spawn-tests.py tools/summarize-spawn-bench.py
git diff --check
```

The fresh full build was forced without running make clean or deleting existing source/work. The runner checks success markers, detects kernel panic output, applies a timeout, and stops QEMU afterward. Physical-memory recovery is checked by eager allocation/free probes after priming parent page tables, not by a per-allocation trace.

## Remaining limitations and unexecuted checks

No native RISC-V run, Linux or copy-on-write baseline, systematic failure injection at every allocator site, race detector, formal proof, or filesystem crash/recovery campaign was executed. Timing does not isolate creation latency or measure total/peak allocation counts. Concurrency tests are finite. Memory-capacity equality and repeated successful creation are useful leak checks but cannot prove all reference counts correct under every failure/interleaving.

The API remains xv6-specific: static executable loading, 31 argument strings within a one-page stack, inherited descriptors/cwd, no environment vector, PATH search, descriptor actions, POSIX spawn attributes, or detailed errno. General register state is newly initialized; open-file descriptions deliberately remain shared. Research references already marked for further bibliographic/full-paper verification remain so; their experiments were not reproduced.

## Push readiness

Ready for a reviewed commit and subsequent GitHub push as an xv6 academic implementation. All planned functional verification runs in this pass completed successfully, with no failed functional test, observed kernel panic, or known blocking kernel defect. The remaining limitations above are explicit; no claim of production readiness or exhaustive correctness is made.

The complete final spawntest passed on one and three CPUs, each recovering 61 process slots and 32,471 physical pages before/after. The full SMP regression and standalone forktest passed. Documentation, demo, and viva measurements were updated to the fresh run while preserving the initial evidence. All changes remain uncommitted, and untracked source/documentation/results must be deliberately included in the future commit. No push was performed.
