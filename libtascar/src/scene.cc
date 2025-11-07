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

#include "scene.h"
#include "amb33defs.h"
#include "errorhandling.h"
#include "filterclass.h"
#include "tascar_os.h"
#include <algorithm>
#include <fstream>
#include <libgen.h>
#include <locale.h>
#include <set>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

using namespace TASCAR;
using namespace TASCAR::Scene;

/*
 * object_t
 */
object_t::object_t(tsccfg::node_t src)
    : dynobject_t(src), route_t(src), endtime(0)
{
  dynobject_t::get_attribute("end", endtime, "s",
                             "end of render activity, or 0 to render always");
  std::string scol;
  dynobject_t::get_attribute("color", scol, "", "html color string");
  color = rgb_color_t(scol);
  dynobject_t::GET_ATTRIBUTE(scale, "", "scale of local coordinates");
}

bool object_t::isactive(double time) const
{
  return (!get_mute()) && (time >= starttime) &&
         ((starttime >= endtime) || (time <= endtime));
}

/*
 *diff_snd_field_obj_t
 */
diff_snd_field_obj_t::diff_snd_field_obj_t(tsccfg::node_t xmlsrc)
    : object_t(xmlsrc), audio_port_t(xmlsrc, true),
      licensed_component_t(typeid(*this).name()), size(1, 1, 1), falloff(1.0),
      layers(0xffffffff), source(NULL), plugins(xmlsrc, get_name(), "")
{
  dynobject_t::GET_ATTRIBUTE(size, "m",
                             "size in which sound field is rendered.");
  dynobject_t::GET_ATTRIBUTE(falloff, "m", "falloff ramp length at boundaries");
  dynobject_t::GET_ATTRIBUTE_BITS(layers, "render layers");
}

void diff_snd_field_obj_t::add_licenses(licensehandler_t* lh)
{
  licensed_component_t::add_licenses(lh);
  if(source)
    source->add_licenses(lh);
}

diff_snd_field_obj_t::~diff_snd_field_obj_t()
{
  if(source)
    delete source;
}

void diff_snd_field_obj_t::geometry_update(double t)
{
  if(source) {
    dynobject_t::geometry_update(t);
    get_6dof(source->center, source->orientation);
    source->layers = layers;
  }
}

void diff_snd_field_obj_t::configure()
{
  n_channels = 4;
  if(source)
    delete source;
  reset_meters();
  addmeter((float)f_sample);
  source = new TASCAR::Acousticmodel::diffuse_t(
      dynobject_t::e, n_fragment, *(rmsmeter[0]), get_name(), plugins);
  source->size = size;
  source->falloff = 1.0f / std::max(falloff, 1.0e-10f);
  source->prepare(cfg());
}

void diff_snd_field_obj_t::post_prepare()
{
  source->post_prepare();
}

void diff_snd_field_obj_t::validate_attributes(std::string& msg) const
{
  dynobject_t::validate_attributes(msg);
  plugins.validate_attributes(msg);
}

audio_port_t::~audio_port_t() {}

void audio_port_t::set_port_index(uint32_t port_index_)
{
  port_index = port_index_;
}

void audio_port_t::set_inv(bool inv)
{
  if(inv)
    gain = -fabsf(gain);
  else
    gain = fabsf(gain);
}

/*
 * src_object_t
 */
src_object_t::src_object_t(tsccfg::node_t xmlsrc)
    : object_t(xmlsrc), licensed_component_t(typeid(*this).name()),
      startframe(0)
{
  if(get_name().empty())
    set_name("in");
  for(auto& sne : tsccfg::node_get_children(dynobject_t::e)) {
    if(tsccfg::node_get_name(sne) == "sound")
      add_sound(sne);
    else if((tsccfg::node_get_name(sne) != "creator") &&
            (tsccfg::node_get_name(sne) != "navmesh") &&
            (tsccfg::node_get_name(sne) != "include") &&
            (tsccfg::node_get_name(sne) != "position") &&
            (tsccfg::node_get_name(sne) != "orientation"))
      TASCAR::add_warning(
          "Invalid sub-node \"" + tsccfg::node_get_name(sne) + "\".", sne);
  }
}

void src_object_t::add_sound(tsccfg::node_t src)
{
  if(!src)
    src = dynobject_t::add_child("sound");
  sound_t* snd(new sound_t(src, this));
  sound.push_back(snd);
  soundmap[snd->get_id()] = snd;
}

sound_t& src_object_t::sound_by_id(const std::string& id)
{
  auto snd(soundmap.find(id));
  if(snd != soundmap.end())
    return *(snd->second);
  throw TASCAR::ErrMsg("Unknown sound id \"" + id + "\" in source \"" +
                       get_name() + "\".");
}

src_object_t::~src_object_t()
{
  for(std::vector<sound_t*>::iterator s = sound.begin(); s != sound.end(); ++s)
    delete *s;
}

void src_object_t::add_licenses(licensehandler_t* lh)
{
  licensed_component_t::add_licenses(lh);
  for(auto it = sound.begin(); it != sound.end(); ++it)
    (*it)->add_licenses(lh);
}

std::string src_object_t::next_sound_name() const
{
  std::set<std::string> names;
  for(std::vector<sound_t*>::const_iterator it = sound.begin();
      it != sound.end(); ++it)
    names.insert((*it)->get_name());
  char ctmp[1024];
  ctmp[1023] = 0;
  uint32_t n(0);
  snprintf(ctmp, 1023, "%u", n);
  while(names.find(ctmp) != names.end()) {
    ++n;
    snprintf(ctmp, 1023, "%u", n);
  }
  return ctmp;
}

void src_object_t::validate_attributes(std::string& msg) const
{
  dynobject_t::validate_attributes(msg);
  for(auto psound : sound)
    psound->validate_attributes(msg);
}

void src_object_t::geometry_update(double t)
{
  dynobject_t::geometry_update(t);
  for(std::vector<sound_t*>::iterator it = sound.begin(); it != sound.end();
      ++it) {
    (*it)->geometry_update(t);
  }
}

void src_object_t::configure()
{
  reset_meters();
  try {
    for(auto it = sound.begin(); it != sound.end(); ++it) {
      chunk_cfg_t cf(cfg());
      // currently, only single channel sounds are supported:
      cf.n_channels = 1;
      (*it)->prepare(cf);
      for(uint32_t k = 0; k < cf.n_channels; ++k) {
        addmeter(cf.f_sample);
        (*it)->add_meter(rmsmeter.back());
      }
    }
  }
  catch(...) {
    for(auto it = sound.begin(); it != sound.end(); ++it)
      if((*it)->is_prepared())
        (*it)->release();
    throw;
  }
  startframe = f_sample * starttime;
}

void src_object_t::post_prepare()
{
  for(auto& snd : sound)
    snd->post_prepare();
}

void src_object_t::release()
{
  for(auto it = sound.begin(); it != sound.end(); ++it)
    (*it)->release();
  audiostates_t::release();
}

