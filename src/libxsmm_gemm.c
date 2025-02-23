/******************************************************************************
** Copyright (c) 2015-2016, Intel Corporation                                **
** All rights reserved.                                                      **
**                                                                           **
** Redistribution and use in source and binary forms, with or without        **
** modification, are permitted provided that the following conditions        **
** are met:                                                                  **
** 1. Redistributions of source code must retain the above copyright         **
**    notice, this list of conditions and the following disclaimer.          **
** 2. Redistributions in binary form must reproduce the above copyright      **
**    notice, this list of conditions and the following disclaimer in the    **
**    documentation and/or other materials provided with the distribution.   **
** 3. Neither the name of the copyright holder nor the names of its          **
**    contributors may be used to endorse or promote products derived        **
**    from this software without specific prior written permission.          **
**                                                                           **
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS       **
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT         **
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR     **
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT      **
** HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,    **
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED  **
** TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR    **
** PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF    **
** LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING      **
** NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS        **
** SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.              **
******************************************************************************/
/* Hans Pabst (Intel Corp.)
******************************************************************************/
#include "libxsmm_gemm.h"

#if defined(__STATIC)
# include "libxsmm_gemm_extwrap.c"
#else
# include "libxsmm_gemm_ext.h"
#endif

#if defined(LIBXSMM_OFFLOAD_TARGET)
# pragma offload_attribute(push,target(LIBXSMM_OFFLOAD_TARGET))
#endif
#include <stdlib.h>
#include <stdint.h>
#if defined(LIBXSMM_OFFLOAD_TARGET)
# pragma offload_attribute(pop)
#endif

#if defined(LIBXSMM_GEMM_EXTWRAP) && defined(__STATIC)
LIBXSMM_RETARGETABLE libxsmm_sgemm_function libxsmm_internal_sgemm = LIBXSMM_FSYMBOL(__real_sgemm);
LIBXSMM_RETARGETABLE libxsmm_dgemm_function libxsmm_internal_dgemm = LIBXSMM_FSYMBOL(__real_dgemm);
#else
LIBXSMM_RETARGETABLE libxsmm_sgemm_function libxsmm_internal_sgemm = LIBXSMM_FSYMBOL(sgemm);
LIBXSMM_RETARGETABLE libxsmm_dgemm_function libxsmm_internal_dgemm = LIBXSMM_FSYMBOL(dgemm);
#endif /*defined(LIBXSMM_GEMM_EXTWRAP)*/

LIBXSMM_RETARGETABLE LIBXSMM_VISIBILITY_INTERNAL int internal_tile_sizes[/*configs*/][2/*DP/SP*/][3/*TILE_M,TILE_N,TILE_K*/] = {
  { { 72, 32, 16 }, { 72, 32, 16 } }, /*generic*/
  { { 72, 32, 16 }, { 72, 32, 16 } }  /*knl*/
};
LIBXSMM_RETARGETABLE int libxsmm_internal_tile_size[/*DP/SP*/][3/*TILE_M,TILE_N,TILE_K*/] = {
  { 0, 0, 0 }, { 0, 0, 0 }
};
LIBXSMM_RETARGETABLE int libxsmm_internal_gemm_prefetch = LIBXSMM_MAX(LIBXSMM_PREFETCH, 0);
LIBXSMM_RETARGETABLE int libxsmm_internal_gemm_nthreads_per_core = 2;
LIBXSMM_RETARGETABLE int libxsmm_internal_gemm_tasks = 0;
LIBXSMM_RETARGETABLE int libxsmm_internal_gemm_omp = 2;
LIBXSMM_RETARGETABLE int libxsmm_internal_gemm = 0;


