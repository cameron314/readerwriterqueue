#pragma once

// Provides portable (VC++2010+, GCC 4.7+, and anything C++11 compliant) implementation of low-level
// memory barriers, plus a few semi-portable utility macros (for inlining and alignment). Also has a
// basic atomic type (limited to hardware-supported atomics with no memory ordering guarantees).
// Uses the AE_* prefix for macros (historical reasons), and the "moodycamel" namespace for symbols.

#include <cassert>


// Platform detection
#if defined(_MSC_VER)
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


// AE_FORCEINLINE
#if defined(AE_VCPP)
#define AE_FORCEINLINE __forceinline
#elif defined(AE_GCC)
//#define AE_FORCEINLINE __attribute__((always_inline)) 
#define AE_FORCEINLINE inline
#else
#define AE_FORCEINLINE inline
#endif


// AE_ALIGN
#if defined(AE_VCPP)
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

#if defined(AE_VCPP)
// VS2010 doesn't support std::atomic*, implement our own fences

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




#if !defined(AE_VCPP) || _MSC_VER >= 1700
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
	template<typename U> weak_atomic(U&& x) : value(std::forward<U>(x)) { }
	weak_atomic(weak_atomic const& other) : value(other.value) { }
	weak_atomic(weak_atomic&& other) : value(std::move(other.value)) { }

#if defined(AE_VCPP) && _MSC_VER < 1700
	template<typename U> AE_FORCEINLINE weak_atomic const& operator=(U&& x) { value = std::forward<U>(x); return *this; }
	AE_FORCEINLINE weak_atomic const& operator=(weak_atomic const& other) { value = other.value; return *this; }
	AE_FORCEINLINE weak_atomic const& operator=(weak_atomic&& other) { value = std::move(other.value); return *this; }

	AE_FORCEINLINE operator T() const { return value; }
#else
	template<typename U>
	AE_FORCEINLINE weak_atomic const& operator=(U&& x)
	{
		value.store(std::forward<U>(x), std::memory_order_relaxed);
		return *this;
	}
	
	AE_FORCEINLINE weak_atomic const& operator=(weak_atomic const& other)
	{
		value.store(other.value, std::memory_order_relaxed);
		return *this;
	}
	
	AE_FORCEINLINE weak_atomic const& operator=(weak_atomic&& other)
	{
		value.store(std::move(other.value), std::memory_order_relaxed);
		return *this;
	}

	AE_FORCEINLINE operator T() const { return value.load(std::memory_order_relaxed); }
#endif	

private:
#if defined(AE_VCPP) && _MSC_VER < 1700
	// No std::atomic support, but still need to circumvent compiler optimizations.
	// `volatile` will make memory access slow, but is guaranteed to be reliable.
	volatile T value;
#else
	std::atomic<T> value;
#endif
};

}	// end namespace moodycamel
