# Viva preparation

These answers describe the implemented working tree. Start with the concise answer, then use the code references when asked for detail.

## Core operating-system concepts

**What is a process?** A running program together with its execution state and resources: address space, registers, PID, scheduling state, open files, and working directory. An executable file is a program on disk; a process is an instance executing it.

**What is fork?** A process-creation interface where the child initially continues the parent's program with logically separate memory. It returns the child PID in the parent and zero in the child. This xv6 implements separation by eager physical-page copying.

**What is exec?** Replacement of the calling process's user image with an executable. It keeps the PID. On success it begins the new program rather than returning to the old user instruction stream. The kernel helper returns argc so the syscall return machinery supplies the new program's first argument register.

**What is spawn in this project?** A syscall taking a path and argument vector. It reserves a new process, loads a fresh executable image into it, configures its registers and inherited resources, and makes it runnable. It returns a PID or `-1`; the child never returns from the parent's spawn call.

**Why does fork copy or share memory?** Its contract lets the child continue with the parent's existing data and execution context. Separate copies implement that directly. Sharing pages with copy-on-write can preserve the same logical behavior while postponing physical copies.

**What is copy-on-write?** Initially share physical pages under read-only mappings, track references, and handle a write fault by creating a private writable copy when needed. Both parent and child need correct write protection, reference accounting, and fault handling. This project does not implement COW; adding it would change the comparison baseline.

**How does this xv6 fork work?** `kernel/proc.c:kfork()` calls allocproc, then `uvmcopy()` in vm.c. It copies each valid mapped user page, copies the trapframe, sets child a0 to zero, duplicates descriptors/cwd, attaches the parent, and sets RUNNABLE. Missing lazy pages are skipped.

**What is a page table?** The hardware translation structure mapping virtual addresses to physical pages with permission bits. xv6 uses RISC-V Sv39's three-level scheme. Each process has its own user translations; equal virtual addresses in two processes need not refer to equal physical pages.

**What is a trapframe?** A per-process kernel-owned structure holding saved user registers and fields needed for trap transitions. It lets the kernel return to the correct user instruction with the correct registers. It differs from `struct context`, which stores kernel registers for switching between scheduler and process kernel execution.

**What is an ELF executable?** A structured binary containing an executable header and program headers describing loadable segments, virtual addresses, file bytes, memory sizes, permissions, and entry point. Zero-filled memory beyond file data supports BSS. This loader handles static xv6 ELF executables, not dynamic libraries or arbitrary Linux binaries.

**How does the scheduler start a new process?** allocproc initializes the kernel context's return address to forkret and its stack pointer to the process's kernel stack. Once RUNNABLE, the scheduler selects it, marks it RUNNING, and switches context. forkret releases the process lock and uses the return-to-user machinery and trampoline to restore the prepared trapframe.

**How are syscalls implemented?** user/usys.pl generates a stub loading the syscall number into a7 and issuing ecall. The trap path saves user state, syscall.c dispatches using a7, and the handler reads arguments from saved registers. The return value is written to a0. Spawn is number 23, declared in user/user.h and dispatched to sys_spawn.

## The implemented construction path

**Walk through spawn internally.** `sys_spawn()` delegates to `sys_launch(1)`, which bounds and copies the path and argument strings into kernel-owned buffers. `kspawn()` calls allocproc and releases the child lock while it is USED. It clears the trapframe, loads via execimage(child,...), duplicates file/cwd references, sets the parent under wait_lock, and publishes RUNNABLE under the child lock. The handler frees argument buffers and returns the PID.

**How is it different from wrapping fork and exec?** kspawn does not call kfork or uvmcopy. The new process never executes the parent's continuation and never receives its copied heap. It starts at the ELF entry point.

**Why are the demonstration markers volatile?** To force the test to perform actual loads and stores instead of allowing constant folding of private static state. Volatile is not a synchronization primitive and does not create isolation; separate page tables enforce isolation. Source inspection establishes that spawn never clones parent user pages.

**Why does the guard test print an unexpected usertrap?** That is xv6's general diagnostic for this deliberate user memory fault. The test specifically requires the spawned child to terminate with status -1. A kernel panic or a child reaching the instruction after the guard write would fail the test.

