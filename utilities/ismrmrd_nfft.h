/*
 * ismrmrd_nfft.h
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

#ifndef ISMRMRD_UTILITIES_ISMRMRD_NFFT_H_
#define ISMRMRD_UTILITIES_ISMRMRD_NFFT_H_

#include <complex>
#include <valarray>
#include "nfft3.h"

class NFFT2
{
 public:
  NFFT2(int, int, int);
  ~NFFT2();
  int precompute(const std::valarray<float>&);
  int trafo(const std::valarray< std::complex<float> >&,
      std::valarray<std::complex<float> >&);
  int adjoint(const std::valarray< std::complex<float> >&,
      std::valarray<std::complex<float> >&);

 protected:
  nfft_plan plan_;
};

#endif
