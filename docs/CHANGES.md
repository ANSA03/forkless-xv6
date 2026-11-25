# Source and artifact inventory

## Existing files changed

| File | Change and purpose |
| --- | --- |
| `kernel/proc.c` | Replace the pre-existing kspawn placeholder with direct child construction, fresh trapframe setup, inheritance, rollback, and locked publication |
| `kernel/exec.c` | Introduce execimage with an explicit target process; retain kexec wrapper; share stack/ELF handling and strengthen executable checks |
| `kernel/defs.h` | Declare execimage and kspawn |
| `kernel/sysfile.c` | Factor existing exec argument copying into sys_launch; add sys_spawn and pointer-array overflow check |
| `kernel/syscall.h` | Allocate syscall number 23 |
| `kernel/syscall.c` | Declare and dispatch sys_spawn |
| `user/user.h` | Declare spawn(path, argv) |
| `user/usys.pl` | Generate the spawn ecall stub |
| `Makefile` | Include three new user binaries in the filesystem image |

No scheduler, fork memory-copy algorithm, exit, wait, filesystem format, or shell launch policy was replaced. The branch remains spawn-development. No commit, reset, push, or publication was performed.

## New code

| File | Role |
| --- | --- |
| `user/spawnchild.c` | Executable target: argument/PID example, quiet workload, blocked child, cwd/descriptor checks, empty/maximum argument cases, and deliberate stack-guard fault |
| `user/spawntest.c` | Demonstration plus adversarial, exhaustion, concurrency, and recovery tests, plus kill/orphan lifecycle checks |
| `user/spawnbench.c` | Equivalent sequential fork+exec/spawn launches over four parent heap sizes |
| `tools/run-spawn-tests.py` | Real QEMU console automation, timeout, completion checking, transcript retention |
| `tools/summarize-spawn-bench.py` | Validate 40 recorded batches, extract CSV, generate summary table; optional output destinations preserve earlier runs |

`user/usys.S` and compiled binaries are generated build products, excluded by the existing .gitignore. The syscall source of truth is usys.pl.

## Documents and evidence

`SPAWN_README.md` is the project entry point. `docs/PROJECT.md` covers architecture and research scope; `IMPLEMENTATION_PLAN.md` records source inspection; `TESTING.md` records verification; `BENCHMARK.md` and `BENCHMARK_RESULTS.md` explain the experiment; `DEMO.md` and `VIVA.md` support presentation.

`results/build.log`, QEMU transcripts, `environment.txt`, and `benchmark.csv` preserve actual build, environment, and execution evidence. Tests run against the working tree; the recorded base Git commit does not include the uncommitted implementation.

The subsequent pre-push audit is in `docs/VERIFICATION.md`, with fresh results in `docs/VERIFICATION_BENCHMARK.md` and `results/verification-20261101/`. It strengthened user tests and the result-saving tool; it did not require a kernel change.
