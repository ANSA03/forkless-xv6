# Five-minute live demonstration

## Preparation before the presentation

Build once and keep the source and results available. Do not run two QEMU instances against the same `fs.img`.

```sh
cd /home/osproject/xv6-riscv
git status --short
make -j4 kernel/kernel fs.img
```

Have `docs/VERIFICATION_BENCHMARK.md` open as a backup if the host becomes slow. It contains a recorded run, not a promised result for every machine. Avoid running the full regression suite during a five-minute presentation.

## 0:00–0:45 — State the problem and show kernel code

Say: “This xv6 fork allocates and copies mapped parent pages. A launcher then replaces those pages with exec. My spawn syscall constructs the requested executable directly.”

On the Ubuntu terminal:

```sh
rg -n 'kspawn|execimage|SYS_spawn|sys_spawn' kernel/proc.c kernel/exec.c kernel/sysfile.c kernel/syscall.c kernel/syscall.h
sed -n '/^kspawn(/,$p' kernel/proc.c
```

Point to `allocproc`, `execimage`, and the final `RUNNABLE` assignment. There is no `kfork` or `uvmcopy` call in kspawn. Explain that `execimage` receives the new child, while kexec passes the calling process. This distinguishes the mechanism from a user-space fork/exec wrapper.

## 0:45–1:00 — Boot

```sh
make qemu CPUS=1
```

Wait for the xv6 `$` prompt. The commands below run *inside xv6*, not the Ubuntu shell.

## 1:00–1:45 — Demonstrate construction and correctness

```text
spawntest
```

The first lines show parent PID, returned child PID, child argc and strings `hello` and `xv6`, and the child's marker value. The parent sets its own marker to 1234; the child verifies its executable's initial marker is 7 and changes it to 99. The test checks the parent still holds 1234 and verifies the child exits with status 42. The parent now waits before printing the returned PID to keep these demonstration lines together. The child is still independently scheduled.

The program then validates missing/non-ELF files, malformed ELF headers/segments, bad pointers, too many/oversized arguments, inherited cwd and shared descriptor offsets. It also accepts empty and 31-string argument vectors and deliberately faults a child on its guard page; that usertrap diagnostic is expected, and the test requires exit status -1. It performs 200 success/failure cycles, fills and recovers the process table, tests four concurrent spawners, kill/wait and orphan-parent exit, and exhausts/reclaims physical memory. Wait for `spawntest: ALL PASSED`.

On a fresh recorded boot there were 61 available process slots. Present that as an observation; the exact count depends on other processes. The allocation-budget failure count can also vary.

## 1:45–3:45 — Run the equivalent-work comparison

```text
spawnbench
```

While it runs, explain that each row represents 100 launches of the same silent executable followed by wait. There are five trials for each method with 0, 1, 4, and 8 MiB extra parent heap. Both methods check successful child completion. The timer is uptime ticks and the experiment includes loading, scheduling, execution, and cleanup.

The fresh pre-push verification means per 100 launches were:

| Extra parent heap | fork+exec ticks | spawn ticks |
| ---: | ---: | ---: |
| 0 MiB | 3.6 | 2.8 |
| 1 MiB | 20.6 | 3.0 |
| 4 MiB | 61.8 | 2.4 |
| 8 MiB | 100.0 | 3.0 |

These values belong to the attached QEMU run. Read the live results as they appear. Do not describe the small zero-heap difference as a precise speedup. If the VM is slower, let the benchmark finish rather than substituting recorded values for live measurements. Successful completion prints `spawnbench: COMPLETE`.

## 3:45–4:30 — Explain design decisions

Describe the fresh page table and zeroed trapframe, ELF-loaded text/data, guarded stack, `a0=argc`, `a1=argv`, and entry/SP. The child stays `USED` during loading so it cannot execute a partial image. Loading may sleep, so its spinlock is released. File descriptions and cwd use reference counts; they are shared resources even though user memory is independent. Parent attachment and publication follow wait-lock ordering.

On failure the loader frees its temporary image and kspawn frees the reserved process. After successful creation, existing exit and wait handle cleanup.

## 4:30–5:00 — Interpret and finish

Say: “The large-parent trend is consistent with avoiding eager page copying in this xv6 baseline. The measurement is completed-launch time, not isolated syscall latency. Linux copy-on-write and glibc spawn are different baselines. The contribution is this explicit kernel construction path and its tested lifecycle, not invention of spawn.”

Exit QEMU using Ctrl-A, release, then X. On Ubuntu, show the preserved evidence:

```sh
cat docs/VERIFICATION_BENCHMARK.md
rg 'ALL PASSED|ALL TESTS PASSED|fork test OK|COMPLETE' results/verification-20261101/single-cpu.log results/verification-20261101/three-cpu.log
```

For full reproduction outside the presentation:

```sh
python3 tools/run-spawn-tests.py --cpus 1 --log results/demo-rerun.log spawntest spawnbench 'usertests -q' forktest
```

The runner operates the real QEMU console and checks completion markers; it does not implement or simulate process creation.
