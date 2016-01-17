﻿// ©2013-2015 Cameron Desrochers.
// Distributed under the simplified BSD license (see the license file that
// should have come with this header).
// Uses Jeff Preshing's semaphore implementation (under the terms of its
// separate zlib license, embedded below).

#pragma once

// Provides portable (VC++2010+, Intel ICC 13, GCC 4.7+, and anything C++11 compliant) implementation
// of low-level memory barriers, plus a few semi-portable utility macros (for inlining and alignment).
// Also has a basic atomic type (limited to hardware-supported atomics with no memory ordering guarantees).
// Uses the AE_* prefix for macros (historical reasons), and the "moodycamel" namespace for symbols.

#include <cassert>
#include <type_traits>
#include <iostream>

// Platform detection
#if defined(__INTEL_COMPILER)
#define AE_ICC
#elif defined(_MSC_VER)
#define AE_VCPP
#elif defined(__GNUC__)
#define AE_GCC
#endif

#if defined(_M_IA64) || defined(__ia64__)
#define AE_ARCH_IA64
#elif defined(_WIN64) || defined(__amd64__) || defined(_M_X64) || defined(__x86_64__)
#define AE_ARCH_X64
#elif defined(_M_IX86) || defined(__i386__)
#define AE_ARCH_X86
#elif defined(_M_PPC) || defined(__powerpc__)
#define AE_ARCH_PPC
#else
#define AE_ARCH_UNKNOWN
#endif


// AE_UNUSED
#define AE_UNUSED(x) ((void)x)


// AE_FORCEINLINE
#if defined(AE_VCPP) || defined(AE_ICC)
#define AE_FORCEINLINE __forceinline
#elif defined(AE_GCC)
//#define AE_FORCEINLINE __attribute__((always_inline)) 
#define AE_FORCEINLINE inline
#else
#define AE_FORCEINLINE inline
#endif


// AE_ALIGN
#if defined(AE_VCPP) || defined(AE_ICC)
#define AE_ALIGN(x) __declspec(align(x))
#elif defined(AE_GCC)
#define AE_ALIGN(x) __attribute__((aligned(x)))
#else
// Assume GCC compliant syntax...
#define AE_ALIGN(x) __attribute__((aligned(x)))
#endif


// Portable atomic fences implemented below:

namespace moodycamel {

enum memory_order {
	memory_order_relaxed,
	memory_order_acquire,
	memory_order_release,
	memory_order_acq_rel,
	memory_order_seq_cst,

	// memory_order_sync: Forces a full sync:
	// #LoadLoad, #LoadStore, #StoreStore, and most significantly, #StoreLoad
	memory_order_sync = memory_order_seq_cst
};

}    // end namespace moodycamel

#if (defined(AE_VCPP) && (_MSC_VER < 1700 || defined(__cplusplus_cli))) || defined(AE_ICC)
// VS2010 and ICC13 don't support std::atomic_*_fence, implement our own fences

#include <intrin.h>

#if defined(AE_ARCH_X64) || defined(AE_ARCH_X86)
#define AeFullSync _mm_mfence
#define AeLiteSync _mm_mfence
#elif defined(AE_ARCH_IA64)
#define AeFullSync __mf
#define AeLiteSync __mf
#elif defined(AE_ARCH_PPC)
#include <ppcintrinsics.h>
#define AeFullSync __sync
#define AeLiteSync __lwsync
#endif


#ifdef AE_VCPP
#pragma warning(push)
#pragma warning(disable: 4365)		// Disable erroneous 'conversion from long to unsigned int, signed/unsigned mismatch' error when using `assert`
#ifdef __cplusplus_cli
#pragma managed(push, off)
#endif
#endif

