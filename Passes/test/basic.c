#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

int PKEY = -1;

void gate_enter() { pkey_set(PKEY, 0x0); }
void gate_exit() { pkey_set(PKEY, 0x3); }

/*__attribute__((noinline))*/
uint8_t *trusted_malloc(size_t size, size_t align) {
  const size_t page_size = 4096;
  const int prot = PROT_READ | PROT_WRITE;
  const int fd = -1;
  const int flags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE;
  const int offset = 0;
  char *page = mmap(NULL, page_size, prot, flags, fd, offset);
  if (page == MAP_FAILED) {
    fprintf(stderr, "mmap size_of_segment=%ld *fd=%d failed %s\n", size, fd,
            strerror(errno));
    exit(EXIT_FAILURE);
  }
  pkey_mprotect(page, page_size, prot, PKEY);

  return (uint8_t *)page;
}

uint8_t *untrusted_malloc(size_t size, size_t align) { return malloc(size); }

void check(int *ptr) {
  assert(ptr != NULL && "checked ptr was NULL\n");
  *ptr = 10;
}

int main() {
  PKEY = pkey_alloc(0, 0);

  if (PKEY == -1) {
    fprintf(stderr, "pkey_alloc() failed\n");
    exit(EXIT_FAILURE);
  }
  printf("pkey = %d\n", PKEY);
  gate_enter();

  int *num_ptr = (int *)trusted_malloc(sizeof(int), _Alignof(int));

  check(num_ptr);
  gate_exit();
  printf("num = %d\n", *num_ptr);
  gate_enter();

  return 0;
}
