#include <stdio.h>
#include <unistd.h>

int main() {

  long sz = sysconf(_SC_PAGESIZE);
  printf("Page Size = %lu", sz);
}
