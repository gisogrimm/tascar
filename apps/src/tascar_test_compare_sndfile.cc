/*
 * This file is part of the TASCAR software, see <http://tascar.org/>
 *
 * Copyright (c) 2021 Giso Grimm
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

#include "audiochunks.h"
#include <stdlib.h>

#define assert_equal(e, t)                                                     \
  if(e != t) {                                                                 \
    std::cerr << #e << "!=" << #t << "(" << e << "!=" << t << ").\n";          \
    return 1;                                                                  \
  }
#define assert_diff_below(e, t, d, k, c)                                       \
  if(fabs(e - t) > d) {                                                        \
    std::cerr << #e << "!=" << #t << "(" << e << "!=" << t << ", "             \
              << fabs(e - t) << ">" << d << " ch " << c << " at " << k         \
              << ").\n";                                                       \
    return 1;                                                                  \
  }
#define assert_rmsdiff_below(e, d, c)                                          \
  if(e > d) {                                                                  \
    std::cerr << "RMS difference is too large (" << e << ">" << d << " ch "    \
              << c << ").\n";                                                  \
    return 1;                                                                  \
  }

int main(int argc, char** argv)
{
  if(argc < 5) {
    std::cerr << "Usage: <expected.wav> <test.wav> <tol> <rmstol>" << std::endl;
    return 1;
  }
  try {
    TASCAR::sndfile_t expected(argv[1]);
    TASCAR::sndfile_t test(argv[2]);
    double tol(atof(argv[3]));
    double rmstol(atof(argv[4]));
    assert_equal(expected.get_frames(), test.get_frames());
    assert_equal(expected.get_channels(), test.get_channels());
    assert_equal(expected.get_srate(), test.get_srate());
    for(uint32_t c = 0; c < expected.get_channels(); ++c) {
      TASCAR::sndfile_t l_expected(argv[1], c);
      TASCAR::sndfile_t l_test(argv[2], c);
      for(uint32_t k = 0; k < test.n; ++k) {
        assert_diff_below(l_expected[k], l_test[k], tol, k, c);
      }
      l_test *= -1.0;
      l_expected += l_test;
      assert_rmsdiff_below(l_expected.rms(), rmstol, c);
    }
    return 0;
  }
  catch(const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 2;
  }
}

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
