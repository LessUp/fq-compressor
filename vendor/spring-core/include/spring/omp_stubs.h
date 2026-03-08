/*
 * OpenMP stub declarations for non-OpenMP builds.
 *
 * This header provides no-op stubs so that vendor code compiles
 * without OpenMP.  It is guarded by SPRING_CORE_OMP_STUBS_H_ to
 * avoid redefinition when multiple headers include it.
 *
 * Extracted for fq-compressor project - Non-commercial/Research use only
 */

#ifndef SPRING_CORE_OMP_STUBS_H_
#define SPRING_CORE_OMP_STUBS_H_

#ifdef USE_OPENMP
#include <omp.h>
#else

// Thread identification
inline int omp_get_thread_num() { return 0; }
inline int omp_get_num_threads() { return 1; }
inline void omp_set_num_threads(int) {}

// Lock type and operations
struct omp_lock_t {};
inline void omp_init_lock(omp_lock_t*) {}
inline void omp_destroy_lock(omp_lock_t*) {}
inline void omp_set_lock(omp_lock_t*) {}
inline void omp_unset_lock(omp_lock_t*) {}
inline int omp_test_lock(omp_lock_t*) { return 1; }

#endif  // USE_OPENMP

#endif  // SPRING_CORE_OMP_STUBS_H_
