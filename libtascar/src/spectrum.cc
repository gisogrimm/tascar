/*
 * This file is part of the TASCAR software, see <http://tascar.org/>
 *
 * Copyright (c) 2018 Giso Grimm
 * Copyright (c) 2019 Giso Grimm
 * Copyright (c) 2020 Giso Grimm
 */
/*
 * TASCAR is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, version 3 of the License.
 *
 * TASCAR is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHATABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 3 for more details.
 *
 * You should have received a copy of the GNU General Public License,
 * Version 3 along with TASCAR. If not, see <http://www.gnu.org/licenses/>.
 */

#include "spectrum.h"
#include <algorithm>
#include <string.h>

TASCAR::spec_t::spec_t(uint32_t n) 
  : n_(n),b(new std::complex<float> [std::max(1u,n_)])
{
  for( uint32_t k=0;k<n_;++k)
    b[k] = 0.0f;
}

TASCAR::spec_t::spec_t(const TASCAR::spec_t& src)
  : n_(src.n_),b(new std::complex<float> [std::max(1u,n_)])
{
  copy(src);
}

void TASCAR::spec_t::clear()
{
  for( uint32_t k=0;k<n_;++k)
    b[k] = 0.0f;
}

TASCAR::spec_t::~spec_t()
{
  delete [] b;
}

void TASCAR::spec_t::copy(const spec_t& src)
{
  memmove(b,src.b,std::min(n_,src.n_)*sizeof(std::complex<float>));
}

void TASCAR::spec_t::resize(uint32_t newsize)
{
  std::complex<float>* b_new(new std::complex<float> [std::max(1u,newsize)]);
  memmove(b_new,b,std::min(n_,newsize)*sizeof(std::complex<float>));
  for( uint32_t k=0;k<std::min(n_,newsize);++k)
    b_new[k] = b[k];
  for( uint32_t k=n_;k<newsize;++k)
    b_new[k] = 0.0f;
  delete [] b;
  b = b_new;
  n_ = newsize;
}

void TASCAR::spec_t::operator/=(const spec_t& o)
{
  for(unsigned int k=0;k<std::min(o.n_,n_);++k){
    if( std::abs(o.b[k]) > 0 )
      b[k] /= o.b[k];
  }
}

void TASCAR::spec_t::operator*=(const spec_t& o)
{
  for(unsigned int k=0;k<std::min(o.n_,n_);++k)
    b[k] *= o.b[k];
}

void TASCAR::spec_t::operator+=(const spec_t& o)
{
  for(unsigned int k=0;k<std::min(o.n_,n_);++k)
    b[k] += o.b[k];
}

void TASCAR::spec_t::add_scaled(const spec_t& o, float gain)
{
  for(unsigned int k=0;k<std::min(o.n_,n_);++k)
    b[k] += o.b[k] * gain;
}

void TASCAR::spec_t::operator*=(const float& o)
{
  for(unsigned int k=0;k<n_;++k)
    b[k] *= o;
}

void TASCAR::spec_t::conj()
{
  for(uint32_t k=0;k<n_;++k)
    b[k] = std::conj( b[k] );
}

std::ostream& operator<<(std::ostream& out, const TASCAR::wave_t& p)
{
  //out << std::string("W(") << p.n << std::string("):");
  for(uint32_t k=0;k<p.n;k++)
    out << std::string(" ") << p.d[k];
  return out;
}

std::ostream& operator<<(std::ostream& out, const TASCAR::spec_t& p)
{
  //out << std::string("S(") << p.n_ << std::string("):");
  for(uint32_t k=0;k<p.n_;k++)
    out << std::string(" ") << p.b[k].real() << std::string((p.b[k].imag()>=0.0)?"+":"") << p.b[k].imag() << "i";
  return out;
}

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
