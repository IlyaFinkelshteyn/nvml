/*
 * Copyright 2015-2016, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * pthread.c -- (imperfect) POSIX threads for Windows
 */

#include <pthread.h>
#include <time.h>
#include <sys/types.h>
#include <sys/timeb.h>


#define	TIMED_LOCK(action, ts) {\
	if ((action) == TRUE)\
		return 0;\
	long long et = (ts)->tv_sec * 1000 + (ts)->tv_nsec / 1000000;\
	while (1) {\
		struct __timeb64 t;\
		_ftime64(&t);\
		if (t.time * 1000 + t.millitm >= et)\
			return ETIMEDOUT;\
		if ((action) == TRUE)\
			return 0;\
		Sleep(1);\
	}\
	return ETIMEDOUT;\
}


int
pthread_mutex_init(pthread_mutex_t *restrict mutex,
	const pthread_mutexattr_t *restrict attr)
{
	(void) attr;

	InitializeCriticalSection(mutex);
	return 0;
}

int
pthread_mutex_destroy(pthread_mutex_t *restrict mutex)
{
	DeleteCriticalSection(mutex);
	return 0;
}

int
pthread_mutex_lock(pthread_mutex_t *restrict mutex)
{
	EnterCriticalSection(mutex);
	return 0;
}

int
pthread_mutex_trylock(pthread_mutex_t *restrict mutex)
{
	return (TryEnterCriticalSection(mutex) == FALSE) ? EBUSY : 0;
}

/* XXX - non POSIX */
int
pthread_mutex_timedlock(pthread_mutex_t *restrict mutex,
	const struct timespec *abstime)
{
	TIMED_LOCK(TryEnterCriticalSection(mutex), abstime);
}

int
pthread_mutex_unlock(pthread_mutex_t *restrict mutex)
{
	LeaveCriticalSection(mutex);
	return 0;
}


#ifdef USE_WIN_SRWLOCK

int
pthread_rwlock_init(pthread_rwlock_t *restrict rwlock,
	const pthread_rwlockattr_t *restrict attr)
{
	(void) attr;

	InitializeSRWLock(rwlock);
	return 0;
}

int
pthread_rwlock_destroy(pthread_rwlock_t *restrict rwlock)
{
	/* do nothing */
	(void) rwlock;

	return 0;
}

int
pthread_rwlock_rdlock(pthread_rwlock_t *restrict rwlock)
{
	AcquireSRWLockShared(rwlock);
	return 0;
}

int
pthread_rwlock_wrlock(pthread_rwlock_t *restrict rwlock)
{
	AcquireSRWLockExclusive(rwlock);
	return 0;
}

int
pthread_rwlock_tryrdlock(pthread_rwlock_t *restrict rwlock)
{
	return (TryAcquireSRWLockShared(rwlock) == FALSE) ? EBUSY : 0;
}

int
pthread_rwlock_trywrlock(pthread_rwlock_t *restrict rwlock)
{
	return (TryAcquireSRWLockExclusive(rwlock) == FALSE) ? EBUSY : 0;
}

int
pthread_rwlock_timedrdlock(pthread_rwlock_t *restrict rwlock,
	const struct timespec *abstime)
{
	TIMED_LOCK(TryAcquireSRWLockShared(rwlock), abstime);
}

int
pthread_rwlock_timedwrlock(pthread_rwlock_t *restrict rwlock,
	const struct timespec *abstime)
{
	TIMED_LOCK(TryAcquireSRWLockExclusive(rwlock), abstime);
}

int
pthread_rwlock_unlock(pthread_rwlock_t *restrict rwlock)
{
	/* XXX - distinquish between shared/exclusive lock */
	ReleaseSRWLockExclusive(rwlock); /* ReleaseSRWLockShared(rwlock); */
	return 0;
}

#endif


int
pthread_cond_init(pthread_cond_t *restrict cond,
	const pthread_condattr_t *restrict attr)
{
	(void) attr;

	InitializeConditionVariable(cond);
	return 0;
}

int
pthread_cond_destroy(pthread_cond_t *restrict cond)
{
	/* do nothing */
	(void) cond;

	return 0;
}

int
pthread_cond_broadcast(pthread_cond_t *restrict cond)
{
	WakeAllConditionVariable(cond);
	return 0;
}

int
pthread_cond_signal(pthread_cond_t *restrict cond)
{
	WakeConditionVariable(cond);
	return 0;
}

int
pthread_cond_timedwait(pthread_cond_t *restrict cond,
	pthread_mutex_t *restrict mutex, const struct timespec *abstime)
{
	DWORD ms = (DWORD)(abstime->tv_sec * 1000 +
					abstime->tv_nsec / 1000000);

	/* XXX - return error code based on GetLastError() */
	return (SleepConditionVariableCS(cond, mutex, ms) == FALSE)
							? ETIMEDOUT : 0;
}

int
pthread_cond_wait(pthread_cond_t *restrict cond,
	pthread_mutex_t *restrict mutex)
{
	/* XXX - return error code based on GetLastError() */
	return (SleepConditionVariableCS(cond, mutex, INFINITE) == FALSE)
							? EINVAL : 0;
}


int
pthread_once(pthread_once_t *once, void (*func)(void))
{
	if (!_InterlockedCompareExchange(once, 1, 0)) {
		func();
	}
	return 0;
}

int
pthread_key_create(pthread_key_t *key, void (*destructor)(void *))
{
	/* XXX - destructor not supported */

	*key = TlsAlloc();
	if (*key == TLS_OUT_OF_INDEXES)
		return EAGAIN;
	if (!TlsSetValue(*key, NULL)) /* XXX - not needed? */
		return ENOMEM;
	return 0;
}

int
pthread_key_delete(pthread_key_t key)
{
	/* XXX - destructor not supported */

	if (!TlsFree(key))
		return EINVAL;
	return 0;
}

int
pthread_setspecific(pthread_key_t key, const void *value)
{
	if (!TlsSetValue(key, (LPVOID)value))
		return ENOENT;
	return 0;
}

void *
pthread_getspecific(pthread_key_t key)
{
	return TlsGetValue(key);
}