scene_t::scene_t(tsccfg::node_t xmlsrc)
    : xml_element_t(xmlsrc), licensed_component_t(typeid(*this).name()),
      description(""), name("scene"), id(TASCAR::get_tuid()), c(340.0),
      ismorder(1), guiscale(200), guitrackobject(NULL), anysolo(0),
      scene_path(""), active(true)
{
  try {
    GET_ATTRIBUTE(name, "", "scene name");
    GET_ATTRIBUTE(id, "", "scene id, or empty to auto-generate id");
    TASCAR::validate_tuid(id, xmlsrc);
    GET_ATTRIBUTE(ismorder, "", "order of image source model");
    GET_ATTRIBUTE(guiscale, "m", "scale of GUI window of this scene");
    GET_ATTRIBUTE(guicenter, "m", "origin of GUI window");
    GET_ATTRIBUTE(c, "m/s", "speed of sound");
    GET_ATTRIBUTE_BOOL(active, "render scene");
    description = tsccfg::node_get_text(e, "description");
    for(auto mat :
        {material_t("parquet",
                    {125.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f},
                    {
                        0.04f,
                        0.04f,
                        0.07f,
                        0.06f,
                        0.06f,
                        0.07f,
                    }),
         material_t("window",
                    {125.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f},
                    {0.35f, 0.25f, 0.18f, 0.12f, 0.07f, 0.04f}),
         material_t("concrete",
                    {125.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f},
                    {0.36f, 0.44f, 0.31f, 0.29f, 0.39f, 0.25f}),
         material_t("acoustic_tiles",
                    {125.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f},
                    {0.05f, 0.22f, 0.52f, 0.56f, 0.45f, 0.32f}),
         material_t("plaster",
                    {125.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f},
                    {0.013f, 0.015f, 0.02f, 0.03f, 0.04f, 0.05f}),
         material_t("carpet_on_concrete",
                    {125.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f},
                    {0.02f, 0.06f, 0.14f, 0.37f, 0.60f, 0.65f}),
         material_t("metal_8mm",
                    {125.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f},
                    {0.50f, 0.35f, 0.15f, 0.05f, 0.05f, 0.00f})})
      materials[mat.name] = mat;
    for(auto& sne : tsccfg::node_get_children(e)) {
      TASCAR::Scene::object_t* obj(NULL);
      // parse nodes:
      if(tsccfg::node_get_name(sne) == "source") {
        source_objects.push_back(new src_object_t(sne));
        obj = source_objects.back();
      } else if(tsccfg::node_get_name(sne) == "diffuse") {
        diff_snd_field_objects.push_back(new diff_snd_field_obj_t(sne));
        obj = diff_snd_field_objects.back();
      } else if(tsccfg::node_get_name(sne) == "receiver") {
        receivermod_objects.push_back(new receiver_obj_t(sne, false));
        obj = receivermod_objects.back();
      } else if(tsccfg::node_get_name(sne) == "face") {
        face_objects.push_back(new face_object_t(sne));
        obj = face_objects.back();
      } else if(tsccfg::node_get_name(sne) == "facegroup") {
        facegroups.push_back(new face_group_t(sne));
        obj = facegroups.back();
      } else if(tsccfg::node_get_name(sne) == "obstacle") {
        obstaclegroups.push_back(new obstacle_group_t(sne));
        obj = obstaclegroups.back();
      } else if(tsccfg::node_get_name(sne) == "mask") {
        mask_objects.push_back(new mask_object_t(sne));
        obj = mask_objects.back();
      } else if(tsccfg::node_get_name(sne) == "reverb") {
        diffuse_reverbs.push_back(new diffuse_reverb_t(sne));
        obj = diffuse_reverbs.back();
      } else if(tsccfg::node_get_name(sne) == "material") {
        material_t mat(sne);
        materials[mat.name] = mat;
      } else if(tsccfg::node_get_name(sne) != "include")
        add_warning("Unrecognized xml element \"" + tsccfg::node_get_name(sne) +
                        "\".",
                    sne);
      if(obj) {
        if(namelist.find(obj->get_name()) != namelist.end())
          throw TASCAR::ErrMsg("An object of name \"" + obj->get_name() +
                               "\" already exists in scene \"" + name + "\"");
        namelist.insert(obj->get_name());
      }
    }
    for(auto& source_object : source_objects) {
      for(auto& sound : source_object->sound) {
        sounds.push_back(sound);
        const std::string id(sound->get_id());
        auto mapsnd(soundmap.find(id));
        if(mapsnd != soundmap.end()) {
          throw TASCAR::ErrMsg("The sound id \"" + id +
                               "\" of source object \"" +
                               source_object->get_name() +
                               "\" is not unique (already used by source \"" +
                               soundmap[id]->get_parent_name() + "\").");
        } else {
          soundmap[sound->get_id()] = sound;
        }
      }
    }
    std::string guitracking;
    GET_ATTRIBUTE(guitracking, "", "object name for scene tracking");
    std::vector<object_t*> objs(find_object(guitracking));
    if(objs.size())
      guitrackobject = objs[0];
  }
  catch(...) {
    clean_children();
    throw;
  }
}

sound_t& scene_t::sound_by_id(const std::string& id)
{
  auto snd(soundmap.find(id));
  if(snd != soundmap.end())
    return *(snd->second);
  throw TASCAR::ErrMsg("Unknown sound id \"" + id + "\" in scene \"" + name +
                       "\".");
}

void scene_t::configure_meter(float tc, TASCAR::levelmeter::weight_t w)
{
  std::vector<object_t*> objs(get_objects());
  for(std::vector<object_t*>::iterator it = objs.begin(); it != objs.end();
      ++it)
    (*it)->configure_meter(tc, w);
}

void scene_t::clean_children()
{
  for(auto& obj : get_objects())
    delete obj;
}

scene_t::~scene_t()
{
  clean_children();
}

void scene_t::geometry_update(double t)
{
  for(auto pobj : all_objects)
    pobj->geometry_update(t);
  // for(std::vector<object_t*>::iterator it = all_objects.begin();
  //    it != all_objects.end(); ++it)
  //  (*it)->geometry_update(t);
}

void scene_t::process_active(double t)
{
  for(std::vector<src_object_t*>::iterator it = source_objects.begin();
      it != source_objects.end(); ++it)
    (*it)->process_active(t, anysolo);
  for(std::vector<diff_snd_field_obj_t*>::iterator it =
          diff_snd_field_objects.begin();
      it != diff_snd_field_objects.end(); ++it)
    (*it)->process_active(t, anysolo);
  for(std::vector<receiver_obj_t*>::iterator it = receivermod_objects.begin();
      it != receivermod_objects.end(); ++it)
    (*it)->process_active(t, anysolo);
  for(std::vector<face_object_t*>::iterator it = face_objects.begin();
      it != face_objects.end(); ++it)
    (*it)->process_active(t, anysolo);
  for(std::vector<face_group_t*>::iterator it = facegroups.begin();
      it != facegroups.end(); ++it)
    (*it)->process_active(t, anysolo);
  for(std::vector<obstacle_group_t*>::iterator it = obstaclegroups.begin();
      it != obstaclegroups.end(); ++it)
    (*it)->process_active(t, anysolo);
  for(std::vector<mask_object_t*>::iterator it = mask_objects.begin();
      it != mask_objects.end(); ++it)
    (*it)->process_active(t, anysolo);
  for(auto it = diffuse_reverbs.begin(); it != diffuse_reverbs.end(); ++it)
    (*it)->process_active(t, anysolo);
}

