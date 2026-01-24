#ifndef PLATF_H
#define PLATF_H
#if _WIN32
#    ifndef WIN32_LEAN_AND_MEAN
#        define WIN32_LEAN_AND_MEAN
#    endif
#    include <windows.h>
#else
#    error Unsupported platform
#endif

#if _WIN32
typedef HANDLE thread_t;
typedef CRITICAL_SECTION mutex_t;
typedef CONDITION_VARIABLE cond_t;
#else
#    error Unsupported platform
#endif

// thread
int thread_init(thread_t *thread, void *(*fn)(void *), void *arg);
void thread_join(thread_t thread);
void thread_detach(thread_t thread);
void thread_sleep(int ms);
int thread_id(void);
void thread_yield(void);

// mtx, recursive?
int mutex_init(mutex_t *mtx);
void mutex_destroy(mutex_t *mtx);
void mutex_lock(mutex_t *mtx);
void mutex_unlock(mutex_t *mtx);

// condition
int cond_init(cond_t *cnd);
void cond_destroy(cond_t *cnd);
void cond_wait(cond_t *cnd, mutex_t *mtx);
void cond_signal(cond_t *cnd);
void cond_broadcast(cond_t *cnd);

#endif // PLATF_H