#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/times.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <stdarg.h>
#include <stdio.h>
#include <errno.h>

#include <sys/debug.hpp>
#include <sys/gthread.hpp>
#include <sys/pmem.hpp>
#include <sys/vmem.hpp>

extern "C" int ebbrt_newlib_exit(int val) {
  UNIMPLEMENTED();
  return 0;
}

extern "C" int ebbrt_newlib_execve(char *name, char **argv, char **env) {
  UNIMPLEMENTED();
  return 0;
}

extern "C" int ebbrt_newlib_getpid(void) {
  UNIMPLEMENTED();
  return 0;
}

extern "C" int ebbrt_newlib_fork(void) {
  UNIMPLEMENTED();
  return 0;
}

extern "C" int ebbrt_newlib_kill(int pid, int sig) {
  UNIMPLEMENTED();
  return 0;
}

extern "C" int ebbrt_newlib_wait(int *status) {
  UNIMPLEMENTED();
  return 0;
}

extern "C" int ebbrt_newlib_isatty(int fd) {
  UNIMPLEMENTED();
  return 0;
}

extern "C" int ebbrt_newlib_close(int file) {
  UNIMPLEMENTED();
  return 0;
}

extern "C" int ebbrt_newlib_link(char *path1, char *path2) {
  UNIMPLEMENTED();
  return 0;
}

extern "C" int ebbrt_newlib_lseek(int file, int ptr, int dir) {
  UNIMPLEMENTED();
  return 0;
}

extern "C" int ebbrt_newlib_open(const char *name, int flags, va_list list) {
  UNIMPLEMENTED();
  return 0;
}

extern "C" int ebbrt_newlib_read(int file, char *ptr, int len) {
  UNIMPLEMENTED();
  return 0;
}

extern "C" int ebbrt_newlib_fstat(int file, struct stat *st) {
  UNIMPLEMENTED();
  return 0;
}

extern "C" int ebbrt_newlib_stat(const char *file, struct stat *st) {
  UNIMPLEMENTED();
  return 0;
}

extern "C" int ebbrt_newlib_unlink(char *name) {
  UNIMPLEMENTED();
  return 0;
}

extern "C" int ebbrt_newlib_write(int file, char *ptr, int len) {
  UNIMPLEMENTED();
  return 0;
}

uint64_t heap_offset;
uint64_t heap_allocated_offset;

extern "C" caddr_t ebbrt_newlib_sbrk(int incr) {
  constexpr uint64_t HEAP_START = 0xFFFF800000000000;
  // Allocate 2mb page chunks
  constexpr uint64_t HEAP_ALLOCATION_SIZE = 1 << 21;
  while (heap_offset + incr > heap_allocated_offset) {
    auto pmem = ebbrt::pmem::allocate_page(HEAP_ALLOCATION_SIZE);
    if (pmem == 0) {
      errno = ENOMEM;
      return reinterpret_cast<caddr_t>(-1);
    }
    ebbrt::vmem::map(HEAP_START + heap_allocated_offset, pmem,
                     HEAP_ALLOCATION_SIZE);
    heap_allocated_offset += HEAP_ALLOCATION_SIZE;
  }

  auto ret = HEAP_START + heap_offset;
  heap_offset += incr;
  return reinterpret_cast<caddr_t>(ret);
}

extern "C" int ebbrt_newlib_gettimeofday(struct timeval *p, void *z) {
  UNIMPLEMENTED();
  return 0;
}

typedef void *_LOCK_T;
typedef void *_LOCK_RECURSIVE_T;

extern "C" void ebbrt_newlib_lock_init_recursive(_LOCK_RECURSIVE_T *) {
  UNIMPLEMENTED();
}

extern "C" void ebbrt_newlib_lock_close_recursive(_LOCK_RECURSIVE_T *) {
  UNIMPLEMENTED();
}

extern "C" void ebbrt_newlib_lock_acquire(_LOCK_T *) { UNIMPLEMENTED(); }

extern "C" int ebbrt_newlib_lock_try_acquire_recursive(_LOCK_RECURSIVE_T *) {
  UNIMPLEMENTED();
  return 0;
}

extern "C" void ebbrt_newlib_lock_acquire_recursive(_LOCK_RECURSIVE_T *lock) {
  if (ebbrt_gthread_active_p()) {
    ebbrt_gthread_recursive_mutex_lock(lock);
  }
}

extern "C" void ebbrt_newlib_lock_release(_LOCK_RECURSIVE_T *) {
  UNIMPLEMENTED();
}

extern "C" void ebbrt_newlib_lock_release_recursive(_LOCK_RECURSIVE_T *lock) {
  if (ebbrt_gthread_active_p()) {
    ebbrt_gthread_recursive_mutex_unlock(lock);
  }
}
