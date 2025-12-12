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

#include "osc_scene.h"
#include "errorhandling.h"

using namespace TASCAR;
using namespace TASCAR::Scene;

int osc_set_sound_position(const char*, const char* types, lo_arg** argv,
                           int argc, lo_message, void* user_data)
{
  TASCAR::Scene::sound_t* h((TASCAR::Scene::sound_t*)user_data);
  if(h && (argc == 3) && (types[0] == 'f') && (types[1] == 'f') &&
     (types[2] == 'f')) {
    pos_t r;
    r.x = argv[0]->f;
    r.y = argv[1]->f;
    r.z = argv[2]->f;
    h->local_position = r;
    return 0;
  }
  return 1;
}

int osc_set_sound_orientation(const char*, const char* types, lo_arg** argv,
                              int argc, lo_message, void* user_data)
{
  TASCAR::Scene::sound_t* h((TASCAR::Scene::sound_t*)user_data);
  if(h && (argc == 3) && (types[0] == 'f') && (types[1] == 'f') &&
     (types[2] == 'f')) {
    zyx_euler_t r;
    r.z = DEG2RAD * argv[0]->f;
    r.y = DEG2RAD * argv[1]->f;
    r.x = DEG2RAD * argv[2]->f;
    h->local_orientation = r;
    return 0;
  }
  if(h && (argc == 1) && (types[0] == 'f')) {
    zyx_euler_t r;
    r.z = DEG2RAD * argv[0]->f;
    h->local_orientation = r;
    return 0;
  }
  return 1;
}

int osc_set_object_position(const char*, const char* types, lo_arg** argv,
                            int argc, lo_message, void* user_data)
{
  TASCAR::Scene::object_t* h((TASCAR::Scene::object_t*)user_data);
  if(h && (argc == 3) && (types[0] == 'f') && (types[1] == 'f') &&
     (types[2] == 'f')) {
    pos_t r;
    r.x = argv[0]->f;
    r.y = argv[1]->f;
    r.z = argv[2]->f;
    h->dlocation = r;
    return 0;
  }
  if(h && (argc == 6) && (types[0] == 'f') && (types[1] == 'f') &&
     (types[2] == 'f') && (types[3] == 'f') && (types[4] == 'f') &&
     (types[5] == 'f')) {
    pos_t rp;
    rp.x = argv[0]->f;
    rp.y = argv[1]->f;
    rp.z = argv[2]->f;
    h->dlocation = rp;
    zyx_euler_t ro;
    ro.z = DEG2RAD * argv[3]->f;
    ro.y = DEG2RAD * argv[4]->f;
    ro.x = DEG2RAD * argv[5]->f;
    h->dorientation = ro;
    return 0;
  }
  return 1;
}

int osc_set_object_orientation(const char*, const char* types, lo_arg** argv,
                               int argc, lo_message, void* user_data)
{
  TASCAR::Scene::object_t* h((TASCAR::Scene::object_t*)user_data);
  if(h && (argc == 3) && (types[0] == 'f') && (types[1] == 'f') &&
     (types[2] == 'f')) {
    zyx_euler_t r;
    r.z = DEG2RAD * argv[0]->f;
    r.y = DEG2RAD * argv[1]->f;
    r.x = DEG2RAD * argv[2]->f;
    h->dorientation = r;
    return 0;
  }
  if(h && (argc == 1) && (types[0] == 'f')) {
    zyx_euler_t r;
    r.z = DEG2RAD * argv[0]->f;
    h->dorientation = r;
    return 0;
  }
  return 1;
}

int osc_route_solo(const char*, const char* types, lo_arg** argv, int argc,
                   lo_message, void* user_data)
{
  route_solo_p_t* h((route_solo_p_t*)user_data);
  if(h && (argc == 1) && (types[0] == 'i')) {
    h->route->set_solo(argv[0]->i, *(h->anysolo));
    return 0;
  }
  return 1;
}

