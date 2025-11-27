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

#include "acousticmodel.h"
#include "errorhandling.h"

using namespace TASCAR;
using namespace TASCAR::Acousticmodel;

mask_t::mask_t() : inv_falloff(1.0), mask_inner(false), active(true) {}

float mask_t::gain(const pos_t& p)
{
  float d = (float)(nextpoint(p).norm());
  d = 0.5f + 0.5f * cosf(TASCAR_PIf * std::min(1.0f, d * inv_falloff));
  if(mask_inner)
    return 1.0f - d;
  return d;
}

diffuse_t::diffuse_t(tsccfg::node_t cfg, uint32_t chunksize,
                     TASCAR::levelmeter_t& rmslevel_, const std::string&,
                     plugin_processor_t& plugins_)
    : xml_element_t(cfg), licensed_component_t(typeid(*this).name()),
      audio(chunksize), falloff(1.0), active(true), layers(0xffffffff),
      rmslevel(rmslevel_), plugins(plugins_)
{
  // GET_ATTRIBUTE_BITS(layers);
}

void diffuse_t::preprocess(const TASCAR::transport_t& tp)
{
  plugins.process_plugins(audio.wyzx, center, orientation, tp);
  rmslevel.update(audio.w());
}

void diffuse_t::post_prepare()
{
  plugins.post_prepare();
}

void diffuse_t::configure()
{
  audiostates_t::configure();
  plugins.prepare(cfg());
}

void diffuse_t::release()
{
  audiostates_t::release();
  plugins.release();
}

void diffuse_t::add_licenses(licensehandler_t* lh)
{
  licensed_component_t::add_licenses(lh);
  plugins.add_licenses(lh);
}

void source_t::add_licenses(licensehandler_t* lh)
{
  licensed_component_t::add_licenses(lh);
  plugins.add_licenses(lh);
}

void receiver_t::add_licenses(licensehandler_t* lh)
{
  licensed_component_t::add_licenses(lh);
  plugins.add_licenses(lh);
}

acoustic_model_t::acoustic_model_t(float c, float fs, uint32_t chunksize,
                                   source_t* src, receiver_t* receiver,
                                   const std::vector<obstacle_t*>& obstacles,
                                   const acoustic_model_t* parent,
                                   const reflector_t* reflector)
    : soundpath_t(src, parent, reflector), c_(c), fs_(fs), src_(src),
      receiver_(receiver),
      receiver_data(receiver_->create_state_data(fs, chunksize)),
      source_data(src->create_state_data(fs, chunksize)), obstacles_(obstacles),
      audio(chunksize), chunksize(audio.size()),
      dt(1.0f / std::max(1.0f, (float)chunksize)), distance(1.0), gain(1.0),
      dscale(fs / (c_ * 7782.0f)), air_absorption(0.5),
      delayline((uint32_t)((src->maxdist / c_) * fs), fs, c_, src->sincorder,
                64),
      airabsorption_state(0.0), layergain(0.0),
      dlayergain(1.0f / (receiver->layerfadelen * fs)), ismorder(getorder())
{
  pos_t prel;
  float traveltime_in_m = 0;
  receiver_->update_refpoint(src_->position, src_->position, prel, distance,
                             traveltime_in_m, gain, false, src_->gainmodel,
                             src_->nearfieldlimit);
  gain = 1.0;
  vstate.resize(obstacles_.size());
  if(receiver_->layers & src_->layers)
    layergain = 1.0;
}

acoustic_model_t::~acoustic_model_t()
{
  if(receiver_data)
    delete receiver_data;
  if(source_data)
    delete source_data;
}

