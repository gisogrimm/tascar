/*
 * This file is part of the TASCAR software, see <http://tascar.org/>
 *
 * Copyright (c) 2018 Giso Grimm
 * Copyright (c) 2019 Giso Grimm
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

#include "jackclient.h"
#include "ola.h"
#include "session.h"
#include <string.h>

class channel_entry_t {
public:
  uint32_t inchannel;
  uint32_t outchannel;
  std::string filename;
  uint32_t filechannel;
};

class hrirconv_t : public jackc_t {
public:
  hrirconv_t(uint32_t inchannels, uint32_t outchannels,
             const std::vector<channel_entry_t>& matrix,
             const std::string& jackname, TASCAR::xml_element_t& e);
  virtual ~hrirconv_t();
  int process(jack_nframes_t nframes, const std::vector<float*>& inBuffer,
              const std::vector<float*>& outBuffer);

private:
  std::vector<TASCAR::partitioned_conv_t*> cnv;
  std::vector<channel_entry_t> matrix_;
};

hrirconv_t::hrirconv_t(uint32_t inchannels, uint32_t outchannels,
                       const std::vector<channel_entry_t>& matrix,
                       const std::string& jackname, TASCAR::xml_element_t& e)
    : jackc_t(jackname), matrix_(matrix)
{
  for(std::vector<channel_entry_t>::iterator mit = matrix_.begin();
      mit != matrix_.end(); ++mit) {
    if(mit->inchannel >= inchannels) {
      DEBUG(mit->inchannel);
      DEBUG(inchannels);
      throw TASCAR::ErrMsg("Invalid input channel number.");
    }
    if(mit->outchannel >= outchannels) {
      DEBUG(mit->outchannel);
      DEBUG(outchannels);
      throw TASCAR::ErrMsg("Invalid output channel number.");
    }
  }
  for(std::vector<channel_entry_t>::iterator mit = matrix_.begin();
      mit != matrix_.end(); ++mit) {
    TASCAR::sndfile_t sndf(mit->filename, mit->filechannel);
    if(sndf.get_srate() != (uint32_t)get_srate()) {
      std::ostringstream msg;
      msg << "Warning: The sample rate of file \"" << mit->filename << "\" ("
          << sndf.get_srate() << ") differs from system sample rate ("
          << get_srate() << ").";
      TASCAR::add_warning(msg.str(), e.e);
    }
    TASCAR::partitioned_conv_t* pcnv(
        new TASCAR::partitioned_conv_t(sndf.get_frames(), get_fragsize()));
    pcnv->set_irs(sndf);
    cnv.push_back(pcnv);
  }
  for(uint32_t k = 0; k < inchannels; k++) {
    char ctmp[256];
    ctmp[255] = 0;
    snprintf(ctmp, 255, "in_%d", k);
    add_input_port(ctmp);
  }
  for(uint32_t k = 0; k < outchannels; k++) {
    char ctmp[256];
    ctmp[255] = 0;
    snprintf(ctmp, 255, "out_%d", k);
    add_output_port(ctmp);
  }
  activate();
}

hrirconv_t::~hrirconv_t()
{
  deactivate();
  for(auto it = cnv.begin(); it != cnv.end(); ++it)
    delete *it;
}

int hrirconv_t::process(jack_nframes_t nframes,
                        const std::vector<float*>& inBuffer,
                        const std::vector<float*>& outBuffer)
{
  uint32_t Nout(outBuffer.size());
  for(uint32_t kOut = 0; kOut < Nout; kOut++)
    memset(outBuffer[kOut], 0, sizeof(float) * nframes);
  for(uint32_t k = 0; k < cnv.size(); k++) {
    if(matrix_[k].inchannel >= inBuffer.size()) {
      DEBUG(k);
      DEBUG(matrix_[k].inchannel);
      DEBUG(inBuffer.size());
    }
    if(matrix_[k].outchannel >= outBuffer.size()) {
      DEBUG(k);
      DEBUG(matrix_[k].outchannel);
      DEBUG(outBuffer.size());
    }
    TASCAR::wave_t w_in(nframes, inBuffer[matrix_[k].inchannel]);
    TASCAR::wave_t w_out(nframes, outBuffer[matrix_[k].outchannel]);
    cnv[k]->process(w_in, w_out);
  }
  return 0;
}

class hrirconv_var_t : public TASCAR::module_base_t {
public:
  hrirconv_var_t(const TASCAR::module_cfg_t& cfg);
  std::string id;
  uint32_t inchannels;
  uint32_t outchannels;
  bool autoconnect;
  std::string connect;
  std::string hrirfile;
  std::vector<channel_entry_t> matrix;
};

hrirconv_var_t::hrirconv_var_t(const TASCAR::module_cfg_t& cfg)
    : module_base_t(cfg), inchannels(0), outchannels(0), autoconnect(false)
{
  GET_ATTRIBUTE_(id);
  if(id.empty())
    id = "hrirconv";
  GET_ATTRIBUTE_(inchannels);
  GET_ATTRIBUTE_(outchannels);
  get_attribute_bool("autoconnect", autoconnect, "", "undocumented");
  GET_ATTRIBUTE_(connect);
  GET_ATTRIBUTE_(hrirfile);
  if(inchannels == 0)
    throw TASCAR::ErrMsg("At least one input channel required");
  if(outchannels == 0)
    throw TASCAR::ErrMsg("At least one output channel required");
  if(hrirfile.empty()) {
    for(auto entry : tsccfg::node_get_children(e, "entry")) {
      channel_entry_t che;
      get_attribute_value(entry, "in", che.inchannel);
      get_attribute_value(entry, "out", che.outchannel);
      get_attribute_value(entry, "file", che.filename);
      get_attribute_value(entry, "channel", che.filechannel);
      matrix.push_back(che);
    }
  } else {
    for(uint32_t kin = 0; kin < inchannels; ++kin) {
      for(uint32_t kout = 0; kout < outchannels; ++kout) {
        channel_entry_t che;
        che.inchannel = kin;
        che.outchannel = kout;
        che.filename = hrirfile;
        che.filechannel = kout + outchannels * kin;
        matrix.push_back(che);
      }
    }
  }
}

class hrirconv_mod_t : public hrirconv_var_t, public hrirconv_t {
public:
  hrirconv_mod_t(const TASCAR::module_cfg_t& cfg);
  void configure();
  virtual ~hrirconv_mod_t();
};

hrirconv_mod_t::hrirconv_mod_t(const TASCAR::module_cfg_t& cfg)
    : hrirconv_var_t(cfg),
      hrirconv_t(inchannels, outchannels, matrix, id, *this)
{
}

void hrirconv_mod_t::configure()
{
  module_base_t::configure();
  if(autoconnect) {
    for(std::vector<TASCAR::scene_render_rt_t*>::iterator iscenes =
            session->scenes.begin();
        iscenes != session->scenes.end(); ++iscenes) {
      for(std::vector<TASCAR::Scene::receiver_obj_t*>::iterator irec =
              (*iscenes)->receivermod_objects.begin();
          irec != (*iscenes)->receivermod_objects.end(); ++irec) {
        std::string prefix("render." + (*iscenes)->name + ":");
        if((*irec)->n_channels == inchannels) {
          for(uint32_t ch = 0; ch < inchannels; ch++) {
            std::string pn(prefix + (*irec)->get_name() + (*irec)->labels[ch]);
            connect_in(ch, pn, true);
          }
        }
      }
    }
  } else {
    // use connect variable:
    if(!hrirconv_var_t::connect.empty()) {
      std::vector<std::string> ports(
          get_port_names_regexp(TASCAR::env_expand(hrirconv_var_t::connect)));
      uint32_t ip(0);
      if(ports.empty())
        TASCAR::add_warning("No port \"" + hrirconv_var_t::connect +
                            "\" found.");
      for(auto it = ports.begin(); it != ports.end(); ++it) {
        if(ip < jackc_t::get_num_input_ports()) {
          connect_in(ip, *it, true, true);
          ++ip;
        }
      }
    }
  }
}

hrirconv_mod_t::~hrirconv_mod_t() {}

REGISTER_MODULE(hrirconv_mod_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