mask_object_t::mask_object_t(tsccfg::node_t xmlsrc)
    : object_t(xmlsrc), xmlfalloff(1)
{
  dynobject_t::get_attribute("size", xmlsize, "m", "dimension of mask");
  dynobject_t::get_attribute("falloff", xmlfalloff, "m",
                             "ramp length at boundaries");
  dynobject_t::get_attribute_bool("inside", mask_inner, "",
                                  "mask inner objects");
}

void mask_object_t::geometry_update(double t)
{
  dynobject_t::geometry_update(t);
  shoebox_t::size.x = std::max(0.0, xmlsize.x - xmlfalloff);
  shoebox_t::size.y = std::max(0.0, xmlsize.y - xmlfalloff);
  shoebox_t::size.z = std::max(0.0, xmlsize.z - xmlfalloff);
  dynobject_t::get_6dof(shoebox_t::center, shoebox_t::orientation);
  inv_falloff = 1.0 / std::max(xmlfalloff, 1e-10);
}

void mask_object_t::process_active(double t, uint32_t anysolo)
{
  mask_object_t::active = is_active(anysolo, t);
}

receiver_obj_t::receiver_obj_t(tsccfg::node_t xmlsrc, bool is_reverb_)
    : object_t(xmlsrc), audio_port_t(xmlsrc, false),
      receiver_t(xmlsrc, default_name("out"), is_reverb_)
{
  // test if this is a speaker-based receiver module:
  TASCAR::receivermod_base_speaker_t* spk(
      dynamic_cast<TASCAR::receivermod_base_speaker_t*>(libdata));
  double maxage(TASCAR::config("tascar.spkcalib.maxage", 30));
  if(spk) {
    if(spk->spkpos.has_caliblevel) {
      if(has_caliblevel)
        TASCAR::add_warning("Caliblevel is defined in receiver \"" +
                            get_name() + "\" and in layout file \"" +
                            spk->spkpos.layout +
                            "\". Will use the value from layout file.");
      caliblevel = spk->spkpos.caliblevel;
    }
    if(spk->spkpos.has_diffusegain) {
      if(has_diffusegain)
        TASCAR::add_warning("Diffusegain is defined in receiver \"" +
                            get_name() + "\" and in layout file \"" +
                            spk->spkpos.layout +
                            "\". Will use the value from layout file.");
      diffusegain = spk->spkpos.diffusegain;
    }
    if(spk->spkpos.has_caliblevel || spk->spkpos.has_diffusegain ||
       spk->spkpos.has_calibdate) {
      if(spk->spkpos.calibage > maxage)
        TASCAR::add_warning("Calibration of layout file \"" +
                                spk->spkpos.layout + "\" is " +
                                TASCAR::days_to_string(spk->spkpos.calibage) +
                                " old (calibrated: " + spk->spkpos.calibdate +
                                ", receiver \"" + get_name() + "\").",
                            xmlsrc);
    }
    bool checkcalibfor(TASCAR::config("tascar.spkcalib.checktypeid", true) >
                       0.0);
    if(checkcalibfor)
      if(spk->spkpos.has_calibfor) {
        std::string spktypeid(spk->get_spktypeid());
        if(spk->spkpos.calibfor != spktypeid)
          TASCAR::add_warning(
              "Calibration of layout file \"" + spk->spkpos.layout +
              "\" was created for '" + spk->spkpos.calibfor +
              "', but the receiver type id is '" + spktypeid + "'.");
      }
  }
}

void receiver_obj_t::validate_attributes(std::string& msg) const
{
  dynobject_t::validate_attributes(msg);
  TASCAR::Acousticmodel::receiver_t::validate_attributes(msg);
}

void receiver_obj_t::configure()
{
  TASCAR::Acousticmodel::receiver_t::configure();
  reset_meters();
  for(uint32_t k = 0; k < n_channels; k++)
    addmeter(f_sample);
}

uint32_t receiver_obj_t::get_num_input_ports()
{
  return 0;
}

uint32_t receiver_obj_t::get_num_output_ports()
{
  return n_channels;
}

void receiver_obj_t::release()
{
  TASCAR::Acousticmodel::receiver_t::release();
}

void receiver_obj_t::postproc(std::vector<wave_t>& output)
{
  starttime_samples = TASCAR::Acousticmodel::receiver_t::f_sample * starttime;
  TASCAR::Acousticmodel::receiver_t::postproc(output);
  for(uint32_t k = 0; k < std::min(output.size(), rmsmeter.size()); k++)
    rmsmeter[k]->update(output[k]);
}

void receiver_obj_t::geometry_update(double t)
{
  dynobject_t::geometry_update(t);
  *(c6dof_t*)this = c6dof;
  boundingbox.geometry_update(t);
}

void receiver_obj_t::process_active(double t, uint32_t anysolo)
{
  TASCAR::Acousticmodel::receiver_t::active = is_active(anysolo, t);
}

src_object_t* scene_t::add_source()
{
  source_objects.push_back(
      new src_object_t(tsccfg::node_add_child(e, "source")));
  return source_objects.back();
}

std::vector<object_t*> scene_t::get_objects()
{
  std::vector<object_t*> r;
  for(std::vector<src_object_t*>::iterator it = source_objects.begin();
      it != source_objects.end(); ++it)
    r.push_back(*it);
  for(std::vector<diff_snd_field_obj_t*>::iterator it =
          diff_snd_field_objects.begin();
      it != diff_snd_field_objects.end(); ++it)
    r.push_back(*it);
  for(std::vector<receiver_obj_t*>::iterator it = receivermod_objects.begin();
      it != receivermod_objects.end(); ++it)
    r.push_back(*it);
  for(std::vector<face_object_t*>::iterator it = face_objects.begin();
      it != face_objects.end(); ++it)
    r.push_back(*it);
  for(std::vector<face_group_t*>::iterator it = facegroups.begin();
      it != facegroups.end(); ++it)
    r.push_back(*it);
  for(std::vector<obstacle_group_t*>::iterator it = obstaclegroups.begin();
      it != obstaclegroups.end(); ++it)
    r.push_back(*it);
  for(std::vector<mask_object_t*>::iterator it = mask_objects.begin();
      it != mask_objects.end(); ++it)
    r.push_back(*it);
  for(auto it = diffuse_reverbs.begin(); it != diffuse_reverbs.end(); ++it)
    r.push_back(*it);
  return r;
}