uint32_t acoustic_model_t::process(const TASCAR::transport_t& tp)
{
  if(src_->active)
    update_position();
  if((!receiver_->gain_zero) && receiver_->active && src_->active &&
     ((!reflector) || reflector->active)) {
    // render only if source, receiver and in case of image source the reflector
    // is active
    if(receiver_->render_point && (src_->ismmin <= ismorder) &&
       (ismorder <= src_->ismmax) && (receiver_->ismmin <= ismorder) &&
       (ismorder <= receiver_->ismmax)) {
      // render only if image source order is in defined range
      if(((receiver_->layers & src_->layers) || (layergain > 0))) {
        // render only if layers are matching
        bool layeractive(0);
        if(!reflector)
          layeractive = (receiver_->layers & src_->layers);
        else {
          layeractive = (receiver_->layers & src_->layers & reflector->layers);
        }
        if(visible) {
          pos_t prel;
          float nextdistance(0.0);
          float nextgain(1.0);
          // calculate relative geometry between source and receiver:
          float srcgainmod(1.0);
          // update effective position/calculate ISM geometry:
          position = get_effective_position(receiver_->position, srcgainmod);
          // read audio from source, update radation position:
          pos_t prelsrc(receiver_->position);
          prelsrc -= src_->position;
          prelsrc /= src_->orientation;
          if(receiver_->volumetric.has_volume()) {
            if(src_->read_source_diffuse(prelsrc, src_->inchannels, audio,
                                         source_data)) {
              prelsrc *= src_->orientation;
              prelsrc -= receiver_->position;
              prelsrc *= -1.0;
              position = prelsrc;
            }
          } else {
            if(src_->read_source(prelsrc, src_->inchannels, audio,
                                 source_data)) {
              prelsrc *= src_->orientation;
              prelsrc -= receiver_->position;
              prelsrc *= -1.0;
              position = prelsrc;
            }
          }
          // calculate obstacles:
          if(ismorder == 0) {
            // direct path only.
            for(uint32_t kobj = 0; kobj != obstacles_.size(); ++kobj) {
              obstacle_t* p_obj(obstacles_[kobj]);
              if(p_obj->active) {
                // apply diffraction model:
                if(p_obj->b_inner)
                  p_obj->process(position, receiver_->position, audio, c_, fs_,
                                 vstate[kobj], p_obj->transmission);
                else
                  position =
                      p_obj->process(position, receiver_->position, audio, c_,
                                     fs_, vstate[kobj], p_obj->transmission);
              }
            }
          }
          // end obstacles
          float nexttraveltime_in_m = 0.0f;
          receiver_->update_refpoint(primary->position, position, prel,
                                     nextdistance, nexttraveltime_in_m,
                                     nextgain, ismorder > 0, src_->gainmodel,
                                     src_->nearfieldlimit);
          if(nextdistance > src_->maxdist)
            return 0;
          nextgain *= srcgainmod;
          nextgain *= receiver_->external_gain;
          if(receiver_->maskplug)
            nextgain *= receiver_->maskplug->get_gain(prel);
          float next_air_absorption(expf(-nextdistance * dscale));
          float new_distance_with_delaycomp = std::max(
              0.0f, nexttraveltime_in_m -
                        c_ * (receiver_->delaycomp + receiver_->recdelaycomp));
          float ddistance = (new_distance_with_delaycomp - distance) * dt;
          float dgain((nextgain - gain) * dt);
          float dairabsorption((next_air_absorption - air_absorption) * dt);
          apply_reflectionfilter(audio);
          if(receiver_->muteonstop && (!tp.rolling)) {
            gain = 0.0f;
            dgain = 0.0f;
          }
          for(uint32_t k = 0; k < chunksize; ++k) {
            float& current_sample(audio[k]);
            distance += ddistance;
            gain += dgain;
            // calculate layer fade gain:
            if(layeractive) {
              if(layergain < 1.0f)
                layergain += dlayergain;
            } else {
              if(layergain > 0.0f)
                layergain -= dlayergain;
            }
            if(src_->delayline)
              current_sample =
                  layergain * gain *
                  delayline.get_dist_push(distance, current_sample);
            else
              current_sample = layergain * gain * current_sample;
            if(src_->airabsorption) {
              float c1(air_absorption += dairabsorption);
              float c2(1.0f - c1);
              // apply air absorption:
              c1 *= current_sample;
              airabsorption_state = c2 * airabsorption_state + c1;
              make_friendly_number(airabsorption_state);
              current_sample = airabsorption_state;
            }
          }
          distance = new_distance_with_delaycomp;
          gain = nextgain;
          air_absorption = next_air_absorption;
          if(((gain != 0.0f) || (dgain != 0.0f))) {
            if(src_->minlevel > 0.0f) {
              if(audio.rms() <= src_->minlevel)
                return 0;
            }
            // add scattering:
            float scattering(0.0f);
            if(reflector)
              scattering = reflector->scattering;
            // add to receiver:
            receiver_->add_pointsource_with_scattering(
                prel,
                std::min(TASCAR_PI2f, 0.25f * TASCAR_PIf * src_->size /
                                          std::max(0.01f, nextdistance)),
                scattering, audio, receiver_data);
            return 1;
          }
        } // of visible
      } // of layers check
    } // of ISM order check
  } else {
    delayline.add_chunk(audio);
  }
  return 0;
}

obstacle_t::obstacle_t() : active(true) {}

reflector_t::reflector_t()
    : active(true), reflectivity(1.0), damping(0.0), edgereflection(true),
      scattering(0), layers(0xffffffff)
{
}

void reflector_t::read_xml(TASCAR::xml_element_t& e)
{
  e.GET_ATTRIBUTE(reflectivity, "", "Reflectivity coefficient");
  e.GET_ATTRIBUTE(damping, "", "Damping coefficient");
  e.GET_ATTRIBUTE(material, "", "Material name, or empty to use coefficients");
  e.GET_ATTRIBUTE_BOOL(
      edgereflection,
      "Apply edge reflection in case of not directly visible image source");
  e.GET_ATTRIBUTE(scattering, "", "Relative amount of scattering");
  e.GET_ATTRIBUTE_BITS(layers, "render layers");
}

void reflector_t::apply_reflectionfilter(TASCAR::wave_t& audio,
                                         float& lpstate) const
{
  float c1 = reflectivity * (1.0f - damping);
  float* p_begin(audio.d);
  float* p_end(p_begin + audio.n);
  for(float* pf = p_begin; pf != p_end; ++pf)
    *pf = (lpstate = lpstate * damping + *pf * c1);
}

receiver_graph_t::receiver_graph_t(
    float c, float fs, uint32_t chunksize,
    const std::vector<source_t*>& sources,
    const std::vector<diffuse_t*>& diffuse_sound_fields,
    const std::vector<reflector_t*>& reflectors,
    const std::vector<obstacle_t*>& obstacles, receiver_t* receiver,
    uint32_t ism_order)
    : active_pointsource(0), active_diffuse_sound_field(0)
{
  // diffuse models:
  if(receiver->render_diffuse)
    for(uint32_t kSrc = 0; kSrc < diffuse_sound_fields.size(); ++kSrc)
      diffuse_acoustic_model.push_back(new diffuse_acoustic_model_t(
          fs, chunksize, diffuse_sound_fields[kSrc], receiver));
  // all primary and image sources:
  if(receiver->render_point) {
    // primary sources:
    for(uint32_t kSrc = 0; kSrc < sources.size(); ++kSrc)
      acoustic_model.push_back(new acoustic_model_t(
          c, fs, chunksize, sources[kSrc], receiver, obstacles));
    if(receiver->render_image && (ism_order > 0)) {
      auto num_mirrors_start = acoustic_model.size();
      // first order image sources:
      for(uint32_t ksrc = 0; ksrc < sources.size(); ++ksrc)
        for(uint32_t kreflector = 0; kreflector < reflectors.size();
            ++kreflector)
          acoustic_model.push_back(new acoustic_model_t(
              c, fs, chunksize, sources[ksrc], receiver, obstacles,
              acoustic_model[ksrc], reflectors[kreflector]));
      // now higher order image sources:
      auto num_mirrors_end = acoustic_model.size();
      for(uint32_t korder = 1; korder < ism_order; ++korder) {
        for(size_t ksrc = num_mirrors_start; ksrc < num_mirrors_end; ++ksrc)
          for(size_t kreflector = 0; kreflector < reflectors.size();
              ++kreflector)
            if(acoustic_model[ksrc]->reflector != reflectors[kreflector])
              acoustic_model.push_back(new acoustic_model_t(
                  c, fs, chunksize, acoustic_model[ksrc]->src_, receiver,
                  obstacles, acoustic_model[ksrc], reflectors[kreflector]));
        num_mirrors_start = num_mirrors_end;
        num_mirrors_end = acoustic_model.size();
      }
    }
  }
}

