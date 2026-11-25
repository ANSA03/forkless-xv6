#include "kernel/types.h"
#include "user/user.h"

static volatile int marker = 7;

int
main(int argc, char **argv)
{
  if (argc == 0)
    exit(argv[0] == 0 ? 0 : 1);
  if (argc == 31 && strcmp(argv[0], "args") == 0) {
    for (int i = 1; i < argc; i++)
      if (strcmp(argv[i], "x"))
        exit(1);
    exit(argv[argc] == 0 ? 0 : 1);
  }
  if (argc == 2 && strcmp(argv[1], "guard") == 0) {
    uint64 sp;
    asm volatile("mv %0, sp" : "=r"(sp));
    *(volatile char *)((sp & ~4095L) - 4096) = 1;
    exit(1); // The guard write must fault instead.
  }
  if (argc == 2 && strcmp(argv[1], "quiet") == 0)
    exit(0);
  if (argc == 3 && strcmp(argv[1], "hold") == 0) {
    char c;
    read(atoi(argv[2]), &c, 1);
    exit(0);
  }
  if (argc == 3 && strcmp(argv[1], "cwd") == 0) {
    int fd = open("marker", 0);
    char c;
    if (fd < 0 || read(fd, &c, 1) != 1 || c != 'C')
      exit(1);
    close(fd);
    fd = atoi(argv[2]);
    if (read(fd, &c, 1) != 1 || c != 'B')
      exit(2);
    close(fd);
    exit(0);
  }
  if (argc != 3 || strcmp(argv[1], "hello") || strcmp(argv[2], "xv6"))
    exit(1);
  if (marker != 7)
    exit(2);
  marker = 99;
  printf("spawnchild: pid=%d argc=%d argv[1]=%s argv[2]=%s marker=%d\n",
         getpid(), argc, argv[1], argv[2], marker);
  exit(42);
}
