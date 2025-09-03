#include "platf.h"

// THREAD
struct thread_starter {
    void *(*fn)(void *);
    void *arg;
};

static DWORD WINAPI m_thread_start(void *arg) {
    struct thread_starter *starter = (struct thread_starter *)arg;
    void *ret = starter->fn(starter->arg);
    free(starter);
    return (DWORD)(uintptr_t)ret;
}

int thrd_init(thrd_t *thread, void *(*fn)(void *), void *arg) {
    struct thread_starter *starter = malloc(sizeof(struct thread_starter));
    starter->fn = fn;
    starter->arg = arg;
    *thread = CreateThread(NULL, 0, m_thread_start, starter, 0, NULL);
    return *thread != NULL ? 0 : -1;
}

void thrd_join(thrd_t thread) {
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
}

void thrd_detach(thrd_t thread) {
    CloseHandle(thread);
}

thrd_t thrd_current(void) {
    return GetCurrentThread();
}

int thrd_id(void) {
    return (int)GetCurrentThreadId();
}

void thrd_yield(void) {
    Sleep(0);
}

void thrd_sleep(int ms) {
    Sleep(ms);
}

// mtx
int mtx_init(mtx_t *mtx) {
    InitializeCriticalSection(mtx);
    return 0;
}

void mtx_destroy(mtx_t *mtx) {
    DeleteCriticalSection(mtx);
}

void mtx_lock(mtx_t *mtx) {
    EnterCriticalSection(mtx);
}

void mtx_unlock(mtx_t *mtx) {
    LeaveCriticalSection(mtx);
}

// COND
int cnd_init(cnd_t *cnd) {
    InitializeConditionVariable(cnd);
    return 0;
}

void cnd_destroy(cnd_t *cnd) {
    (void)cnd;
    // nothing to free on windows I guess
    // good practice to call it anyway for when we add pthread
}

void cnd_wait(cnd_t *cnd, mtx_t *mtx) {
    SleepConditionVariableCS(cnd, mtx, INFINITE);
}

void cnd_signal(cnd_t *cnd) {
    WakeConditionVariable(cnd);
}

void cnd_broadcast(cnd_t *cnd) {
    WakeAllConditionVariable(cnd);
}