world_t::world_t(float c, float fs, uint32_t chunksize,
                 const std::vector<source_t*>& sources,
                 const std::vector<diffuse_t*>& diffuse_sound_fields,
                 const std::vector<reflector_t*>& reflectors,
                 const std::vector<obstacle_t*>& obstacles,
                 const std::vector<receiver_t*>& receivers,
                 const std::vector<mask_t*>& masks, uint32_t ism_order)
    : receivers_(receivers), masks_(masks), active_pointsource(0),
      active_diffuse_sound_field(0), total_pointsource(0),
      total_diffuse_sound_field(0)
{
  for(uint32_t krec = 0; krec < receivers.size(); ++krec) {
    receivergraphs.push_back(new receiver_graph_t(
        c, fs, chunksize, sources, diffuse_sound_fields, reflectors, obstacles,
        receivers[krec], ism_order));
    total_pointsource += receivergraphs.back()->get_total_pointsource();
    total_diffuse_sound_field +=
        receivergraphs.back()->get_total_diffuse_sound_field();
  }
}

world_t::~world_t()
{
  for(std::vector<receiver_graph_t*>::reverse_iterator it =
          receivergraphs.rbegin();
      it != receivergraphs.rend(); ++it)
    delete(*it);
}

void world_t::process(const TASCAR::transport_t& tp)
{
  uint32_t local_active_point(0);
  uint32_t local_active_diffuse(0);
  // calculate mask gains:
  for(uint32_t k = 0; k < receivers_.size(); ++k) {
    float gain_inner(1.0);
    if(receivers_[k]->use_global_mask || receivers_[k]->boundingbox.active) {
      // first calculate attentuation based on bounding box:
      if(receivers_[k]->boundingbox.active) {
        shoebox_t maskbox;
        maskbox.size = receivers_[k]->boundingbox.size;
        maskbox.center = receivers_[k]->boundingbox.c6dof.position;
        maskbox.orientation = receivers_[k]->boundingbox.c6dof.orientation;
        float d(maskbox.nextpoint(receivers_[k]->position).normf());
        gain_inner *=
            0.5f +
            0.5f *
                cosf(TASCAR_PIf *
                     std::min(1.0f,
                              d / std::max(receivers_[k]->boundingbox.falloff,
                                           1e-10f)));
      }
      // then calculate attenuation based on global masks:
      if(receivers_[k]->use_global_mask) {
        uint32_t c_outer(0);
        float gain_outer(0.0);
        for(uint32_t km = 0; km < masks_.size(); ++km) {
          if(masks_[km]->active) {
            pos_t p(receivers_[k]->position);
            if(masks_[km]->mask_inner) {
              gain_inner = std::min(gain_inner, masks_[km]->gain(p));
            } else {
              c_outer++;
              gain_outer = std::max(gain_outer, masks_[km]->gain(p));
            }
          }
        }
        if(c_outer > 0)
          gain_inner *= gain_outer;
      }
    }
    receivers_[k]->set_next_gain(gain_inner);
  }
  // calculate acoustic models:
  for(std::vector<receiver_graph_t*>::iterator ig = receivergraphs.begin();
      ig != receivergraphs.end(); ++ig) {
    (*ig)->process(tp);
    local_active_point += (*ig)->get_active_pointsource();
  }
  // apply post-processing and receiver gain of reverb receivers:
  for(auto it = receivers_.begin(); it != receivers_.end(); ++it)
    if((*it)->is_reverb) {
      (*it)->post_proc(tp);
      (*it)->apply_gain();
    }
  // calculate diffuse sound fields:
  for(std::vector<receiver_graph_t*>::iterator ig = receivergraphs.begin();
      ig != receivergraphs.end(); ++ig) {
    (*ig)->process_diffuse(tp);
    local_active_diffuse += (*ig)->get_active_diffuse_sound_field();
  }
  // apply post-processing and receiver gain on non-reverb receivers:
  for(auto it = receivers_.begin(); it != receivers_.end(); ++it)
    if(!(*it)->is_reverb) {
      (*it)->post_proc(tp);
      (*it)->apply_gain();
    }
  active_pointsource = local_active_point;
  active_diffuse_sound_field = local_active_diffuse;
}

void receiver_graph_t::process(const TASCAR::transport_t& tp)
{
  uint32_t local_active_point(0);
  // calculate acoustic model:
  for(unsigned int k = 0; k < acoustic_model.size(); k++)
    local_active_point += acoustic_model[k]->process(tp);
  active_pointsource = local_active_point;
}