LIBXSMM_EXTERN_C LIBXSMM_RETARGETABLE void libxsmm_gemm_configure(int archid, int prefetch,
  libxsmm_sgemm_function sgemm_function, libxsmm_dgemm_function dgemm_function)
{
  int config = 0;

  libxsmm_internal_gemm_prefetch = LIBXSMM_PREFETCH_AL2_AHEAD;
  LIBXSMM_UNUSED(prefetch);

#if defined(__MIC__)
  LIBXSMM_UNUSED(archid);
#else
  if (LIBXSMM_X86_AVX512_MIC == archid)
#endif
  {
    libxsmm_internal_gemm_nthreads_per_core = 4;
    config = 1;
  }

  { /* behaviour of libxsmm_omp_?gemm routines (OpenMP based)
     * 1: parallelized but without internal parallel region,
     * 2: parallelized with internal parallel region" )
     */
    const char *const env = getenv("LIBXSMM_OMP");
    if (0 != env && 0 != *env) {
      libxsmm_internal_gemm_omp = atoi(env);
    }
  }

  { /* behaviour of LD_PRELOAD ?GEMM routines
     * 0: sequential below-threshold routine (no OpenMP) with fallback to BLAS,
     * 1: OpenMP-parallelized but without internal parallel region,
     * 2: OpenMP-parallelized with internal parallel region" )
     */
    const char *const env = getenv("LIBXSMM_GEMM");
    if (0 != env && 0 != *env) {
      libxsmm_internal_gemm = atoi(env);
    }
  }

  { /* attempt to setup tile sizes from the environment (LIBXSMM_M, LIBXSMM_N, and LIBXSMM_K) */
    const char* env[3];
    env[0] = getenv("LIBXSMM_M"); env[1] = getenv("LIBXSMM_N"); env[2] = getenv("LIBXSMM_K");
    libxsmm_internal_tile_size[0/*DP*/][0/*M*/] = (env[0] ? atoi(env[0]) : 0);
    libxsmm_internal_tile_size[0/*DP*/][1/*N*/] = (env[1] ? atoi(env[1]) : 0);
    libxsmm_internal_tile_size[0/*DP*/][2/*K*/] = (env[2] ? atoi(env[2]) : 0);
    /* environment-defined tile sizes applies for DP and SP */
    libxsmm_internal_tile_size[1/*SP*/][0/*M*/] = libxsmm_internal_tile_size[0/*DP*/][0];
    libxsmm_internal_tile_size[1/*SP*/][1/*N*/] = libxsmm_internal_tile_size[0/*DP*/][1];
    libxsmm_internal_tile_size[1/*SP*/][2/*K*/] = libxsmm_internal_tile_size[0/*DP*/][2];

    /* load predefined configuration if tile size is not setup by the environment */
    if (0 >= libxsmm_internal_tile_size[0/*DP*/][0/*M*/]) libxsmm_internal_tile_size[0][0] = internal_tile_sizes[config][0][0];
    if (0 >= libxsmm_internal_tile_size[0/*DP*/][1/*N*/]) libxsmm_internal_tile_size[0][1] = internal_tile_sizes[config][0][1];
    if (0 >= libxsmm_internal_tile_size[0/*DP*/][2/*K*/]) libxsmm_internal_tile_size[0][2] = internal_tile_sizes[config][0][2];
    if (0 >= libxsmm_internal_tile_size[1/*SP*/][0/*M*/]) libxsmm_internal_tile_size[1][0] = internal_tile_sizes[config][1][0];
    if (0 >= libxsmm_internal_tile_size[1/*SP*/][1/*N*/]) libxsmm_internal_tile_size[1][1] = internal_tile_sizes[config][1][1];
    if (0 >= libxsmm_internal_tile_size[1/*SP*/][2/*K*/]) libxsmm_internal_tile_size[1][2] = internal_tile_sizes[config][1][2];
  }

  if (NULL != sgemm_function) {
    libxsmm_internal_sgemm = sgemm_function;
  }

  if (NULL != dgemm_function) {
    libxsmm_internal_dgemm = dgemm_function;
  }
}


LIBXSMM_EXTERN_C LIBXSMM_RETARGETABLE LIBXSMM_GEMM_WEAK_DLIB int libxsmm_gemm_init(int archid, int prefetch)
{
  /* internal pre-initialization step */
  libxsmm_gemm_configure(archid, prefetch, 0/*auto-discovered*/, 0/*auto-discovered*/);

  return (NULL != libxsmm_internal_sgemm
       && NULL != libxsmm_internal_dgemm)
    ? EXIT_SUCCESS
    : EXIT_FAILURE;
}


LIBXSMM_EXTERN_C LIBXSMM_RETARGETABLE LIBXSMM_GEMM_WEAK_DLIB void libxsmm_gemm_finalize(void)
{
}


LIBXSMM_EXTERN_C LIBXSMM_RETARGETABLE void libxsmm_sgemm(const char* transa, const char* transb,
  const libxsmm_blasint* m, const libxsmm_blasint* n, const libxsmm_blasint* k,
  const float* alpha, const float* a, const libxsmm_blasint* lda,
  const float* b, const libxsmm_blasint* ldb,
  const float* beta, float* c, const libxsmm_blasint* ldc)
{
  LIBXSMM_GEMM_DECLARE_FLAGS(flags, transa, transb, m, n, k, a, b, c);
  LIBXSMM_SGEMM(flags, *m, *n, *k,
    0 != alpha ? *alpha : ((float)LIBXSMM_ALPHA),
    a, *(lda ? lda : LIBXSMM_LD(m, k)), b, *(ldb ? ldb : LIBXSMM_LD(k, n)),
    0 != beta ? *beta : ((float)LIBXSMM_BETA),
    c, *(ldc ? ldc : LIBXSMM_LD(m, n)));
}


