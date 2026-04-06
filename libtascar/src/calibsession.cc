/*
 * This file is part of the TASCAR software, see <http://tascar.org/>
 *
 * Copyright (c) 2022 Giso Grimm
 */
/**
 * @file calibsession.cc
 * @brief Speaker calibration class
 * @ingroup libtascar
 * @author Giso Grimm
 * @date 2022
 *
 * @section license License (GPL)
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

#include "calibsession.h"
#include "fft.h"
#include "jackiowav.h"
#include <fstream>
#include <unistd.h>

using namespace TASCAR;

float get_coherence(const TASCAR::wave_t& w1, const TASCAR::wave_t& w2,
                    float fmin, float fmax, float fs)
{
  if(w1.n != w2.n)
    return 0.0f;
  TASCAR::fft_t fft1(w1.n);
  TASCAR::fft_t fft2(w2.n);
  fft1.execute(w1);
  fft2.execute(w2);
  float xy = 0.0f;
  float xx = 0.0f;
  float yy = 0.0f;
  size_t b_min = std::min((float)fft1.s.n_, fmin / fs * w1.n);
  size_t b_max = std::min((float)fft1.s.n_, fmax / fs * w1.n);
  TASCAR::spec_t XY(fft1.s.n_);
  TASCAR::spec_t XX(fft1.s.n_);
  TASCAR::spec_t YY(fft1.s.n_);
  for(size_t k = b_min; k < b_max; ++k) {
    XY.b[k] = fft1.s.b[k] * std::conj(fft2.s.b[k]);
    XX.b[k] = fft1.s.b[k] * std::conj(fft1.s.b[k]);
    YY.b[k] = fft2.s.b[k] * std::conj(fft2.s.b[k]);
  }
  fft1.execute(XY);
  xy = fft1.w.ms();
  fft1.execute(XX);
  xx = fft1.w.ms();
  fft1.execute(YY);
  yy = fft1.w.ms();
  return xy / sqrtf(xx * yy);
}

std::string get_calibfor(const std::string& fname)
{
  TASCAR::xml_doc_t doc(fname, TASCAR::xml_doc_t::LOAD_FILE);
  return doc.root.get_attribute("calibfor");
}

std::vector<std::string> string_token(std::string s,
                                      const std::string& delimiter)
{
  std::vector<std::string> rv;
  size_t pos = 0;
  std::string token;
  while((pos = s.find(delimiter)) != std::string::npos) {
    token = s.substr(0, pos);
    rv.push_back(token);
    s.erase(0, pos + delimiter.length());
  }
  rv.push_back(s);
  return rv;
}

spk_eq_param_t::spk_eq_param_t(bool issub_) : issub(issub_)
{
  factory_reset();
}

void spk_eq_param_t::factory_reset()
{
  if(issub) {
    fmin = 31.25f;
    fmax = 62.5f;
    duration = 4.0f;
    prewait = 0.125f;
    reflevel = 80.0f;
    bandsperoctave = 8.0f;
    bandoverlap = 2.0f;
    max_eqstages = 0u;
    max_eqfirlen = 0u;
  } else {
    fmin = 62.5f;
    fmax = 4000.0f;
    duration = 1.0f;
    prewait = 0.125;
    reflevel = 80.0f;
    bandsperoctave = 3.0f;
    bandoverlap = 2.0f;
    max_eqstages = 0u;
    max_eqfirlen = 0u;
  }
}

#define READ_DEF(x) x = TASCAR::config(path + "." #x, x)

void spk_eq_param_t::read_defaults()
{
  factory_reset();
  std::string path = "tascar.spkcalib";
  if(issub)
    path = "tascar.spkcalib.sub";
  READ_DEF(fmin);
  READ_DEF(fmax);
  READ_DEF(duration);
  READ_DEF(prewait);
  READ_DEF(reflevel);
  READ_DEF(bandsperoctave);
  READ_DEF(bandoverlap);
  READ_DEF(max_eqstages);
  READ_DEF(max_eqfirlen);
  try {
    validate();
  }
  catch(const std::exception& e) {
    throw TASCAR::ErrMsg(
        std::string("While reading ") + (issub ? "subwoofer" : "speaker") +
        " parameters from global configuration file:\n" + e.what());
  }
}

void spk_eq_param_t::read_xml(const tsccfg::node_t& layoutnode)
{
  TASCAR::xml_element_t xml(layoutnode);
  tsccfg::node_t spkcalibnode;
  if(issub)
    spkcalibnode = xml.find_or_add_child("subcalibconfig");
  else
    spkcalibnode = xml.find_or_add_child("speakercalibconfig");
  TASCAR::xml_element_t e(spkcalibnode);
  e.GET_ATTRIBUTE(fmin, "Hz", "Lower frequency limit of calibration.");
  e.GET_ATTRIBUTE(fmax, "Hz", "Upper frequency limit of calibration.");
  e.GET_ATTRIBUTE(duration, "s", "Stimulus duration.");
  e.GET_ATTRIBUTE(prewait, "s",
                  "Time between stimulus onset and measurement start.");
  e.GET_ATTRIBUTE(reflevel, "dB", "Reference level.");
  e.GET_ATTRIBUTE(bandsperoctave, "bpo",
                  "Bands per octave in filterbank for level equalization.");
  e.GET_ATTRIBUTE(
      bandoverlap, "bands",
      "Overlap in frequency bands in filterbank for level equalization.");
  e.GET_ATTRIBUTE(max_eqstages, "",
                  "Number of filter stages for frequency compensation.");
  e.GET_ATTRIBUTE(
      max_eqfirlen, "",
      "Length of FIR filters for speaker equalization (0 = disable).");
  try {
    validate();
  }
  catch(const std::exception& e) {
    throw TASCAR::ErrMsg(std::string("While reading ") +
                         (issub ? "subwoofer" : "speaker") +
                         " parameters from XML file:\n" + e.what());
  }
}

void spk_eq_param_t::save_xml(const tsccfg::node_t& layoutnode) const
{
  validate();
  TASCAR::xml_element_t xml(layoutnode);
  tsccfg::node_t spkcalibnode;
  if(issub)
    spkcalibnode = xml.find_or_add_child("subcalibconfig");
  else
    spkcalibnode = xml.find_or_add_child("speakercalibconfig");
  TASCAR::xml_element_t e(spkcalibnode);
  e.SET_ATTRIBUTE(fmin);
  e.SET_ATTRIBUTE(fmax);
  e.SET_ATTRIBUTE(duration);
  e.SET_ATTRIBUTE(prewait);
  e.SET_ATTRIBUTE(reflevel);
  e.SET_ATTRIBUTE(bandsperoctave);
  e.SET_ATTRIBUTE(bandoverlap);
  e.SET_ATTRIBUTE(max_eqstages);
  e.SET_ATTRIBUTE(max_eqfirlen);
}

void spk_eq_param_t::validate() const
{
  if(!(fmin > 0.0f))
    throw TASCAR::ErrMsg(
        std::string("fmin needs to be above zero (current value: ") +
        TASCAR::to_string(fmin) + " Hz).");
  if(!(fmax > fmin))
    throw TASCAR::ErrMsg(
        std::string("fmax needs to be larger than fmin (fmin=") +
        TASCAR::to_string(fmin) + " Hz, fmax=" + TASCAR::to_string(fmax) +
        " Hz).");
  if(!(duration > 0.0f))
    throw TASCAR::ErrMsg(
        std::string("duration needs to be above zero (current value: ") +
        TASCAR::to_string(duration) + " s).");
  if(!(prewait > 0.0f))
    throw TASCAR::ErrMsg(
        std::string("prewait needs to be above zero (current value: ") +
        TASCAR::to_string(prewait) + " s).");
  if(!(reflevel < 85.0f))
    throw TASCAR::ErrMsg(
        std::string("reflevel needs to be below 85 dB SPL (current value: ") +
        TASCAR::to_string(reflevel) + " dB SPL).");
  if(!(reflevel > 0.0f))
    throw TASCAR::ErrMsg(
        std::string("reflevel needs to be above 0 dB SPL (current value: ") +
        TASCAR::to_string(reflevel) + " dB SPL).");
  if(!(bandsperoctave > 0.0f))
    throw TASCAR::ErrMsg(
        std::string("bandsperoctave needs to be above 0 (current value: ") +
        TASCAR::to_string(bandsperoctave) + ").");
  if(!(bandoverlap >= 0.0f))
    throw TASCAR::ErrMsg(
        std::string("bandoverlap cannot be negative (current value: ") +
        TASCAR::to_string(bandoverlap) + ").");
  if((max_eqstages > 0) && (max_eqfirlen > 0))
    throw TASCAR::ErrMsg(
        "cannot use parametric equalizers and FIR filter for simultation. "
        "Either eqstages or eqfirlen needs to be zero");
  // std::max(0.0f,log2f(fmax/fmin)*bandsperoctave/3.0f-1.0f)
  // if(max_eqstages > )
  //  throw TASCAR::ErrMsg(
  //      std::string("bandoverlap cannot be negative (current value: ") +
  //      TASCAR::to_string(bandoverlap) + ").");
  // e.SET_ATTRIBUTE(max_eqstages);
}

spkeq_report_t::spkeq_report_t() {}

spkeq_report_t::spkeq_report_t(std::string label, const std::vector<float>& vF,
                               const std::vector<float>& vG_precalib,
                               const std::vector<float>& vG_postcalib,
                               float gain_db)
    : label(label), vF(vF), vG_precalib(vG_precalib),
      vG_postcalib(vG_postcalib), gain_db(gain_db)
{
  DEBUG(label);
  DEBUG(TASCAR::to_string(vF));
  DEBUG(TASCAR::to_string(vG_precalib));
  DEBUG(TASCAR::to_string(vG_postcalib));
  DEBUG(gain_db);
}

calib_cfg_t::calib_cfg_t() : par_speaker(), par_sub(true), has_sub(true)
{
  read_defaults();
}

void calib_cfg_t::validate() const
{
  par_speaker.validate();
  if(has_sub)
    par_sub.validate();
  if(refport.empty())
    throw TASCAR::ErrMsg("At least one measurement microphone connection is "
                         "required for calibration");
  if(refport.size() != miccalib.size())
    throw TASCAR::ErrMsg("For each connected measurement microphone a "
                         "calibration value is required.");
}

void calib_cfg_t::factory_reset()
{
  par_speaker.factory_reset();
  par_sub.factory_reset();
  refport.clear();
  miccalib.clear();
  initcal = true;
}

void calib_cfg_t::read_defaults()
{
  factory_reset();
  par_speaker.read_defaults();
  par_sub.read_defaults();
  refport = str2vecstr(
      TASCAR::config("tascar.spkcalib.inputport", "system:capture_1"));
  miccalib = str2vecfloat(
      TASCAR::config("tascar.spkcalib.miccalib",
                     TASCAR::to_string(std::vector<float>({0.0f}))));
  for(auto& c : miccalib)
    c = TASCAR::dbspl2lin(c);
}

#define WRITE_DEF(x)                                                           \
  TASCAR::config_forceoverwrite(path + "." #x, TASCAR::to_string(x))

void spk_eq_param_t::write_defaults()
{
  std::string path = "tascar.spkcalib";
  if(issub)
    path = "tascar.spkcalib.sub";
  WRITE_DEF(fmin);
  WRITE_DEF(fmax);
  WRITE_DEF(duration);
  WRITE_DEF(prewait);
  WRITE_DEF(reflevel);
  WRITE_DEF(bandsperoctave);
  WRITE_DEF(bandoverlap);
  WRITE_DEF(max_eqstages);
  WRITE_DEF(max_eqfirlen);
}

void calib_cfg_t::save_defaults()
{
  par_speaker.write_defaults();
  if(has_sub)
    par_sub.write_defaults();
  std::string path = "tascar.spkcalib";
  TASCAR::config_forceoverwrite(path + ".inputport",
                                TASCAR::vecstr2str(refport));
  TASCAR::config_forceoverwrite(path + ".miccalib",
                                TASCAR::to_string_dbspl(miccalib));
  std::vector<std::string> keys = {"tascar.spkcalib.inputport",
                                   "tascar.spkcalib.miccalib",
                                   "tascar.spkcalib.fc"};
  for(auto key :
      {"fmin", "fmax", "duration", "prewait", "reflevel", "bandsperoctave",
       "bandoverlap", "max_eqstages", "max_eqfirlen"}) {
    keys.push_back(std::string("tascar.spkcalib.") + key);
    if(has_sub)
      keys.push_back(std::string("tascar.spkcalib.sub.") + key);
  }
  config_save_keys(keys);
}

void calib_cfg_t::read_xml(const tsccfg::node_t& layoutnode)
{
  par_speaker.read_xml(layoutnode);
  if(has_sub)
    par_sub.read_xml(layoutnode);
  TASCAR::xml_element_t e(layoutnode);
  initcal = tsccfg::node_get_attribute_value(layoutnode, "calibdate").empty();
}

void calib_cfg_t::save_xml(const tsccfg::node_t& layoutnode) const
{
  par_speaker.save_xml(layoutnode);
  if(has_sub)
    par_sub.save_xml(layoutnode);
}

void add_stimulus_plugin(xml_element_t node, const spk_eq_param_t& par)
{
  xml_element_t e_plugs(node.find_or_add_child("plugins"));
  xml_element_t e_pink(e_plugs.add_child("pink"));
  e_pink.set_attribute("level", TASCAR::to_string(par.reflevel));
  e_pink.set_attribute("period", TASCAR::to_string(par.duration));
  e_pink.set_attribute("fmin", TASCAR::to_string(par.fmin));
  e_pink.set_attribute("fmax", TASCAR::to_string(par.fmax));
  e_pink.set_attribute("alpha", "1");
}

calibsession_t::calibsession_t(const std::string& fname, const calib_cfg_t& cfg)
    : session_t(
          "<?xml version=\"1.0\"?><session srv_port=\"none\" license=\"CC0\"/>",
          LOAD_STRING, ""),
      gainmodified(false), levelsrecorded(false), calibrated(false),
      calibrated_diff(false), startlevel(0), startdiffgain(0), delta(0),
      delta_diff(0), spkname(fname), spk_file(NULL), cfg_(cfg), lmin(0),
      lmax(0), lmean(0), calibfor(get_calibfor(fname)),
      jackrec(cfg_.refport.size() + 1, "spkcalibrec")
{
  cfg.validate();
  if(calibfor.empty())
    calibfor = "type:nsp";
  // create a new session, no OSC port:
  root.set_attribute("srv_port", "none");
  // add the calibration scene:
  xml_element_t e_scene(root.add_child("scene"));
  e_scene.set_attribute("name", "calib");
  // add a point source for broadband stimulus, muted for now:
  xml_element_t e_src(e_scene.add_child("source"));
  e_src.set_attribute("mute", "true");
  // add pink noise generator:
  add_stimulus_plugin(e_src.add_child("sound"), cfg_.par_speaker);
  // add a point source for subwoofer stimulus, muted for now:
  xml_element_t e_subsrc(e_scene.add_child("source"));
  e_subsrc.set_attribute("name", "srcsub");
  e_subsrc.set_attribute("mute", "true");
  add_stimulus_plugin(e_subsrc.add_child("sound"), cfg_.par_sub);
  // receiver 1 is always nsp, for speaker level differences:
  xml_element_t e_rcvr(e_scene.add_child("receiver"));
  e_rcvr.set_attribute("name", "rec_nsp");
  e_rcvr.set_attribute("type", "nsp");
  e_rcvr.set_attribute("layout", fname);
  e_rcvr.set_attribute("create_input_ports", "true");
  // receiver 2 is specific to the layout, for overall calibration:
  xml_element_t e_rcvr2(e_scene.add_child("receiver"));
  e_rcvr2.set_attribute("name", "rec_spec");
  e_rcvr2.set_attribute("mute", "true");
  e_rcvr2.set_attribute("layout", fname);
  e_rcvr2.set_attribute("create_input_ports", "true");
  // receiver 3 is omni, for reference signal:
  xml_element_t e_rcvr3(e_scene.add_child("receiver"));
  e_rcvr3.set_attribute("type", "omni");
  e_rcvr3.set_attribute("name", "ref");
  xml_element_t e_rcvr3plug(e_rcvr3.add_child("plugins"));
  xml_element_t e_rcvr3delay(e_rcvr3plug.add_child("delay"));
  e_rcvr3delay.set_attribute(
      "delay",
      TASCAR::to_string(2.0f * jackrec.get_fragsize() / jackrec.get_srate()));
  std::vector<std::string> receivertypeattr(string_token(calibfor, ","));
  for(auto typeattr : receivertypeattr) {
    std::vector<std::string> pair(string_token(typeattr, ":"));
    if(pair.size() != 2)
      throw TASCAR::ErrMsg(
          "Invalid format of 'calibfor' attribute '" + calibfor +
          "': Expected comma separated list of name:value pairs.");
    e_rcvr2.set_attribute(pair[0], pair[1]);
  }
  // add diffuse source for diffuse gain calibration:
  xml_element_t e_diff(e_scene.add_child("diffuse"));
  e_diff.set_attribute("mute", "true");
  e_diff.set_attribute("name", "diffuse");
  add_stimulus_plugin(e_diff, cfg_.par_speaker);
  // extra routes:
  xml_element_t e_mods(root.add_child("modules"));
  xml_element_t e_route_pink(e_mods.add_child("route"));
  e_route_pink.set_attribute("name", "pink");
  e_route_pink.set_attribute("channels", "1");
  add_stimulus_plugin(e_route_pink, cfg_.par_speaker);
  xml_element_t e_route_sub(e_mods.add_child("route"));
  e_route_sub.set_attribute("name", "sub");
  e_route_sub.set_attribute("channels", "1");
  add_stimulus_plugin(e_route_sub, cfg_.par_sub);
  xml_element_t e_route_levels(e_mods.add_child("route"));
  e_route_levels.set_attribute("name", "levels");
  e_route_levels.set_attribute("channels", std::to_string(cfg_.refport.size()));
  e_route_levels.set_attribute("connect", TASCAR::vecstr2str(cfg_.refport));
  e_route_levels.set_attribute("caliblevel_in",
                               TASCAR::to_string_dbspl(cfg_.miccalib));
  e_route_levels.set_attribute("levelmeter_tc",
                               TASCAR::to_string(cfg_.par_speaker.duration));
  e_route_levels.set_attribute("levelmeter_weight", "C");
  // end of scene creation.
  // for debugging store calibration session file:
  // save("temp.cfg");
  add_scene(e_scene.e);
  add_module(e_route_pink.e);
  add_module(e_route_sub.e);
  add_module(e_route_levels.e);
  spk_file = new spk_array_diff_render_t(e_rcvr.e, false);
  levels = std::vector<float>(spk_file->size(), 0.0);
  sublevels = std::vector<float>(spk_file->subs.size(), 0.0);
  // levelsfrg = std::vector<float>(spk_file->size(), 0.0);
  // sublevelsfrg = std::vector<float>(spk_file->subs.size(), 0.0);
  // validate scene:
  if(scenes.empty())
    throw TASCAR::ErrMsg("Programming error: no scene");
  if(scenes[0]->source_objects.size() != 2) {
    for(const auto& scene : scenes) {
      DEBUG(scene->source_objects.size());
    }
    throw TASCAR::ErrMsg("Programming error: not exactly two sources.");
  }
  if(scenes[0]->receivermod_objects.size() != 3)
    throw TASCAR::ErrMsg("Programming error: not exactly three receivers.");
  scenes.back()->source_objects[0]->dlocation = pos_t(1, 0, 0);
  rec_nsp = scenes.back()->receivermod_objects[0];
  spk_nsp = dynamic_cast<TASCAR::receivermod_base_speaker_t*>(rec_nsp->libdata);
  if(!spk_nsp)
    throw TASCAR::ErrMsg("Programming error: Invalid speaker type.");
  rec_spec = scenes.back()->receivermod_objects[1];
  spk_spec =
      dynamic_cast<TASCAR::receivermod_base_speaker_t*>(rec_spec->libdata);
  if(!spk_spec)
    throw TASCAR::ErrMsg("Programming error: Invalid speaker type.");
  startlevel = get_caliblevel();
  startdiffgain = get_diffusegain();
  for(auto recspk : {spk_nsp, spk_spec}) {
    for(auto& spk : recspk->spkpos) {
      spk.eqstages = 0;
      spk.eqfirlen = 0;
    }
    for(auto& spk : recspk->spkpos.subs) {
      spk.eqstages = 0;
      spk.eqfirlen = 0;
    }
  }
  for(size_t ich = 0; ich < cfg_.refport.size() + 1; ++ich) {
    bbrecbuf.push_back(
        (uint32_t)(jackrec.get_srate() * cfg_.par_speaker.duration));
    subrecbuf.push_back(
        (uint32_t)(jackrec.get_srate() * cfg_.par_sub.duration));
  }
  start();
  for(auto& pmod : modules) {
    TASCAR::Scene::route_t* rp(
        dynamic_cast<TASCAR::Scene::route_t*>(pmod->libdata));
    if(rp && (rp->get_name() == "levels")) {
      levelroute = rp;
    }
  }
}

calibsession_t::~calibsession_t()
{
  delete spk_file;
}

void calibsession_t::reset_levels()
{
  levelsrecorded = false;
  for(auto& r : levelsfrg)
    r = 0.0f;
  for(auto& r : sublevelsfrg)
    r = 0.0f;
  for(auto recspk : {spk_nsp, spk_spec}) {
    for(uint32_t k = 0; k < levels.size(); ++k)
      if(recspk->spkpos[k].calibrate)
        recspk->spkpos[k].gain = 1.0;
    for(uint32_t k = 0; k < sublevels.size(); ++k)
      if(recspk->spkpos.subs[k].calibrate)
        recspk->spkpos.subs[k].gain = 1.0;
  }
}

void calibsession_t::enable_spkcorr_spec(bool b)
{
  for(uint32_t k = 0; k < levels.size(); ++k)
    if(b) {
      spk_spec->spkpos[k].eq = spk_nsp->spkpos[k].eq;
      spk_spec->spkpos[k].eqstages = spk_nsp->spkpos[k].eqstages;
      spk_spec->spkpos[k].eqfirlen = spk_nsp->spkpos[k].eqfirlen;
      spk_spec->spkpos[k].eqfreq = spk_nsp->spkpos[k].eqfreq;
      spk_spec->spkpos[k].eqgain = spk_nsp->spkpos[k].eqgain;
      spk_spec->spkpos[k].set_comp(jackrec.get_fragsize(),
                                   (float)jackrec.get_srate());
    } else {
      spk_spec->spkpos[k].eqstages = 0u;
      spk_spec->spkpos[k].eqfirlen = 0u;
      spk_spec->spkpos[k].clear_comp();
    }
  for(uint32_t k = 0; k < sublevels.size(); ++k)
    if(b) {
      spk_spec->spkpos.subs[k].eq = spk_nsp->spkpos.subs[k].eq;
      spk_spec->spkpos.subs[k].eqstages = spk_nsp->spkpos.subs[k].eqstages;
      spk_spec->spkpos.subs[k].eqfirlen = spk_nsp->spkpos.subs[k].eqfirlen;
      spk_spec->spkpos.subs[k].eqfreq = spk_nsp->spkpos.subs[k].eqfreq;
      spk_spec->spkpos.subs[k].eqgain = spk_nsp->spkpos.subs[k].eqgain;
      spk_spec->spkpos.subs[k].set_comp(jackrec.get_fragsize(),
                                        (float)jackrec.get_srate());
    } else {
      spk_spec->spkpos.subs[k].eqstages = 0u;
      spk_spec->spkpos.subs[k].eqfirlen = 0u;
      spk_spec->spkpos.subs[k].clear_comp();
    }
}

float getmedian(std::vector<float> vec)
{
  size_t size = vec.size();
  if(size == 0)
    return 0.0f;
  sort(vec.begin(), vec.end());
  if(size % 2 == 0)
    return 0.5f * (vec[size / 2 - 1] + vec[size / 2]);
  return vec[size / 2];
}

/**
 * @brief Record calibration of one single speaker.
 */
void get_speaker_equalization(
    const spk_descriptor_t& spk, TASCAR::Scene::src_object_t& src,
    jackrec2wave_t& jackrec, const std::vector<TASCAR::wave_t>& recbuf,
    const std::vector<float>& miccalib, const std::vector<std::string>& ports,
    levelmeter::weight_t weight, const spk_eq_param_t& calibpar,
    std::vector<float>& levels, std::vector<float>& vF, std::vector<float>& vG,
    std::vector<float>& level_fs, std::vector<float>* vcoh)
{
  vF.clear();
  vG.clear();
  level_fs.clear();
  // move source to speaker position:
  src.dlocation = spk.unitvector;
  usleep((unsigned int)(1e6f * calibpar.prewait));
  // create level meter:
  TASCAR::levelmeter_t levelmeter((float)jackrec.get_srate(), calibpar.duration,
                                  weight);
  // record measurement signal:
  jackrec.rec(recbuf, ports);
  // squared broadband levels for averaging:
  float lev_sqr = 0.0f;
  // container for frequency-dependent levels, non-averaged:
  std::vector<float> vL;
  // container for frequency-dependent averaged levels:
  std::vector<float> vLmean;
  // container for frequency-dependent reference levels (test stimulus):
  std::vector<float> vLref;
  // calc average across input channels:
  for(size_t ch = 0u; ch < ports.size() - 1u; ++ch) {
    // calculated calibrated input levels:
    auto& wav = recbuf[ch];
    if(vcoh)
      vcoh->push_back(get_coherence(wav, recbuf.back(), calibpar.fmin,
                                    calibpar.fmax, jackrec.get_srate()));
    level_fs.push_back(10.0f * log10f(wav.ms()));
    float calgain = miccalib[ch];
    float* wav_begin = wav.d;
    float* wav_end = wav_begin + wav.n;
    for(float* pv = wav_begin; pv < wav_end; ++pv)
      *pv *= calgain;
    // read out level meter based on calibrated inputs:
    levelmeter.update(wav);
    lev_sqr += levelmeter.ms();
    // get levels in filter bands:
    TASCAR::get_bandlevels(wav, calibpar.fmin, calibpar.fmax,
                           (float)jackrec.get_srate(), calibpar.bandsperoctave,
                           calibpar.bandoverlap, vF, vL);
    for(auto& l : vL)
      l = powf(10.0f, 0.1f * l);
    if(vLmean.empty())
      vLmean = vL;
    else {
      for(size_t k = 0; k < vL.size(); ++k)
        vLmean[k] += vL[k];
    }
  }
  for(auto& l : vLmean) {
    l /= (float)recbuf.size();
    l = 10.0f * log10f(l);
  }
  lev_sqr /= (float)recbuf.size();
  lev_sqr = 10.0f * log10f(lev_sqr) - calibpar.reflevel;
  levels.push_back(lev_sqr);
  // get reference stimulus properties:
  levelmeter.update(recbuf.back());
  TASCAR::get_bandlevels(recbuf.back(), calibpar.fmin, calibpar.fmax,
                         (float)jackrec.get_srate(), calibpar.bandsperoctave,
                         calibpar.bandoverlap, vF, vLref);
  for(size_t ch = 0; ch < std::min(vLmean.size(), vLref.size()); ++ch)
    vG.push_back(vLref[ch] - vLmean[ch]);
  // auto vl_max = vG.back();
  // for(const auto& l : vG)
  //  vl_max = std::max(vl_max, l);
  // for(auto& l : vG)
  //  l -= vl_max;
  auto med = getmedian(vG);
  for(auto& g : vG)
    g -= med;
}

void get_levels_(spk_array_t& spks, TASCAR::Scene::src_object_t& src,
                 jackrec2wave_t& jackrec,
                 const std::vector<TASCAR::wave_t>& recbuf,
                 const std::vector<float>& miccalib,
                 const std::vector<std::string>& ports,
                 levelmeter::weight_t weight, const spk_eq_param_t& calibpar,
                 std::vector<float>& levels,
                 std::vector<spkeq_report_t>& reports,
                 std::vector<float>* vcoh = NULL)
{
  levels.clear();
  std::vector<float> vF;
  std::vector<float> levels_tmp;
  std::vector<float> vG;
  if(miccalib.size() + 1 != recbuf.size())
    throw TASCAR::ErrMsg(std::string("Programming error ") + __FILE__ + ":" +
                         std::to_string(__LINE__));
  // measure levels of all broadband speakers:
  size_t c = 0u;
  for(auto& spk : spks) {
    ++c;
    spkeq_report_t report;
    if(spk.calibrate &&
       ((calibpar.max_eqstages > 0u) || (calibpar.max_eqfirlen > 0u))) {
      // deactivate frequency correction:
      spk.eqstages = 0u;
      spk.eqfirlen = 0u;
      spk.clear_comp();
      // move source to speaker position:
      get_speaker_equalization(spk, src, jackrec, recbuf, miccalib, ports,
                               weight, calibpar, levels_tmp, vF, vG,
                               report.level_db_re_fs, vcoh);
      report.vF = vF;
      report.vG_precalib = vG;
      for(auto& g : report.vG_precalib)
        g *= -1.0f;
      spk.eqfreq = vF;
      spk.eqgain = vG;
      spk.eqstages = calibpar.max_eqstages;
      spk.eqfirlen = calibpar.max_eqfirlen;
      spk.set_comp(jackrec.get_fragsize(), (float)jackrec.get_srate());
      if(calibpar.max_eqstages > 0u) {
        report.eq_f = spk.eq.get_f();
        report.eq_g = spk.eq.get_g();
        report.eq_q = spk.eq.get_q();
      }
      if(calibpar.max_eqfirlen > 0u) {
        report.eq_f.clear();
        report.eq_g.clear();
        report.eq_q.clear();
      }
    }
    if(spk.calibrate) {
      get_speaker_equalization(spk, src, jackrec, recbuf, miccalib, ports,
                               weight, calibpar, levels, vF, vG,
                               report.level_db_re_fs, &report.coh);
    }
    report.label = calibpar.issub ? "sub" : "spk";
    report.label += std::to_string(c);
    if(spk.label.size())
      report.label += " " + spk.label;
    // report.label += " " + TASCAR::to_string(levels.back()) + " dB";
    report.vF = vF;
    report.vG_postcalib = vG;
    for(auto& g : report.vG_postcalib)
      g *= -1.0f;
    // auto med = getmedian(report.vG_postcalib);
    // for(auto& g : report.vG_postcalib)
    //  g -= med;
    // auto vl_min = vG.back();
    // auto vl_max = vG.back();
    // for(const auto& l : vG) {
    //  vl_min = std::min(vl_min, l);
    //  vl_max = std::max(vl_max, l);
    //}
    // levelrange.push_back(vl_max - vl_min);
    if(spk.calibrate)
      reports.push_back(report);
  }
  // return reports;
}

void calibsession_t::get_levels()
{
  spkeq_report.clear();
  //
  // first broadband speakers:
  //
  auto allports = cfg_.refport;
  allports.push_back("render.calib:ref.0");
  // mute subwoofer source:
  scenes.back()->source_objects[1]->set_mute(true);
  // unmute broadband source:
  scenes.back()->source_objects[0]->set_mute(false);
  // unmute the NSP receiver:
  rec_spec->set_mute(true);
  rec_nsp->set_mute(false);
  spk_nsp->spkpos.set_enable_subs(false);
  get_levels_(spk_nsp->spkpos, *(scenes.back()->source_objects[0]), jackrec,
              bbrecbuf, cfg_.miccalib, allports, TASCAR::levelmeter::C,
              cfg_.par_speaker, levels, spkeq_report);
  spk_nsp->spkpos.set_enable_subs(true);
  //
  // subwoofer:
  //
  if(!spk_nsp->spkpos.subs.empty()) {
    // mute broadband source:
    scenes.back()->source_objects[0]->set_mute(true);
    // unmute subwoofer source:
    scenes.back()->source_objects[1]->set_mute(false);
    spk_nsp->spkpos.set_enable_speaker(false);
    get_levels_(spk_nsp->spkpos.subs, *(scenes.back()->source_objects[1]),
                jackrec, subrecbuf, cfg_.miccalib, allports,
                TASCAR::levelmeter::Z, cfg_.par_sub, sublevels, spkeq_report);
    spk_nsp->spkpos.set_enable_speaker(true);
  }
  // mute source and reset position:
  for(auto src : scenes.back()->source_objects) {
    src->set_mute(true);
    src->dlocation = pos_t(1, 0, 0);
  }
  // convert levels into gains:
  lmin = levels[0];
  lmax = levels[0];
  lmean = 0;
  for(auto l : levels) {
    lmean += l;
    lmin = std::min(l, lmin);
    lmax = std::max(l, lmax);
  }
  lmean /= (float)levels.size();
  // update gains of all receiver objects:
  for(auto recspk : {spk_nsp, spk_spec}) {
    // first modify gains:
    for(uint32_t k = 0; k < levels.size(); ++k)
      recspk->spkpos[k].gain *= pow(10.0, 0.05 * (lmin - levels[k]));
    for(uint32_t k = 0; k < sublevels.size(); ++k)
      recspk->spkpos.subs[k].gain *= pow(10.0, 0.05 * (lmin - sublevels[k]));
    // set max gain of broadband speakers to zero:
    double lmax(0);
    for(uint32_t k = 0; k < levels.size(); ++k)
      lmax = std::max(lmax, recspk->spkpos[k].gain);
    for(uint32_t k = 0; k < levels.size(); ++k)
      recspk->spkpos[k].gain /= lmax;
    for(uint32_t k = 0; k < sublevels.size(); ++k)
      recspk->spkpos.subs[k].gain /= lmax;
  }
  for(uint32_t k = 0; k < levels.size(); ++k)
    spkeq_report[k].gain_db = TASCAR::lin2db(spk_nsp->spkpos[k].gain);
  for(uint32_t k = 0; k < sublevels.size(); ++k)
    spkeq_report[k + levels.size()].gain_db =
        TASCAR::lin2db(spk_nsp->spkpos.subs[k].gain);
  levelsrecorded = true;
}

void calibsession_t::file_saveas(const std::string& fname)
{
  // convert levels into gains:
  std::vector<double> gains;
  float lmin(levels[0]);
  for(auto l : levels)
    lmin = std::min(l, lmin);
  for(uint32_t k = 0; k < levels.size(); ++k)
    gains.push_back(20 * log10((*spk_file)[k].gain) + lmin - levels[k]);
  // rewrite file:
  TASCAR::xml_doc_t doc(spkname, TASCAR::xml_doc_t::LOAD_FILE);
  if(doc.root.get_element_name() != "layout")
    throw TASCAR::ErrMsg(
        "Invalid file type, expected root node type \"layout\", got \"" +
        doc.root.get_element_name() + "\".");
  TASCAR::xml_element_t elayout(doc.root);
  elayout.set_attribute("caliblevel", TASCAR::to_string(get_caliblevel()));
  elayout.set_attribute("diffusegain", TASCAR::to_string(get_diffusegain()));
  // update gains:
  TASCAR::spk_array_diff_render_t array(doc.root(), true);
  size_t k = 0;
  for(auto spk : doc.root.get_children("speaker")) {
    xml_element_t espk(spk);
    auto& tscspk = spk_spec->spkpos[std::min(k, spk_spec->spkpos.size() - 1)];
    espk.set_attribute("gain", TASCAR::to_string(20 * log10(tscspk.gain)));
    espk.set_attribute("eqstages", std::to_string(spk_nsp->spkpos[k].eqstages));
    espk.set_attribute("eqfirlen", std::to_string(spk_nsp->spkpos[k].eqfirlen));
    if((spk_nsp->spkpos[k].eqstages > 0u) ||
       (spk_nsp->spkpos[k].eqfirlen > 0u)) {
      espk.set_attribute("eqfreq",
                         TASCAR::to_string(spk_nsp->spkpos[k].eqfreq));
      espk.set_attribute("eqgain",
                         TASCAR::to_string(spk_nsp->spkpos[k].eqgain));
    } else {
      espk.set_attribute("eqfreq", "");
      espk.set_attribute("eqgain", "");
    }
    ++k;
  }
  k = 0;
  for(auto spk : doc.root.get_children("sub")) {
    xml_element_t espk(spk);
    auto& tscspk =
        spk_spec->spkpos.subs[std::min(k, spk_spec->spkpos.subs.size() - 1)];
    espk.set_attribute("gain", TASCAR::to_string(20 * log10(tscspk.gain)));
    espk.set_attribute("eqstages",
                       std::to_string(spk_nsp->spkpos.subs[k].eqstages));
    espk.set_attribute("eqfirlen",
                       std::to_string(spk_nsp->spkpos.subs[k].eqfirlen));
    if((spk_nsp->spkpos.subs[k].eqstages > 0u) ||
       (spk_nsp->spkpos.subs[k].eqfirlen > 0u)) {
      espk.set_attribute("eqfreq",
                         TASCAR::to_string(spk_nsp->spkpos.subs[k].eqfreq));
      espk.set_attribute("eqgain",
                         TASCAR::to_string(spk_nsp->spkpos.subs[k].eqgain));
    } else {
      espk.set_attribute("eqfreq", "");
      espk.set_attribute("eqgain", "");
    }
    ++k;
  }
  size_t checksum = get_spklayout_checksum(elayout);
  elayout.set_attribute("checksum", (uint64_t)checksum);
  char ctmp[1024];
  memset(ctmp, 0, 1024);
  std::time_t t(std::time(nullptr));
  std::strftime(ctmp, 1023, "%Y-%m-%d %H:%M:%S", std::localtime(&t));
  doc.root.set_attribute("calibdate", ctmp);
  doc.root.set_attribute("calibfor", calibfor);
  cfg_.par_speaker.save_xml(doc.root());
  cfg_.par_sub.save_xml(doc.root());
  doc.save(fname);
  gainmodified = false;
  levelsrecorded = false;
  calibrated = false;
  calibrated_diff = false;
}

void calibsession_t::file_save()
{
  file_saveas(spkname);
}

void calibsession_t::set_active(bool b)
{
  // activate broadband source and type-specific receiver.
  scenes.back()->source_objects[1]->set_mute(true);
  if(!b) {
    // inactive broadband, so enable nsp receiver:
    rec_nsp->set_mute(false);
    rec_spec->set_mute(true);
  }
  if(b)
    // active, so mute diffuse sound:
    set_active_diff(false);
  scenes.back()->source_objects[0]->dlocation = pos_t(1, 0, 0);
  // activate broadband source if needed:
  scenes.back()->source_objects[0]->set_mute(!b);
  if(b) {
    // enable saving of file:
    calibrated = true;
    // active, so activate type-specific receiver:
    rec_nsp->set_mute(true);
    rec_spec->set_mute(false);
  }
  isactive_pointsource = b;
}

void calibsession_t::set_active_diff(bool b)
{
  // control diffuse source:
  scenes.back()->source_objects[1]->set_mute(true);
  if(!b) {
    rec_nsp->set_mute(false);
    rec_spec->set_mute(true);
  }
  if(b)
    set_active(false);
  scenes.back()->diff_snd_field_objects.back()->set_mute(!b);
  if(b) {
    calibrated_diff = true;
    rec_nsp->set_mute(true);
    rec_spec->set_mute(false);
  }
  isactive_diffuse = b;
}

double calibsession_t::get_caliblevel() const
{
  return 20.0 * log10(rec_spec->caliblevel * 5e4);
}

double calibsession_t::get_diffusegain() const
{
  return 20.0 * log10(rec_spec->diffusegain);
}

void calibsession_t::inc_caliblevel(float dl)
{
  if(startlevel + delta + dl < cfg_.par_speaker.reflevel + 15.0f)
    throw TASCAR::ErrMsg(std::string("Decreasing the calibration level to ") +
                         TASCAR::to_string(startlevel + delta + dl) +
                         " dB SPL would result in clipping.");
  if((dl < 0.0f) && !isactive_pointsource)
    throw TASCAR::ErrMsg("Please activate source before increasing the level.");
  if((dl >= 0.0f) || isactive_pointsource) {
    gainmodified = true;
    delta += dl;
    double newlevel_pa(2e-5 * pow(10.0, 0.05 * (startlevel + delta)));
    rec_nsp->caliblevel = (float)newlevel_pa;
    rec_spec->caliblevel = (float)newlevel_pa;
    spk_spec->spkpos.caliblevel = (float)newlevel_pa;
  }
}

void calibsession_t::set_caliblevel(float dl)
{
  if(dl < cfg_.par_speaker.reflevel + 15.0f)
    throw TASCAR::ErrMsg(std::string("Decreasing the calibration level to ") +
                         TASCAR::to_string(dl) +
                         " dB SPL would result in clipping.");
  gainmodified = true;
  delta = dl - startlevel;
  double newlevel_pa(2e-5 * pow(10.0, 0.05 * (startlevel + delta)));
  rec_nsp->caliblevel = (float)newlevel_pa;
  rec_spec->caliblevel = (float)newlevel_pa;
  spk_spec->spkpos.caliblevel = (float)newlevel_pa;
}

void calibsession_t::set_diffusegain(float g)
{
  if(g > 50.0f)
    throw TASCAR::ErrMsg(
        std::string("Setting a diffuse gain of ") + TASCAR::to_string(g) +
        " dB is not possible. If you are sure you need such extreme diffuse "
        "gains, please edit your speaker layout manually.");
  previous_delta_diff = delta_diff;
  if(g - startdiffgain - previous_delta_diff > 20.0)
    throw TASCAR::ErrMsg(
        std::string(
            "This operation would lead to an increase of diffuse gain by ") +
        TASCAR::to_string(g - startdiffgain - previous_delta_diff) +
        " dB. Please increase gain in smaller steps.");
  gainmodified = true;
  delta_diff = g - startdiffgain;
  float gain(powf(10.0f, 0.05f * (startdiffgain + delta_diff)));
  rec_nsp->diffusegain = (float)gain;
  rec_spec->diffusegain = (float)gain;
  spk_spec->spkpos.diffusegain = (float)gain;
}

void calibsession_t::inc_diffusegain(float dl)
{
  if((dl > 0.0f) && !isactive_diffuse)
    throw TASCAR::ErrMsg(
        "Please activate diffuse field before increasing the gain.");
  if((dl <= 0.0f) || isactive_diffuse) {
    gainmodified = true;
    previous_delta_diff = delta_diff;
    delta_diff += dl;
    double gain(pow(10.0, 0.05 * (startdiffgain + delta_diff)));
    rec_nsp->diffusegain = (float)gain;
    rec_spec->diffusegain = (float)gain;
    spk_spec->spkpos.diffusegain = (float)gain;
  }
}

const spk_array_diff_render_t& calibsession_t::get_current_layout() const
{
  if(!spk_spec)
    throw TASCAR::ErrMsg("No layout loaded");
  return spk_spec->spkpos;
}

spkcalibrator_t::spkcalibrator_t() : fallbackmeter(8000.0, 0.1, levelmeter::Z)
{
}

void spkcalibrator_t::set_filename(const std::string& name)
{
  if(currentstep != 0u)
    throw TASCAR::ErrMsg("It is not possible to change the name of the layout "
                         "file while the calibration is running.");
  filename = name;
  if(p_layout)
    delete p_layout;
  p_layout = NULL;
  if(p_layout_doc)
    delete p_layout_doc;
  p_layout_doc = NULL;
  // open layout file, read calib parameters:
  p_layout_doc = new xml_doc_t(filename, TASCAR::xml_doc_t::LOAD_FILE);
  try {
    p_layout = new spk_array_diff_render_t(p_layout_doc->root(), true);
    cfg.set_has_sub(has_sub());
    cfg.read_xml(p_layout_doc->root());
  }
  catch(...) {
    delete p_layout_doc;
    throw;
  }
}

void spkcalibrator_t::step1_file_selected()
{
  while(currentstep > 0u)
    go_back();
  if(filename.empty() || (!p_layout)) {
    throw TASCAR::ErrMsg(
        "No layout file selected. Please select a valid speaker layout file.");
  }
  // update initcalib flag:
  // end
  currentstep = 1u;
}

void spkcalibrator_t::step2_config_revised()
{
  while(currentstep > 1u)
    go_back();
  if(currentstep != 1u)
    throw TASCAR::ErrMsg("Please select a layout file first.");
  // create calib session:
  if(p_session)
    delete p_session;
  p_session = NULL;
  p_session = new calibsession_t(filename, cfg);
  // end
  currentstep = 2u;
}

void spkcalibrator_t::step3_calib_initialized()
{
  while(currentstep > 2u)
    go_back();
  if(currentstep != 2u)
    throw TASCAR::ErrMsg("Please revise your configuration first.");
  // prepare speaker equalization if needed:
  if(p_session)
    p_session->enable_spkcorr_spec(false);
  // end
  currentstep = 3u;
}

void spkcalibrator_t::step4_speaker_equalized()
{
  while(currentstep > 3u)
    go_back();
  if(currentstep != 3u)
    throw TASCAR::ErrMsg("Please ensure your calibration is initialized.");
  // prepare level adjustment:
  if(p_session)
    p_session->enable_spkcorr_spec(true);
  // end
  currentstep = 4u;
}

void spkcalibrator_t::step5_levels_adjusted()
{
  while(currentstep > 4u)
    go_back();
  if(currentstep != 4u)
    throw TASCAR::ErrMsg(
        "Please equalize the speakers before adjusting the levels.");
  // save data:
  // end
  currentstep = 5u;
}

void spkcalibrator_t::go_back()
{
  switch(currentstep) {
  case 0u:
    if(p_session)
      delete p_session;
    p_session = NULL;
    return;
  case 1u:
    if(p_session)
      delete p_session;
    p_session = NULL;
    break;
  case 2u:
    if(p_session)
      delete p_session;
    p_session = NULL;
    break;
  }
  --currentstep;
}

spkcalibrator_t::~spkcalibrator_t()
{
  if(p_layout)
    delete p_layout;
  if(p_layout_doc)
    delete p_layout_doc;
  if(p_session)
    delete p_session;
}

std::string spkcalibrator_t::get_orig_speaker_desc() const
{
  if(p_layout)
    return p_layout->to_string();
  return "";
}

std::string spkcalibrator_t::get_current_speaker_desc() const
{
  if(p_session)
    return p_session->get_current_layout().to_string();
  return "";
}

const TASCAR::levelmeter_t& spkcalibrator_t::get_meter(uint32_t k) const
{
  if(p_session && p_session->levelroute &&
     (p_session->levelroute->metercnt() > k))
    return p_session->levelroute->get_meter(k);
  return fallbackmeter;
}

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