void receiver_graph_t::process_diffuse(const TASCAR::transport_t& tp)
{
  uint32_t local_active_diffuse(0);
  // calculate diffuse sound fields:
  for(unsigned int k = 0; k < diffuse_acoustic_model.size(); k++)
    local_active_diffuse += diffuse_acoustic_model[k]->process(tp);
  active_diffuse_sound_field = local_active_diffuse;
}

receiver_graph_t::~receiver_graph_t()
{
  for(std::vector<acoustic_model_t*>::reverse_iterator it =
          acoustic_model.rbegin();
      it != acoustic_model.rend(); ++it)
    delete(*it);
  for(std::vector<diffuse_acoustic_model_t*>::reverse_iterator it =
          diffuse_acoustic_model.rbegin();
      it != diffuse_acoustic_model.rend(); ++it)
    delete(*it);
}

diffuse_acoustic_model_t::diffuse_acoustic_model_t(float fs, uint32_t chunksize,
                                                   diffuse_t* src,
                                                   receiver_t* receiver)
    : src_(src), receiver_(receiver),
      receiver_data(receiver_->create_diffuse_state_data(fs, chunksize)),
      audio(src->audio.size()), chunksize(audio.size()),
      dt(1.0f / (float)(std::max(1u, chunksize)))
{
  memset(gainmat, 0, sizeof(float) * 16);
  gainmat[0] = gainmat[5] = gainmat[10] = gainmat[15] = 1.0f;
  pos_t prel;
  float d = 1.0f;
  float gain = 1.0f;
  float nflimit = 0.1f;
  float traveltime_in_m = 1.0f;
  receiver_->update_refpoint(src_->center, src_->center, prel, d,
                             traveltime_in_m, gain, false, GAIN_INVR, nflimit);
}

diffuse_acoustic_model_t::~diffuse_acoustic_model_t()
{
  if(receiver_data)
    delete receiver_data;
}

/**
   \ingroup callgraph
 */
uint32_t diffuse_acoustic_model_t::process(const TASCAR::transport_t&)
{
  pos_t prel;
  float d = 0.0f;
  float nextgain = 1.0f;
  float nflimit = 0.1f;
  float nexttraveltime_in_m = 1.0f;
  // calculate relative geometry between source and receiver:
  receiver_->update_refpoint(src_->center, src_->center, prel, d,
                             nexttraveltime_in_m, nextgain, false, GAIN_INVR,
                             nflimit);
  shoebox_t box(*src_);
  // box.size = src_->size;
  box.center = pos_t();
  pos_t prel_nonrot(prel);
  prel_nonrot *= receiver_->orientation;
  d = box.nextpoint(prel_nonrot).normf();
  nextgain = 0.5f + 0.5f * cosf(TASCAR_PIf * std::min(1.0f, d * src_->falloff));
  if(!((gain == 0) && (nextgain == 0))) {
    audio.rotate(src_->audio, receiver_->orientation);
    memset(gainmat, 0, sizeof(float) * 16);
    gainmat[0] = gainmat[5] = gainmat[10] = gainmat[15] = 1.0f;
    if(receiver_->maskplug)
      receiver_->maskplug->get_diff_gain(gainmat);
    float dgain((nextgain - gain) * dt);
    for(uint32_t k = 0; k < chunksize; k++) {
      gain += dgain;
      if(receiver_->active && src_->active) {
        audio.w()[k] *= gain;
        audio.y()[k] *= gain;
        audio.z()[k] *= gain;
        audio.x()[k] *= gain;
      }
    }
    audio.apply_matrix(gainmat);
    gain = nextgain;
    if(receiver_->render_diffuse && receiver_->active && src_->active &&
       (!receiver_->gain_zero) && (receiver_->layers & src_->layers)) {
      audio *= receiver_->diffusegain;
      receiver_->add_diffuse_sound_field_rec(audio, receiver_data);
      return 1;
    }
  }
  return 0;
}

