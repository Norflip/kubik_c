#ifndef PLATF_H
#define PLATF_H

// platform-specific stuff, this is probably a bad name
// basically just a wrapper for win32 threading stuff atm
// based on tinycthread... which we could use but this was more fun

#if _WIN32
#    ifndef WIN32_LEAN_AND_MEAN
#        define WIN32_LEAN_AND_MEAN
#    endif
#    include <windows.h>
#else
#    error Unsupported platform
#endif

typedef HANDLE thread_t;
typedef CRITICAL_SECTION mutex_t;
typedef CONDITION_VARIABLE cond_t;
typedef struct rwlock {
    SRWLOCK lock;
    int is_write;
} rwlock_t;

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

// cndition
int cond_init(cond_t *cnd);
void cond_destroy(cond_t *cnd);
void cond_wait(cond_t *cnd, mutex_t *mtx);
void cond_signal(cond_t *cnd);
void cond_broadcast(cond_t *cnd);

#endif // PLATF_H