LIBXSMM_EXTERN_C LIBXSMM_RETARGETABLE void libxsmm_dgemm(const char* transa, const char* transb,
  const libxsmm_blasint* m, const libxsmm_blasint* n, const libxsmm_blasint* k,
  const double* alpha, const double* a, const libxsmm_blasint* lda,
  const double* b, const libxsmm_blasint* ldb,
  const double* beta, double* c, const libxsmm_blasint* ldc)
{
  LIBXSMM_GEMM_DECLARE_FLAGS(flags, transa, transb, m, n, k, a, b, c);
  LIBXSMM_DGEMM(flags, *m, *n, *k,
    0 != alpha ? *alpha : ((double)LIBXSMM_ALPHA),
    a, *(lda ? lda : LIBXSMM_LD(m, k)), b, *(ldb ? ldb : LIBXSMM_LD(k, n)),
    0 != beta ? *beta : ((double)LIBXSMM_BETA),
    c, *(ldc ? ldc : LIBXSMM_LD(m, n)));
}


LIBXSMM_EXTERN_C LIBXSMM_RETARGETABLE void LIBXSMM_FSYMBOL(libxsmm_sgemm)(const char*, const char*,
  const libxsmm_blasint*, const libxsmm_blasint*, const libxsmm_blasint*,
  const float*, const float*, const libxsmm_blasint*,
  const float*, const libxsmm_blasint*,
  const float*, float*, const libxsmm_blasint*);
LIBXSMM_EXTERN_C LIBXSMM_RETARGETABLE void LIBXSMM_FSYMBOL(libxsmm_sgemm)(const char* transa, const char* transb,
  const libxsmm_blasint* m, const libxsmm_blasint* n, const libxsmm_blasint* k,
  const float* alpha, const float* a, const libxsmm_blasint* lda,
  const float* b, const libxsmm_blasint* ldb,
  const float* beta, float* c, const libxsmm_blasint* ldc)
{
  libxsmm_sgemm(transa, transb, m, n, k, alpha, a, lda, b, ldb, beta, c, ldc);
}


LIBXSMM_EXTERN_C LIBXSMM_RETARGETABLE void LIBXSMM_FSYMBOL(libxsmm_dgemm)(const char*, const char*,
  const libxsmm_blasint*, const libxsmm_blasint*, const libxsmm_blasint*,
  const double*, const double*, const libxsmm_blasint*,
  const double*, const libxsmm_blasint*,
  const double*, double*, const libxsmm_blasint*);
LIBXSMM_EXTERN_C LIBXSMM_RETARGETABLE void LIBXSMM_FSYMBOL(libxsmm_dgemm)(const char* transa, const char* transb,
  const libxsmm_blasint* m, const libxsmm_blasint* n, const libxsmm_blasint* k,
  const double* alpha, const double* a, const libxsmm_blasint* lda,
  const double* b, const libxsmm_blasint* ldb,
  const double* beta, double* c, const libxsmm_blasint* ldc)
{
  libxsmm_dgemm(transa, transb, m, n, k, alpha, a, lda, b, ldb, beta, c, ldc);
}


LIBXSMM_EXTERN_C LIBXSMM_RETARGETABLE void libxsmm_blas_sgemm(const char* transa, const char* transb,
  const libxsmm_blasint* m, const libxsmm_blasint* n, const libxsmm_blasint* k,
  const float* alpha, const float* a, const libxsmm_blasint* lda,
  const float* b, const libxsmm_blasint* ldb,
  const float* beta, float* c, const libxsmm_blasint* ldc)
{
  LIBXSMM_GEMM_DECLARE_FLAGS(flags, transa, transb, m, n, k, a, b, c);
  LIBXSMM_BLAS_SGEMM(flags, *m, *n, *k,
    0 != alpha ? *alpha : ((float)LIBXSMM_ALPHA),
    a, *(lda ? lda : LIBXSMM_LD(m, k)), b, *(ldb ? ldb : LIBXSMM_LD(k, n)),
    0 != beta ? *beta : ((float)LIBXSMM_BETA),
    c, *(ldc ? ldc : LIBXSMM_LD(m, n)));
}


LIBXSMM_EXTERN_C LIBXSMM_RETARGETABLE void libxsmm_blas_dgemm(const char* transa, const char* transb,
  const libxsmm_blasint* m, const libxsmm_blasint* n, const libxsmm_blasint* k,
  const double* alpha, const double* a, const libxsmm_blasint* lda,
  const double* b, const libxsmm_blasint* ldb,
  const double* beta, double* c, const libxsmm_blasint* ldc)
{
  LIBXSMM_GEMM_DECLARE_FLAGS(flags, transa, transb, m, n, k, a, b, c);
  LIBXSMM_BLAS_DGEMM(flags, *m, *n, *k,
    0 != alpha ? *alpha : ((double)LIBXSMM_ALPHA),
    a, *(lda ? lda : LIBXSMM_LD(m, k)), b, *(ldb ? ldb : LIBXSMM_LD(k, n)),
    0 != beta ? *beta : ((double)LIBXSMM_BETA),
    c, *(ldc ? ldc : LIBXSMM_LD(m, n)));
}