void scene_t::configure()
{
  if(!name.size())
    throw TASCAR::ErrMsg("Invalid empty scene name (please set \"name\" "
                         "attribute of scene node).");
  if(name.find(" ") != std::string::npos)
    throw TASCAR::ErrMsg("Spaces in scene name are not supported (\"" + name +
                         "\")");
  if(name.find(":") != std::string::npos)
    throw TASCAR::ErrMsg("Colons in scene name are not supported (\"" + name +
                         "\")");
  all_objects = get_objects();
  // configure parents:
  for(auto po : all_objects)
    for(auto pp : all_objects)
      if((po->parent.size() > 0) && (po->parent == pp->get_name())) {
        try {
          po->set_parent(pp);
        }
        catch(const std::exception& e) {
          std::string msg = e.what();
          throw TASCAR::ErrMsg(msg + " (object \"" + po->parent +
                               "\", scene \"" + name + "\").");
        }
      }
  // sort all objects to start with those who have most descendants, to get
  // correct position updates for children.
  std::sort(all_objects.begin(), all_objects.end(),
            [](dynobject_t* a, dynobject_t* b) {
              return a->get_num_descendants() > b->get_num_descendants();
            });
  try {
    // update reflectors with material entry:
    // first, get list of used materials:
    std::set<std::string> used_materials;
    for(auto r : face_objects)
      if(!r->material.empty())
        used_materials.insert(r->material);
    for(auto r : facegroups)
      if(!r->material.empty())
        used_materials.insert(r->material);
    for(auto mat : used_materials)
      if(materials.find(mat) == materials.end())
        throw TASCAR::ErrMsg("Material \"" + mat + "\" is not defined.");
    for(auto mat : used_materials)
      materials[mat].update_coeff(f_sample);
    for(auto r : face_objects)
      if(!r->material.empty()) {
        r->reflectivity = materials[r->material].reflectivity;
        r->damping = materials[r->material].damping;
      }
    for(auto r : facegroups)
      if(!r->material.empty()) {
        r->reflectivity = materials[r->material].reflectivity;
        r->damping = materials[r->material].damping;
      }
    // prepare all objects:
    for(auto it = all_objects.begin(); it != all_objects.end(); ++it) {
      // prepare all objects which are derived from audiostates:
      audiostates_t* p_as(dynamic_cast<audiostates_t*>(*it));
      if(p_as) {
        chunk_cfg_t cf(cfg());
        p_as->prepare(cf);
      }
    }
  }
  catch(...) {
    for(auto it = all_objects.begin(); it != all_objects.end(); ++it) {
      // prepare all objects which are derived from audiostates:
      audiostates_t* p_as(dynamic_cast<audiostates_t*>(*it));
      if(p_as && p_as->is_prepared())
        p_as->release();
    }
    throw;
  }
}

void scene_t::post_prepare()
{
  for(auto it = all_objects.begin(); it != all_objects.end(); ++it) {
    // prepare all objects which are derived from audiostates:
    audiostates_t* p_as(dynamic_cast<audiostates_t*>(*it));
    if(p_as) {
      p_as->post_prepare();
    }
  }
}

void scene_t::release()
{
  audiostates_t::release();
  all_objects = get_objects();
  for(auto it = all_objects.begin(); it != all_objects.end(); ++it) {
    // release all objects which are derived from audiostates and are
    // in prepared state:
    audiostates_t* p_as(dynamic_cast<audiostates_t*>(*it));
    if(p_as) {
      if(p_as->is_prepared())
        p_as->release();
      if(p_as->is_prepared()) {
        auto& r = **it;
        DEBUG(typeid(r).name());
        DEBUG((*it)->get_name());
      }
    }
  }
}

void scene_t::add_licenses(licensehandler_t* session)
{
  licensed_component_t::add_licenses(session);
  std::vector<TASCAR::Scene::object_t*> obj(get_objects());
  for(auto it = obj.begin(); it != obj.end(); ++it) {
    // add licenses of all licensed components to global list:
    licensed_component_t* lc(dynamic_cast<licensed_component_t*>(*it));
    if(lc)
      lc->add_licenses(session);
  }
}

rgb_color_t::rgb_color_t(const std::string& webc) : r(0), g(0), b(0)
{
  if((webc.size() == 7) && (webc[0] == '#')) {
    unsigned int c(0);
    sscanf(webc.c_str(), "#%x", &c);
    r = ((c >> 16) & 0xff) / 255.0;
    g = ((c >> 8) & 0xff) / 255.0;
    b = (c & 0xff) / 255.0;
  }
}

std::string rgb_color_t::str()
{
  char ctmp[64];
  ctmp[63] = 0;
  unsigned int c(((unsigned int)(round(r * 255.0)) << 16) +
                 ((unsigned int)(round(g * 255.0)) << 8) +
                 ((unsigned int)(round(b * 255.0))));
  snprintf(ctmp, 63, "#%06x", c);
  return ctmp;
}

void route_t::reset_meters()
{
  rmsmeter.clear();
  meterval.clear();
}

std::string route_t::get_type() const
{
  if(dynamic_cast<const TASCAR::Scene::face_object_t*>(this))
    return "face";
  if(dynamic_cast<const TASCAR::Scene::face_group_t*>(this))
    return "facegroup";
  if(dynamic_cast<const TASCAR::Scene::obstacle_group_t*>(this))
    return "obstacle";
  if(dynamic_cast<const TASCAR::Scene::src_object_t*>(this))
    return "source";
  if(dynamic_cast<const TASCAR::Scene::diff_snd_field_obj_t*>(this))
    return "diffuse";
  if(dynamic_cast<const TASCAR::Scene::receiver_obj_t*>(this))
    return "receiver";
  if(dynamic_cast<const TASCAR::Scene::diffuse_reverb_t*>(this))
    return "reverb";
  return "unknwon";
}

void route_t::addmeter(float fs)
{
  rmsmeter.push_back(new TASCAR::levelmeter_t(fs, meter_tc, meter_weight));
  meterval.push_back(0);
}

void route_t::set_meterweight(TASCAR::levelmeter::weight_t w)
{
  meter_weight = w;
  for(auto m = rmsmeter.begin(); m != rmsmeter.end(); ++m)
    (*m)->set_weight(w);
}

route_t::~route_t()
{
  for(uint32_t k = 0; k < rmsmeter.size(); ++k)
    delete rmsmeter[k];
}

void route_t::configure_meter(float tc, TASCAR::levelmeter::weight_t w)
{
  meter_tc = tc;
  meter_weight = w;
}

const std::vector<float>& route_t::readmeter()
{
  for(uint32_t k = 0; k < rmsmeter.size(); k++)
    meterval[k] = rmsmeter[k]->spldb();
  return meterval;
}

float route_t::read_meter_max()
{
  float rv(std::numeric_limits<float>::lowest());
  for(uint32_t k = 0; k < rmsmeter.size(); k++) {
    float l(rmsmeter[k]->spldb());
    if(!(l < rv))
      rv = l;
  }
  return rv;
}

float sound_t::read_meter()
{
  if(meter.size() > 0)
    if(meter[0])
      return meter[0]->spldb();
  return std::numeric_limits<float>::lowest();
}

uint32_t sound_t::get_num_input_ports()
{
  return n_channels;
}

uint32_t sound_t::get_num_output_ports()
{
  return 0u;
}

route_t::route_t(tsccfg::node_t xmlsrc)
    : xml_element_t(xmlsrc), id(TASCAR::get_tuid()), mute(false), solo(false),
      meter_tc(2), meter_weight(TASCAR::levelmeter::Z), targetlevel(0)
{
  GET_ATTRIBUTE(name, "", "Route name");
  GET_ATTRIBUTE(id, "", "Unique route id, empty to autogenerate");
  TASCAR::validate_tuid(id, xmlsrc);
  GET_ATTRIBUTE_BOOL(mute, "Mute flag of route");
  GET_ATTRIBUTE_BOOL(solo, "Solo flag of route");
}