int osc_set_sound_gain(const char*, const char* types, lo_arg** argv, int argc,
                       lo_message, void* user_data)
{
  sound_t* h((sound_t*)user_data);
  if(h && (argc == 1) && (types[0] == 'f')) {
    h->set_gain_db(argv[0]->f);
    return 0;
  }
  return 1;
}

int osc_set_sound_gain_lin(const char*, const char* types, lo_arg** argv,
                           int argc, lo_message, void* user_data)
{
  sound_t* h((sound_t*)user_data);
  if(h && (argc == 1) && (types[0] == 'f')) {
    h->set_gain_lin(argv[0]->f);
    return 0;
  }
  return 1;
}

int osc_set_diffuse_gain(const char*, const char* types, lo_arg** argv,
                         int argc, lo_message, void* user_data)
{
  diff_snd_field_obj_t* h((diff_snd_field_obj_t*)user_data);
  if(h && (argc == 1) && (types[0] == 'f')) {
    h->set_gain_db(argv[0]->f);
    return 0;
  }
  return 1;
}

int osc_set_diffuse_gain_lin(const char*, const char* types, lo_arg** argv,
                             int argc, lo_message, void* user_data)
{
  diff_snd_field_obj_t* h((diff_snd_field_obj_t*)user_data);
  if(h && (argc == 1) && (types[0] == 'f')) {
    h->set_gain_lin(argv[0]->f);
    return 0;
  }
  return 1;
}

int osc_set_receiver_gain(const char*, const char* types, lo_arg** argv,
                          int argc, lo_message, void* user_data)
{
  receiver_obj_t* h((receiver_obj_t*)user_data);
  if(h && (argc == 1) && (types[0] == 'f')) {
    h->set_gain_db(argv[0]->f);
    return 0;
  }
  return 1;
}

int osc_set_receiver_lingain(const char*, const char* types, lo_arg** argv,
                             int argc, lo_message, void* user_data)
{
  receiver_obj_t* h((receiver_obj_t*)user_data);
  if(h && (argc == 1) && (types[0] == 'f')) {
    h->set_gain_lin(argv[0]->f);
    return 0;
  }
  return 1;
}

int osc_set_receiver_fade(const char*, const char* types, lo_arg** argv,
                          int argc, lo_message, void* user_data)
{
  receiver_obj_t* h((receiver_obj_t*)user_data);
  if(h && (argc == 2) && (types[0] == 'f') && (types[1] == 'f')) {
    h->set_fade(argv[0]->f, argv[1]->f);
    return 0;
  }
  if(h && (argc == 3) && (types[0] == 'f') && (types[1] == 'f') &&
     (types[2] == 'f')) {
    h->set_fade(argv[0]->f, argv[1]->f, argv[2]->f);
    return 0;
  }
  return 1;
}

void osc_scene_t::add_object_methods(TASCAR::osc_server_t* srv,
                                     TASCAR::Scene::object_t* o)
{
  std::string oldpref(srv->get_prefix());
  std::string ctlname = "/" + scene->name + "/" + o->get_name();
  srv->set_prefix(ctlname);
  srv->set_variable_owner("object_t");
  srv->add_method("/pos", "fff", osc_set_object_position, o, true, false, "",
                  "XYZ Translation in m");
  srv->add_method("/pos", "ffffff", osc_set_object_position, o, true, false, "",
                  "XYZ Translation in m and ZYX Euler angles in degree");
  srv->add_method("/zyxeuler", "fff", osc_set_object_orientation, o, true,
                  false, "", "ZYX Euler angles in degree");
  srv->add_float("/scale", &(o->scale), "", "object scale");
  srv->set_prefix(oldpref);
  srv->unset_variable_owner();
}