**Why refactor exec rather than copy its source?** The same segment checks, guarded-stack construction, argv layout, and cleanup should apply to both paths. `execimage(struct proc *,...)` makes the target explicit; `kexec()` preserves the original current-process interface.

**Which address space is used when reading argv?** The parent's. `fetchaddr` and `fetchstr` run before kspawn and use myproc. After copying, the loader receives kernel pointers and uses copyout to place data in the child's page table. It does not interpret a parent pointer as a child pointer.

**What registers are initialized?** The new trapframe is cleared. execimage assigns epc to ELF entry, sp to the aligned stack, a0 to argc, and a1 to argv. Kernel trap-return fields are supplied by the normal return machinery. Copying the parent's trapframe would unnecessarily inherit its user register state.

**Why explicitly set a0?** A fork child normally gets a0=0, and an exec caller gets argc through the syscall return path. Spawn's child has neither path. It must receive argc in its own trapframe before being scheduled.

**How are the stack and guard page created?** After rounding up the loaded image size, allocate USERSTACK+1 pages. Clear PTE_U on the first. Place aligned strings and the argv pointer array in the remaining stack page. Reject data that crosses stackbase.

**Why does allocproc create a page table if the loader creates another?** allocproc already owns a valid empty hierarchy containing special mappings. Reusing the established exec transaction means a replacement hierarchy is built and the empty original freed at commit. It costs a few temporary page-table pages but simplifies ownership. It does not copy user memory.

**Why is a spinlock released during loading?** Filesystem operations can block and schedule. Holding an unrelated spinlock across that sleep violates xv6's locking rules. USED reserves the slot while preventing scheduler selection, so the loader need not hold the lock continuously.

**Can wait reap an unfinished child?** The parent relationship is attached only after construction succeeds; the child is not ZOMBIE during loading. Publication holds wait_lock then child lock, respecting wait/exit order.

**Can the child exit before spawn returns?** Yes, especially on another CPU. The parent relation is established before publication, so wait can still collect its status. A caller must not infer execution order from printed lines.

**What is inherited?** The descriptor numbers refer to the same reference-counted open file descriptions, so offsets and pipe state are shared. The descriptor array itself is separate. Cwd gets an inode reference. User memory and general user register values are freshly constructed.

**Does independent memory mean completely independent resources?** No. Sharing open file descriptions and cwd is deliberate. A child writing a shared file or pipe can affect observable external state. This project provides normal address-space isolation, not filesystem transactions or sandboxing.

**What happens if loading fails halfway?** execimage frees its temporary image and inode/log resources. kspawn locks and frees the reserved process, including allocproc's trapframe and empty page table. Inheritance has not happened yet. The syscall frees argument buffers. No child becomes runnable.

**Does a failed spawn consume a PID?** It may. allocproc allocates the PID before loading. Failure frees the process slot but does not rewind the monotonic PID allocator; contiguous PID sequences are not guaranteed.

**How are invalid pointers handled?** The syscall copies user data with bounded fetch helpers. The argv pointer calculation checks for wraparound. Bad path pointers, arrays, strings, excessive argument counts, and unterminated strings return failure rather than being dereferenced directly in kernel C code. Existing lazy-allocation semantics still apply to readable user ranges.

**Which executable checks were added?** Format/machine/type/version and program-header bounds; segment file/memory sizes, overflow, alignment, ascending nonoverlap, virtual limit with stack room, and file bounds; plus a user-executable entry mapping. The loader remains intentionally smaller than a production ELF loader.

## Contribution, evidence, and limits

**What exactly is the contribution?** A concrete kernel-level executable-first child construction mechanism in this xv6 version, with a shared loader, explicit resource semantics, delayed runnable publication, cleanup paths, tests, and a controlled benchmark. The broad spawn concept is established prior work.

**What are its advantages?** It avoids allocating and copying the parent's mapped user image when launching another executable. It gives the caller a single launch operation and reports construction failure before publication. Tests and benchmarks support these statements within the stated scope.

**What does it give up?** There is no arbitrary child-side code between creation and exec, nor an API for descriptor actions, environment vectors, or POSIX attributes. All errors return -1. Fork remains available for continuing a computation in another process.