void route_t::set_solo(bool b, uint32_t& anysolo)
{
  if(b != solo) {
    if(b) {
      anysolo++;
    } else {
      if(anysolo)
        anysolo--;
    }
    solo = b;
  }
}

bool route_t::is_active(uint32_t anysolo)
{
  return (!mute && ((anysolo == 0) || (solo)));
}

bool object_t::is_active(uint32_t anysolo, double t)
{
  return (route_t::is_active(anysolo) && (t >= starttime) &&
          ((t <= endtime) || (endtime <= starttime)));
}

face_object_t::face_object_t(tsccfg::node_t xmlsrc)
    : object_t(xmlsrc), width(1.0), height(1.0)
{
  dynobject_t::GET_ATTRIBUTE(width, "m", "Width of reflector");
  dynobject_t::GET_ATTRIBUTE(height, "m", "Height of reflector");
  reflector_t::read_xml(*(dynobject_t*)this);
  dynobject_t::GET_ATTRIBUTE(
      vertices, "m", "List of Cartesian coordinates to define polygon surface");
  if(vertices.size() > 2)
    nonrt_set(vertices);
  else
    nonrt_set_rect(width, height);
}

face_object_t::~face_object_t() {}

void face_object_t::geometry_update(double t)
{
  dynobject_t::geometry_update(t);
  apply_rot_loc(get_location(), get_orientation());
}

audio_port_t::audio_port_t(tsccfg::node_t xmlsrc, bool is_input_)
    : xml_element_t(xmlsrc), ctlname(""), port_index(0), is_input(is_input_),
      gain(1), caliblevel(1.0)
{
  GET_ATTRIBUTE(connect, "",
                "Regular expressions of port names for connections");
  GET_ATTRIBUTE_DB(gain, "port gain");
  has_caliblevel = has_attribute("caliblevel");
  GET_ATTRIBUTE_DBSPL(caliblevel, "calibration level");
  bool inv(false);
  GET_ATTRIBUTE_BOOL(inv, "phase invert");
  set_inv(inv);
}

void audio_port_t::set_gain_db(float g)
{
  if(gain < 0.0f)
    gain = -pow(10.0, 0.05 * g);
  else
    gain = pow(10.0, 0.05 * g);
}

void audio_port_t::set_gain_lin(float g)
{
  gain = g;
}

void src_object_t::process_active(double t, uint32_t anysolo)
{
  bool a(is_active(anysolo, t));
  for(std::vector<sound_t*>::iterator it = sound.begin(); it != sound.end();
      ++it)
    (*it)->active = a;
}

void diff_snd_field_obj_t::release()
{
  audiostates_t::release();
  if(source)
    source->release();
}

uint32_t diff_snd_field_obj_t::get_num_input_ports()
{
  return n_channels;
}

uint32_t diff_snd_field_obj_t::get_num_output_ports()
{
  return 0;
}

void diff_snd_field_obj_t::process_active(double t, uint32_t anysolo)
{
  bool a(is_active(anysolo, t));
  if(source)
    source->active = a;
}

void face_object_t::process_active(double t, uint32_t anysolo)
{
  active = is_active(anysolo, t);
}

std::string jacknamer(const std::string& prefix, const std::string& name)
{
  if(name.empty()) {
    if(prefix.size())
      return prefix + ".tascar";
    else
      return "tascar";
  }
  if(prefix.size())
    return prefix + "." + name;
  return name;
}

std::vector<TASCAR::Scene::object_t*>
TASCAR::Scene::scene_t::find_object(const std::string& pattern)
{
  std::vector<TASCAR::Scene::object_t*> retv;
  std::vector<TASCAR::Scene::object_t*> objs(get_objects());
  for(std::vector<TASCAR::Scene::object_t*>::iterator it = objs.begin();
      it != objs.end(); ++it)
    if(TASCAR::fnmatch(pattern.c_str(), (*it)->get_name().c_str(), true) == 0)
      retv.push_back(*it);
  return retv;
}

void TASCAR::Scene::scene_t::validate_attributes(std::string& msg) const
{
  xml_element_t::validate_attributes(msg);
  for(auto p : source_objects)
    p->validate_attributes(msg);
  for(auto p : diff_snd_field_objects)
    p->validate_attributes(msg);
  for(auto p : face_objects)
    p->dynobject_t::validate_attributes(msg);
  for(auto p : facegroups)
    p->dynobject_t::validate_attributes(msg);
  for(auto p : obstaclegroups)
    p->dynobject_t::validate_attributes(msg);
  for(auto p : receivermod_objects)
    p->validate_attributes(msg);
  for(auto p : mask_objects)
    p->dynobject_t::validate_attributes(msg);
  for(auto p : diffuse_reverbs)
    p->dynobject_t::validate_attributes(msg);
  for(auto& mat : materials)
    if(mat.second.e)
      mat.second.validate_attributes(msg);
}

