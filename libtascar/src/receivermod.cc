#include "receivermod.h"
#include "errorhandling.h"
#include <dlfcn.h>

using namespace TASCAR;

TASCAR_RESOLVER(receivermod_base_t, xmlpp::Element*);

TASCAR::receivermod_t::receivermod_t(xmlpp::Element* cfg)
    : receivermod_base_t(cfg), receivertype("omni"), lib(NULL), libdata(NULL)
{
  get_attribute("type", receivertype);
  receivertype = env_expand(receivertype);
  std::string libname("tascarreceiver_");
#ifdef PLUGINPREFIX
  libname = PLUGINPREFIX + libname;
#endif
#if defined(__APPLE__)
  libname += receivertype + ".dylib";
#elif __linux__
  libname += receivertype + ".so";
#elif WIN32
  libname += receivertype + ".dll";
#else
#error not supported
#endif
  lib = dlopen(libname.c_str(), RTLD_NOW);
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

void TASCAR::receivermod_t::add_variables(TASCAR::osc_server_t* srv)
{
  return libdata->add_variables(srv);
}

void TASCAR::receivermod_t::configure()
{
  receivermod_base_t::configure();
  libdata->prepare(*this);
}

void TASCAR::receivermod_t::release()
{
  receivermod_base_t::release();
  libdata->release();
}

TASCAR::receivermod_base_t::data_t*
TASCAR::receivermod_t::create_data(double srate, uint32_t fragsize)
{
  return libdata->create_data(srate, fragsize);
}

TASCAR::receivermod_t::~receivermod_t()
{
  delete libdata;
  dlclose(lib);
}

TASCAR::receivermod_base_t::receivermod_base_t(xmlpp::Element* xmlsrc)
    : xml_element_t(xmlsrc)
{
}

TASCAR::receivermod_base_t::~receivermod_base_t() {}

TASCAR::receivermod_base_speaker_t::receivermod_base_speaker_t(
    xmlpp::Element* xmlsrc)
    : receivermod_base_t(xmlsrc), spkpos(xmlsrc, false), typeidattr({"type"})
{
}

std::string TASCAR::receivermod_base_speaker_t::get_spktypeid() const
{
  std::string r;
  for(auto ta : typeidattr)
    r += ta + ":" + e->get_attribute_value(ta) + ",";
  if(r.size() && (r[r.size() - 1] == ','))
    r.erase(r.size() - 1);
  return r;
}

void TASCAR::receivermod_base_speaker_t::add_variables(
    TASCAR::osc_server_t* srv)
{
  receivermod_base_t::add_variables(srv);
  srv->add_bool("/decorr", &(spkpos.decorr));
  srv->add_bool("/densitycorr", &(spkpos.densitycorr));
}

void TASCAR::receivermod_base_speaker_t::validate_attributes(
    std::string& msg) const
{
  receivermod_base_t::validate_attributes(msg);
  spkpos.validate_attributes(msg);
  spkpos.elayout.validate_attributes(msg);
}

void TASCAR::receivermod_base_speaker_t::add_diffuse_sound_field(
    const TASCAR::amb1wave_t& chunk, std::vector<TASCAR::wave_t>& output,
    receivermod_base_t::data_t* sd)
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
  // update diffuse signals:
  spkpos.render_diffuse(output);
  // subwoofer post processing:
  if(spkpos.use_subs) {
    if(output.size() != spkpos.subs.size() + spkpos.size())
      throw TASCAR::ErrMsg(
          "Programming error: output.size()==" + std::to_string(output.size()) +
          ", spkpos.size()==" + std::to_string(spkpos.size()) +
          ", subs.size()==" + std::to_string(spkpos.subs.size()));
    // first create raw subwoofer signals:
    for(size_t ksub = 0; ksub < spkpos.subs.size(); ++ksub) {
      // clear sub signals:
      output[ksub + spkpos.size()].clear();
      for(size_t kbroadband = 0; kbroadband < spkpos.size(); ++kbroadband)
        output[ksub + spkpos.size()].add(output[kbroadband],
                                         spkpos.subweight[ksub][kbroadband]);
    }
    // now apply lp-filters to subs:
    for(size_t k = 0; k < spkpos.subs.size(); ++k) {
      spkpos.flt_lowp[k].filter(output[k + spkpos.size()]);
    }
    // then apply hp and allp filters to broad band speakers:
    for(size_t k = 0; k < spkpos.size(); ++k) {
      spkpos.flt_hp[k].filter(output[k]);
      spkpos.flt_allp[k].filter(output[k]);
    }
  }
  // apply calibration:
  if(spkpos.delaycomp.size() != spkpos.size())
    throw TASCAR::ErrMsg("Invalid delay compensation array");
  for(uint32_t k = 0; k < spkpos.size(); ++k) {
    float sgain(spkpos[k].spkgain * spkpos[k].gain);
    for(uint32_t f = 0; f < output[k].n; ++f) {
      output[k].d[f] = sgain * spkpos.delaycomp[k](output[k].d[f]);
    }
    if(spkpos[k].comp)
      spkpos[k].comp->process(output[k], output[k], false);
  }
  // calibration of subs:
  for(uint32_t k = 0; k < spkpos.subs.size(); ++k) {
    float sgain(spkpos.subs[k].spkgain * spkpos.subs[k].gain);
    output[k+spkpos.size()] *= sgain;
    if(spkpos.subs[k].comp)
      spkpos.subs[k].comp->process(output[k+spkpos.size()], output[k+spkpos.size()], false);
  }
}

void TASCAR::receivermod_base_speaker_t::configure()
{
  receivermod_base_t::configure();
  n_channels = spkpos.size() + spkpos.subs.size();
  spkpos.prepare(cfg());
  labels.clear();
  for(uint32_t ch = 0; ch < n_channels; ++ch) {
    if(ch < spkpos.size())
      labels.push_back("." + TASCAR::to_string(ch) + spkpos[ch].label);
    else
      labels.push_back(".S" + TASCAR::to_string(ch - spkpos.size()) +
                       spkpos.subs[ch - spkpos.size()].label);
  }
}

void TASCAR::receivermod_base_speaker_t::release()
{
  receivermod_base_t::release();
  spkpos.release();
}

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
