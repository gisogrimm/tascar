/**
 * @file   audiochunks.h
 * @author Giso Grimm
 * @brief  Chunks for block-wise audio processing
 */
/*
 * License (GPL)
 *
 * Copyright (C) 2018  Giso Grimm
 *
 * This program is free software; you can redistribute it and/ or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */
#ifndef AUDIOCHUNKS_H
#define AUDIOCHUNKS_H

#include "coordinates.h"
#include <sndfile.h>

namespace TASCAR {

  /** \brief Class for single-channel time-domain audio chunks
   */
  class wave_t {
  public:
    wave_t();
    wave_t(uint32_t chunksize);
    wave_t(const wave_t& src);
    wave_t(const std::vector<float>& src);
    wave_t(const std::vector<double>& src);
    wave_t(uint32_t n, float* ptr);
    virtual ~wave_t();
    void use_external_buffer(uint32_t n, float* ptr);
    inline float& operator[](uint32_t k) { return d[k]; };
    inline const float& operator[](uint32_t k) const { return d[k]; };
    inline uint32_t size() const { return n; };
    void clear();
    void copy(const wave_t& src, float gain = 1.0);
    void add(const wave_t& src, float gain = 1.0);
    /**
     * @brief Copies a specified number of samples from the input data to the
     * wave, applying a gain factor to each sample.
     *
     * This function copies samples from the provided float array `data` into
     * the internal buffer `d`, multiplying each sample by the specified gain.
     * If the number of samples to copy is less than the current size of the
     * wave, the remaining elements in the wave are set to zero.
     *
     * @param data Pointer to the input float array containing the samples to
     * copy.
     * @param cnt The number of samples to copy from the input data.
     * @param gain The linear gain factor to apply to each sample during the
     * copy.
     *
     * @return The number of samples actually copied to the wave, which will be
     *         the minimum of `n` (the size of the internal buffer) and `cnt`.
     */
    uint32_t copy(float* data, uint32_t cnt, float gain = 1.0);
    /**
     * @brief Copies a specified number of samples from the wave to the output
     * data, applying a gain factor to each sample.
     *
     * This function copies samples from the internal buffer `d` into the
     * provided float array `data`, multiplying each sample by the specified
     * gain. If the number of samples to copy is less than `cnt`, the remaining
     * elements in the output array are set to zero.
     *
     * @param[out] data Pointer to the output float array where the samples will
     * be copied.
     * @param[in] cnt The number of samples to copy from the wave.
     * @param[in] gain The gain factor to apply to each sample during the copy.
     *
     * @return The number of samples actually copied to the output array, which
     * will be the minimum of `n` (the current size of the internal buffer) and
     * `cnt`.
     */
    uint32_t copy_to(float* data, uint32_t cnt, float gain = 1.0);
    /**
     * @brief Copies a specified number of samples from the input data to the
     * wave, using a defined stride and applying a gain factor to each sample.
     *
     * This function is similar to the `copy()` function, but it allows for
     * specifying a stride. It copies from the input float array `data` at the
     * specified stride, applying the given gain factor to each sampled value.
     * If fewer samples are copied than the current size of the wave, the
     * remaining elements are set to zero.
     *
     * @param data Pointer to the input float array containing the samples to
     * copy.
     * @param cnt The number of samples to copy from the input data.
     * @param stride The number of elements to skip in the input data between
     * copies.
     * @param gain The gain factor to apply to each sample during the copy.
     *
     * @return The number of samples actually copied to the wave, which will be
     *         the minimum of `n` (the size of the internal buffer) and `cnt`.
     */
    uint32_t copy_stride(float* data, uint32_t cnt, uint32_t stride,
                         float gain = 1.0);
    /**
     * @brief Copies a specified number of samples from the wave to the output
     * data, using a defined stride and applying a gain factor to each sample.
     *
     * This function is similar to the `copy_to()` function, but it allows for
     * specifying a stride. It copies from the internal buffer `d` to the output
     * float array `data` at the specified stride, applying the given gain
     * factor to each sampled value. If fewer samples are copied than `cnt`, the
     * remaining elements in the output array are set to zero.
     *
     * @param[out] data Pointer to the output float array where the samples will
     * be copied.
     * @param[in] cnt The number of samples to copy from the wave.
     * @param[in] stride The number of elements to skip in the output array
     * between copies.
     * @param[in] gain The gain factor to apply to each sample during the copy.
     *
     * @return The number of samples actually copied to the output array, which
     * will be the minimum of `n` (the current size of the internal buffer) and
     * `cnt`.
     */
    uint32_t copy_to_stride(float* data, uint32_t cnt, uint32_t stride,
                            float gain = 1.0);
    float ms() const;
    float rms() const;
    inline float maxabs() const
    {
      float rv = 0.0f;
      for(uint32_t k = 0; k < n; ++k)
        rv = std::max(rv, fabsf(d[k]));
      return rv;
    };
    float spldb() const;
    float maxabsdb() const;
    void append(const wave_t& src);
    /**
     * @brief Shift all samples to make space at end, then insert all
     * samples from source. This will discard the first samples. This
     * function will throw an exception when src is not smaller than
     * this.
     */
    void insert_at_end(const wave_t& src);
    void resize(uint32_t chunksize);
    virtual void resample(double ratio);
    /**
       @brief Make loopable sound by applying cross-fade and shortening sound.
       @param fadelen Fade length in samples; must be less than available number
       of samples.
     */
    virtual void make_loopable(uint32_t fadelen, float crossexp);
    inline void append_sample(float src)
    {
      d[append_pos] = src;
      ++append_pos;
      if(append_pos >= n)
        append_pos = 0;
    };
    // void operator*=(double v);
    void operator*=(float v);
    // void operator+=(double v);
    void operator+=(float v);
    void operator+=(const wave_t& o);
    void operator*=(const wave_t& src);
    float* d;
    uint32_t n;
    float* begin() { return d; };
    float* end() { return d + n; };