face_group_t::face_group_t(tsccfg::node_t xmlsrc) : object_t(xmlsrc)
{
  reflector_t::read_xml(*(dynobject_t*)this);
  dynobject_t::GET_ATTRIBUTE(
      importraw, "",
      "File name of raw file containing list of polygon surfaces");
  dynobject_t::GET_ATTRIBUTE(shoebox, "m",
                             "Generate a shoebox room of these dimensions");
  if(!shoebox.is_null()) {
    TASCAR::pos_t sb(shoebox);
    sb *= 0.5;
    std::vector<TASCAR::pos_t> verts;
    verts.resize(4);
    TASCAR::Acousticmodel::reflector_t* p_reflector(NULL);
    // face1:
    verts[0] = TASCAR::pos_t(sb.x, -sb.y, -sb.z);
    verts[1] = TASCAR::pos_t(sb.x, -sb.y, sb.z);
    verts[2] = TASCAR::pos_t(sb.x, sb.y, sb.z);
    verts[3] = TASCAR::pos_t(sb.x, sb.y, -sb.z);
    p_reflector = new TASCAR::Acousticmodel::reflector_t();
    p_reflector->nonrt_set(verts);
    reflectors.push_back(p_reflector);
    // face2:
    verts[0] = TASCAR::pos_t(-sb.x, -sb.y, -sb.z);
    verts[1] = TASCAR::pos_t(-sb.x, sb.y, -sb.z);
    verts[2] = TASCAR::pos_t(-sb.x, sb.y, sb.z);
    verts[3] = TASCAR::pos_t(-sb.x, -sb.y, sb.z);
    p_reflector = new TASCAR::Acousticmodel::reflector_t();
    p_reflector->nonrt_set(verts);
    reflectors.push_back(p_reflector);
    // face3:
    verts[0] = TASCAR::pos_t(-sb.x, -sb.y, -sb.z);
    verts[1] = TASCAR::pos_t(-sb.x, -sb.y, sb.z);
    verts[2] = TASCAR::pos_t(sb.x, -sb.y, sb.z);
    verts[3] = TASCAR::pos_t(sb.x, -sb.y, -sb.z);
    p_reflector = new TASCAR::Acousticmodel::reflector_t();
    p_reflector->nonrt_set(verts);
    reflectors.push_back(p_reflector);
    // face4:
    verts[0] = TASCAR::pos_t(-sb.x, sb.y, -sb.z);
    verts[1] = TASCAR::pos_t(sb.x, sb.y, -sb.z);
    verts[2] = TASCAR::pos_t(sb.x, sb.y, sb.z);
    verts[3] = TASCAR::pos_t(-sb.x, sb.y, sb.z);
    p_reflector = new TASCAR::Acousticmodel::reflector_t();
    p_reflector->nonrt_set(verts);
    reflectors.push_back(p_reflector);
    // face5:
    verts[0] = TASCAR::pos_t(-sb.x, -sb.y, sb.z);
    verts[1] = TASCAR::pos_t(-sb.x, sb.y, sb.z);
    verts[2] = TASCAR::pos_t(sb.x, sb.y, sb.z);
    verts[3] = TASCAR::pos_t(sb.x, -sb.y, sb.z);
    p_reflector = new TASCAR::Acousticmodel::reflector_t();
    p_reflector->nonrt_set(verts);
    reflectors.push_back(p_reflector);
    // face6:
    verts[0] = TASCAR::pos_t(-sb.x, -sb.y, -sb.z);
    verts[1] = TASCAR::pos_t(sb.x, -sb.y, -sb.z);
    verts[2] = TASCAR::pos_t(sb.x, sb.y, -sb.z);
    verts[3] = TASCAR::pos_t(-sb.x, sb.y, -sb.z);
    p_reflector = new TASCAR::Acousticmodel::reflector_t();
    p_reflector->nonrt_set(verts);
    reflectors.push_back(p_reflector);
  }
  dynobject_t::GET_ATTRIBUTE(shoeboxwalls, "m",
                             "generate shoebox room without floor and ceiling");
  if(!shoeboxwalls.is_null()) {
    TASCAR::pos_t sb(shoeboxwalls);
    sb *= 0.5;
    std::vector<TASCAR::pos_t> verts;
    verts.resize(4);
    TASCAR::Acousticmodel::reflector_t* p_reflector(NULL);
    // face1:
    verts[0] = TASCAR::pos_t(sb.x, -sb.y, -sb.z);
    verts[1] = TASCAR::pos_t(sb.x, -sb.y, sb.z);
    verts[2] = TASCAR::pos_t(sb.x, sb.y, sb.z);
    verts[3] = TASCAR::pos_t(sb.x, sb.y, -sb.z);
    p_reflector = new TASCAR::Acousticmodel::reflector_t();
    p_reflector->nonrt_set(verts);
    reflectors.push_back(p_reflector);
    // face2:
    verts[0] = TASCAR::pos_t(-sb.x, -sb.y, -sb.z);
    verts[1] = TASCAR::pos_t(-sb.x, sb.y, -sb.z);
    verts[2] = TASCAR::pos_t(-sb.x, sb.y, sb.z);
    verts[3] = TASCAR::pos_t(-sb.x, -sb.y, sb.z);
    p_reflector = new TASCAR::Acousticmodel::reflector_t();
    p_reflector->nonrt_set(verts);
    reflectors.push_back(p_reflector);
    // face3:
    verts[0] = TASCAR::pos_t(-sb.x, -sb.y, -sb.z);
    verts[1] = TASCAR::pos_t(-sb.x, -sb.y, sb.z);
    verts[2] = TASCAR::pos_t(sb.x, -sb.y, sb.z);
    verts[3] = TASCAR::pos_t(sb.x, -sb.y, -sb.z);
    p_reflector = new TASCAR::Acousticmodel::reflector_t();
    p_reflector->nonrt_set(verts);
    reflectors.push_back(p_reflector);
    // face4:
    verts[0] = TASCAR::pos_t(-sb.x, sb.y, -sb.z);
    verts[1] = TASCAR::pos_t(sb.x, sb.y, -sb.z);
    verts[2] = TASCAR::pos_t(sb.x, sb.y, sb.z);
    verts[3] = TASCAR::pos_t(-sb.x, sb.y, sb.z);
    p_reflector = new TASCAR::Acousticmodel::reflector_t();
    p_reflector->nonrt_set(verts);
    reflectors.push_back(p_reflector);
  }
  dynobject_t::GET_ATTRIBUTE(
      shoeboxfloor, "m", "generate shoebox room with only the floor surface");
  if(!shoeboxfloor.is_null()) {
    TASCAR::pos_t sb(shoeboxfloor);
    sb *= 0.5;
    std::vector<TASCAR::pos_t> verts;
    verts.resize(4);
    TASCAR::Acousticmodel::reflector_t* p_reflector(NULL);
    // face6:
    verts[0] = TASCAR::pos_t(-sb.x, -sb.y, -sb.z);
    verts[1] = TASCAR::pos_t(sb.x, -sb.y, -sb.z);
    verts[2] = TASCAR::pos_t(sb.x, sb.y, -sb.z);
    verts[3] = TASCAR::pos_t(-sb.x, sb.y, -sb.z);
    p_reflector = new TASCAR::Acousticmodel::reflector_t();
    p_reflector->nonrt_set(verts);
    reflectors.push_back(p_reflector);
  }
  dynobject_t::GET_ATTRIBUTE(
      shoeboxceiling, "m",
      "generate shoebox room with only the ceiling surface");
  if(!shoeboxceiling.is_null()) {
    TASCAR::pos_t sb(shoeboxceiling);
    sb *= 0.5;
    std::vector<TASCAR::pos_t> verts;
    verts.resize(4);
    TASCAR::Acousticmodel::reflector_t* p_reflector(NULL);
    // face5:
    verts[0] = TASCAR::pos_t(-sb.x, -sb.y, sb.z);
    verts[1] = TASCAR::pos_t(-sb.x, sb.y, sb.z);
    verts[2] = TASCAR::pos_t(sb.x, sb.y, sb.z);
    verts[3] = TASCAR::pos_t(sb.x, -sb.y, sb.z);
    p_reflector = new TASCAR::Acousticmodel::reflector_t();
    p_reflector->nonrt_set(verts);
    reflectors.push_back(p_reflector);
  }
  if(!importraw.empty()) {
    std::ifstream rawmesh(TASCAR::env_expand(importraw).c_str());
    if(!rawmesh.good())
      throw TASCAR::ErrMsg("Unable to open mesh file \"" +
                           TASCAR::env_expand(importraw) + "\".");
    while(!rawmesh.eof()) {
      std::string meshline;
      getline(rawmesh, meshline, '\n');
      if(!meshline.empty()) {
        TASCAR::Acousticmodel::reflector_t* p_reflector(
            new TASCAR::Acousticmodel::reflector_t());
        p_reflector->nonrt_set(TASCAR::str2vecpos(meshline));
        reflectors.push_back(p_reflector);
      }
    }
  }
  std::stringstream txtmesh(tsccfg::node_get_text(xmlsrc, "faces"));
  while(!txtmesh.eof()) {
    std::string meshline;
    getline(txtmesh, meshline, '\n');
    if(!meshline.empty()) {
      TASCAR::Acousticmodel::reflector_t* p_reflector(
          new TASCAR::Acousticmodel::reflector_t());
      p_reflector->nonrt_set(TASCAR::str2vecpos(meshline));
      reflectors.push_back(p_reflector);
    }
  }
}

