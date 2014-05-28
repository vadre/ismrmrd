/*
 * ismrmrd_nfft.cpp
 * 
 * Copyright 2014 Ghislain Antony Vaillant <ghisvail@gmail.com>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 * 
 */

#include <complex>
#include <valarray>
#include <vector>
#include <cstring>
#include "nfft3.h"
#include "ismrmrd_nfft.h"

NFFT2::NFFT2(int N1, int N2, int M)
{  
  /* initialize plan internals for 2D transforms */
  nfft_init_2d(&plan_, N1, N2, M);
}

NFFT2::~NFFT2()
{
  nfft_finalize(&plan_);
}

int NFFT2::precompute(const std::valarray<float> &x)
{
  /* sanity checks */
  if (x.size() != 2 * plan_.M_total)
    return -1;
  
  /* set trajectory */
  for (int i = 0; i < 2 * plan_.M_total; i++)
    plan_.x[i] = x[i];
  
  /* precompute if needed */
  if (plan_.nfft_flags & PRE_ONE_PSI)
    nfft_precompute_one_psi(&plan_);
}

int NFFT2::trafo(const std::valarray<std::complex<float> > &f_hat,
                 std::valarray<std::complex<float> > &f)
{
  /* sanity checks */
  if ((f_hat.size() != plan_.N_total) || (f.size() != plan_.M_total))
    return -1;
  
  /* copy input to plan */
  std::complex<double> cast_f_hat;
  for (int i = 0; i < plan_.N_total; i++) {
    cast_f_hat = std::complex<double>(f_hat[i]);
    memcpy(&plan_.f_hat[i],
        reinterpret_cast<fftw_complex*>(&cast_f_hat),
        sizeof(fftw_complex));
  }
  
  /* execute forward transform */
  nfft_trafo(&plan_);
  
  /* copy results back */
  std::complex<double> cast_f;
  for (int i = 0; i < plan_.M_total; i++) {
    memcpy(reinterpret_cast<fftw_complex*>(&cast_f),
        &plan_.f[i],
        sizeof(fftw_complex));
    f[i] = std::complex<float>(cast_f);
  }
  
  return 0;
}

int NFFT2::adjoint(const std::valarray<std::complex<float> > &f,
                   std::valarray<std::complex<float> > &f_hat)
{
  /* sanity checks */
  if ((f_hat.size() != plan_.N_total) || (f.size() != plan_.M_total))
    return -1;
  
  /* copy input to plan */
  std::complex<double> cast_f;
  for (int i = 0; i < plan_.M_total; i++) {
    cast_f = std::complex<double>(f[i]);
    memcpy(&plan_.f[i],
        reinterpret_cast<fftw_complex*>(&cast_f),
        sizeof(fftw_complex));
  }
  
  /* execute adjoint transform */
  nfft_adjoint(&plan_);
  
  /* copy results back */
  std::complex<double> cast_f_hat;
  for (int i = 0; i < plan_.N_total; i++) {
    memcpy(reinterpret_cast<fftw_complex*>(&cast_f_hat),
        &plan_.f_hat[i],
        sizeof(fftw_complex));
    f_hat[i] = std::complex<float>(cast_f_hat); 
  }
  
  return 0;  
}
