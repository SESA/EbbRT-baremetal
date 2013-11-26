#include <cstring>
#include <cstdint>
#include <cerrno>

#include <sys/debug.hpp>
#include <sys/gthread.hpp>

using namespace ebbrt;

// static_assert(sizeof(spinlock) <= sizeof(uintptr_t), "spinlock is too large!");
// static_assert(sizeof(recursive_spinlock) <= sizeof(uintptr_t),
//               "recursive_spinlock is too large!");
extern "C" void ebbrt_gthread_mutex_init(__gthread_mutex_t *mutex) {
  // *reinterpret_cast<uintptr_t *>(mutex) = 0;
  UNIMPLEMENTED();
}

extern "C" void
ebbrt_gthread_recursive_mutex_init(__gthread_recursive_mutex_t *mutex) {
  // *reinterpret_cast<uintptr_t *>(mutex) = 0;
  UNIMPLEMENTED();
}

// bool active = false;

extern "C" int ebbrt_gthread_active_p(void) {
  UNIMPLEMENTED();
  // return active;
}

extern "C" int ebbrt_gthread_once(__gthread_once_t *, void (*)(void)) {
  UNIMPLEMENTED();
  return 0;
}

extern "C" int ebbrt_gthread_key_create(__gthread_key_t *, void (*)(void *)) {
  UNIMPLEMENTED();
  return 0;
}

extern "C" int ebbrt_gthread_key_delete(__gthread_key_t key) {
  UNIMPLEMENTED();
  return 0;
}

extern "C" void *ebbrt_gthread_getspecific(__gthread_key_t key) {
  UNIMPLEMENTED();
  return 0;
}

extern "C" int ebbrt_gthread_setspecific(__gthread_key_t key, const void *ptr) {
  UNIMPLEMENTED();
  return 0;
}

extern "C" int ebbrt_gthread_mutex_destroy(__gthread_mutex_t *mutex) {
  UNIMPLEMENTED();
  return 0;
}

extern "C" int
ebbrt_gthread_recursive_mutex_destroy(__gthread_recursive_mutex_t *mutex) {
  UNIMPLEMENTED();
  return 0;
}

extern "C" int ebbrt_gthread_mutex_lock(__gthread_mutex_t *mutex) {
  // auto mut = reinterpret_cast<spinlock *>(mutex);
  // mut->lock();
  UNIMPLEMENTED();
  return 0;
}

extern "C" int ebbrt_gthread_mutex_trylock(__gthread_mutex_t *mutex) {
  UNIMPLEMENTED();
  return 0;
}

extern "C" int ebbrt_gthread_mutex_unlock(__gthread_mutex_t *mutex) {
  // auto mut = reinterpret_cast<spinlock *>(mutex);
  // mut->unlock();
  UNIMPLEMENTED();
  return 0;
}

extern "C" int
ebbrt_gthread_recursive_mutex_trylock(__gthread_recursive_mutex_t *mutex) {
  UNIMPLEMENTED();
  return 0;
}

extern "C" int
ebbrt_gthread_recursive_mutex_lock(__gthread_recursive_mutex_t *mutex) {
  // auto mut = reinterpret_cast<recursive_spinlock *>(mutex);
  // mut->lock();
  UNIMPLEMENTED();
  return 0;
}

extern "C" int
ebbrt_gthread_recursive_mutex_unlock(__gthread_recursive_mutex_t *mutex) {
  // auto mut = reinterpret_cast<recursive_spinlock *>(mutex);
  // mut->unlock();
  UNIMPLEMENTED();
  return 0;
}

extern "C" void ebbrt_gthread_cond_init(__gthread_cond_t *cond) {
  // *cond = new condition_variable();
  UNIMPLEMENTED();
}

extern "C" int ebbrt_gthread_cond_broadcast(__gthread_cond_t *cond) {
  UNIMPLEMENTED();
  return 0;
}

extern "C" int ebbrt_gthread_cond_wait(__gthread_cond_t *cond,
                                       __gthread_mutex_t *mutex) {
  UNIMPLEMENTED();
  return 0;
}

extern "C" int
ebbrt_gthread_cond_wait_recursive(__gthread_cond_t *,
                                  __gthread_recursive_mutex_t *) {
  UNIMPLEMENTED();
  return 0;
}

extern "C" int ebbrt_gthread_cond_destroy(__gthread_cond_t *cond) {
  UNIMPLEMENTED();
  return 0;
}

extern "C" int ebbrt_gthread_create(__gthread_t *thread, void *(*func)(void *),
                                    void *args) {
  UNIMPLEMENTED();
  return 0;
}

extern "C" int ebbrt_gthread_join(__gthread_t thread, void **value_ptr) {
  UNIMPLEMENTED();
  return 0;
}

extern "C" int ebbrt_gthread_detach(__gthread_t thread) {
  UNIMPLEMENTED();
  return 0;
}

extern "C" int ebbrt_gthread_equal(__gthread_t t1, __gthread_t t2) {
  UNIMPLEMENTED();
  return 0;
}

extern "C" __gthread_t ebbrt_gthread_self() {
  UNIMPLEMENTED();
  return nullptr;
}

extern "C" int ebbrt_gthread_yield() {
  UNIMPLEMENTED();
  return 0;
}

extern "C" int
ebbrt_gthread_mutex_timedlock(__gthread_mutex_t *m,
                              const __gthread_time_t *abs_timeout) {
  UNIMPLEMENTED();
  return 0;
}

extern "C" int
ebbrt_gthread_recursive_mutex_timedlock(__gthread_recursive_mutex_t *m,
                                        const __gthread_time_t *abs_time) {
  UNIMPLEMENTED();
  return 0;
}

extern "C" int ebbrt_gthread_cond_signal(__gthread_cond_t *cond) {
  UNIMPLEMENTED();
  return 0;
}

extern "C" int
ebbrt_gthread_cond_timedwait(__gthread_cond_t *cond, __gthread_mutex_t *mutex,
                             const __gthread_time_t *abs_timeout) {
  UNIMPLEMENTED();
  return 0;
}

// namespace ebbrt {
// namespace gthread {
// void init() { active = true; }
// }
// }
