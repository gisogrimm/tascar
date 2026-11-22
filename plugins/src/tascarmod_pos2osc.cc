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

#include "session.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

using namespace std::chrono_literals;

class pos2osc_t : public TASCAR::module_base_t {
public:
  pos2osc_t(const TASCAR::module_cfg_t& cfg);
  ~pos2osc_t();
  void configure();
  void release();
  void update(uint32_t frame, bool running);
  void update_local();

private:
  std::string name = "pos2osc";
  std::string url = "osc.udp://localhost:9999/";
  std::vector<std::string> pattern = {"/*/*"};
  uint32_t mode = 0;
  uint32_t ttl = 1;
  bool transport = true;
  uint32_t skip = 0;
  uint32_t skipcnt = 0;
  std::string avatar;
  double lookatlen = 1.0;
  bool triggered = false;
  bool ignoreorientation = false;
  bool trigger = true;
  bool sendsounds = false;
  bool addparentname = false;
  float taumin = 0.0f;
  float oscale = 1.0f;
  lo_address target;
  std::vector<TASCAR::named_object_t> objects;
  bool bypass = false;
  std::string orientationname = "/headGaze";
  bool threaded = true;
  std::thread thread;
  std::atomic_bool run_thread = true;
  void sendthread();
  std::mutex mtx;
  std::condition_variable cond;
  std::atomic_bool has_data = false;
  TASCAR::tictoc_t tictoc;
};

pos2osc_t::pos2osc_t(const TASCAR::module_cfg_t& cfg) : module_base_t(cfg)
{
  GET_ATTRIBUTE(name, "", "Default name used in OSC variables");
  GET_ATTRIBUTE(url, "", "Target URL");
  GET_ATTRIBUTE(pattern, "",
                "Pattern of TASCAR object names; see actor module "
                "documentation for details.");
  GET_ATTRIBUTE(ttl, "", "Time to live of OSC multicast messages");
  GET_ATTRIBUTE(mode, "", "Message format mode");
  GET_ATTRIBUTE_BOOL(transport, "Send only while transport is rolling");
  GET_ATTRIBUTE(
      avatar, "",
      "Name of object to be controlled (for control of game engines)");
  GET_ATTRIBUTE(lookatlen, "s",
                "Duration of look-at animation (for control of game engines)");
  GET_ATTRIBUTE_BOOL(triggered, "Send data only when triggered via OSC");
  GET_ATTRIBUTE_BOOL(ignoreorientation,
                     "Ignore delta-orientation of source, send zeros instead");
  GET_ATTRIBUTE_BOOL(
      sendsounds, "Send also position of sound vertices (modes 2 and 3 only)");
  GET_ATTRIBUTE_BOOL(
      addparentname,
      "When sending sound vertex positions, add parent name to vertex name");
  GET_ATTRIBUTE(skip, "", "Skip frames to reduce network traffic");
  GET_ATTRIBUTE(oscale, "", "Scaling factor for orientations");
  GET_ATTRIBUTE(orientationname, "", "Name for orientation variables");
  GET_ATTRIBUTE_BOOL(threaded, "Use additional thread for sending data to "
                               "avoid blocking of real-time audio thread");
  GET_ATTRIBUTE(taumin, "s", "Minimum period time between two transmissions.");
  GET_ATTRIBUTE_BOOL(bypass, "Bypass sending of data");
  if(url.empty())
    url = "osc.udp://localhost:9999/";
  target = lo_address_new_from_url(url.c_str());
  if(!target)
    throw TASCAR::ErrMsg("Unable to create target adress \"" + url + "\".");
  lo_address_set_ttl(target, ttl);
  objects = session->find_objects(pattern);
  if(!objects.size())
    throw TASCAR::ErrMsg("No target objects found (target pattern: \"" +
                         TASCAR::vecstr2str(pattern) + "\").");
  std::string path = std::string("/") + name;
  if(avatar.size())
    path += "/" + avatar;
  if(mode == 4) {
    cfg.session->add_bool(path + "/active", &trigger);
    cfg.session->add_double(path + "/lookatlen", &lookatlen);
    cfg.session->add_bool(path + "/triggered", &triggered);
  }
  cfg.session->add_uint(path + "/mode", &mode);
  cfg.session->add_bool(path + "/bypass", &bypass);
  if(triggered) {
    cfg.session->add_bool_true(path + "/trigger", &trigger);
  }
  if(triggered)
    trigger = false;
}

