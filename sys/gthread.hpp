#pragma once

//DO NOT CHANGE: they are defined in gcc's source tree like this
typedef void *__gthread_key_t;
typedef void *__gthread_once_t;
typedef void *__gthread_mutex_t;
typedef void *__gthread_recursive_mutex_t;
typedef void *__gthread_cond_t;
typedef void *__gthread_t;
typedef void *__gthread_time_t;

extern "C" void ebbrt_gthread_mutex_init(__gthread_mutex_t *);
extern "C" void
ebbrt_gthread_recursive_mutex_init(__gthread_recursive_mutex_t *);
extern "C" int ebbrt_gthread_active_p(void);
extern "C" int ebbrt_gthread_once(__gthread_once_t *, void (*func)(void));
extern "C" int ebbrt_gthread_key_create(__gthread_key_t *keyp,
                                        void (*dtor)(void *));
extern "C" int ebbrt_gthread_key_delete(__gthread_key_t key);
extern "C" void *ebbrt_gthread_getspecific(__gthread_key_t key);
extern "C" int ebbrt_gthread_setspecific(__gthread_key_t key, const void *ptr);
extern "C" int ebbrt_gthread_mutex_destroy(__gthread_mutex_t *mutex);
extern "C" int
ebbrt_gthread_recursive_mutex_destroy(__gthread_recursive_mutex_t *mutex);
extern "C" int ebbrt_gthread_mutex_lock(__gthread_mutex_t *mutex);
extern "C" int ebbrt_gthread_mutex_trylock(__gthread_mutex_t *mutex);
extern "C" int ebbrt_gthread_mutex_unlock(__gthread_mutex_t *mutex);
extern "C" int
ebbrt_gthread_recursive_mutex_lock(__gthread_recursive_mutex_t *mutex);
extern "C" int
ebbrt_gthread_recursive_mutex_trylock(__gthread_recursive_mutex_t *mutex);
extern "C" int
ebbrt_gthread_recursive_mutex_unlock(__gthread_recursive_mutex_t *mutex);
extern "C" void ebbrt_gthread_cond_init(__gthread_cond_t *);
extern "C" int ebbrt_gthread_cond_broadcast(__gthread_cond_t *cond);
extern "C" int ebbrt_gthread_cond_wait(__gthread_cond_t *cond,
                                       __gthread_mutex_t *mutex);
extern "C" int ebbrt_gthread_cond_wait_recursive(__gthread_cond_t *,
                                                 __gthread_recursive_mutex_t *);
extern "C" int ebbrt_gthread_cond_destroy(__gthread_cond_t *cond);
extern "C" int ebbrt_gthread_create(__gthread_t *thread, void *(*func)(void *),
                                    void *args);
extern "C" int ebbrt_gthread_join(__gthread_t thread, void **value_ptr);
extern "C" int ebbrt_gthread_detach(__gthread_t thread);
extern "C" int ebbrt_gthread_equal(__gthread_t t1, __gthread_t t2);
extern "C" __gthread_t ebbrt_gthread_self(void);
extern "C" int ebbrt_gthread_yield(void);
extern "C" int
ebbrt_gthread_mutex_timedlock(__gthread_mutex_t *m,
                              const __gthread_time_t *abs_timeout);
extern "C" int
ebbrt_gthread_recursive_mutex_timedlock(__gthread_recursive_mutex_t *m,
                                        const __gthread_time_t *abs_time);
extern "C" int ebbrt_gthread_cond_signal(__gthread_cond_t *cond);
extern "C" int
ebbrt_gthread_cond_timedwait(__gthread_cond_t *cond, __gthread_mutex_t *mutex,
                             const __gthread_time_t *abs_timeout);

// namespace ebbrt {
// namespace gthread {
// void init();
// }
// }