void osc_scene_t::add_face_object_methods(TASCAR::osc_server_t* srv,
                                          TASCAR::Scene::face_object_t* o)
{
  std::string oldpref(srv->get_prefix());
  std::string ctlname = "/" + scene->name + "/" + o->get_name();
  srv->set_prefix(ctlname);
  srv->set_variable_owner("face_t");
  srv->add_float("/reflectivity", &(o->reflectivity), "[0,1]",
                 "Reflectivity of object");
  srv->add_float("/damping", &(o->damping), "[0,1[", "Damping coefficient");
  srv->add_float("/scattering", &(o->scattering), "[0,1]",
                 "Scattering coefficient");
  srv->add_uint("/layers", &(o->layers), "",
                "Number representing the layers. Each layer is represented by "
                "a bit, i.e., for layers 1+3 use 10");
  srv->set_prefix(oldpref);
  srv->unset_variable_owner();
}

void osc_scene_t::add_face_group_methods(TASCAR::osc_server_t* srv,
                                         TASCAR::Scene::face_group_t* o)
{
  std::string oldpref(srv->get_prefix());
  std::string ctlname = "/" + scene->name + "/" + o->get_name();
  srv->set_prefix(ctlname);
  srv->set_variable_owner("face_t");
  srv->add_float("/reflectivity", &(o->reflectivity), "[0,1]",
                 "Reflectivity of object");
  srv->add_float("/damping", &(o->damping), "[0,1[", "Damping coefficient");
  srv->add_float("/scattering", &(o->scattering), "[0,1]",
                 "Scattering coefficient");
  srv->add_uint("/layers", &(o->layers), "",
                "Number representing the layers. Each layer is represented by "
                "a bit, i.e., for layers 1+3 use 10");
  srv->set_prefix(oldpref);
  srv->unset_variable_owner();
}

void osc_scene_t::add_route_methods(TASCAR::osc_server_t* srv,
                                    TASCAR::Scene::route_t* o)
{
  route_solo_p_t* rs(new route_solo_p_t);
  rs->route = o;
  rs->anysolo = &(scene->anysolo);
  vprs.push_back(rs);
  std::string oldpref(srv->get_prefix());
  std::string ctlname = "/" + scene->name + "/" + o->get_name();
  srv->set_prefix(ctlname);
  srv->set_variable_owner("route_t");
  srv->add_bool("/mute", &(o->mute), "mute flag, 1 = muted, 0 = unmuted");
  srv->add_method("/solo", "i", osc_route_solo, rs);
  srv->add_float("/targetlevel", &o->targetlevel, "dB",
                 "Indicator position in level meter display");
  srv->set_prefix(oldpref);
  srv->unset_variable_owner();
}

void osc_scene_t::add_sound_methods(TASCAR::osc_server_t* srv,
                                    TASCAR::Scene::sound_t* s)
{
  std::string oldpref(srv->get_prefix());
  std::string ctlname("/" + scene->name + "/" + s->get_parent_name() + "/" +
                      s->get_name());
  srv->set_prefix(ctlname);
  s->set_ctlname(ctlname);
  srv->set_variable_owner("sound_t");
  srv->add_float("/lingain", &(s->gain), "", "Linear gain");
  srv->add_float_db("/gain", &(s->gain), "", "Gain in dB");
  srv->add_float_dbspl("/caliblevel", &(s->caliblevel), "",
                       "calibration level in dB");
  srv->add_uint("/ismmin", &(s->ismmin), "",
                "Minimal Image Source Model order");
  srv->add_uint("/ismmax", &(s->ismmax), "",
                "Maximal Image Source Model order");
  srv->add_uint("/layers", &(s->layers), "",
                "Number representing the layers. Each layer is represented by "
                "a bit, i.e., for layers 1+3 use 10");
  srv->add_float("/size", &(s->size), "", "Object size in meter");
  srv->add_bool("/mute", &(s->b_mute),
                "Mute state of individual sound, independent of parent");
  s->plugins.add_variables(srv);
  srv->add_pos("/pos", &(s->local_position), "",
               "local position of sound vertex in meters");
  srv->add_pos("/globalpos", &(s->global_position), "",
               "global position of sound vertex in meters");
  srv->add_method("/zyxeuler", "fff", osc_set_sound_orientation, s, true, false,
                  "", "ZYX orientation of the sound vertex, in degree");
  srv->add_method("/zeuler", "f", osc_set_sound_orientation, s, true, false, "",
                  "Z orientation of the sound vertex, in degree");
  srv->set_prefix(oldpref);
  srv->unset_variable_owner();
}