receiver_t::receiver_t(tsccfg::node_t xmlsrc, const std::string& name,
                       bool is_reverb_)
    : receivermod_t(xmlsrc), licensed_component_t(typeid(*this).name()),
      boundingbox(find_or_add_child("boundingbox")), is_reverb(is_reverb_),
      x_gain(1.0), next_gain(1.0), fade_timer(0), fade_rate(1),
      next_fade_gain(1), previous_fade_gain(1), prelim_next_fade_gain(1),
      prelim_previous_fade_gain(1), fade_gain(1), starttime_samples(0),
      plugins(xmlsrc, name, "")
{
  GET_ATTRIBUTE(
      volumetric, "m",
      "volume in which receiver does not apply distance based gain model");
  GET_ATTRIBUTE_BOOL(volumetricgainwithdistance,
                     "For volumetric receivers, increase gain with distance");
  GET_ATTRIBUTE_BOOL(create_input_ports,
                     "Create jack input ports for direct speaker feed");
  GET_ATTRIBUTE(avgdist, "m",
                "Average distance which is assumed inside receiver boxes, or 0 "
                "to use $(\\frac18 V)^{1/3}$");
  if(!is_reverb) {
    get_attribute_bool("point", render_point, "", "render point sources");
    get_attribute_bool("diffuse", render_diffuse, "", "render diffuse sources");
  }
  get_attribute_bool("image", render_image, "", "render image sources");
  get_attribute_bool("globalmask", use_global_mask, "", "use global mask");
  if(!is_reverb) {
    has_diffusegain = has_attribute("diffusegain");
    GET_ATTRIBUTE_DB(diffusegain, "gain of diffuse sources");
  }
  GET_ATTRIBUTE(fade_gain, "", "linear fade gain");
  next_fade_gain = previous_fade_gain = prelim_next_fade_gain =
      prelim_previous_fade_gain = fade_gain;
  GET_ATTRIBUTE(ismmin, "", "minimal ISM order to render");
  GET_ATTRIBUTE(ismmax, "", "maximal ISM order to render");
  GET_ATTRIBUTE_BITS(layers, "render layers");
  GET_ATTRIBUTE(falloff, "m",
                "Length of von-Hann ramp at volume boundaries, or -1 for "
                "normal distance model");
  GET_ATTRIBUTE(delaycomp, "s",
                "subtract this value from delay in delay lines");
  GET_ATTRIBUTE(layerfadelen, "s", "duration of fades between layers");
  GET_ATTRIBUTE_BOOL(muteonstop,
                     "mute when transport stopped to prevent playback of "
                     "sounds from delaylines and reverb");
  // proxy:
  GET_ATTRIBUTE(proxy_position, "m", "Proxy position");
  GET_ATTRIBUTE_BOOL(proxy_is_relative, "Proxy is relative to receiver (true) "
                                        "or in absolute coordinates (false)");
  GET_ATTRIBUTE_BOOL(proxy_delay, "Use proxy position for delay");
  GET_ATTRIBUTE_BOOL(proxy_airabsorption,
                     "Use proxy position for air absorption");
  GET_ATTRIBUTE_BOOL(proxy_gain, "Use proxy position for gain");
  GET_ATTRIBUTE_BOOL(proxy_direction, "Use proxy position for direction");
  // scattering:
  GET_ATTRIBUTE(scatterreflections, "",
                "Number of reflections created by scattering filter");
  GET_ATTRIBUTE(scatterstructuresize, "m", "size of scatter structure");
  GET_ATTRIBUTE(scatterdamping, "", "damping of scatter reflection filter");
  GET_ATTRIBUTE_DEG(scatterspread, "Spatial spread of scattering");
  // end proxy
  if(avgdist <= 0)
    avgdist = 0.5f * powf(volumetric.boxvolumef(), 0.33333f);
  // check for mask plugins:
  for(auto& sne : tsccfg::node_get_children(xmlsrc)) {
    std::string node_name = tsccfg::node_get_name(sne);
    if(node_name == "maskplugin") {
      if(maskplug)
        throw TASCAR::ErrMsg("More than one mask plugin was defined, only zero "
                             "or one are allowed. " +
                             tsccfg::node_get_path(sne));
      maskplug = new TASCAR::maskplugin_t(TASCAR::maskplugin_cfg_t(sne));
    }
  }
}

void receiver_t::configure()
{
  receivermod_t::configure();
  update();
  // initialize scatterbuffer filter parameters:
  scatterbuffer = new amb1wave_t(n_fragment);
  scatter_handle = create_diffuse_state_data(f_sample, n_fragment);
  scatterfilterpath.resize(scatterreflections);
  if(scatterreflections > 0) {
    scatterfilter =
        new TASCAR::fdn_t(scatterreflections, (uint32_t)f_sample, true,
                          TASCAR::fdn_t::mean, false, {0.0f, 0.0f, 0.0f, 0.0f});
    scatterfilter->set_scatterpar(
        scatterspread, (float)f_sample * (0.1f * scatterstructuresize / 340.0f),
        (float)f_sample * (scatterstructuresize / 340.0f), (float)f_sample,
        std::max(0.0f, std::min(0.999f, scatterdamping)));
    scatterallpass_w.resize(scatterreflections);
    scatterallpass_x.resize(scatterreflections);
    scatterallpass_y.resize(scatterreflections);
    scatterallpass_z.resize(scatterreflections);
    size_t k = 1;
    for(auto& flt : scatterallpass_x) {
      flt.set_allpass(0.89f, TASCAR_2PIf * 0.25f * (float)k /
                                 (float)scatterreflections);
      ++k;
    }
    k = 1;
    for(auto& flt : scatterallpass_y) {
      flt.set_allpass(0.9f, TASCAR_2PIf * 0.25f * (float)k /
                                (float)scatterreflections);
      ++k;
    }
    k = 1;
    for(auto& flt : scatterallpass_z) {
      flt.set_allpass(0.905f, TASCAR_2PIf * 0.25f * (float)k /
                                  (float)scatterreflections);
      ++k;
    }
    k = 1;
    for(auto& flt : scatterallpass_w) {
      flt.set_allpass(0.91f, TASCAR_2PIf * 0.25f * (float)k /
                                 (float)scatterreflections);
      ++k;
    }
  }
  for(uint32_t k = 0; k < n_channels; k++) {
    outchannelsp.push_back(new wave_t(n_fragment));
    outchannels.push_back(wave_t(*(outchannelsp.back())));
  }
  plugins.prepare(cfg());
  if(n_channels != outchannels.size()) {
    plugins.release();
    throw TASCAR::ErrMsg("Implementation error. Number of channels (" +
                         std::to_string(n_channels) +
                         ") differs from number of output buffers (" +
                         std::to_string(outchannels.size()) + ").");
  }
  recdelaycomp = get_delay_comp();
}

void receiver_t::post_prepare()
{
  receivermod_t::post_prepare();
  plugins.post_prepare();
}

void receiver_t::release()
{
  receivermod_t::release();
  plugins.release();
  outchannels.clear();
  for(uint32_t k = 0; k < outchannelsp.size(); ++k)
    delete outchannelsp[k];
  delete scatterbuffer;
  if(scatter_handle)
    delete scatter_handle;
  outchannelsp.clear();
  if(scatterfilter)
    delete scatterfilter;
  scatterfilter = nullptr;
}

receiver_t::~receiver_t()
{
  if(maskplug)
    delete maskplug;
}

void receiver_t::clear_output()
{
  for(auto & outchannel : outchannels )
    outchannel.clear();
  scatterbuffer->clear();
}