namespace moodycamel {

AE_FORCEINLINE void compiler_fence(memory_order order)
{
	switch (order) {
		case memory_order_relaxed: break;
		case memory_order_acquire: _ReadBarrier(); break;
		case memory_order_release: _WriteBarrier(); break;
		case memory_order_acq_rel: _ReadWriteBarrier(); break;
		case memory_order_seq_cst: _ReadWriteBarrier(); break;
		default: assert(false);
	}
}

// x86/x64 have a strong memory model -- all loads and stores have
// acquire and release semantics automatically (so only need compiler
// barriers for those).
#if defined(AE_ARCH_X86) || defined(AE_ARCH_X64)
AE_FORCEINLINE void fence(memory_order order)
{
	switch (order) {
		case memory_order_relaxed: break;
		case memory_order_acquire: _ReadBarrier(); break;
		case memory_order_release: _WriteBarrier(); break;
		case memory_order_acq_rel: _ReadWriteBarrier(); break;
		case memory_order_seq_cst:
			_ReadWriteBarrier();
			AeFullSync();
			_ReadWriteBarrier();
			break;
		default: assert(false);
	}
}
#else
AE_FORCEINLINE void fence(memory_order order)
{
	// Non-specialized arch, use heavier memory barriers everywhere just in case :-(
	switch (order) {
		case memory_order_relaxed:
			break;
		case memory_order_acquire:
			_ReadBarrier();
			AeLiteSync();
			_ReadBarrier();
			break;
		case memory_order_release:
			_WriteBarrier();
			AeLiteSync();
			_WriteBarrier();
			break;
		case memory_order_acq_rel:
			_ReadWriteBarrier();
			AeLiteSync();
			_ReadWriteBarrier();
			break;
		case memory_order_seq_cst:
			_ReadWriteBarrier();
			AeFullSync();
			_ReadWriteBarrier();
			break;
		default: assert(false);
	}
}
#endif
}    // end namespace moodycamel
#else
// Use standard library of atomics
#include <atomic>

namespace moodycamel {

AE_FORCEINLINE void compiler_fence(memory_order order)
{
	switch (order) {
		case memory_order_relaxed: break;
		case memory_order_acquire: std::atomic_signal_fence(std::memory_order_acquire); break;
		case memory_order_release: std::atomic_signal_fence(std::memory_order_release); break;
		case memory_order_acq_rel: std::atomic_signal_fence(std::memory_order_acq_rel); break;
		case memory_order_seq_cst: std::atomic_signal_fence(std::memory_order_seq_cst); break;
		default: assert(false);
	}
}

AE_FORCEINLINE void fence(memory_order order)
{
	switch (order) {
		case memory_order_relaxed: break;
		case memory_order_acquire: std::atomic_thread_fence(std::memory_order_acquire); break;
		case memory_order_release: std::atomic_thread_fence(std::memory_order_release); break;
		case memory_order_acq_rel: std::atomic_thread_fence(std::memory_order_acq_rel); break;
		case memory_order_seq_cst: std::atomic_thread_fence(std::memory_order_seq_cst); break;
		default: assert(false);
	}
}

}    // end namespace moodycamel

#endif


#if !defined(AE_VCPP) || (_MSC_VER >= 1700 && !defined(__cplusplus_cli))
#define AE_USE_STD_ATOMIC_FOR_WEAK_ATOMIC
#endif

#ifdef AE_USE_STD_ATOMIC_FOR_WEAK_ATOMIC
#include <atomic>
#endif
#include <utility>

// WARNING: *NOT* A REPLACEMENT FOR std::atomic. READ CAREFULLY:
// Provides basic support for atomic variables -- no memory ordering guarantees are provided.
// The guarantee of atomicity is only made for types that already have atomic load and store guarantees
// at the hardware level -- on most platforms this generally means aligned pointers and integers (only).
namespace moodycamel {
template<typename T>
class weak_atomic
{
public:
	weak_atomic() { }
#ifdef AE_VCPP
#pragma warning(disable: 4100)		// Get rid of (erroneous) 'unreferenced formal parameter' warning
#endif
	template<typename U> weak_atomic(U&& x) : value(std::forward<U>(x)) {  }
#ifdef __cplusplus_cli
	// Work around bug with universal reference/nullptr combination that only appears when /clr is on
	weak_atomic(nullptr_t) : value(nullptr) {  }
#endif
	weak_atomic(weak_atomic const& other) : value(other.value) {  }
	weak_atomic(weak_atomic&& other) : value(std::move(other.value)) {  }
#ifdef AE_VCPP
#pragma warning(default: 4100)
#endif