**How was correctness verified?** Build with warnings treated as errors; QEMU spawn tests covering arguments, PID/status, memory independence behavior, inheritance, bad inputs, malformed ELF, repeated failures/successes, process exhaustion, and allocation pressure; plus recorded existing xv6 regressions. Refer to TESTING.md for exact completed runs. Passing tests do not prove every concurrent interleaving.

**How were leaks checked?** Repeated create/reap and failure cycles, process-table exhaustion/recovery, and near-zero-memory budget recovery would expose many persistent resource losses. The final test also primes the parent page tables and compares recoverable physical-page counts before and after: the three-CPU run recovered 32,471 pages at both points. This is a capacity check, not a trace of every allocation or a proof that no resource can leak.

**What is a zombie?** An exited process retaining PID and exit status until its parent waits. exit closes files and releases cwd, while wait invokes final process cleanup. Spawn uses that same mechanism. If a parent exits first, the existing reparent logic transfers its children to init.

**How were performance measurements obtained?** Five batches of 100 sequential completed launches for each method at each eager heap size, timed with uptime. Same quiet child, same wait/status checking, warm-up, alternating method order, single QEMU CPU. Raw rows and a validating Python summarizer are included.

**Are these syscall latencies?** No. They are elapsed end-to-end launch, execute, exit, and wait times. Dividing by 100 gives mean completed-launch time in ticks. A precise syscall-only latency would require separate high-resolution instrumentation and careful treatment of fork's deferred exec work.

**What were the actual results?** Mean ticks per 100 launches for fork+exec versus spawn: 3.6 versus 2.8 at zero extra heap; 20.6 versus 3.0 at 1 MiB; 61.8 versus 2.4 at 4 MiB; 100.0 versus 3.0 at 8 MiB in the fresh pre-push verification run. The smallest differences are limited by tick quantization. The exact raw trial values are in results/verification-20261101/benchmark.csv. The original implementation run is preserved separately.

**How many pages were copied?** From the source, the extra eager heaps add 256, 1024, and 2048 copied user pages per fork at 1, 4, and 8 MiB. Spawn copies none of those heap pages. Those are analytical incremental counts, not instrumented total allocation counts; arguments and executable bytes still move.

**Would Linux show the same improvement?** That has not been tested. Linux typically uses copy-on-write for fork, and its glibc spawn uses a different creation strategy. The benchmark only establishes behavior for this eager-copy xv6 baseline in QEMU.

**Can spawn be slower?** It can be in other implementations or workloads if additional construction costs exceed avoided copying, or if measurement noise dominates. Here the zero-extra-heap result is close at coarse resolution. No broader guarantee follows from the large-parent measurements.

**How does this compare to μFork and Fork, Explore, Commit?** μFork addresses fork semantics in a single-address-space system with CHERI; this project uses separate page tables and starts a different executable. Fork, Explore, Commit addresses exploration isolation and commit/abort across filesystem/process state; this project does not provide those semantics. The report cites their checked abstracts and avoids importing performance claims into this project.

**Why call this research if spawn already exists?** The research component is a scoped question, a working mechanism, explicit design tradeoffs, a controlled baseline comparison, and reproducible observations. It is an implementation/evaluation study rather than a new universal process model.

**What would you improve next?** Add carefully scoped file actions and better errors, instrument allocations and high-resolution timing, stress concurrent spawners, compare with COW fork, and test more child images and native hardware. Each extension needs its own tests and honest baseline.

## Source map to study

| Function/file | What to explain |
| --- | --- |
| proc.c:allocproc | Reservation, trapframe, initial page table, kernel context |
| proc.c:kspawn | Fresh construction, inheritance, failure rollback, publication |
| exec.c:execimage | Shared ELF loader, private image, stack, commit |
| exec.c:kexec | Current-process wrapper preserving exec |
| sysfile.c:sys_launch | Parent argument validation/copy and buffer ownership |
| syscall.c / syscall.h / usys.pl | Dispatch and user-to-kernel entry |
| vm.c:uvmalloc / uvmcopy / copyout | Allocation, baseline eager copying, argument installation |
| proc.c:forkret / scheduler / kexit / kwait | Starting, running, and reclaiming the child |
| spawntest.c / spawnchild.c | Observable behavior and failure tests |
| spawnbench.c | Equivalent-work experiment and controls |