void receiver_t::validate_attributes(std::string& msg) const
{
  receivermod_t::validate_attributes(msg);
  plugins.validate_attributes(msg);
  if(maskplug)
    maskplug->validate_attributes(msg);
}

/**
   \ingroup callgraph
 */
void receiver_t::add_pointsource_with_scattering(
    const pos_t& prel, float width, float scattering, const wave_t& chunk,
    receivermod_base_t::data_t* data)
{
  scatterbuffer->add_panned(prel, chunk, scattering);
  receivermod_t::add_pointsource(prel, width, chunk, outchannels, data);
}

/**
   \ingroup callgraph
 */
void receiver_t::postproc(std::vector<wave_t>& output)
{
  // apply scatter buffer diffusion:
  if(scatterfilter) {
    for(uint32_t k = 0; k < n_fragment; ++k) {
      TASCAR::foa_sample_t x(scatterbuffer->w()[k], scatterbuffer->x()[k],
                             scatterbuffer->y()[k], scatterbuffer->z()[k]);
      size_t kflt = 0;
      for(auto& path : scatterfilterpath) {
        path.dlout.w = scatterallpass_w[kflt].filter(x.w);
        path.dlout.x = scatterallpass_x[kflt].filter(x.x);
        path.dlout.y = scatterallpass_y[kflt].filter(x.y);
        path.dlout.z = scatterallpass_z[kflt].filter(x.z);
        // path.dlout = x;
        ++kflt;
      }
      scatterfilter->process(scatterfilterpath, false);
      scatterbuffer->w()[k] = scatterfilter->outval.w;
      scatterbuffer->x()[k] = scatterfilter->outval.x;
      scatterbuffer->y()[k] = scatterfilter->outval.y;
      scatterbuffer->z()[k] = scatterfilter->outval.z;
      //, scatterbuffer->x()[k],
      // scatterbuffer->y()[k], scatterbuffer->z()[k]
    }
  }
  // add scatter buffer to diffuse sound field:
  add_diffuse_sound_field_rec(*scatterbuffer, scatter_handle);
  // perform post processing of base class:
  receivermod_t::postproc(output);
  // process any audio plugins of receiver:
  plugins.process_plugins(output, position, orientation, ltp);
}

/**
   \ingroup callgraph
 */
void receiver_t::post_proc(const TASCAR::transport_t& tp)
{
  ltp = tp;
  ltp.object_time_samples = ltp.session_time_samples - starttime_samples;
  ltp.object_time_seconds = (double)ltp.object_time_samples * t_sample;
  postproc(outchannels);
}

/**
   \ingroup callgraph
 */
void receiver_t::add_diffuse_sound_field_rec(const amb1wave_t& chunk,
                                             receivermod_base_t::data_t* data)
{
  receivermod_t::add_diffuse_sound_field(chunk, outchannels, data);
}

void receiver_t::update_refpoint(const pos_t& psrc_physical,
                                 const pos_t& psrc_virtual, pos_t& prel,
                                 float& distance, float& traveltime_in_m,
                                 float& gain, bool b_img, gainmodel_t gainmodel,
                                 float& nearfieldlimit)
{
  if(volumetric.has_volume()) {
    /*
     * The receiver uses volumetric rendering. The relative direction
     * (prel) and distance is controlled by the physical source
     * position, but for the gain the average distance in the volume
     * is taken.
     */
    prel = psrc_physical;
    prel -= position;
    prel /= orientation;
    distance = prel.normf();
    traveltime_in_m = distance;
    shoebox_t box;
    box.size = volumetric;
    float d(box.nextpoint(prel).normf());
    if(falloff > 0)
      gain = (0.5f + 0.5f * cosf(TASCAR_PIf * std::min(1.0f, d / falloff))) /
             std::max(nearfieldlimit, avgdist);
    else {
      switch(gainmodel) {
      case GAIN_INVR:
        gain = 1.0f / std::max(1.0f, d + avgdist);
        break;
      case GAIN_UNITY:
        gain = 1.0f / std::max(1.0f, avgdist);
        break;
      }
    }
    if(volumetricgainwithdistance)
      gain *= distance;
  } else {
    /*
     * The receiver uses a normal acoustic model. Relative direction
     * depends on the virtual (image source) position.
     */
    prel = psrc_virtual;
    prel -= position;
    prel /= orientation;
    distance = prel.normf();
    traveltime_in_m = distance;
    switch(gainmodel) {
    case GAIN_INVR:
      gain = 1.0f / std::max(nearfieldlimit, distance);
      break;
    case GAIN_UNITY:
      gain = 1.0f;
      break;
    }
    float physical_dist(TASCAR::distancef(psrc_physical, position));
    // if image source position is closer to receiver than the direct
    // path then something is wrong, do not render.
    if(b_img && (physical_dist > distance)) {
      gain = 0.0;
    }
    if(proxy_delay || proxy_airabsorption || proxy_gain || proxy_direction) {
      auto proxy_prel = proxy_position;
      if(!proxy_is_relative) {
        proxy_prel -= position;
        proxy_prel /= orientation;
      }
      auto proxy_distance = proxy_prel.normf();
      if(proxy_delay)
        traveltime_in_m = proxy_distance;
      if(proxy_airabsorption)
        distance = proxy_distance;
      if(proxy_gain && (gainmodel == GAIN_INVR))
        gain = 1.0f / std::max(0.1f, proxy_distance);
      if(proxy_direction)
        prel = proxy_prel;
    }
  }
  make_friendly_number(gain);
}

void receiver_t::set_next_gain(float g)
{
  next_gain = g;
  gain_zero = (next_gain == 0) && (x_gain == 0);
}