	AE_FORCEINLINE operator T() const { return load(); }

	
#ifndef AE_USE_STD_ATOMIC_FOR_WEAK_ATOMIC
	template<typename U> AE_FORCEINLINE weak_atomic const& operator=(U&& x) { value = std::forward<U>(x); return *this; }
	AE_FORCEINLINE weak_atomic const& operator=(weak_atomic const& other) { value = other.value; return *this; }
	
	AE_FORCEINLINE T load() const { return value; }
	
	AE_FORCEINLINE T fetch_add_acquire(T increment)
	{
#if defined(AE_ARCH_X64) || defined(AE_ARCH_X86)
		if (sizeof(T) == 4) return _InterlockedExchangeAdd((long volatile*)&value, (long)increment);
#if defined(_M_AMD64)
		else if (sizeof(T) == 8) return _InterlockedExchangeAdd64((long long volatile*)&value, (long long)increment);
#endif
#else
#error Unsupported platform
#endif
		assert(false && "T must be either a 32 or 64 bit type");
		return value;
	}
	
	AE_FORCEINLINE T fetch_add_release(T increment)
	{
#if defined(AE_ARCH_X64) || defined(AE_ARCH_X86)
		if (sizeof(T) == 4) return _InterlockedExchangeAdd((long volatile*)&value, (long)increment);
#if defined(_M_AMD64)
		else if (sizeof(T) == 8) return _InterlockedExchangeAdd64((long long volatile*)&value, (long long)increment);
#endif
#else
#error Unsupported platform
#endif
		assert(false && "T must be either a 32 or 64 bit type");
		return value;
	}
#else
	template<typename U>
	AE_FORCEINLINE weak_atomic const& operator=(U&& x)
	{
		value.store(std::forward<U>(x), std::memory_order_relaxed);
		return *this;
	}
	
	AE_FORCEINLINE weak_atomic const& operator=(weak_atomic const& other)
	{
		value.store(other.value.load(std::memory_order_relaxed), std::memory_order_relaxed);
		return *this;
	}

	AE_FORCEINLINE T load() const { return value.load(std::memory_order_relaxed); }
	
	AE_FORCEINLINE T fetch_add_acquire(T increment)
	{
		return value.fetch_add(increment, std::memory_order_acquire);
	}
	
	AE_FORCEINLINE T fetch_add_release(T increment)
	{
		return value.fetch_add(increment, std::memory_order_release);
	}
#endif
	

private:
#ifndef AE_USE_STD_ATOMIC_FOR_WEAK_ATOMIC
	// No std::atomic support, but still need to circumvent compiler optimizations.
	// `volatile` will make memory access slow, but is guaranteed to be reliable.
	volatile T value;
#else
	std::atomic<T> value;
#endif
};

}	// end namespace moodycamel



// Portable single-producer, single-consumer semaphore below:

#if defined(_WIN32)
// Avoid including windows.h in a header; we only need a handful of
// items, so we'll redeclare them here (this is relatively safe since
// the API generally has to remain stable between Windows versions).
// I know this is an ugly hack but it still beats polluting the global
// namespace with thousands of generic names or adding a .cpp for nothing.
extern "C" {
	struct _SECURITY_ATTRIBUTES;
	__declspec(dllimport) void* __stdcall CreateSemaphoreW(_SECURITY_ATTRIBUTES* lpSemaphoreAttributes, long lInitialCount, long lMaximumCount, const wchar_t* lpName);
	__declspec(dllimport) int __stdcall CloseHandle(void* hObject);
	__declspec(dllimport) unsigned long __stdcall WaitForSingleObject(void* hHandle, unsigned long dwMilliseconds);
	__declspec(dllimport) int __stdcall ReleaseSemaphore(void* hSemaphore, long lReleaseCount, long* lpPreviousCount);
}
#elif defined(__MACH__)
#include <mach/mach.h>
#include <mach/mach_time.h>
#elif defined(__unix__)
#include <semaphore.h>
#endif

