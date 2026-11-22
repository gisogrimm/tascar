/*
 * This file is part of the TASCAR software, see <http://tascar.org/>
 *
 * Copyright (c) 2019 Giso Grimm
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

#include "session.h"
#include <complex>
#include <gtkmm.h>
#include <gtkmm/window.h>
#include <mutex>

class geopresets_t : public TASCAR::actor_module_t {
public:
  geopresets_t(const TASCAR::module_cfg_t& cfg);
  ~geopresets_t();
  void update(uint32_t frame, bool);
  static int osc_setpreset(const char* path, const char* types, lo_arg** argv,
                           int argc, lo_message msg, void* user_data);
  void setpreset(const std::string&);
  static int osc_addposition(const char* path, const char* types, lo_arg** argv,
                             int argc, lo_message msg, void* user_data);
  void addposition(const std::string&, const TASCAR::pos_t& pos);
  static int osc_addorientation(const char* path, const char* types,
                                lo_arg** argv, int argc, lo_message msg,
                                void* user_data);
  void addorientation(const std::string&, const TASCAR::zyx_euler_t& ori);
  void add_to_list(const std::string& preset);

private:
  double time;
  double duration;
  double current_duration;
  std::string startpreset;
  int32_t width;
  int32_t buttonheight;
  bool running;
  std::string id;
  bool enable;
  std::string preset;
  std::map<std::string, TASCAR::pos_t> positions;
  std::map<std::string, TASCAR::zyx_euler_t> orientations;
  std::map<std::string, std::map<std::string, TASCAR::pos_t>> osc_positions;
  std::map<std::string, std::map<std::string, TASCAR::zyx_euler_t>>
      osc_orientations;
  std::vector<std::string> allpresets;
  TASCAR::zyx_euler_t current_r;
  TASCAR::pos_t current_p;
  TASCAR::zyx_euler_t new_r;
  TASCAR::pos_t new_p;
  TASCAR::zyx_euler_t old_r;
  TASCAR::pos_t old_p;
  std::complex<float> phase;
  std::complex<float> d_phase;
  std::mutex mtx;
  bool b_newpos;
  bool showgui;
  bool unlock;
  bool applyonaddition = true;
  Gtk::Window* win;
  Gtk::Box* box;
  std::vector<Gtk::Button*> buttons;
  bool apply_trans;
  bool apply_rot;
  lo_message msg;
  lo_arg** oscmsgargv;
};

int geopresets_t::osc_setpreset(const char*, const char* types, lo_arg** argv,
                                int argc, lo_message, void* user_data)
{
  if(user_data && (argc == 1) && (types[0] == 's'))
    ((geopresets_t*)user_data)->setpreset(&(argv[0]->s));
  return 0;
}

int geopresets_t::osc_addposition(const char*, const char* types, lo_arg** argv,
                                  int argc, lo_message, void* user_data)
{
  if(user_data && (argc == 4) && (types[0] == 's') && (types[1] == 'f') &&
     (types[2] == 'f') && (types[3] == 'f'))
    ((geopresets_t*)user_data)
        ->addposition(&(argv[0]->s),
                      TASCAR::pos_t(argv[1]->f, argv[2]->f, argv[3]->f));
  return 0;
}

void geopresets_t::add_to_list(const std::string& s)
{
  bool is_in_list(false);
  for(auto it = allpresets.begin(); it != allpresets.end(); ++it)
    if(*it == s)
      is_in_list = true;
  if(!is_in_list) {
    allpresets.push_back(s);
    if(showgui) {
      buttons.push_back(new Gtk::Button());
      buttons.back()->set_label(s);
      buttons.back()->signal_pressed().connect(
          sigc::bind(sigc::mem_fun(*this, &geopresets_t::setpreset), s));
      buttons.back()->set_size_request(-1, buttonheight);
      box->add(*(buttons.back()));
      box->show_all();
    }
  }
}

void geopresets_t::addposition(const std::string& s, const TASCAR::pos_t& pos)
{
  mtx.lock();
  positions[s] = pos;
  if(applyonaddition)
    b_newpos = true;
  add_to_list(s);
  mtx.unlock();
}

int geopresets_t::osc_addorientation(const char*, const char* types,
                                     lo_arg** argv, int argc, lo_message,
                                     void* user_data)
{
  if(user_data && (argc == 4) && (types[0] == 's') && (types[1] == 'f') &&
     (types[2] == 'f') && (types[3] == 'f'))
    ((geopresets_t*)user_data)
        ->addorientation(&(argv[0]->s),
                         TASCAR::zyx_euler_t(DEG2RAD * argv[1]->f,
                                             DEG2RAD * argv[2]->f,
                                             DEG2RAD * argv[3]->f));
  return 0;
}

void geopresets_t::addorientation(const std::string& s,
                                  const TASCAR::zyx_euler_t& pos)
{
  mtx.lock();
  orientations[s] = pos;
  if(applyonaddition)
    b_newpos = true;
  add_to_list(s);
  mtx.unlock();
}

void geopresets_t::setpreset(const std::string& s)
{
  mtx.lock();
  preset = s;
  b_newpos = true;
  mtx.unlock();
  // upload OSC:
  auto pos_it(osc_positions.find(s));
  if(pos_it != osc_positions.end()) {
    // preset is available in OSC positions, iterate through all paths:
    for(auto pair : osc_positions[s]) {
      oscmsgargv[0]->f = (float)pair.second.x;
      oscmsgargv[1]->f = (float)pair.second.y;
      oscmsgargv[2]->f = (float)pair.second.z;
      session->dispatch_data_message(pair.first.c_str(), msg);
    }
  }
  auto rot_it(osc_orientations.find(s));
  if(rot_it != osc_orientations.end()) {
    // preset is available in OSC positions, iterate through all paths:
    for(auto pair : osc_orientations[s]) {
      oscmsgargv[0]->f = (float)pair.second.z;
      oscmsgargv[1]->f = (float)pair.second.y;
      oscmsgargv[2]->f = (float)pair.second.x;
      session->dispatch_data_message(pair.first.c_str(), msg);
    }
  }
}

geopresets_t::geopresets_t(const TASCAR::module_cfg_t& cfg)
    : actor_module_t(cfg), time(0), duration(2.0), current_duration(duration),
      width(200), buttonheight(-1), running(false), enable(true), phase(1.0f),
      d_phase(1.0f), b_newpos(false), showgui(false), unlock(false), win(NULL),
      apply_trans(false), apply_rot(false)
{
  msg = lo_message_new();
  lo_message_add_float(msg, 0);
  lo_message_add_float(msg, 0);
  lo_message_add_float(msg, 0);
  oscmsgargv = lo_message_get_argv(msg);
  GET_ATTRIBUTE_(duration);
  GET_ATTRIBUTE_BOOL_(enable);
  GET_ATTRIBUTE_(id);
  GET_ATTRIBUTE_(startpreset);
  GET_ATTRIBUTE_BOOL_(unlock);
  if(id.empty())
    id = "geopresets";
  GET_ATTRIBUTE_BOOL_(showgui);
  GET_ATTRIBUTE_(width);
  GET_ATTRIBUTE_(buttonheight);
  GET_ATTRIBUTE_BOOL(
      applyonaddition,
      "If true then addposition/addorientation trigger a movement");
  for(auto preset : tsccfg::node_get_children(e, "preset")) {
    xml_element_t pres(preset);
    std::string name;
    pres.get_attribute("name", name, "", "undocumented");
    allpresets.push_back(name);
    if(pres.has_attribute("position")) {
      TASCAR::pos_t pos;
      pres.get_attribute("position", pos, "", "undocumented");
      positions[name] = pos;
    }
    if(pres.has_attribute("orientation")) {
      TASCAR::pos_t pos;
      pres.get_attribute("orientation", pos, "", "undocumented");
      pos *= DEG2RAD;
      orientations[name] = TASCAR::zyx_euler_t(pos.x, pos.y, pos.z);
    }
    for(auto presetl : tsccfg::node_get_children(preset, "osc")) {
      xml_element_t pres(presetl);
      std::string path;
      pres.GET_ATTRIBUTE_(path);
      if(!path.empty()) {
        if(pres.has_attribute("pos")) {
          TASCAR::pos_t pos;
          pres.GET_ATTRIBUTE_(pos);
          osc_positions[name][path + "/pos"] = pos;
        }
        if(pres.has_attribute("rot")) {
          TASCAR::pos_t rot;
          pres.GET_ATTRIBUTE_(rot);
          osc_orientations[name][path + "/zyxeuler"] =
              TASCAR::zyx_euler_t(rot.x, rot.y, rot.z);
        }
      }
    }
  }
  session->add_method("/" + id, "s", &geopresets_t::osc_setpreset, this);
  session->add_method("/" + id + "/addposition", "sfff",
                      &geopresets_t::osc_addposition, this);
  session->add_method("/" + id + "/addorientation", "sfff",
                      &geopresets_t::osc_addorientation, this);
  session->add_double("/" + id + "/duration", &duration);
  session->add_bool("/" + id + "/enable", &enable);
  session->add_bool("/" + id + "/unlock", &unlock);
  setpreset(startpreset);
  if(showgui) {
    win = new Gtk::Window();
    win->set_size_request(width, -1);
    box = new Gtk::Box(Gtk::ORIENTATION_VERTICAL);
    win->add(*box);
    for(auto p = allpresets.begin(); p != allpresets.end(); ++p) {
      buttons.push_back(new Gtk::Button());
      buttons.back()->set_label(*p);
      buttons.back()->signal_pressed().connect(
          sigc::bind(sigc::mem_fun(*this, &geopresets_t::setpreset), *p));
      buttons.back()->set_size_request(-1, buttonheight);
      box->add(*(buttons.back()));
    }
    win->show_all();
  }
}

geopresets_t::~geopresets_t()
{
  if(showgui) {
    for(auto p = buttons.begin(); p != buttons.end(); ++p)
      delete(*p);
    delete box;
    delete win;
  }
  lo_message_free(msg);
}

void geopresets_t::update(uint32_t, bool)
{
  if(!enable)
    return;
  if(b_newpos) {
    if(mtx.try_lock()) {
      apply_trans = false;
      apply_rot = false;
      current_duration = duration;
      new_p = old_p = current_p;
      new_r = old_r = current_r;
      auto pos_it(positions.find(preset));
      if(pos_it != positions.end()) {
        apply_trans = true;
        new_p = pos_it->second;
      }
      auto rot_it(orientations.find(preset));
      if(rot_it != orientations.end()) {
        apply_rot = true;
        new_r = rot_it->second;
      }
      const std::complex<double> i(0, 1);
      d_phase = std::exp(i * TASCAR_PI * t_fragment / duration);
      phase = 1.0f;
      b_newpos = false;
      running = true;
      time = 0.0f;
      mtx.unlock();
    }
  }
  if(time > current_duration) {
    time = current_duration;
    phase = -1.0f;
    running = false;
  }
  if(running) {
    time += t_fragment;
    phase *= d_phase;
  }
  // raised-cosine weights:
  float w(0.5f - 0.5f * std::real(phase));
  float w1(1.0f - w);
  // get weighted position:
  current_p.x = w * new_p.x + w1 * old_p.x;
  current_p.y = w * new_p.y + w1 * old_p.y;
  current_p.z = w * new_p.z + w1 * old_p.z;
  // get weighted orientation:
  current_r.x = w * new_r.x + w1 * old_r.x;
  current_r.y = w * new_r.y + w1 * old_r.y;
  current_r.z = w * new_r.z + w1 * old_r.z;
  // apply transformation:
  if((!unlock) || running)
    for(std::vector<TASCAR::named_object_t>::iterator iobj = obj.begin();
        iobj != obj.end(); ++iobj) {
      if(apply_rot)
        iobj->obj->dorientation = current_r;
      if(apply_trans)
        iobj->obj->dlocation = current_p;
    }
  else
    for(std::vector<TASCAR::named_object_t>::iterator iobj = obj.begin();
        iobj != obj.end(); ++iobj) {
      new_r = current_r = iobj->obj->dorientation.to_euler();
      new_p = current_p = iobj->obj->dlocation;
    }
}

REGISTER_MODULE(geopresets_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
