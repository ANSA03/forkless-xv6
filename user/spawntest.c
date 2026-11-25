#include "kernel/types.h"
#include "kernel/param.h"
#include "kernel/fcntl.h"
#include "kernel/elf.h"
#include "user/user.h"

static volatile int marker = 7;

static void
check(int ok, char *what)
{
  if (!ok) {
    printf("spawntest: FAIL %s\n", what);
    exit(1);
  }
}

static void
reap(int pid, int status)
{
  int s = -1;
  check(pid > 0 && wait(&s) == pid && s == status, "pid/wait/status");
}

static void
badelf(int kind)
{
  struct elfhdr e;
  struct proghdr p;
  memset(&e, 0, sizeof(e));
  memset(&p, 0, sizeof(p));
  e.magic = ELF_MAGIC;
  e.elf[0] = 2;
  e.elf[1] = 1;
  e.type = 2;
  e.machine = 243;
  e.version = 1;
  e.phentsize = sizeof(p);
  e.phnum = 1;
  e.phoff = sizeof(e);
  p.type = ELF_PROG_LOAD;
  p.flags = ELF_PROG_FLAG_EXEC | ELF_PROG_FLAG_READ;
  p.memsz = 4096;
  if (kind == 0)
    e.phoff = 0xffffffffffffffffL;
  if (kind == 1)
    p.memsz = 0xffffffffffffffffL;
  if (kind == 2)
    p.filesz = 4096; // truncated file
  if (kind == 3)
    e.entry = 8192; // entry outside image
  if (kind == 4)
    p.flags = ELF_PROG_FLAG_READ;
  int fd = open("badspawn", O_CREATE | O_WRONLY | O_TRUNC);
  check(fd >= 0, "create malformed ELF");
  check(write(fd, &e, sizeof(e)) == sizeof(e), "write ELF header");
  check(write(fd, &p, sizeof(p)) == sizeof(p), "write program header");
  close(fd);
  check(spawn("badspawn", (char *[]){"badspawn", 0}) == -1, "malformed ELF");
  unlink("badspawn");
}

// Like usertests countfree: prime parent page-table structure on the first
// pass, then compare recoverable physical pages after all child cleanup.
static int
countfree(void)
{
  int pages = 0;
  while (sbrk(4096) != SBRK_ERROR)
    pages++;
  check(sbrk(-pages * 4096) != SBRK_ERROR, "countfree cleanup");
  return pages;
}