namespace moodycamel
{
	// Code in the spsc_sema namespace below is an adaptation of Jeff Preshing's
	// portable + lightweight semaphore implementations, originally from
	// https://github.com/preshing/cpp11-on-multicore/blob/master/common/sema.h
	// LICENSE:
	// Copyright (c) 2015 Jeff Preshing
	//
	// This software is provided 'as-is', without any express or implied
	// warranty. In no event will the authors be held liable for any damages
	// arising from the use of this software.
	//
	// Permission is granted to anyone to use this software for any purpose,
	// including commercial applications, and to alter it and redistribute it
	// freely, subject to the following restrictions:
	//
	// 1. The origin of this software must not be misrepresented; you must not
	//    claim that you wrote the original software. If you use this software
	//    in a product, an acknowledgement in the product documentation would be
	//    appreciated but is not required.
	// 2. Altered source versions must be plainly marked as such, and must not be
	//    misrepresented as being the original software.
	// 3. This notice may not be removed or altered from any source distribution.
	namespace spsc_sema
	{
#if defined(_WIN32)
		class Semaphore
		{
		private:
		    void* m_hSema;

		    Semaphore(const Semaphore& other);
		    Semaphore& operator=(const Semaphore& other);

		public:
		    Semaphore(int initialCount = 0)
		    {
		        assert(initialCount >= 0);
		        const long maxLong = 0x7fffffff;
		        m_hSema = CreateSemaphoreW(nullptr, initialCount, maxLong, nullptr);
		    }

		    ~Semaphore()
		    {
		        CloseHandle(m_hSema);
		    }

		    bool wait(const unsigned long &ms)
		    {
		    	const unsigned long timeout = 0xffffffff;
                if(ms > 0UL)
                {
                    timeout = ms;
                }
		        const DWORD rc = WaitForSingleObject(m_hSema, timeout);
                return (rc != WAIT_TIMEOUT);
		    }

		    void signal(int count = 1)
		    {
		        ReleaseSemaphore(m_hSema, count, nullptr);
		    }
		};
#elif defined(__MACH__)
		//---------------------------------------------------------
		// Semaphore (Apple iOS and OSX)
		// Can't use POSIX semaphores due to http://lists.apple.com/archives/darwin-kernel/2009/Apr/msg00010.html
		//---------------------------------------------------------
		class Semaphore
		{
		private:
		    semaphore_t m_sema;

		    Semaphore(const Semaphore& other);
		    Semaphore& operator=(const Semaphore& other);

		public:
		    Semaphore(int initialCount = 0)
		    {
		        assert(initialCount >= 0);
		        semaphore_create(mach_task_self(), &m_sema, SYNC_POLICY_FIFO, initialCount);
		    }

		    ~Semaphore()
		    {
		        semaphore_destroy(mach_task_self(), m_sema);
		    }

		    bool wait(const unsigned long &ms)
		    {
                if(ms == 0UL)
                {
		            semaphore_wait(m_sema);
                    return true;
                }

		        kern_return_t rc;
                mach_timespec_t ts;
                ts.tv_sec = ms / 1000;
                ts.tv_nsec = (ms % 1000) * 1000000;

                // added in OSX 10.10: https://developer.apple.com/library/prerelease/mac/documentation/General/Reference/APIDiffsMacOSX10_10SeedDiff/modules/Darwin.html
                rc = semaphore_timedwait(m_sema, ts);

                return (rc != KERN_OPERATION_TIMED_OUT);
		    }

		    void signal()
		    {
		        semaphore_signal(m_sema);
		    }

		    void signal(int count)
		    {
		        while (count-- > 0)
		        {
		            semaphore_signal(m_sema);
		        }
		    }
		};
#elif defined(__unix__)
		//---------------------------------------------------------
		// Semaphore (POSIX, Linux)
		//---------------------------------------------------------
		class Semaphore
		{
		private:
		    sem_t m_sema;

