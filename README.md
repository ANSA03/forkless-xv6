# Forkless

## Rethinking Process Creation in xv6 RISC-V

Forkless is an academic systems project based on MIT's xv6 RISC-V teaching operating system. It adds a kernel-level `spawn(path, argv)` system call that constructs a child directly from an executable, avoiding the parent address-space copy performed by xv6's `fork()` before `exec()` replaces that copy.

The Forkless contribution is the spawn syscall, shared executable-loading path, and accompanying tests, benchmarks, tooling, and documentation. The underlying operating system is xv6. Existing `fork()` and `exec()` interfaces remain available, and the shell continues to use fork/exec.

## Process creation

```c
int spawn(char *path, char **argv);
```

On success, `spawn` returns the child's PID to the parent. The child starts at the executable's entry point with its own user address space and receives `argc` and `argv`; it does not return from the parent's spawn call. Failure returns `-1`. The parent uses the existing `wait()` interface to reap the child and collect its exit status.

The implementation follows this construction path:

1. The syscall wrapper copies the pathname and argument strings from the calling process into kernel memory, validating user pointers and argument bounds.
2. `kspawn()` reserves a process slot through `allocproc()`. The child remains in `USED` state while construction proceeds. Its spinlock is released before loading operations that may sleep.
3. The shared `execimage()` loader validates the RISC-V ELF image and builds a fresh user page table, executable segments, a guarded stack, and the argument vector. It initializes the child's entry point, stack pointer, and argument registers. Both spawn and ordinary exec use this loader.
4. After loading succeeds, the child inherits reference-counted open-file descriptions and its parent's current directory. Descriptor slots are separate, while underlying file offsets are shared.
5. The kernel attaches the parent and marks the child `RUNNABLE`, using the existing wait-lock/child-lock order. Failed loading releases the temporary image and reserved process resources before publication.

The spawn path calls neither `kfork()` nor `uvmcopy()`. It still allocates and loads the new executable; the work avoided is cloning the parent's user memory.

### Implementation map

| Component | Source |
| --- | --- |
| Child construction and publication | [kernel/proc.c](kernel/proc.c) |
| Shared ELF loader and exec integration | [kernel/exec.c](kernel/exec.c) |
| User argument copying and syscall entry | [kernel/sysfile.c](kernel/sysfile.c) |
| Syscall 23 registration and dispatch | [kernel/syscall.h](kernel/syscall.h), [kernel/syscall.c](kernel/syscall.c) |
| User API and generated syscall stub | [user/user.h](user/user.h), [user/usys.pl](user/usys.pl) |
| Correctness tests and child workloads | [user/spawntest.c](user/spawntest.c), [user/spawnchild.c](user/spawnchild.c) |
| Benchmark workload | [user/spawnbench.c](user/spawnbench.c) |

## Features and scope

- Direct executable-based child creation with normal xv6 PID, exit, wait, and scheduling behavior.
- Shared ELF validation and image construction for spawn and exec.
- Inherited file descriptors and current directory, with a fresh executable address space and initialized register state.
- Tests for successful creation, invalid inputs, resource exhaustion and recovery, concurrent callers, and process lifecycle behavior.
- Reproducible QEMU test recording and benchmark summarization using Python's standard library.

This is an xv6-specific interface, not a POSIX `posix_spawn()` implementation. It supports at most 31 argument strings, subject to the one-page user stack's capacity. It does not provide an environment vector, PATH search, descriptor actions, spawn attributes, or detailed `errno` reporting. Forkless does not replace all uses of fork or claim to introduce spawn-style process creation as a new concept.

## Build and run

Requirements: GNU Make, a host C compiler, RISC-V GCC/binutils, QEMU with `qemu-system-riscv64` support (version 7.2 or later, as checked by the Makefile), and `bc`. Python 3 is needed for the automated tools; no additional Python packages are required.

From the repository root:

```sh
make -j4 kernel/kernel fs.img
make qemu CPUS=1
```

The Makefile detects supported RISC-V toolchain prefixes. If needed, specify yours explicitly, for example:

```sh
make qemu CPUS=1 TOOLPREFIX=riscv64-unknown-elf-
```

At the xv6 shell:

```text
spawntest
spawnbench
```

