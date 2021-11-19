#ifndef MPK_FORMATTER_H
#define MPK_FORMATTER_H

#include "alloc_site_handler.h"
#include "provsan_common.h"

namespace __provsan {

void flush_allocs();

} // namespace __provsan

extern "C" {
// Registers flush_allocs to be called at program exit.
__attribute__((visibility("default"))) static void __attribute__((constructor))
register_flush_allocs();
}

#endif
