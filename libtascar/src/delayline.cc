/*
 * This file is part of the TASCAR software, see <http://tascar.org/>
 *
 * Copyright (c) 2018 Giso Grimm
 * Copyright (c) 2019 Giso Grimm
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

#include "delayline.h"
#include <algorithm>
#include <math.h>
#include <string.h>

using namespace TASCAR;

sinctable_t::sinctable_t(uint32_t order, uint32_t oversampling)
    : O(order), N0(order * oversampling), N(N0 + 1), N1(N - 1),
      scale(oversampling), data(new float[N])
{
  data[0] = 1.0f;
  for(uint32_t k = 1; k < N; k++) {
    const float x = TASCAR_PIf * k / scale;
    data[k] = sinf(x) / x;
  }
  data[N1] = 0.0f;
}

sinctable_t::sinctable_t(const sinctable_t& src)
    : O(src.O), N0(src.N0), N(src.N), N1(N - 1), scale(src.scale),
      data(new float[N])
{
  data[0] = 1.0f;
  for(uint32_t k = 1; k < N; k++) {
    const float x = TASCAR_PIf * k / scale;
    data[k] = sinf(x) / x;
  }
  data[N1] = 0.0f;
}

sinctable_t::~sinctable_t()
{
  delete[] data;
}

varidelay_t::varidelay_t(uint32_t maxdelay, double fs, double c, uint32_t order,
                         uint32_t oversampling)
    : dline(new float[maxdelay + 1]), dmax(maxdelay + 1), dist2sample(fs / c),
      delay2sample(fs), pos(0), sinc(order, oversampling)
{
  memset(dline, 0, sizeof(float) * dmax);
}

varidelay_t::varidelay_t(const varidelay_t& src)
    : dline(new float[src.dmax]), dmax(src.dmax), dist2sample(src.dist2sample),
      delay2sample(src.delay2sample), pos(0), sinc(src.sinc)
{
  memset(dline, 0, sizeof(float) * dmax);
}

varidelay_t::~varidelay_t()
{
  delete[] dline;
}

void varidelay_t::add_chunk(const TASCAR::wave_t& x)
{
  for(uint32_t k = 0; k < x.n; k++) {
    pos++;
    if(pos == dmax)
      pos = 0;
    dline[pos] = x.d[k];
  }
}

static_delay_t::static_delay_t(uint32_t d) : wave_t(d)
{
  is_zero = (d == 0u);
}

void static_delay_t::set_delay_nonrt(uint32_t d)
{
  resize(d);
  is_zero = (d == 0u);
  pos = 0u;
}


/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
