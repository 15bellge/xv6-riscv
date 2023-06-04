#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"
#include "kernel/riscv.h"

#define MAX_ALLOC 512

void* ustack_malloc(uint len);
int ustack_free(void);