void receiver_t::apply_gain()
{
  float dx_gain = (next_gain - x_gain) * (float)t_inc;
  if(n_channels > 0) {
    uint32_t psize(outchannels[0].size());
    for(uint32_t k = 0; k < psize; k++) {
      float g(x_gain += dx_gain);
      if((fade_timer > 0) &&
         ((fade_startsample == FADE_START_NOW) ||
          ((fade_startsample <= ltp.session_time_samples + k) &&
           ltp.rolling))) {
        --fade_timer;
        previous_fade_gain = prelim_previous_fade_gain;
        next_fade_gain = prelim_next_fade_gain;
        fade_gain = previous_fade_gain +
                    (next_fade_gain - previous_fade_gain) *
                        (0.5f + 0.5f * cosf((float)fade_timer * fade_rate));
      }
      g *= fade_gain;
      for(uint32_t c = 0; c < n_channels; c++) {
        outchannels[c][k] *= g;
      }
    }
  }
  x_gain = next_gain;
}

void receiver_t::set_fade(float targetgain, float duration, float start)
{
  fade_timer = 0;
  duration = std::max((float)t_sample, duration);
  if(start < 0)
    fade_startsample = FADE_START_NOW;
  else
    fade_startsample = (uint64_t)(f_sample * start);
  prelim_previous_fade_gain = fade_gain;
  prelim_next_fade_gain = targetgain;
  fade_rate = TASCAR_PIf * (float)t_sample / duration;
  fade_timer = std::max(1u, (uint32_t)(f_sample * duration));
}

TASCAR::Acousticmodel::boundingbox_t::boundingbox_t(tsccfg::node_t xmlsrc)
    : dynobject_t(xmlsrc), falloff(1.0), active(false)
{
  dynobject_t::GET_ATTRIBUTE(size, "m", "dimension of bounding box");
  dynobject_t::GET_ATTRIBUTE(falloff, "m",
                             "fade-out ramp length at boundaries");
  dynobject_t::GET_ATTRIBUTE_BOOL(active, "use bounding box");
}

pos_t diffractor_t::process(pos_t p_src, const pos_t& p_rec, wave_t& audio,
                            float c, float fs, state_t& state, float drywet)
{
  // calculate intersection:
  pos_t p_is;
  double w(0);
  // test for intersection with infinite plane:
  bool is_intersect(intersection(p_src, p_rec, p_is, &w));
  if((w <= 0) || (w >= 1))
    is_intersect = false;
  if(is_intersect) {
    // test for intersection with the limited plane:
    bool is_outside(false);
    pos_t pne;
    nearest(p_is, &is_outside, &pne);
    p_is = pne;
    if(b_inner) {
      if(is_outside)
        is_intersect = false;
    } else {
      if(!is_outside)
        is_intersect = false;
    }
  }
  // calculate filter:
  float dt(1.0f / (float)audio.n);
  double dA1(-state.A1 * dt);
  if(is_intersect) {
    // calculate geometry:
    pos_t p_is_src(p_src - p_is);
    pos_t p_rec_is(p_is - p_rec);
    p_rec_is.normalize();
    const float d_is_src(p_is_src.normf());
    if(d_is_src > 0)
      p_is_src *= 1.0 / d_is_src;
    // calculate first zero crossing frequency:
    const float cos_theta(std::max(0.0f, dot_prodf(p_is_src, p_rec_is)));
    const float sin_theta(std::max(EPSf, sqrtf(1.0f - cos_theta * cos_theta)));
    float loc_aperture = (float)aperture;
    if(manual_aperture > 0.0)
      loc_aperture = manual_aperture;
    const float f0(3.8317f * c / (TASCAR_2PIf * loc_aperture * sin_theta));
    // calculate filter coefficient increment:
    dA1 = (exp(-TASCAR_PI * f0 / fs) - state.A1) * dt;
    // return effective source position:
    p_rec_is *= d_is_src;
    p_src = p_is + p_rec_is;
  }
  // apply low pass filter to audio chunk:
  for(uint32_t k = 0; k < audio.n; k++) {
    state.A1 += dA1;
    double B0(1.0 - state.A1);
    state.s1 = state.s1 * state.A1 + (double)audio[k] * B0;
    audio[k] = drywet * audio[k] +
               (1.0f - drywet) *
                   (float)(state.s2 = state.s2 * state.A1 + state.s1 * B0);
  }
  return p_src;
}

source_t::source_t(tsccfg::node_t xmlsrc, const std::string& name,
                   const std::string& parentname)
    : sourcemod_t(xmlsrc), licensed_component_t(typeid(*this).name()),
      ismmin(0), ismmax(2147483647), layers(0xffffffff), maxdist(3700),
      minlevel(0), nearfieldlimit(0.1f), sincorder(0), gainmodel(GAIN_INVR),
      airabsorption(true), delayline(true), size(0), active(true),
      // is_prepared(false),
      plugins(xmlsrc, name, parentname)
{
  GET_ATTRIBUTE(
      size, "m",
      "physical size of sound source (effect depends on rendering method)");
  GET_ATTRIBUTE(maxdist, "m", "maximum distance to be used in delay lines");
  GET_ATTRIBUTE_DBSPL(minlevel, "Level threshold for rendering");
  GET_ATTRIBUTE(nearfieldlimit, "m",
                "distance arond 1/r source where the gain is constant");
  GET_ATTRIBUTE_BOOL(airabsorption, "apply air absorption filter");
  GET_ATTRIBUTE_BOOL(delayline, "use delayline");
  std::string gr("1/r");
  get_attribute("gainmodel", gr, "",
                "gain rule, valid gain models: \"1/r\", \"1\"");
  if(gr == "1/r")
    gainmodel = GAIN_INVR;
  else if(gr == "1")
    gainmodel = GAIN_UNITY;
  else
    throw TASCAR::ErrMsg("Invalid gain model " + gr +
                         "(valid gain models: \"1/r\", \"1\").");
  GET_ATTRIBUTE(sincorder, "", "order of sinc interpolation in delayline");
  GET_ATTRIBUTE(ismmin, "", "minimal ISM order to render");
  GET_ATTRIBUTE(ismmax, "", "maximal ISM order to render");
  GET_ATTRIBUTE_BITS(layers, "render layers");
}

