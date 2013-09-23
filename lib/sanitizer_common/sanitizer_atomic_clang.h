//===-- sanitizer_atomic_clang.h --------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file is a part of ThreadSanitizer/AddressSanitizer runtime.
// Not intended for direct inclusion. Include sanitizer_atomic.h.
//
//===----------------------------------------------------------------------===//

#ifndef SANITIZER_ATOMIC_CLANG_H
#define SANITIZER_ATOMIC_CLANG_H

/**
	When compiling with gcc-4.0 during stage 1, 
	these workaround definitions are needed.
	Stage 2 and beyond should use clang's built-ins.
	See also workarounds in llvm/lib/Support/Atomic.cpp.
 */
#define USE_DARWIN_ATOMICS		(defined(__APPLE__) && defined(__GNUC__) && (__GNUC__ == 4) && (__GNUC_MINOR__ < 2))
#if USE_DARWIN_ATOMICS
#include <libkern/OSAtomic.h>

#define	__sync_synchronize		OSMemoryBarrier
// argument and return types for OSAtomic library functions
typedef	int32_t		atomic32_t;

template <class T>
static inline
T* vcast(volatile T* ptr) { return const_cast<T*>(ptr); }

template <class T, class S>
static inline
T as_a(S ptr) {
//  static_assert(sizeof(S) == sizeof(T));
//  return static_cast<T>(ptr);
  return reinterpret_cast<T>(ptr);
}

template <class T>
static inline
T
__sync_fetch_and_add(volatile T* ptr, const T v) {
	const T ret = *ptr;
	OSAtomicAdd32Barrier(v, as_a<atomic32_t*>(vcast(ptr)));
	return ret;
}

template <class T>
static
inline
T
__sync_val_compare_and_swap(volatile T* ptr, const T oldv, const T newv) {
  const T ret = *ptr;
  OSAtomicCompareAndSwap32Barrier(oldv, newv, as_a<atomic32_t*>(vcast(ptr)));
  return ret;
}
#endif	// USE_DARWIN_ATOMICS

namespace __sanitizer {

INLINE void atomic_signal_fence(memory_order) {
  __asm__ __volatile__("" ::: "memory");
}

INLINE void atomic_thread_fence(memory_order) {
  __sync_synchronize();
}

INLINE void proc_yield(int cnt) {
  __asm__ __volatile__("" ::: "memory");
#if defined(__i386__) || defined(__x86_64__)
  for (int i = 0; i < cnt; i++)
    __asm__ __volatile__("pause");
#endif
  __asm__ __volatile__("" ::: "memory");
}

template<typename T>
INLINE typename T::Type atomic_load(
    const volatile T *a, memory_order mo) {
  DCHECK(mo & (memory_order_relaxed | memory_order_consume
      | memory_order_acquire | memory_order_seq_cst));
  DCHECK(!((uptr)a % sizeof(*a)));
  typename T::Type v;
  // FIXME:
  // 64-bit atomic operations are not atomic on 32-bit platforms.
  // The implementation lacks necessary memory fences on ARM/PPC.
  // We would like to use compiler builtin atomic operations,
  // but they are mostly broken:
  // - they lead to vastly inefficient code generation
  // (http://llvm.org/bugs/show_bug.cgi?id=17281)
  // - 64-bit atomic operations are not implemented on x86_32
  // (http://llvm.org/bugs/show_bug.cgi?id=15034)
  // - they are not implemented on ARM
  // error: undefined reference to '__atomic_load_4'
  if (mo == memory_order_relaxed) {
    v = a->val_dont_use;
  } else {
    atomic_signal_fence(memory_order_seq_cst);
    v = a->val_dont_use;
    atomic_signal_fence(memory_order_seq_cst);
  }
  return v;
}

template<typename T>
INLINE void atomic_store(volatile T *a, typename T::Type v, memory_order mo) {
  DCHECK(mo & (memory_order_relaxed | memory_order_release
      | memory_order_seq_cst));
  DCHECK(!((uptr)a % sizeof(*a)));
  if (mo == memory_order_relaxed) {
    a->val_dont_use = v;
  } else {
    atomic_signal_fence(memory_order_seq_cst);
    a->val_dont_use = v;
    atomic_signal_fence(memory_order_seq_cst);
  }
  if (mo == memory_order_seq_cst)
    atomic_thread_fence(memory_order_seq_cst);
}

template<typename T>
INLINE typename T::Type atomic_fetch_add(volatile T *a,
    typename T::Type v, memory_order mo) {
  (void)mo;
  DCHECK(!((uptr)a % sizeof(*a)));
  return __sync_fetch_and_add(&a->val_dont_use, v);
}

template<typename T>
INLINE typename T::Type atomic_fetch_sub(volatile T *a,
    typename T::Type v, memory_order mo) {
  (void)mo;
  DCHECK(!((uptr)a % sizeof(*a)));
  return __sync_fetch_and_add(&a->val_dont_use, -v);
}

template<typename T>
INLINE typename T::Type atomic_exchange(volatile T *a,
    typename T::Type v, memory_order mo) {
  DCHECK(!((uptr)a % sizeof(*a)));
  if (mo & (memory_order_release | memory_order_acq_rel | memory_order_seq_cst))
    __sync_synchronize();
#if USE_DARWIN_ATOMICS
  v = OSAtomicTestAndSetBarrier(v, vcast(&a->val_dont_use));
#else
  v = __sync_lock_test_and_set(&a->val_dont_use, v);
#endif
  if (mo == memory_order_seq_cst)
    __sync_synchronize();
  return v;
}

template<typename T>
INLINE bool atomic_compare_exchange_strong(volatile T *a,
                                           typename T::Type *cmp,
                                           typename T::Type xchg,
                                           memory_order mo) {
  typedef typename T::Type Type;
  Type cmpv = *cmp;
  Type prev = __sync_val_compare_and_swap(&a->val_dont_use, cmpv, xchg);
  if (prev == cmpv)
    return true;
  *cmp = prev;
  return false;
}

template<typename T>
INLINE bool atomic_compare_exchange_weak(volatile T *a,
                                         typename T::Type *cmp,
                                         typename T::Type xchg,
                                         memory_order mo) {
  return atomic_compare_exchange_strong(a, cmp, xchg, mo);
}

}  // namespace __sanitizer

#undef ATOMIC_ORDER

#endif  // SANITIZER_ATOMIC_CLANG_H
