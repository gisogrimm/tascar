/* License (GPL)
 *
 * Copyright (C) 2014,2015,2016,2017,2018  Giso Grimm
 *
 * This program is free software; you can redistribute it and/or
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
#ifndef SPEAKERARRAY_H
#define SPEAKERARRAY_H

#include "audiostates.h"
#include "delayline.h"
#include "filterclass.h"
#include "ola.h"
#include "tscconfig.h"

namespace TASCAR {

  uint32_t get_spklayout_checksum(const xml_element_t&);

  /**
     \brief Description of a single loudspeaker in a speaker array.
   */
  class spk_descriptor_t : public xml_element_t, public pos_t {
  public:
    // if you add members please update copy constructor!
    spk_descriptor_t(tsccfg::node_t);
    spk_descriptor_t(const spk_descriptor_t&);
    virtual ~spk_descriptor_t();
    double get_rel_azim(double az_src) const;
    double get_cos_adist(pos_t src_unit) const;
    void set_comp(uint32_t n_fragment, float f_sample);
    void clear_comp();
    void set_comp_from_coeff(const std::vector<double>& c, uint32_t n_fragment);
    void set_comp_from_gains(uint32_t n_fragment, float f_sample);
    void postproc_spkeq(TASCAR::wave_t& w);
    double az;
    double el;
    double r;
    double delay;
    std::string label;
    std::string connect;
    std::vector<double> compB;
    double gain;
    // derived parameters:
    pos_t unitvector;
    double spkgain;
    double dr;
    // decoder matrix:
    void update_foa_decoder(float gain, float xyzgain);
    float d_w;
    float d_x;
    float d_y;
    float d_z;
    float densityweight;
    // frequency response correction:
  private:
    // FIR filter:
    TASCAR::partitioned_conv_t* comp;
    std::mutex eqmtx;

  public:
    // IIR filter:
    TASCAR::multiband_pareq_t eq;
    std::vector<float> eqfreq;
    std::vector<float> eqgain;
    uint32_t eqstages = 0u;
    uint32_t eqfirlen = 0u;
    bool calibrate = true;
  };

  class spk_array_cfg_t : public xml_element_t {
  public:
    spk_array_cfg_t(tsccfg::node_t, bool use_parent_xml);
    ~spk_array_cfg_t();
    std::string layout;
    std::string name;

  protected:
    TASCAR::xml_doc_t* doc;
    tsccfg::node_t e_layout;
  };

  /**
     \brief Loudspeaker array.
   */
  class spk_array_t : public spk_array_cfg_t,
                      public std::vector<spk_descriptor_t>,
                      public audiostates_t {
  public:
    spk_array_t(tsccfg::node_t, bool use_parent_xml,
                const std::string& elementname_ = "speaker",
                bool allow_empty = false);
    ~spk_array_t();

  private:
    spk_array_t(const spk_array_t&);

  public:
    enum dffusedecoder_t { basic, maxre, inphase };
    double get_rmax() const { return rmax; };
    double get_rmin() const { return rmin; };
    class didx_t {
    public:
      didx_t() : d(0), idx(0){};
      double d;
      uint32_t idx;
    };
    const std::vector<didx_t>& sort_distance(const pos_t& psrc);
    void configure();
    xml_element_t elayout;
    void validate_attributes(std::string& msg) const;
    std::vector<TASCAR::pos_t> get_positions() const;

  private:
    double rmax;
    double rmin;
    dffusedecoder_t diffusedecoder_enum = maxre;
    std::string onload;
    std::string onunload;
    std::vector<didx_t> didx;
    std::string elementname;

  public:
    std::vector<std::string> connections;
    std::vector<TASCAR::static_delay_t> delaycomp;
    double mean_rotation;
  };

  /**
   * @brief Loudspeaker array with diffuse renderer
   *
   * If for each speaker an impulse response file for convolution
   * (attribute "conv") is specified, then the speaker output is
   * convolved with the impulse response, and that convolution output
   * is returned instead. All impulse responses need to have the same
   * number of channels.
   */
  class spk_array_diff_render_t : public spk_array_t {
  public:
    spk_array_diff_render_t(tsccfg::node_t, bool use_parent_xml,
                            const std::string& elementname_ = "speaker");
    ~spk_array_diff_render_t();
    void render_diffuse(std::vector<TASCAR::wave_t>& output);
    void add_diffuse_sound_field(const TASCAR::amb1wave_t& diff);
    void configure();
    virtual void release();
    void clear_states();
    std::string to_string() const;
    size_t num_input_channels() const { return size(); };
    size_t num_output_channels() const
    {
      return size() + subs.size() + conv_channels;
    };
    /**
     * @brief Enable or disable subwoofer output
     */
    void set_enable_subs(bool enable) { enable_subs = enable; };
    /**
     * @brief Enable or disable broadband output
     */
    void set_enable_speaker(bool enable) { enable_bb = enable; };
    std::string get_label(size_t ch) const;
    /**
     * @brief Apply calibration, delay compensation, subwoofer
     * processing and convolution
     *
     * @retval output Signal buffer to be updated
     *
     * The first num_input_channels() of the signal buffer contain the
     * input signal. After post processing, the input channels are
     * updated (calibration), the channels with indices
     * num_input_channels() to
     * num_input_channels()+num_sub_channels()-1 contain the subwoofer
     * signals, and the channels with indices
     * num_input_channels()+num_sub_channels() to
     * num_output_channels()-1 contain the (optional) convolution
     * channels.
     */
    void postproc(std::vector<wave_t>& output);
    spk_array_t subs;

  private:
    void read_xml(tsccfg::node_t elem);
    TASCAR::amb1wave_t* diffuse_field_accumulator;
    TASCAR::wave_t* diffuse_render_buffer;
    std::vector<TASCAR::overlap_save_t> decorrflt;
    bool has_diffuse = false;

  public:
    double decorr_length;
    bool decorr;
    bool densitycorr;
    // SOFA binaural convolution:
    std::string sofa_file;
    // cross-over frequency for subwoofer, if subwoofers are defined:
    double fcsub;
    double caliblevel;
    bool has_caliblevel;
    double diffusegain;
    bool has_diffusegain;
    double calibage;
    std::string calibdate;
    bool has_calibdate;
    std::string calibfor;
    bool has_calibfor;
    bool use_subs;
    // highpass filters for cross-overs, broad band speakers (24 dB/Oct):
    std::vector<TASCAR::biquadf_t> flt_hp1;
    std::vector<TASCAR::biquadf_t> flt_hp2;
    // lowpass filters for subwoofer (24 dB/Oct):
    std::vector<TASCAR::biquadf_t> flt_lowp1;
    std::vector<TASCAR::biquadf_t> flt_lowp2;
    std::vector<std::vector<float>> subweight;
    std::vector<std::string>
        convolution_ir; //< file name of impulse response for convolution
    bool use_conv = false;
    size_t conv_channels = 0;
    bool convprecalib = true;
    std::vector<std::vector<TASCAR::partitioned_conv_t*>> vvp_convolver;
    std::vector<std::string> convlabels;
    bool enable_subs = true;
    bool enable_bb = true;
  };
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
