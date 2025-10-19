#include "thread.h"

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

int thread_init(thread_t *thread, void *(*fn)(void *), void *arg) {
    struct thread_starter *starter = malloc(sizeof(struct thread_starter));
    starter->fn = fn;
    starter->arg = arg;
    *thread = CreateThread(NULL, 0, m_thread_start, starter, 0, NULL);
    return *thread != NULL ? 0 : -1;
}

void thread_join(thread_t thread) {
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
}

void thread_detach(thread_t thread) {
    CloseHandle(thread);
}

thread_t thread_current(void) {
    return GetCurrentThread();
}

int thread_id(void) {
    return (int)GetCurrentThreadId();
}

void thread_yield(void) {
    Sleep(0);
}

void thread_sleep(int ms) {
    Sleep(ms);
}

// mtx
int mutex_init(mutex_t *mtx) {
    InitializeCriticalSection(mtx);
    return 0;
}

void mutex_destroy(mutex_t *mtx) {
    DeleteCriticalSection(mtx);
}

void mutex_lock(mutex_t *mtx) {
    EnterCriticalSection(mtx);
}

void mutex_unlock(mutex_t *mtx) {
    LeaveCriticalSection(mtx);
}

// COND
int cond_init(cond_t *cnd) {
    InitializeConditionVariable(cnd);
    return 0;
}

void cond_destroy(cond_t *cnd) {
    (void)cnd;
    // nothing to free on windows I guess
    // good practice to call it anyway for when we add pthread
}

void cond_wait(cond_t *cnd, mutex_t *mtx) {
    SleepConditionVariableCS(cnd, mtx, INFINITE);
}

void cond_signal(cond_t *cnd) {
    WakeConditionVariable(cnd);
}

void cond_broadcast(cond_t *cnd) {
    WakeAllConditionVariable(cnd);
}