pos2osc_t::~pos2osc_t()
{
  lo_address_free(target);
}

void pos2osc_t::configure()
{
  TASCAR::module_base_t::configure();
  run_thread = true;
  if(threaded)
    thread = std::thread(&pos2osc_t::sendthread, this);
  tictoc.tic();
}

void pos2osc_t::release()
{
  run_thread = false;
  if(threaded)
    thread.join();
  TASCAR::module_base_t::release();
}

void pos2osc_t::update(uint32_t, bool tp_rolling)
{
  if(bypass)
    return;
  if(trigger && ((!triggered && (tp_rolling || (!transport))) || triggered)) {
    if(threaded) {
      if(mtx.try_lock()) {
        has_data = true;
        mtx.unlock();
      }
      cond.notify_one();
    } else {
      update_local();
    }
  }
  if(triggered)
    trigger = false;
}

void pos2osc_t::sendthread()
{
  std::unique_lock<std::mutex> lk(mtx);
  while(run_thread) {
    cond.wait_for(lk, 100ms);
    if(has_data) {
      update_local();
      has_data = false;
    }
  }
}

void pos2osc_t::update_local()
{
  if(skipcnt)
    skipcnt--;
  else {
    skipcnt = skip;
    if((taumin == 0.0f) || (tictoc.toc() >= taumin)) {
      if(taumin > 0.0f)
        tictoc.tic();
      // for(std::vector<TASCAR::named_object_t>::iterator it = obj.begin();
      // it != obj.end(); ++it) {
      for(auto& obj : objects) {
        if(obj.scene->mtx_geometry.try_lock()) {
          // copy position from parent object:
          auto p = obj.obj->c6dof.position;
          auto o = obj.obj->c6dof.orientation;
          TASCAR::rotmat_t rot_delta;
          TASCAR::quaternion_t q_delta;
          TASCAR::quaternion_t q_nodelta;
          // update rotation in local coordinate system:
          rot_delta = obj.obj->dorientation;
          rot_delta *= obj.obj->c6dof_nodelta.orientation;
          q_delta.set_euler_zyx(rot_delta.to_euler());
          q_nodelta.set_euler_zyx(
              obj.obj->c6dof_nodelta.orientation.to_euler());
          q_delta.lmul(q_nodelta.inverse());
          rot_delta = q_delta.to_euler_xyz();
          auto eul_delta = rot_delta.to_euler();
          //
          if(ignoreorientation)
            o = obj.obj->c6dof_nodelta.orientation;
          obj.scene->mtx_geometry.unlock();
          std::string path;
          auto oeul = o.to_euler();
          switch(mode) {
          case 0:
            path = obj.name + "/pos";
            lo_send(target, path.c_str(), "fff", p.x, p.y, p.z);
            path = obj.name + "/rot";
            lo_send(target, path.c_str(), "fff", RAD2DEG * oeul.z * oscale,
                    RAD2DEG * oeul.y * oscale, RAD2DEG * oeul.x * oscale);
            break;
          case 1:
            path = obj.name + "/pos";
            lo_send(target, path.c_str(), "ffffff", p.x, p.y, p.z,
                    RAD2DEG * oeul.z * oscale, RAD2DEG * oeul.y * oscale,
                    RAD2DEG * oeul.x * oscale);
            break;
          case 2:
            path = "/tascarpos";
            lo_send(target, path.c_str(), "sffffff", obj.name.c_str(), p.x, p.y,
                    p.z, RAD2DEG * oeul.z * oscale, RAD2DEG * oeul.y * oscale,
                    RAD2DEG * oeul.x * oscale);
            if(sendsounds) {
              TASCAR::Scene::src_object_t* src(
                  dynamic_cast<TASCAR::Scene::src_object_t*>(obj.obj));
              if(src) {
                std::string parentname(obj.name);
                for(const auto& isnd : src->sound) {
                  std::string soundname;
                  if(addparentname)
                    soundname = parentname + "." + isnd->get_name();
                  else
                    soundname = isnd->get_name();
                  if(obj.scene->mtx_geometry.try_lock()) {
                    auto ipos = isnd->position;
                    auto iori = isnd->orientation.to_euler();
                    obj.scene->mtx_geometry.unlock();
                    lo_send(target, path.c_str(), "sffffff", soundname.c_str(),
                            ipos.x, ipos.y, ipos.z, RAD2DEG * iori.z * oscale,
                            RAD2DEG * iori.y * oscale,
                            RAD2DEG * iori.x * oscale);
                  }
                }
              }
            }
            break;
          case 3:
            path = "/tascarpos";
            lo_send(target, path.c_str(), "sffffff",
                    obj.obj->get_name().c_str(), p.x, p.y, p.z,
                    RAD2DEG * oeul.z * oscale, RAD2DEG * oeul.y * oscale,
                    RAD2DEG * oeul.x * oscale);
            if(sendsounds) {
              TASCAR::Scene::src_object_t* src(
                  dynamic_cast<TASCAR::Scene::src_object_t*>(obj.obj));
              if(src) {
                std::string parentname(obj.obj->get_name());
                for(const auto& isnd : src->sound) {
                  std::string soundname;
                  if(addparentname)
                    soundname = parentname + "." + isnd->get_name();
                  else
                    soundname = isnd->get_name();
                  if(obj.scene->mtx_geometry.try_lock()) {
                    auto ipos = isnd->position;
                    auto iori = isnd->orientation.to_euler();
                    obj.scene->mtx_geometry.unlock();
                    lo_send(target, path.c_str(), "sffffff", soundname.c_str(),
                            ipos.x, ipos.y, ipos.z, RAD2DEG * iori.z * oscale,
                            RAD2DEG * iori.y * oscale,
                            RAD2DEG * iori.x * oscale);
                  }
                }
              }
            }
            break;
          case 4:
            path = "/" + avatar;
            if(lookatlen > 0)
              lo_send(target, path.c_str(), "sffff", "/lookAt", p.x, p.y, p.z,
                      lookatlen);
            else
              lo_send(target, path.c_str(), "sfff", "/lookAt", p.x, p.y, p.z);
            break;
          case 5:
            path = "/" + avatar;
            lo_send(target, path.c_str(), "f", RAD2DEG * oeul.z * oscale);
            break;
          case 6:
            if(avatar.size())
              path = "/" + avatar;
            else
              path = "/" + obj.obj->get_name();
            lo_send(target, path.c_str(), "sfff", orientationname.c_str(),
                    eul_delta.y * oscale, eul_delta.z * oscale,
                    eul_delta.x * oscale);
            break;
          case 7:
            if(avatar.size())
              path = "/" + avatar;
            else
              path = "/" + obj.obj->get_name();
            lo_send(target, path.c_str(), "sfff", orientationname.c_str(),
                    oeul.y * oscale, oeul.z * oscale, oeul.x * oscale);
            break;
          case 8:
            if(avatar.size())
              path = "/" + avatar;
            else
              path = "/" + obj.obj->get_name();
            lo_send(target, path.c_str(), "fff", RAD2DEG * eul_delta.z * oscale,
                    RAD2DEG * eul_delta.y * oscale,
                    RAD2DEG * eul_delta.x * oscale);
            break;
          case 9:
            if(avatar.size())
              path = "/" + avatar;
            else
              path = "/" + obj.obj->get_name();
            lo_send(target, path.c_str(), "sfff", orientationname.c_str(),
                    eul_delta.x * oscale, eul_delta.y * oscale,
                    eul_delta.z * oscale);
            break;
          case 10:
            if(avatar.size())
              path = "/" + avatar;
            else
              path = "/" + obj.obj->get_name();
            {
              auto leul = obj.obj->dorientation.to_euler();
              lo_send(target, path.c_str(), "sfff", orientationname.c_str(),
                      leul.y * oscale, leul.x * oscale, leul.z * oscale);
            }
            break;
          case 11:
            path = "/" + avatar + "/" + obj.obj->get_name();
            lo_send(target, path.c_str(), "ffffff", p.x, p.y, p.z,
                    RAD2DEG * oeul.z * oscale, RAD2DEG * oeul.y * oscale,
                    RAD2DEG * oeul.x * oscale);
            break;
          case 12:
            path = "/" + avatar;
            TASCAR::pos_t p(0, 0, 1);
            p *= o;
            lo_send(target, path.c_str(), "f", (float)(1.0 - p.z));
            break;
          }
        }
      }
    }
  }
}

REGISTER_MODULE(pos2osc_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
