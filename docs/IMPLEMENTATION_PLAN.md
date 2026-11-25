# Inspection and implementation plan

Inspection found branch `spawn-development`, base commit `e93dabeb98431235c6e1a101cb96c73c923fdbc4`, and one existing uncommitted edit: a `kspawn(void)` allocation/free placeholder appended to `kernel/proc.c`. No applicable repository AGENTS.md was found. The implementation replaced that placeholder without resetting the repository or switching branches.

The source uses kfork/kexec, an exec argument copier in sysfile.c, page-size-aware copyin/copyout, a lazy-allocation-capable VM, and a scheduler that selects only RUNNABLE processes.

1. Parameterize the existing ELF loader with the target process; retain kexec as a wrapper and its failure-preserves-old-image behavior.
2. Reserve an allocproc slot, load without holding a spinlock, initialize fresh registers, inherit reference-counted descriptors/cwd, and attach/publish in wait-lock order.
3. Share bounded argument copying between exec and spawn, then add syscall number, dispatch, declaration, stub, and build entries.
4. Build, boot, and exercise valid launches, invalid inputs, resource exhaustion, and existing process regressions.
5. Benchmark equivalent completed launches with controlled eager parent heaps, retain raw results, and document the implemented architecture and viva answers.

These steps were executed; the test record identifies exactly which validations completed.
