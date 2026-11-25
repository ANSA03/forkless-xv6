# xv6 direct process construction

This project implements `spawn(path, argv)` as syscall 23. It creates a new child from an ELF executable without calling fork or copying the parent's user address space. The existing shell and fork/exec APIs remain available.

```sh
cd /home/osproject/xv6-riscv
make -j4 kernel/kernel fs.img
make qemu CPUS=1
```

At the xv6 shell:

```text
spawntest
spawnbench
```

Exit QEMU with Ctrl-A then X. For automated recording:

```sh
python3 tools/run-spawn-tests.py --cpus 1 --log results/rerun.log spawntest spawnbench 'usertests -q' forktest
python3 tools/summarize-spawn-bench.py results/rerun.log
```

Do not run multiple QEMU instances on the same filesystem image. No extra Python packages are needed.

- [Pre-push verification report](docs/VERIFICATION.md) and [fresh benchmark](docs/VERIFICATION_BENCHMARK.md)
- [Technical report and architecture](docs/PROJECT.md)
- [Test record and commands](docs/TESTING.md)
- [Benchmark methodology](docs/BENCHMARK.md) and [actual results](docs/BENCHMARK_RESULTS.md)
- [Five-minute demonstration](docs/DEMO.md)
- [Viva preparation](docs/VIVA.md)
- [Modified files](docs/CHANGES.md)
- [Inspection and implementation plan](docs/IMPLEMENTATION_PLAN.md)

Raw evidence is in `results/`. Benchmarks measure completed launch/execute/wait batches using coarse xv6 ticks. Results apply to this eager-copy xv6 implementation under QEMU, not a Linux copy-on-write baseline. Spawn-style creation is established prior work; the report describes the specific implementation contribution and limitations.