Exit QEMU with **Ctrl-A**, then **X**. Run only one QEMU instance against `fs.img` at a time, and stop it before rebuilding the image.

## Testing

`spawntest` checks PID/wait/status behavior, argument boundaries, executable memory isolation, invalid pointers and malformed ELF rejection, stack-guard protection, inherited cwd and file offsets, repeated success/failure cycles, process and memory exhaustion recovery, concurrent spawners, kill/wait, and parent exit with an outstanding spawned child.

Existing recorded verification includes passing spawn tests on one and three emulated CPUs, one-CPU `usertests -q`, three-CPU full `usertests`, and standalone `forktest`. These are previously recorded results, not tests rerun for this README update. See the [verification report](docs/VERIFICATION.md) and [raw transcripts](results/verification-20261101/) for exact coverage and evidence.

To run a new verification and preserve separate logs:

```sh
python3 tools/run-spawn-tests.py --cpus 1 --log results/readme-rerun-single.log spawntest spawnbench 'usertests -q' forktest
python3 tools/run-spawn-tests.py --cpus 3 --log results/readme-rerun-smp.log spawntest usertests forktest
```

Run these commands sequentially. The runner records console output, checks known success markers, detects kernel panic output, applies timeouts, and terminates QEMU afterward. Finite stress tests and memory-capacity recovery checks do not establish exhaustive correctness or freedom from every race or resource leak.

## Benchmarking

`spawnbench` compares `fork()` followed by `exec()` against `spawn()` using the same child executable and checked exit/wait behavior. It touches 0, 1, 4, or 8 MiB of extra parent heap, warms both paths, and alternates their order across five trials of 100 sequential launches per method and heap size.

Measurements use coarse xv6 uptime ticks for complete launch/execute/exit/wait batches. They include loading, scheduling, and cleanup; they do not isolate syscall latency or measure peak memory allocation. The baseline is xv6's eager-copy fork under QEMU, so these measurements do not establish performance relative to Linux or copy-on-write fork.

Recorded measurements, ranges, and limitations are available in the [benchmark methodology](docs/BENCHMARK.md), [initial results](docs/BENCHMARK_RESULTS.md), and [later verification results](docs/VERIFICATION_BENCHMARK.md). Raw logs and CSV files are retained in [results/](results/).

After the one-CPU test command above, generate a separate report without replacing the existing result sets:

```sh
python3 tools/summarize-spawn-bench.py results/readme-rerun-single.log --csv results/readme-rerun-benchmark.csv --report docs/README_RERUN_BENCHMARK.md
```

The summarizer requires all 40 unique batches and the benchmark completion marker.

## Documentation

| Document | Contents |
| --- | --- |
| [Project report](docs/PROJECT.md) | Architecture, design rationale, and limitations |
| [Testing](docs/TESTING.md) | Correctness coverage and reproduction commands |
| [Verification](docs/VERIFICATION.md) | Implementation audit and recorded execution evidence |
| [Benchmark methodology](docs/BENCHMARK.md) | Experimental controls and interpretation limits |
| [Initial benchmark results](docs/BENCHMARK_RESULTS.md) | Initial recorded measurements |
| [Verification benchmark results](docs/VERIFICATION_BENCHMARK.md) | Later recorded measurements |
| [Demonstration](docs/DEMO.md) | Guided project walkthrough |
| [Viva preparation](docs/VIVA.md) | Design discussion and questions |
| [Change inventory](docs/CHANGES.md) | Implementation file changes |
| [Implementation plan](docs/IMPLEMENTATION_PLAN.md) | Inspection and development notes |

## Author

Forkless modifications and project work: [ANSA03](https://github.com/ANSA03).

## Acknowledgments and license

Forkless is based on [MIT xv6 RISC-V](https://github.com/mit-pdos/xv6-riscv), a teaching operating system inspired by Unix Version 6. Credit for the original operating system belongs to its authors and contributors. The Forkless additions described above extend that foundation; they do not represent authorship of the original xv6 kernel.

The original xv6 README, including its contributor acknowledgments, is preserved verbatim in [README.xv6-original](README.xv6-original). The original [README](README) is also retained. The xv6 MIT license and copyright notice remain in [LICENSE](LICENSE); original source notices and acknowledgments are preserved.