face_group_t::~face_group_t()
{
  for(std::vector<TASCAR::Acousticmodel::reflector_t*>::iterator it =
          reflectors.begin();
      it != reflectors.end(); ++it)
    delete *it;
}

void face_group_t::geometry_update(double t)
{
  dynobject_t::geometry_update(t);
  for(std::vector<TASCAR::Acousticmodel::reflector_t*>::iterator it =
          reflectors.begin();
      it != reflectors.end(); ++it) {
    (*it)->apply_rot_loc(c6dof.position, c6dof.orientation);
    (*it)->reflectivity = reflectivity;
    (*it)->damping = damping;
    (*it)->edgereflection = edgereflection;
    (*it)->scattering = scattering;
    (*it)->layers = layers;
  }
}

void face_group_t::process_active(double t, uint32_t anysolo)
{
  bool a(is_active(anysolo, t));
  for(std::vector<TASCAR::Acousticmodel::reflector_t*>::iterator it =
          reflectors.begin();
      it != reflectors.end(); ++it) {
    (*it)->active = a;
  }
}

// obstacles:
obstacle_group_t::obstacle_group_t(tsccfg::node_t xmlsrc)
    : object_t(xmlsrc), transmission(0), ishole(false), aperture(0)
{
  dynobject_t::GET_ATTRIBUTE(transmission, "", "transmission coefficient");
  dynobject_t::GET_ATTRIBUTE(importraw, "", "file name of vertex list");
  dynobject_t::GET_ATTRIBUTE_BOOL(
      ishole, "Simulate infinite plane with hole instead of finite surface");
  dynobject_t::GET_ATTRIBUTE(aperture, "m",
                             "Override aperture of airy disk calculation, zero "
                             "for calculation from area");
  if(!importraw.empty()) {
    std::ifstream rawmesh(TASCAR::env_expand(importraw).c_str());
    if(!rawmesh.good())
      throw TASCAR::ErrMsg("Unable to open mesh file \"" +
                           TASCAR::env_expand(importraw) + "\".");
    while(!rawmesh.eof()) {
      std::string meshline;
      getline(rawmesh, meshline, '\n');
      if(!meshline.empty()) {
        TASCAR::Acousticmodel::obstacle_t* p_obstacle(
            new TASCAR::Acousticmodel::obstacle_t());
        p_obstacle->nonrt_set(TASCAR::str2vecpos(meshline));
        p_obstacle->b_inner = !ishole;
        p_obstacle->manual_aperture = aperture;
        obstacles.push_back(p_obstacle);
      }
    }
  }
  std::stringstream txtmesh(tsccfg::node_get_text(xmlsrc, "faces"));
  while(!txtmesh.eof()) {
    std::string meshline;
    getline(txtmesh, meshline, '\n');
    if(!meshline.empty()) {
      TASCAR::Acousticmodel::obstacle_t* p_obstacle(
          new TASCAR::Acousticmodel::obstacle_t());
      p_obstacle->nonrt_set(TASCAR::str2vecpos(meshline));
      p_obstacle->b_inner = !ishole;
      p_obstacle->manual_aperture = aperture;
      obstacles.push_back(p_obstacle);
    }
  }
}

obstacle_group_t::~obstacle_group_t()
{
  for(std::vector<TASCAR::Acousticmodel::obstacle_t*>::iterator it =
          obstacles.begin();
      it != obstacles.end(); ++it)
    delete *it;
}

void obstacle_group_t::geometry_update(double t)
{
  dynobject_t::geometry_update(t);
  for(std::vector<TASCAR::Acousticmodel::obstacle_t*>::iterator it =
          obstacles.begin();
      it != obstacles.end(); ++it) {
    (*it)->apply_rot_loc(c6dof.position, c6dof.orientation);
    (*it)->transmission = transmission;
  }
}

void obstacle_group_t::process_active(double t, uint32_t anysolo)
{
  bool a(is_active(anysolo, t));
  for(std::vector<TASCAR::Acousticmodel::obstacle_t*>::iterator it =
          obstacles.begin();
      it != obstacles.end(); ++it) {
    (*it)->active = a;
    (*it)->transmission = transmission;
  }
}

sound_name_t::sound_name_t(tsccfg::node_t xmlsrc, src_object_t* parent_)
    : xml_element_t(xmlsrc), id(TASCAR::get_tuid())
{
  GET_ATTRIBUTE(name, "", "name of sound vertex");
  if(parent_ && name.empty())
    name = parent_->next_sound_name();
  if(name.empty())
    throw TASCAR::ErrMsg("Invalid (empty) sound name.");
  GET_ATTRIBUTE(id, "", "id of sound vertex");
  TASCAR::validate_tuid(id, xmlsrc);
  if(parent_)
    parentname = parent_->get_name();
}

sound_t::sound_t(tsccfg::node_t xmlsrc, src_object_t* parent_)
    : sound_name_t(xmlsrc, parent_),
      source_t(xmlsrc, get_name(), get_parent_name()),
      audio_port_t(xmlsrc, true), parent(parent_), chaindist(0), gain_(1)
{
  if(source_t::has_attribute("az") || source_t::has_attribute("el") ||
     source_t::has_attribute("r")) {
    if(source_t::has_attribute("x") || source_t::has_attribute("y") ||
       source_t::has_attribute("z"))
      TASCAR::add_warning("Relative sound position is specified in cartesian "
                          "and spherical coordinates. Using spherical.",
                          source_t::e);
    double az(0.0);
    double el(0.0);
    double r(1.0);
    source_t::GET_ATTRIBUTE_DEG(az, "azimuth relatve to parent");
    source_t::GET_ATTRIBUTE_DEG(el, "elevation relative to parent");
    source_t::GET_ATTRIBUTE(r, "m", "distance from parent origin");
    local_position.set_sphere(r, az, el);
  } else {
    source_t::get_attribute("x", local_position.x, "m",
                            "position relative to parent");
    source_t::get_attribute("y", local_position.y, "m",
                            "position relative to parent");
    source_t::get_attribute("z", local_position.z, "m",
                            "position relative to parent");
  }
  source_t::get_attribute_deg("rz", local_orientation.z,
                              "Euler orientation (Z) relative to parent");
  source_t::get_attribute_deg("ry", local_orientation.y,
                              "Euler orientation (Y) relative to parent");
  source_t::get_attribute_deg("rx", local_orientation.x,
                              "Euler orientation (X) relative to parent");
  source_t::get_attribute(
      "d", chaindist, "m",
      "distance to next sound along trajectory, or 0 for normal mode");
  source_t::get_attribute_bool("mute", b_mute, "", "mute state");
  // parse plugins:
  for(auto sne : tsccfg::node_get_children(source_t::e)) {
    if(tsccfg::node_get_name(sne) != "plugins") {
      // add_warning:
      TASCAR::add_warning("Ignoring entry \"" + tsccfg::node_get_name(sne) +
                              "\" in sound \"" + get_fullname() + "\".",
                          sne);
    }
  }
}