void osc_scene_t::add_diffuse_methods(TASCAR::osc_server_t* srv,
                                      TASCAR::Scene::diff_snd_field_obj_t* s)
{
  std::string oldpref(srv->get_prefix());
  std::string ctlname("/" + scene->name + "/" + s->object_t::get_name());
  srv->set_prefix(ctlname);
  s->set_ctlname(ctlname);
  srv->add_method("/gain", "f", osc_set_diffuse_gain, s);
  srv->add_method("/lingain", "f", osc_set_diffuse_gain_lin, s);
  srv->add_float_dbspl("/caliblevel", &(s->caliblevel));
  srv->add_uint("/layers", &(s->layers));
  s->plugins.add_variables(srv);
  srv->set_prefix(oldpref);
}

void osc_scene_t::add_receiver_methods(TASCAR::osc_server_t* srv,
                                       TASCAR::Scene::receiver_obj_t* s)
{
  std::string ctlname("/" + scene->name + "/" + s->object_t::get_name());
  s->set_ctlname(ctlname);
  std::string oldpref(srv->get_prefix());
  srv->set_prefix(ctlname);
  srv->set_variable_owner("receiver_t");
  srv->add_method("/gain", "f", osc_set_receiver_gain, s);
  srv->add_method("/lingain", "f", osc_set_receiver_lingain, s);
  srv->add_float_db("/diffusegain", &(s->diffusegain), "[-30,30]",
                    "relative gain of diffuse sound field model");
  srv->add_method("/fade", "ff", osc_set_receiver_fade, s);
  srv->add_method("/fade", "fff", osc_set_receiver_fade, s);
  srv->add_uint("/ismmin", &(s->ismmin));
  srv->add_uint("/ismmax", &(s->ismmax));
  srv->add_uint("/layers", &(s->layers));
  srv->add_float_dbspl("/caliblevel", &(s->caliblevel));
  srv->unset_variable_owner();
  s->add_variables(srv);
  srv->set_prefix(oldpref);
}

void osc_scene_t::add_child_methods(TASCAR::osc_server_t* srv)
{
  std::string ctlname("/" + scene->name);
  std::string oldpref(srv->get_prefix());
  srv->set_prefix(ctlname);
  srv->add_bool("/active", &(scene->active));
  srv->set_prefix(oldpref);
  std::vector<object_t*> obj(scene->get_objects());
  for(std::vector<object_t*>::iterator it = obj.begin(); it != obj.end();
      ++it) {
    if(TASCAR::Scene::face_object_t* po =
           dynamic_cast<TASCAR::Scene::face_object_t*>(*it))
      add_face_object_methods(srv, po);
    if(TASCAR::Scene::face_group_t* po =
           dynamic_cast<TASCAR::Scene::face_group_t*>(*it))
      add_face_group_methods(srv, po);
    if(TASCAR::Scene::diff_snd_field_obj_t* po =
           dynamic_cast<TASCAR::Scene::diff_snd_field_obj_t*>(*it))
      add_diffuse_methods(srv, po);
    if(TASCAR::Scene::receiver_obj_t* po =
           dynamic_cast<TASCAR::Scene::receiver_obj_t*>(*it))
      add_receiver_methods(srv, po);
    add_object_methods(srv, *it);
    add_route_methods(srv, *it);
  }
  for(std::vector<sound_t*>::iterator it = scene->sounds.begin();
      it != scene->sounds.end(); ++it) {
    add_sound_methods(srv, *it);
  }
}

osc_scene_t::osc_scene_t(tsccfg::node_t, TASCAR::render_core_t* scene_)
    : scene(scene_)
{
  if(!scene)
    throw TASCAR::ErrMsg("Invalid scene pointer");
}

osc_scene_t::~osc_scene_t()
{
  for(std::vector<route_solo_p_t*>::iterator it = vprs.begin();
      it != vprs.end(); ++it)
    delete *it;
}

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
