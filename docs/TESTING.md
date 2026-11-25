# Correctness and regression testing

See [the fresh pre-push verification](VERIFICATION.md) for the subsequent audit, strengthened tests, and new measurements. The table below preserves the original implementation evidence.

## Environment and evidence

Ubuntu ARM64 VM, RISC-V GCC 13.2.0, QEMU 8.2.2, 128 MiB guest memory. The timing run used one emulated CPU; the SMP check used three. See `results/environment.txt` for recorded versions and base Git commit. The implementation remains uncommitted on spawn-development.

The initial kernel build and filesystem-image build succeeded with `-Wall -Werror`. The linker reports xv6's existing kernel RWX LOAD-segment warning. `git diff --check` and Python script syntax checks succeeded.

## Executed tests

| Run | Result | Evidence |
| --- | --- | --- |
| Initial interactive one-CPU spawntest | Passed | Preliminary console observation; superseded by recorded tests |
| Recorded one-CPU spawntest including allocation pressure | ALL PASSED; 61 slots recovered; 14 partial-budget failures | `results/single-cpu.log` |
| One-CPU benchmark | All 40 batches / 4,000 measured launches completed | `results/single-cpu.log`, `results/benchmark.csv` |
| One-CPU usertests -q | ALL TESTS PASSED | `results/single-cpu.log` |
| One-CPU standalone forktest | fork test OK | `results/single-cpu.log` |
| Three-CPU spawntest (before concurrency/count additions) | ALL PASSED | `results/three-cpu.log` |
| Three-CPU full usertests, including all six slow tests | ALL TESTS PASSED | `results/three-cpu.log` |
| Three-CPU standalone forktest after full suite | fork test OK | `results/three-cpu.log` |
| Final strengthened three-CPU spawntest | ALL PASSED; four concurrent spawners; 32,471 recoverable pages both before and after | `results/final-spawn-smp.log` |
| Final strengthened one-CPU spawntest | ALL PASSED; four worker parents; 32,471 recoverable pages both before and after | `results/final-spawn-single.log` |

A test is not considered passed merely because it was launched. The benchmark and original regression transcripts predate the final test-only additions (concurrent workers and page counting); kernel changes afterward were comment/line formatting only. The final strengthened test was rebuilt and booted separately.

## What spawntest checks

- Distinct child PID, matching returned/waited PID, and exit status 42.
- Actual argument strings/argc received by a separately loaded executable.
- Parent marker remains 1234 while the child observes its executable initialization and modifies its own marker.
- No remaining child after wait; failed launches do not publish children.
- Missing file, ordinary non-ELF file, invalid path/argv/string pointers.
- Unterminated argument, too many arguments, and stack-capacity failure. The pre-push extension also tests empty argv and the maximum accepted 31 strings.
- Malformed program-header offset, virtual overflow, truncated segment, invalid entry, and non-executable entry.
- Relative file access through inherited cwd, shared open-file offset, separate descriptor slots, and close behavior.
- 200 alternating failed/successful launches with successful children reaped.
- Process table exhaustion using children blocked on a pipe, followed by release, wait, and a successful fresh launch.
- Complete eager-memory exhaustion and progressively released page budgets, followed by full recovery.
- Pre-push extensions: actual guard-page fault/status, kill/wait on a spawned child, and an intermediate parent exiting without waiting for its spawned child.

The final strengthened version also checks four concurrent parent workers performing 30 spawn/wait cycles each and compares recoverable physical pages before and after the suite. Its initial count primes parent page-table allocation, following the approach used by xv6 usertests. The count is an observation of recoverable capacity at those points, not a general allocation tracer.

The shared loader is also exercised by existing exec tests. Existing usertests cover fork, wait, exit, reparenting, memory permissions, guard-page faults, lazy memory, files, and pipes. Some intentionally trigger usertrap diagnostics; a diagnostic followed by that test's OK and ALL TESTS PASSED is expected protection behavior, not a kernel panic.

## Exact reproduction

Run one QEMU instance at a time. On Ubuntu:

```sh
cd /home/osproject/xv6-riscv
make -j4 kernel/kernel fs.img
python3 tools/run-spawn-tests.py --cpus 1 --log results/single-rerun.log spawntest spawnbench 'usertests -q' forktest
python3 tools/run-spawn-tests.py --cpus 3 --log results/smp-rerun.log spawntest usertests forktest
python3 tools/summarize-spawn-bench.py results/single-rerun.log
git diff --check
```

The runner defaults to a 1,200-second timeout per shell command. It exits with failure if a known test lacks its success marker, if QEMU exits early, if a kernel panic is detected, or if no shell prompt arrives before the timeout. It leaves a console transcript and terminates QEMU. It does not recreate the filesystem deliberately; ordinary make dependencies may rebuild it if inputs changed. Do not edit/rebuild fs.img while QEMU is using it.

For manual use, `make qemu CPUS=3`, then run `spawntest`, `usertests`, and `forktest` at the xv6 prompt. Exit QEMU with Ctrl-A then X.

## Limits

No formal verification, random failure injection at every allocator site, dedicated race detector, native RISC-V test, or filesystem crash/recovery campaign was performed. The spawn-specific physical-pressure experiment covers several budgets but does not identify every individual allocation site. Standard usertests run after spawn stress, so they can expose damage but do not independently account for every spawn resource. Parent-marker behavior demonstrates ordinary isolation; source inspection establishes that the implementation never clones parent user pages. No change should be described as universally leak-free or race-free solely from these runs.

## Completed pre-push re-verification

The fresh forced build, single-CPU benchmark and quick usertests/forktest, three-CPU full usertests/forktest, and complete final spawntest on both CPU configurations all passed. Both final spawn runs recovered 61 process slots and exactly 32,471 pages before/after. No functional test failed and no kernel panic was observed. The new guard fault is intentional and status-checked. See [VERIFICATION.md](VERIFICATION.md) for the exact evidence files, test improvements, commands, readiness assessment, and unexecuted checks. Earlier tables above remain historical records, not substitutes for the new runs.