int
main(void)
{
  int pid, fd, s;
  char *args[] = {"spawnchild", "hello", "xv6", 0};
  countfree();
  int free_before = countfree();
  marker = 1234;
  printf("spawntest: parent pid=%d marker=%d\n", getpid(), marker);
  pid = spawn("spawnchild", args);
  reap(pid, 42);
  printf("spawntest: returned child pid=%d\n", pid);
  check(marker == 1234, "parent memory unchanged");
  check(wait(&s) == -1, "no zombie remains");
  check(spawn("missing-spawn", args) == -1, "missing executable");
  check(spawn("README", args) == -1, "non-ELF file");
  check(spawn((char *)~0L, args) == -1, "invalid path pointer");
  check(spawn("spawnchild", (char **)~0L) == -1, "invalid argv pointer");
  check(spawn("spawnchild", (char *[]){(char *)~0L, 0}) == -1,
        "invalid argument string");
  char *many[MAXARG + 1];
  for (int i = 0; i < MAXARG; i++)
    many[i] = "x";
  many[MAXARG] = 0;
  check(spawn("spawnchild", many) == -1, "too many arguments");
  many[0] = "args";
  many[MAXARG - 1] = 0;
  reap(spawn("spawnchild", many), 0);
  reap(spawn("spawnchild", (char *[]){0}), 0);
  reap(spawn("spawnchild", (char *[]){"spawnchild", "guard", 0}), -1);
  printf("spawntest: argument boundaries and stack guard passed\n");
  char *large = sbrk(8192);
  check(large != SBRK_ERROR, "argument memory");
  memset(large, 'x', 8192);
  check(spawn("spawnchild", (char *[]){large, 0}) == -1, "unterminated argument");
  large[4095] = 0;
  check(spawn("spawnchild", (char *[]){large, 0}) == -1, "stack overflow");
  check(sbrk(-8192) != SBRK_ERROR, "release argument memory");
  for (int i = 0; i < 5; i++)
    badelf(i);
  printf("spawntest: argument and ELF rejection passed\n");

  // A shared file description has a shared offset; closing in child is local.
  mkdir("spawndir");
  check(chdir("spawndir") == 0, "chdir");
  fd = open("marker", O_CREATE | O_RDWR | O_TRUNC);
  check(fd >= 0 && write(fd, "C", 1) == 1, "cwd marker");
  close(fd);
  fd = open("offset", O_CREATE | O_RDWR | O_TRUNC);
  check(fd >= 0 && write(fd, "ABC", 3) == 3, "offset file");
  close(fd);
  fd = open("offset", O_RDONLY);
  char c, number[4];
  check(fd >= 0 && read(fd, &c, 1) == 1 && c == 'A', "parent first read");
  number[0] = '0' + fd / 10;
  number[1] = '0' + fd % 10;
  number[2] = 0;
  reap(spawn("/spawnchild", (char *[]){"spawnchild", "cwd", number, 0}), 0);
  check(read(fd, &c, 1) == 1 && c == 'C', "shared offset and separate fd table");
  close(fd);
  unlink("marker");
  unlink("offset");
  check(chdir("/") == 0 && unlink("spawndir") == 0, "cwd cleanup");

  for (int i = 0; i < 200; i++) {
    check(spawn("missing-spawn", args) == -1, "repeated failed spawn");
    reap(spawn("spawnchild", (char *[]){"spawnchild", "quiet", 0}), 0);
  }
  printf("spawntest: 200 success/failure cycles passed\n");

  // Block children on a pipe until process-table allocation fails.
  int fds[2], count = 0;
  check(pipe(fds) == 0, "pipe");
  number[0] = '0' + fds[0] / 10;
  number[1] = '0' + fds[0] % 10;
  number[2] = 0;
  while ((pid = spawn("spawnchild", (char *[]){"spawnchild", "hold", number, 0})) > 0) {
    count++;
    check(count < NPROC, "bounded process table");
  }
  check(count > 0 && pid == -1, "process exhaustion");
  for (int i = 0; i < count; i++)
    check(write(fds[1], "x", 1) == 1, "release held child");
  for (int i = 0; i < count; i++)
    check(wait(&s) > 0 && s == 0, "reap exhausted table");
  pid = spawn("spawnchild", (char *[]){"spawnchild", "hold", number, 0});
  check(pid > 0 && kill(pid) == 0, "kill spawned child");
  reap(pid, -1);
  close(fds[0]);
  close(fds[1]);
  reap(spawn("spawnchild", (char *[]){"spawnchild", "quiet", 0}), 0);
  printf("spawntest: exhausted and recovered %d slots\n", count);
  // Different parent processes construct children concurrently on SMP.
  for (int worker = 0; worker < 4; worker++) {
    pid = fork();
    check(pid >= 0, "concurrent worker fork");
    if (pid == 0) {
      for (int i = 0; i < 30; i++)
        reap(spawn("spawnchild", (char *[]){"spawnchild", "quiet", 0}), 0);
      exit(0);
    }
  }
  for (int worker = 0; worker < 4; worker++)
    check(wait(&s) > 0 && s == 0, "concurrent spawner status");
  printf("spawntest: four concurrent spawners passed\n");

  // Leave a spawned child for init to adopt, whether already exited or not.
  pid = fork();
  check(pid >= 0, "orphan worker fork");
  if (pid == 0) {
    int child = spawn("spawnchild", (char *[]){"spawnchild", "quiet", 0});
    exit(child < 0 ? 1 : 0);
  }
  reap(pid, 0);
  check(wait(&s) == -1, "grandchild is not a direct child");
  printf("spawntest: kill/wait and orphan-parent exit passed\n");

  // Force allocation failures, then progressively release enough pages for
  // argument copying, allocproc, page tables, and ELF/stack construction.
  int allocated = 0;
  while (sbrk(1024 * 1024) != SBRK_ERROR)
    allocated += 1024 * 1024;
  while (sbrk(4096) != SBRK_ERROR)
    allocated += 4096;
  check(spawn("spawnchild", (char *[]){"spawnchild", "quiet", 0}) == -1,
        "out-of-memory rejection");
  int failures = 0;
  for (int i = 0; i < 24; i++) {
    check(sbrk(-4096) != SBRK_ERROR, "release pressure page");
    allocated -= 4096;
    pid = spawn("spawnchild", (char *[]){"spawnchild", "quiet", 0});
    if (pid < 0)
      failures++;
    else
      reap(pid, 0);
  }
  check(sbrk(-allocated) != SBRK_ERROR, "release pressure heap");
  reap(spawn("spawnchild", (char *[]){"spawnchild", "quiet", 0}), 0);
  printf("spawntest: memory pressure recovered (%d partial-budget failures)\n",
         failures);
  int free_after = countfree();
  check(free_after >= free_before, "recoverable physical pages");
  printf("spawntest: recoverable pages before=%d after=%d\n",
         free_before, free_after);
  printf("spawntest: ALL PASSED\n");
  exit(0);
}
