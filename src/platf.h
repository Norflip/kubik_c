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

typedef HANDLE thrd_t;
typedef CRITICAL_SECTION mtx_t;
typedef CONDITION_VARIABLE cnd_t;

// thread
int thrd_init(thrd_t *thread, void *(*fn)(void *), void *arg);
void thrd_join(thrd_t thread);
void thrd_detach(thrd_t thread);
void thrd_sleep(int ms);
int thrd_id(void);
void thrd_yield(void);

// mtx, recursive?
int mtx_init(mtx_t *mtx);
void mtx_destroy(mtx_t *mtx);
void mtx_lock(mtx_t *mtx);
void mtx_unlock(mtx_t *mtx);

// cndition
int cnd_init(cnd_t *cnd);
void cnd_destroy(cnd_t *cnd);
void cnd_wait(cnd_t *cnd, mtx_t *mtx);
void cnd_signal(cnd_t *cnd);
void cnd_broadcast(cnd_t *cnd);

#endif // PLATF_H