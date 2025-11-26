/*
 * This file is part of the TASCAR software, see <http://tascar.org/>
 *
 * Copyright (c) 2018 Giso Grimm
 * Copyright (c) 2020 Giso Grimm
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

#include <gtest/gtest.h>

#include "audiochunks.h"

TEST(wave_t, constructor)
{
  unsigned int frames(1024);
  TASCAR::wave_t wave(frames);
  EXPECT_EQ(0.0, wave[0]);
}

TEST(wave_t, empty)
{
  TASCAR::wave_t wave(0);
  EXPECT_EQ(0u, wave.size());
}

TEST(wave_t, rms)
{
  TASCAR::wave_t wave(2);
  wave[0] = 1.0f;
  ASSERT_NEAR(0.707107f, wave.rms(), 1e-6f);
  wave[1] = -1.0f;
  ASSERT_NEAR(1.0f, wave.rms(), 1e-9f);
}

TEST(wave_t, plusop)
{
  TASCAR::wave_t wave(4);
  wave += 1.0f;
  EXPECT_EQ(1.0f, wave.d[0]);
  EXPECT_EQ(1.0f, wave.d[1]);
  EXPECT_EQ(1.0f, wave.d[2]);
  EXPECT_EQ(1.0f, wave.d[3]);
}

TEST(wave_t, resample)
{
  TASCAR::wave_t wave(16);
  wave[7] = 1.0f;
  wave[8] = 1.0f;
  float orms(wave.rms());
  wave.resample(2.0);
  EXPECT_EQ(32u, wave.size());
  ASSERT_NEAR(orms, wave.rms(), 4e-4);
  ASSERT_NEAR(0.00903543f, wave[0], 1e-6);
  ASSERT_NEAR(0.00838445f, wave[1], 1e-6);
  ASSERT_NEAR(-0.00914826f, wave[2], 1e-6);
  ASSERT_NEAR(-0.0126697f, wave[3], 1e-6);
  ASSERT_NEAR(0.00881281f, wave[4], 1e-6);
  ASSERT_NEAR(0.0185329f, wave[5], 1e-6);
  ASSERT_NEAR(-0.00801491f, wave[6], 1e-6);
  ASSERT_NEAR(-0.0276613f, wave[7], 1e-6);
  ASSERT_NEAR(0.00677472f, wave[8], 1e-6);
  ASSERT_NEAR(0.0453624f, wave[9], 1e-6);
  ASSERT_NEAR(-0.00514671f, wave[10], 1e-6);
  ASSERT_NEAR(-0.0950471f, wave[11], 1e-6);
  ASSERT_NEAR(0.00321641f, wave[12], 1e-6);
  ASSERT_NEAR(0.435323f, wave[13], 1e-6);
  ASSERT_NEAR(0.998906f, wave[14], 1e-6);
  ASSERT_NEAR(1.26208f, wave[15], 1e-5);
  ASSERT_NEAR(0.998906f, wave[16], 1e-6);
  ASSERT_NEAR(0.435323f, wave[17], 1e-6);
  ASSERT_NEAR(0.00321641f, wave[18], 1e-6);
  ASSERT_NEAR(-0.0950471f, wave[19], 1e-6);
  ASSERT_NEAR(-0.00514671f, wave[20], 1e-6);
  ASSERT_NEAR(0.0453624f, wave[21], 1e-6);
  ASSERT_NEAR(0.00677472f, wave[22], 1e-6);
  ASSERT_NEAR(-0.0276613f, wave[23], 1e-6);
  ASSERT_NEAR(-0.00801491f, wave[24], 1e-6);
  ASSERT_NEAR(0.0185329f, wave[25], 1e-6);
  ASSERT_NEAR(0.00881281f, wave[26], 1e-6);
  ASSERT_NEAR(-0.0126697f, wave[27], 1e-6);
  ASSERT_NEAR(-0.00914826f, wave[28], 1e-6);
  ASSERT_NEAR(0.00838445f, wave[29], 1e-6);
  ASSERT_NEAR(0.00903543f, wave[30], 1e-6);
  // wave[31] = 0.00504668f;
  wave.clear();
  wave[15] = 1.0f;
  wave[16] = 1.0f;
  orms = wave.rms();
  wave.resample(0.5);
  // for(uint32_t k = 0; k < wave.size(); ++k)
  //  std::cout << k << " " << wave[k] << std::endl;
  EXPECT_EQ(16u, wave.size());
  ASSERT_NEAR(orms, wave.rms(), 0.04);
  ASSERT_NEAR(-0.00887539f, wave[0], 1e-6);
  ASSERT_NEAR(0.0175853f, wave[1], 1e-6);
  ASSERT_NEAR(-0.0284943f, wave[2], 1e-6);
  ASSERT_NEAR(0.0421672f, wave[3], 1e-6);
  ASSERT_NEAR(-0.0600053f, wave[4], 1e-6);
  ASSERT_NEAR(0.0860738f, wave[5], 1e-6);
  ASSERT_NEAR(-0.136171f, wave[6], 1e-6);
  ASSERT_NEAR(0.35544f, wave[7], 1e-6);
  ASSERT_NEAR(0.77505f, wave[8], 1e-6);
  ASSERT_NEAR(-0.0579361f, wave[9], 1e-6);
  ASSERT_NEAR(0.0120207f, wave[10], 1e-6);
  ASSERT_NEAR(0.00808709f, wave[11], 1e-6);
  ASSERT_NEAR(-0.0185304f, wave[12], 1e-6);
  ASSERT_NEAR(0.0237894f, wave[13], 1e-6);
  ASSERT_NEAR(-0.0257178f, wave[14], 1e-6);
  // ASSERT_NEAR(0.0253359f, wave[15], 1e-6);
}

TEST(wave_t, insertatend)
{
  TASCAR::wave_t wlong(
      std::vector<float>({0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f}));
  TASCAR::wave_t wshort(std::vector<float>({6.0f, 7.0f}));
  EXPECT_EQ(6.0f, wshort.d[0]);
  EXPECT_EQ(7.0f, wshort.d[1]);
  EXPECT_EQ(0.0f, wlong.d[0]);
  EXPECT_EQ(1.0f, wlong.d[1]);
  EXPECT_EQ(2.0f, wlong.d[2]);
  EXPECT_EQ(3.0f, wlong.d[3]);
  EXPECT_EQ(4.0f, wlong.d[4]);
  EXPECT_EQ(5.0f, wlong.d[5]);
  wlong.insert_at_end(wshort);
  EXPECT_EQ(2.0f, wlong.d[0]);
  EXPECT_EQ(3.0f, wlong.d[1]);
  EXPECT_EQ(4.0f, wlong.d[2]);
  EXPECT_EQ(5.0f, wlong.d[3]);
  EXPECT_EQ(6.0f, wlong.d[4]);
  EXPECT_EQ(7.0f, wlong.d[5]);
}

// Local Variables:
// compile-command: "make -C ../.. unit-tests"
// coding: utf-8-unix
// c-basic-offset: 2
// indent-tabs-mode: nil
// End:
