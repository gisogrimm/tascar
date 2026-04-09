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

#include "receivermod.h"
#include "errorhandling.h"
#include "tascar_os.h"
#include <dlfcn.h>

using namespace TASCAR;

TASCAR_RESOLVER(receivermod_base_t, tsccfg::node_t);

TASCAR::receivermod_t::receivermod_t(tsccfg::node_t cfg)
    : receivermod_base_t(cfg), receivertype("omni"), lib(NULL), libdata(NULL)
{
  get_attribute("type", receivertype, "", "receiver type");
  receivertype = env_expand(receivertype);
  std::string libname("tascarreceiver_");
#ifdef PLUGINPREFIX
  libname = PLUGINPREFIX + libname;
#endif
  libname += receivertype + TASCAR::dynamic_lib_extension();
  lib = TASCAR::dlopen((TASCAR::get_libdir() + libname).c_str(), RTLD_NOW);
  if(!lib)
    throw TASCAR::ErrMsg("Unable to open receiver module \"" + receivertype +
                         "\": " + dlerror());
  try {
    receivermod_base_t_resolver(&libdata, cfg, lib, libname);
  }
  catch(...) {
    dlclose(lib);
    throw;
  }
}

void TASCAR::receivermod_t::add_pointsource(const pos_t& prel, double width,
                                            const wave_t& chunk,
                                            std::vector<wave_t>& output,
                                            receivermod_base_t::data_t* data)
{
  libdata->add_pointsource(prel, width, chunk, output, data);
}

void TASCAR::receivermod_t::add_diffuse_sound_field(
    const amb1wave_t& chunk, std::vector<wave_t>& output,
    receivermod_base_t::data_t* data)
{
  libdata->add_diffuse_sound_field(chunk, output, data);
}

void TASCAR::receivermod_t::postproc(std::vector<wave_t>& output)
{
  libdata->postproc(output);
}

void TASCAR::receivermod_t::validate_attributes(std::string& msg) const
{
  receivermod_base_t::validate_attributes(msg);
  libdata->validate_attributes(msg);
}

std::vector<std::string> TASCAR::receivermod_t::get_connections() const
{
  return libdata->get_connections();
}

float TASCAR::receivermod_t::get_delay_comp() const
{
  return libdata->get_delay_comp();
}

void TASCAR::receivermod_t::add_variables(TASCAR::osc_server_t* srv)
{
  return libdata->add_variables(srv);
}

void TASCAR::receivermod_t::configure()
{
  receivermod_base_t::configure();
  libdata->prepare(*this);
}

void TASCAR::receivermod_t::post_prepare()
{
  receivermod_base_t::post_prepare();
  libdata->post_prepare();
}

void TASCAR::receivermod_t::release()
{
  receivermod_base_t::release();
  libdata->release();
}

TASCAR::receivermod_base_t::data_t*
TASCAR::receivermod_t::create_state_data(double srate, uint32_t fragsize) const
{
  return libdata->create_state_data(srate, fragsize);
}

TASCAR::receivermod_base_t::data_t*
TASCAR::receivermod_t::create_diffuse_state_data(double srate,
                                                 uint32_t fragsize) const
{
  return libdata->create_diffuse_state_data(srate, fragsize);
}

TASCAR::receivermod_t::~receivermod_t()
{
  delete libdata;
  dlclose(lib);
}

TASCAR::receivermod_base_t::receivermod_base_t(tsccfg::node_t xmlsrc)
    : xml_element_t(xmlsrc)
{
}

TASCAR::receivermod_base_t::~receivermod_base_t() {}

std::string spatial_error_t::to_string(const std::string& label,
                                       const std::string& sampling)
{
  return "% error " + sampling + ":\n" + "e." + label +
         ".abs_rV = " + TASCAR::to_string(abs_rV_error) + ";\n" + "e." + label +
         ".abs_rE = " + TASCAR::to_string(abs_rE_error) + ";\n" + "e." + label +
         ".ang_rV = " + TASCAR::to_string(RAD2DEG * angular_rV_error) + ";\n" +
         "e." + label +
         ".ang_rE = " + TASCAR::to_string(RAD2DEG * angular_rE_error) + ";\n" +
         "e." + label +
         ".ang_rV_std = " + TASCAR::to_string(RAD2DEG * angular_rV_error_std) +
         ";\n" + "e." + label +
         ".ang_rE_std = " + TASCAR::to_string(RAD2DEG * angular_rE_error_std) +
         ";\n" + "e." + label +
         ".az_rV = " + TASCAR::to_string(RAD2DEG * azim_rV_error) + ";\n" +
         "e." + label +
         ".az_rE = " + TASCAR::to_string(RAD2DEG * azim_rE_error) + ";\n" +
         "e." + label +
         ".el_rV = " + TASCAR::to_string(RAD2DEG * elev_rV_error) + ";\n" +
         "e." + label +
         ".el_rE = " + TASCAR::to_string(RAD2DEG * elev_rE_error) + ";\n" +
         "e." + label + ".median_az_rV = " +
         TASCAR::to_string(RAD2DEG * median_azim_rV_error) + ";\n" + "e." +
         label + ".median_az_rE = " +
         TASCAR::to_string(RAD2DEG * median_azim_rE_error) + ";\n" + "e." +
         label + ".median_el_rV = " +
         TASCAR::to_string(RAD2DEG * median_elev_rV_error) + ";\n" + "e." +
         label + ".median_el_rE = " +
         TASCAR::to_string(RAD2DEG * median_elev_rE_error) + ";\n" + "e." +
         label +
         ".q25_az_rV = " + TASCAR::to_string(RAD2DEG * q25_azim_rV_error) +
         ";\n" + "e." + label +
         ".q25_az_rE = " + TASCAR::to_string(RAD2DEG * q25_azim_rE_error) +
         ";\n" + "e." + label +
         ".q25_el_rV = " + TASCAR::to_string(RAD2DEG * q25_elev_rV_error) +
         ";\n" + "e." + label +
         ".q25_el_rE = " + TASCAR::to_string(RAD2DEG * q25_elev_rE_error) +
         ";\n" + "e." + label +
         ".q75_az_rV = " + TASCAR::to_string(RAD2DEG * q75_azim_rV_error) +
         ";\n" + "e." + label +
         ".q75_az_rE = " + TASCAR::to_string(RAD2DEG * q75_azim_rE_error) +
         ";\n" + "e." + label +
         ".q75_el_rV = " + TASCAR::to_string(RAD2DEG * q75_elev_rV_error) +
         ";\n" + "e." + label +
         ".q75_el_rE = " + TASCAR::to_string(RAD2DEG * q75_elev_rE_error) +
         ";\n" + "e." + label +
         ".abs_az_rV = " + TASCAR::to_string(RAD2DEG * abs_azim_rV_error) +
         ";\n" + "e." + label +
         ".abs_az_rE = " + TASCAR::to_string(RAD2DEG * abs_azim_rE_error) +
         ";\n" + "e." + label +
         ".abs_el_rV = " + TASCAR::to_string(RAD2DEG * abs_elev_rV_error) +
         ";\n" + "e." + label +
         ".abs_el_rE = " + TASCAR::to_string(RAD2DEG * abs_elev_rE_error) +
         ";\n" + "e." + label + ".N = " + std::to_string(N) + ";\n";
}

TASCAR::receivermod_base_speaker_t::receivermod_base_speaker_t(
    tsccfg::node_t xmlsrc)
    : receivermod_base_t(xmlsrc), spkpos(xmlsrc, false), typeidattr({"type"}),
      showspatialerror(false)
{
  GET_ATTRIBUTE_BOOL(
      showspatialerror,
      "show absolute and angular error for rE and rV for 2D and 3D rendering, "
      "given the actual speaker layout and settings");
  GET_ATTRIBUTE(spatialerrorpos, "m",
                "Additional point list in Cartesian coordinates for testing "
                "spatial error");
}

std::string TASCAR::receivermod_base_speaker_t::get_spktypeid() const
{
  std::string r;
  for(auto& ta : typeidattr)
    r += ta + ":" + tsccfg::node_get_attribute_value(e, ta) + ",";
  if(r.size() && (r[r.size() - 1] == ','))
    r.erase(r.size() - 1);
  return r;
}

void TASCAR::receivermod_base_speaker_t::add_variables(
    TASCAR::osc_server_t* srv)
{
  receivermod_base_t::add_variables(srv);
  srv->set_variable_owner("receivermod_base_speaker");
  srv->add_bool("/decorr", &(spkpos.decorr));
  srv->add_bool("/densitycorr", &(spkpos.densitycorr));
  srv->add_bool("/enable_eq", &(spkpos.enable_eq));
  srv->unset_variable_owner();
}

void TASCAR::receivermod_base_speaker_t::validate_attributes(
    std::string& msg) const
{
  receivermod_base_t::validate_attributes(msg);
  spkpos.validate_attributes(msg);
  spkpos.elayout.validate_attributes(msg);
}

void TASCAR::receivermod_base_speaker_t::add_diffuse_sound_field(
    const TASCAR::amb1wave_t& chunk, std::vector<TASCAR::wave_t>&,
    receivermod_base_t::data_t*)
{
  spkpos.add_diffuse_sound_field(chunk);
}

std::vector<std::string>
TASCAR::receivermod_base_speaker_t::get_connections() const
{
  return spkpos.connections;
}

void TASCAR::receivermod_base_speaker_t::postproc(std::vector<wave_t>& output)
{
  spkpos.postproc(output);
}

void TASCAR::receivermod_base_speaker_t::configure()
{
  receivermod_base_t::configure();
  n_channels = (uint32_t)spkpos.num_output_channels();
  spkpos.prepare(cfg());
  labels.clear();
  for(uint32_t ch = 0; ch < n_channels; ++ch) {
    if(ch < spkpos.size())
      labels.push_back("." + TASCAR::to_string(ch) + spkpos[ch].label);
    else if(ch < spkpos.size() + spkpos.subs.size())
      labels.push_back(".S" + std::to_string(ch - spkpos.size()) +
                       spkpos.subs[ch - spkpos.size()].label);
    else if(ch < spkpos.size() + spkpos.subs.size() + spkpos.convlabels.size())
      labels.push_back(
          spkpos.convlabels[ch - (spkpos.size() + spkpos.subs.size())]);
    else
      labels.push_back(
          ".conv." + std::to_string(ch - (spkpos.size() + spkpos.subs.size())));
  }
}

void TASCAR::receivermod_base_speaker_t::release()
{
  receivermod_base_t::release();
  spkpos.release();
}

spatial_error_t TASCAR::receivermod_base_speaker_t::get_spatial_error(
    const std::vector<TASCAR::pos_t>& srcpos)
{
  if(!is_prepared())
    throw TASCAR::ErrMsg("not in configured state. unable to calculate "
                         "get_rE() of an unconfigured receiver.");
  TASCAR::receivermod_base_t::data_t* sd(
      create_state_data(f_sample, n_fragment));
  TASCAR::wave_t ones(n_fragment);
  TASCAR::wave_t zeros(n_fragment);
  ones += 1.0f;
  std::vector<wave_t> output(spkpos.size() + spkpos.subs.size(),
                             TASCAR::wave_t(n_fragment));
  spatial_error_t err;
  std::vector<double> vaz_rV;
  std::vector<double> vaz_rE;
  std::vector<double> vel_rV;
  std::vector<double> vel_rE;
  std::vector<double> vang_rV;
  std::vector<double> vang_rE;
  for(auto pos : srcpos) {
    pos.normalize();
    for(size_t ch = 0; ch < output.size(); ++ch)
      output[ch].clear();
    add_pointsource(pos, 0.0, zeros, output, sd);
    postproc(output);
    for(size_t ch = 0; ch < output.size(); ++ch)
      output[ch].clear();
    add_pointsource(pos, 0.0, ones, output, sd);
    postproc(output);
    for(size_t outch(0); outch < spkpos.size(); ++outch)
      output[outch] *= 1.0f / (spkpos[outch].gain *
                               spkpos[outch].gain_for_distance_correction);
    spkpos.clear_states();
    TASCAR::pos_t rE;
    TASCAR::pos_t rV;
    double norm_E(0.0);
    double norm_V(0.0);
    for(size_t outch(0); outch < spkpos.size(); ++outch) {
      double w(output[outch].d[output[outch].n - 1]);
      // calculate rV:
      TASCAR::pos_t pV(spkpos[outch].unitvector);
      pV *= w;
      rV += pV;
      norm_V += w;
      // calculate rE:
      w *= w;
      TASCAR::pos_t pE(spkpos[outch].unitvector);
      pE *= w;
      rE += pE;
      norm_E += w;
    }
    if(norm_E > 0.0)
      rE /= norm_E;
    if(norm_V != 0.0)
      rV /= norm_V;
    err.abs_rV_error += pow(1.0 - rV.norm(), 2.0);
    err.abs_rE_error += pow(1.0 - rE.norm(), 2.0);
    // err.angular_rV_error +=
    //  acos(std::min(1.0, std::max(-1.0, dot_prod(rV.normal(), pos))));
    vang_rV.push_back(
        acos(std::min(1.0, std::max(-1.0, dot_prod(rV.normal(), pos)))));
    // err.angular_rE_error +=
    //    acos(std::min(1.0, std::max(-1.0, dot_prod(rE.normal(), pos))));
    vang_rE.push_back(
        acos(std::min(1.0, std::max(-1.0, dot_prod(rE.normal(), pos)))));
    err.azim_rV_error += acos(cos(rV.azim() - pos.azim()));
    err.azim_rE_error += acos(cos(rE.azim() - pos.azim()));
    err.elev_rV_error += acos(cos(rV.elev() - pos.elev()));
    err.elev_rE_error += acos(cos(rE.elev() - pos.elev()));
    vaz_rV.push_back(rV.azim() - pos.azim());
    vaz_rE.push_back(rE.azim() - pos.azim());
    vel_rV.push_back(rV.elev() - pos.elev());
    vel_rE.push_back(rE.elev() - pos.elev());
    err.abs_azim_rV_error += fabs(rV.azim() - pos.azim());
    err.abs_azim_rE_error += fabs(rE.azim() - pos.azim());
    err.abs_elev_rV_error += fabs(rV.elev() - pos.elev());
    err.abs_elev_rE_error += fabs(rE.elev() - pos.elev());
  }
  err.abs_rV_error = sqrt(err.abs_rV_error / (double)(srcpos.size()));
  err.abs_rE_error = sqrt(err.abs_rE_error / (double)(srcpos.size()));
  vector_get_mean_std(vang_rV, err.angular_rV_error, err.angular_rV_error_std);
  vector_get_mean_std(vang_rE, err.angular_rE_error, err.angular_rE_error_std);
  err.azim_rV_error /= (double)(srcpos.size());
  err.azim_rE_error /= (double)(srcpos.size());
  err.elev_rV_error /= (double)(srcpos.size());
  err.elev_rE_error /= (double)(srcpos.size());
  err.abs_azim_rV_error /= (double)(srcpos.size());
  err.abs_azim_rE_error /= (double)(srcpos.size());
  err.abs_elev_rV_error /= (double)(srcpos.size());
  err.abs_elev_rE_error /= (double)(srcpos.size());
  err.median_azim_rV_error = TASCAR::median(vaz_rV.begin(), vaz_rV.end());
  err.median_azim_rE_error = TASCAR::median(vaz_rE.begin(), vaz_rE.end());
  err.median_elev_rV_error = TASCAR::median(vel_rV.begin(), vel_rV.end());
  err.median_elev_rE_error = TASCAR::median(vel_rE.begin(), vel_rE.end());
  err.q25_azim_rV_error = TASCAR::median(vaz_rV.begin(), vaz_rV.end(), 0.25);
  err.q25_azim_rE_error = TASCAR::median(vaz_rE.begin(), vaz_rE.end(), 0.25);
  err.q25_elev_rV_error = TASCAR::median(vel_rV.begin(), vel_rV.end(), 0.25);
  err.q25_elev_rE_error = TASCAR::median(vel_rE.begin(), vel_rE.end(), 0.25);
  err.q75_azim_rV_error = TASCAR::median(vaz_rV.begin(), vaz_rV.end(), 0.75);
  err.q75_azim_rE_error = TASCAR::median(vaz_rE.begin(), vaz_rE.end(), 0.75);
  err.q75_elev_rV_error = TASCAR::median(vel_rV.begin(), vel_rV.end(), 0.75);
  err.q75_elev_rE_error = TASCAR::median(vel_rE.begin(), vel_rE.end(), 0.75);
  err.N = srcpos.size();
  return err;
}

void TASCAR::receivermod_base_speaker_t::post_prepare()
{
  receivermod_base_t::post_prepare();
  spkpos.post_prepare();
  if(showspatialerror) {
    std::vector<TASCAR::pos_t> ring;
    ring.resize(360);
    for(size_t k = 0; k < ring.size(); ++k)
      ring[k].set_sphere(1.0, (double)k * TASCAR_2PI / (double)ring.size(),
                         0.0);
    spatial_error_t err = get_spatial_error(ring);
    std::cout << "% spatial error:\n";
    std::cout << "e.layout = '" << spkpos.layout << "';\n";
    std::cout << "e.typeid = '" << get_spktypeid() << "';\n";
    std::cout << "e.numchannels = " << spkpos.size() << ";\n";
    std::cout << err.to_string("err2d", "on a ring");
    std::vector<TASCAR::pos_t> sphere(TASCAR::generate_icosahedron());
    sphere = subdivide_and_normalize_mesh(sphere, 5);
    err = get_spatial_error(sphere);
    std::cout << err.to_string("err3d", "on a sphere");
    if(spatialerrorpos.size()) {
      err = get_spatial_error(spatialerrorpos);
      std::cout << err.to_string("user",
                                 "on " + TASCAR::to_string(spatialerrorpos));
    }
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