sound_t::~sound_t() {}

void sound_t::geometry_update(double t)
{
  orientation = local_orientation;
  if(parent)
    orientation += parent->c6dof.orientation;
  if(global_position != position) {
    // this can only happen if global_position was changed
    // externally. Now we need to update local_position based on the
    // parent position and orientation:
    position = global_position;
    local_position = position;
    if(parent) {
      if(chaindist != 0) {
        double tp(t - parent->starttime);
        tp = parent->location.get_time(parent->location.get_dist(tp) -
                                       chaindist);
        local_position -= parent->location.interp(tp);
      } else {
        local_position -= parent->c6dof.position;
      }
      local_position /= parent->c6dof.orientation;
      local_position *= 1.0 / parent->scale;
    }
  } else {
    // update vertex position and orientation from local_orientation
    // and parent geometry variables:
    pos_t rp(local_position);
    if(parent) {
      rp *= parent->scale;
      rp *= parent->c6dof.orientation;
      if(chaindist != 0) {
        double tp(t - parent->starttime);
        tp = parent->location.get_time(parent->location.get_dist(tp) -
                                       chaindist);
        rp += parent->location.interp(tp);
      } else {
        rp += parent->c6dof.position;
      }
    }
    position = rp;
  }
  global_position = position;
}

void sound_t::process_plugins(const TASCAR::transport_t& tp)
{
  TASCAR::transport_t ltp(tp);
  if(parent) {
    ltp.object_time_seconds = ltp.session_time_seconds - parent->starttime;
    ltp.object_time_samples =
        ltp.session_time_samples - f_sample * parent->starttime;
  }
  source_t::process_plugins(ltp);
}

void sound_t::validate_attributes(std::string& msg) const
{
  TASCAR::Acousticmodel::source_t::validate_attributes(msg);
  plugins.validate_attributes(msg);
}

void sound_t::add_meter(TASCAR::levelmeter_t* m)
{
  meter.push_back(m);
}

void sound_t::apply_gain()
{
  float newgain = get_gain();
  if(b_mute)
    newgain = 0.0f;
  float dg((newgain - gain_) * t_inc);
  uint32_t channels(inchannels.size());
  for(uint32_t k = 0; k < inchannels[0].n; ++k) {
    gain_ += dg;
    for(uint32_t c = 0; c < channels; ++c)
      inchannels[c].d[k] *= gain_;
  }
  gain_ = newgain;
  for(uint32_t k = 0; k < n_channels; ++k)
    meter[k]->update(inchannels[k]);
}

rgb_color_t sound_t::get_color() const
{
  if(parent)
    return parent->color;
  else
    return rgb_color_t();
}

diffuse_reverb_defaults_t::diffuse_reverb_defaults_t(tsccfg::node_t e)
{
  xml_element_t el(e);
  std::string name("reverb");
  std::string type("simplefdn");
  TASCAR::pos_t volumetric(3, 4, 5);
  bool diffuse(false);
  double falloff(1.0);
  el.GET_ATTRIBUTE(name, "", "diffuse reverb name");
  el.GET_ATTRIBUTE(type, "", "diffuse reverb type");
  el.GET_ATTRIBUTE(volumetric, "m", "size of diffuse reverberation");
  el.GET_ATTRIBUTE_BOOL(diffuse, "render diffuse input sound fields");
  el.GET_ATTRIBUTE(falloff, "m", "ramp length at boundaries");
}

diffuse_reverb_t::diffuse_reverb_t(tsccfg::node_t e)
    : diffuse_reverb_defaults_t(e), TASCAR::Scene::receiver_obj_t(e, true),
      outputlayers(0xffffffff), source(NULL), plugins(e, get_name(), "")
{
  dynobject_t::GET_ATTRIBUTE_BITS(outputlayers, "output layers");
}

diffuse_reverb_t::~diffuse_reverb_t()
{
  if(source)
    delete source;
}

void diffuse_reverb_t::validate_attributes(std::string& msg) const
{
  dynobject_t::validate_attributes(msg);
  plugins.validate_attributes(msg);
}

void diffuse_reverb_t::configure()
{
  reset_meters();
  receiver_obj_t::configure();
  // is_reverb = true;
  if(n_channels != 4)
    throw TASCAR::ErrMsg("Four channels are required for FOA rendering. Please "
                         "check reverb receiver type.");
  if(source)
    delete source;
  source = NULL;
  addmeter(f_sample);
  source = new TASCAR::Acousticmodel::diffuse_t(
      dynobject_t::e, n_fragment, *(rmsmeter.back()), get_name(), plugins);
  source->size = volumetric;
  source->falloff = 1.0f / std::max(falloff, 1.0e-10f);
  source->prepare(cfg());
  for(uint32_t acn = 0; acn < AMB11ACN::idx::channels; ++acn)
    source->audio[acn].use_external_buffer(outchannels[acn].n,
                                           outchannels[acn].d);
}

void diffuse_reverb_t::post_prepare()
{
  receiver_obj_t::post_prepare();
  source->post_prepare();
}

void diffuse_reverb_t::release()
{
  receiver_obj_t::release();
  if(source) {
    source->release();
    delete source;
  }
  source = NULL;
}

void diffuse_reverb_t::geometry_update(double t)
{
  receiver_obj_t::geometry_update(t);
  if(source) {
    get_6dof(source->center, source->orientation);
    source->layers = outputlayers;
  }
}

void diffuse_reverb_t::process_active(double t, uint32_t anysolo)
{
  receiver_obj_t::process_active(t, anysolo);
  bool a(is_active(anysolo, t));
  if(source)
    source->active = a;
}

void diffuse_reverb_t::add_licenses(licensehandler_t* lh)
{
  receiver_obj_t::add_licenses(lh);
  if(source)
    source->add_licenses(lh);
}

material_t::material_t()
{
  validate();
}
material_t::material_t(tsccfg::node_t e) : TASCAR::xml_element_t(e)
{
  GET_ATTRIBUTE(name, "", "Name of material");
  GET_ATTRIBUTE(f, "Hz", "Frequencies at which alpha is provided");
  GET_ATTRIBUTE(alpha, "", "Absorption coefficients");
  validate();
}

material_t::material_t(const std::string& name, const std::vector<float>& f,
                       const std::vector<float>& alpha)
    : name(name), f(f), alpha(alpha)
{
  validate();
}

void material_t::validate()
{
  if(alpha.empty())
    throw TASCAR::ErrMsg(
        "Invalid alpha coefficients in material definition (empty)");
  if(alpha.size() != f.size())
    throw TASCAR::ErrMsg(
        "Different number of alpha coefficients and frequencies: alpha has " +
        std::to_string(alpha.size()) + " coefficients, freq has " +
        std::to_string(f.size()) + " entries.");
  if(name.empty())
    throw TASCAR::ErrMsg("No name of material provided");
}

material_t::~material_t() {}

void material_t::update_coeff(float fs)
{
  TASCAR::alpha2rflt(reflectivity, damping, alpha, f, fs, 1000);
}

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