		    Semaphore(const Semaphore& other);
		    Semaphore& operator=(const Semaphore& other);

		public:
		    Semaphore(int initialCount = 0)
		    {
		        assert(initialCount >= 0);
		        sem_init(&m_sema, 0, initialCount);
		    }

		    ~Semaphore()
		    {
		        sem_destroy(&m_sema);
		    }

		    bool wait(const unsigned long &ms)
		    {
		        // http://stackoverflow.com/questions/2013181/gdb-causes-sem-wait-to-fail-with-eintr-error
		        int rc;
                if(ms == 0UL)
                {
                    do
                    {
                        rc = sem_wait(&m_sema);
                    }
                    while (rc == -1 && errno == EINTR);

                    return true;
                }

                // wait with timeout
                struct timespec ts;
                if (clock_gettime(CLOCK_REALTIME, &ts) == -1)
                {
                    return true;
                }
                ts.tv_nsec += (ms % 1000) * 1000000000;
                ts.tv_sec += ms / 1000 + ts.tv_nsec / 1000000000;
                ts.tv_nsec %= 1000000000;

		        do
		        {
		            rc = sem_timedwait(&m_sema, &ts);
		        }
		        while (rc == -1 && errno == EINTR);

                return !(rc == -1 && errno == ETIMEDOUT);
		    }

		    void signal()
		    {
		        sem_post(&m_sema);
		    }

		    void signal(int count)
		    {
		        while (count-- > 0)
		        {
		            sem_post(&m_sema);
		        }
		    }
		};
#else
#error Unsupported platform! (No semaphore wrapper available)
#endif

		//---------------------------------------------------------
		// LightweightSemaphore
		//---------------------------------------------------------
		class LightweightSemaphore
		{
		public:
			typedef std::make_signed<std::size_t>::type ssize_t;

		private:
		    weak_atomic<ssize_t> m_count;
		    Semaphore m_sema;

		    bool waitWithPartialSpinning(const unsigned long &ms)
		    {
		        ssize_t oldCount;
		        // Is there a better way to set the initial spin count?
		        // If we lower it to 1000, testBenaphore becomes 15x slower on my Core i7-5930K Windows PC,
		        // as threads start hitting the kernel semaphore.
		        int spin = 10000;
		        while (--spin >= 0)
		        {
		            if (m_count.load() > 0)
		            {
		                m_count.fetch_add_acquire(-1);
		                return true;
		            }
		            compiler_fence(memory_order_acquire);     // Prevent the compiler from collapsing the loop.
		        }
		        oldCount = m_count.fetch_add_acquire(-1);
		        if (oldCount <= 0)
		        {
		            return m_sema.wait(ms);
		        }

                return true;
		    }

		public:
		    LightweightSemaphore(ssize_t initialCount = 0) : m_count(initialCount)
		    {
		        assert(initialCount >= 0);
		    }

		    bool tryWait()
		    {
		        if (m_count.load() > 0)
		        {
		        	m_count.fetch_add_acquire(-1);
		        	return true;
		        }
		        return false;
		    }

		    bool wait(const unsigned long ms = 0UL)
		    {
		        if (!tryWait())
		            return waitWithPartialSpinning(ms);
                return true;
		    }

		    void signal(ssize_t count = 1)
		    {
		    	assert(count >= 0);
		        ssize_t oldCount = m_count.fetch_add_release(count);
		        assert(oldCount >= -1);
		        if (oldCount < 0)
		        {
		            m_sema.signal(1);
		        }
		    }

		    ssize_t availableApprox() const
		    {
		    	ssize_t count = m_count.load();
		    	return count > 0 ? count : 0;
		    }
		};
	}	// end namespace spsc_sema
}	// end namespace moodycamel

#if defined(AE_VCPP) && (_MSC_VER < 1700 || defined(__cplusplus_cli))
#pragma warning(pop)
#ifdef __cplusplus_cli
#pragma managed(pop)
#endif
#endif
