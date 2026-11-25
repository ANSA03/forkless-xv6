#include "kernel/types.h"
#include "user/user.h"

#define TRIALS 5
#define LAUNCHES 100

static int
run(int direct)
{
  char *args[] = {"spawnchild", "quiet", 0};
  int pid, s;
  if (direct) {
    pid = spawn("spawnchild", args);
  } else {
    pid = fork();
    if (pid == 0) {
      exec("spawnchild", args);
      exit(1);
    }
  }
  return pid > 0 && wait(&s) == pid && s == 0 ? 0 : -1;
}

int
main(void)
{
  int sizes[] = {0, 1, 4, 8};
  printf("spawnbench: uptime ticks; %d launches/batch; %d trials; warm cache\n",
         LAUNCHES, TRIALS);
  printf("BENCH,heap_MiB,trial,method,launches,ticks\n");
  for (int size = 0; size < 4; size++) {
    int bytes = sizes[size] * 1024 * 1024;
    char *heap = sbrk(bytes);
    if (heap == SBRK_ERROR)
      goto bad;
    for (int i = 0; i < bytes; i += 4096)
      heap[i] = 1;
    if (run(0) < 0 || run(1) < 0)
      goto bad;
    for (int trial = 0; trial < TRIALS; trial++) {
      // Alternate order to reduce a systematic first-method bias.
      for (int order = 0; order < 2; order++) {
        int direct = (trial + order) % 2;
        uint start = uptime();
        for (int i = 0; i < LAUNCHES; i++)
          if (run(direct) < 0)
            goto bad;
        uint elapsed = (uint)uptime() - start;
        printf("BENCH,%d,%d,%s,%d,%u\n", sizes[size], trial + 1,
               direct ? "spawn" : "fork_exec", LAUNCHES, elapsed);
      }
    }
    if (bytes && sbrk(-bytes) == SBRK_ERROR)
      goto bad;
  }
  printf("spawnbench: COMPLETE\n");
  exit(0);
bad:
  printf("spawnbench: FAILED\n");
  exit(1);
}