  private:
    bool own_pointer;

  protected:
    uint32_t append_pos;
    float rmsscale;
  };

  /** \brief Class for first-order-Ambisonics audio chunks
   */
  class amb1wave_t {
  public:
    amb1wave_t(uint32_t chunksize);
    amb1wave_t(const amb1wave_t&) = delete;
    inline wave_t& w() { return w_; };
    inline wave_t& x() { return x_; };
    inline wave_t& y() { return y_; };
    inline wave_t& z() { return z_; };
    inline const wave_t& w() const { return w_; };
    inline const wave_t& x() const { return x_; };
    inline const wave_t& y() const { return y_; };
    inline const wave_t& z() const { return z_; };
    inline uint32_t size() const { return w_.size(); };
    void clear();
    void operator*=(float v);
    void operator+=(const amb1wave_t& v);
    void add_panned(pos_t p, const wave_t& v, float g = 1.0f);
    std::vector<wave_t> wyzx;
    void print_levels() const;
    wave_t& operator[](uint32_t acn);
    void copy(const amb1wave_t&);
    void apply_matrix(float* m);

  protected:
    wave_t w_;
    wave_t x_;
    wave_t y_;
    wave_t z_;
  };

  class amb1rotator_t : public amb1wave_t {
  public:
    amb1rotator_t(uint32_t chunksize);
    amb1rotator_t& rotate(const amb1wave_t& src, const TASCAR::zyx_euler_t& o,
                          bool invert = false);
    void rotate(const TASCAR::zyx_euler_t& o, bool invert = false);

  private:
    double wxx, wxy, wxz, wyx, wyy, wyz, wzx, wzy, wzz, dt;
  };

  class sndfile_handle_t {
  public:
    sndfile_handle_t(const std::string& fname);
    sndfile_handle_t(const std::string& fname, int samplerate, int channels,
                     int format = SF_FORMAT_WAV | SF_FORMAT_FLOAT);
    ~sndfile_handle_t();
    sndfile_handle_t(const sndfile_handle_t&) = delete;
    uint32_t get_channels() const { return sf_inf.channels; };
    uint32_t get_frames() const { return (uint32_t)sf_inf.frames; };
    uint32_t get_srate() const { return sf_inf.samplerate; };
    uint32_t readf_float(float* buf, uint32_t frames);
    uint32_t writef_float(float* buf, uint32_t frames);
    static SF_INFO sf_info_configurator(int samplerate, int channels,
                                        int format = SF_FORMAT_WAV |
                                                     SF_FORMAT_FLOAT);

  protected:
    SF_INFO sf_inf;
    SNDFILE* sfile;
  };

  class looped_wave_t : public wave_t {
  public:
    looped_wave_t(uint32_t length);
    void set_iposition(int64_t position);
    void set_loop(uint32_t loop);
    void add_to_chunk(int64_t chunktime, wave_t& chunk);
    void add_chunk(int32_t chunk_time, int32_t start_time, float gain,
                   wave_t& chunk);
    void add_chunk_looped(float gain, wave_t& chunk);
    void restart() { looped_t = 0; };

  private:
    uint32_t looped_t;
    float looped_gain;
    // position parameters:
    int64_t iposition_;
    uint32_t loop_;
  };

  class sndfile_t : public sndfile_handle_t, public looped_wave_t {
  public:
    sndfile_t(const std::string& fname, uint32_t channel = 0, double start = 0,
              double length = 0);
    void set_position(double position);
    void resample(double ratio);
    void make_loopable(uint32_t fadelen, float crossexp);
  };

  void audiowrite(const std::string& name, const std::vector<TASCAR::wave_t>& y,
                  float fs,
                  int format = SF_FORMAT_WAV | SF_FORMAT_PCM_16 |
                               SF_ENDIAN_FILE);

  std::vector<TASCAR::wave_t> audioread(const std::string& name, float& fs);

} // namespace TASCAR

#endif

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