source_t::~source_t() {}

void source_t::configure()
{
  sourcemod_t::configure();
  update();
  for(uint32_t k = 0; k < n_channels; k++) {
    inchannelsp.push_back(new wave_t(n_fragment));
    inchannels.push_back(wave_t(*(inchannelsp.back())));
  }
  plugins.prepare(cfg());
}

void source_t::post_prepare()
{
  sourcemod_t::post_prepare();
  plugins.post_prepare();
}

void source_t::release()
{
  plugins.release();
  sourcemod_t::release();
  inchannels.clear();
  for(uint32_t k = 0; k < inchannelsp.size(); ++k)
    delete inchannelsp[k];
  inchannelsp.clear();
}

void source_t::process_plugins(const TASCAR::transport_t& tp)
{
  plugins.process_plugins(inchannels, position, orientation, tp);
}

void receiver_t::add_variables(TASCAR::osc_server_t* srv)
{
  receivermod_t::add_variables(srv);
  plugins.add_variables(srv);
  if(maskplug) {
    std::string oldpref(srv->get_prefix());
    srv->set_prefix(oldpref + "/mask");
    maskplug->add_variables(srv);
    srv->set_prefix(oldpref);
  }
  srv->set_variable_owner("receiver_t");
  // uint32_t scatterreflections = 0;
  srv->add_float_degree("/scatterspread", &scatterspread, "",
                        "Spatial spread of scattering");
  srv->add_float("/scatterstructuresize", &scatterstructuresize, "[0,10]",
                 "size of scatter structure in m");
  srv->add_float("/scatterdamping", &scatterdamping, "[0,1]",
                 "damping of scatter reflection filter");
  srv->add_pos("/proxy/position", &proxy_position, "", "Proxy position in m");
  srv->add_bool("/proxy/is_relative", &proxy_is_relative,
                "Proxy is relative to receiver (true) or in absolute "
                "coordinates (false)");
  srv->add_bool("/proxy/delay", &proxy_delay, "Use proxy position for delay");
  srv->add_bool("/proxy/airabsorption", &proxy_airabsorption,
                "Use proxy position for air absorption");
  srv->add_bool("/proxy/gain", &proxy_gain, "Use proxy position for gain");
  srv->add_bool("/proxy/direction", &proxy_direction,
                "Use proxy position for direction");
  srv->unset_variable_owner();
}

soundpath_t::soundpath_t(const source_t* src, const soundpath_t* parent_,
                         const reflector_t* generator_)
    : parent((parent_ ? parent_ : this)),
      primary((parent_ ? (parent_->primary) : src)), reflector(generator_),
      visible(true)
{
  reflectionfilterstates.resize(getorder());
  for(uint32_t k = 0; k < reflectionfilterstates.size(); ++k)
    reflectionfilterstates[k] = 0;
}

void soundpath_t::update_position()
{
  visible = true;
  if(reflector) {
    // calculate image position and orientation:
    p_cut = reflector->nearest_on_plane(parent->position);
    // calculate nominal image source position:
    pos_t p_img(p_cut);
    p_img *= 2.0;
    p_img -= parent->position;
    // if image source is in front of reflector then return:
    if(dot_prod(p_img - p_cut, reflector->get_normal()) > 0)
      visible = false;
    position = p_img;
    orientation = parent->orientation;
  } else {
    position = primary->position;
    orientation = primary->orientation;
  }
}

uint32_t soundpath_t::getorder() const
{
  if(parent != this)
    return parent->getorder() + 1;
  else
    return 0;
}

void soundpath_t::apply_reflectionfilter(TASCAR::wave_t& audio)
{
  uint32_t k(0);
  const reflector_t* pr(reflector);
  const soundpath_t* ps(this);
  while(pr) {
    pr->apply_reflectionfilter(audio, reflectionfilterstates[k]);
    ++k;
    ps = ps->parent;
    pr = ps->reflector;
  }
}

pos_t soundpath_t::get_effective_position(const pos_t& p_rec, float& gain)
{
  if(!reflector)
    return position;
  // calculate orthogonal point on plane:
  pos_t pcut_rec(reflector->nearest_on_plane(p_rec));
  // if receiver is behind reflector then return zero:
  if(dot_prod(p_rec - pcut_rec, reflector->get_normal()) < 0) {
    gain = 0;
    return position;
  }
  float len_receiver(distancef(pcut_rec, p_rec));
  float len_src(distancef(p_cut, position));
  // calculate intersection:
  float ratio(len_receiver / std::max(1e-6f, (len_receiver + len_src)));
  pos_t p_is(p_cut - pcut_rec);
  p_is *= ratio;
  p_is += pcut_rec;
  p_is = reflector->nearest(p_is);
  gain = powf(std::max(0.0f, dot_prodf((p_rec - p_is).normal(),
                                       (p_is - position).normal())),
              2.7f);
  make_friendly_number(gain);
  if(reflector->edgereflection) {
    float len_img(distancef(p_is, position));
    pos_t p_eff((p_is - p_rec).normal());
    p_eff *= len_img;
    p_eff += p_is;
    return p_eff;
  }
  return position;
}

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
