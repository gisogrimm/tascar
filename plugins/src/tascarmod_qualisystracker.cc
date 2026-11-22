/*
 * This file is part of the TASCAR software, see <http://tascar.org/>
 *
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
#include <mutex>
#include <sys/time.h>

using namespace TASCAR;

double gettime()
{
  struct timeval tv;
  memset(&tv, 0, sizeof(timeval));
  gettimeofday(&tv, NULL);
  return (double)(tv.tv_sec) + 0.000001 * tv.tv_usec;
}

#define OSCHANDLER(classname, methodname)                                      \
  static int methodname(const char* path, const char* types, lo_arg** argv,    \
                        int argc, lo_message msg, void* user_data)             \
  {                                                                            \
    return ((classname*)(user_data))                                           \
        ->methodname(path, types, argv, argc, msg);                            \
  }                                                                            \
  int methodname(const char* path, const char* types, lo_arg** argv, int argc, \
                 lo_message msg)

class qualisys_tracker_t : public TASCAR::actor_module_t {
public:
  qualisys_tracker_t(const TASCAR::module_cfg_t& cfg);
  ~qualisys_tracker_t();
  void add_variables(TASCAR::osc_server_t* srv);
  void configure();
  void release();
  void update(uint32_t frame, bool running);
  OSCHANDLER(qualisys_tracker_t, qtmres);
  OSCHANDLER(qualisys_tracker_t, qtmxml);
  OSCHANDLER(qualisys_tracker_t, qtm6d);

private:
  std::string qtmurl;
  double timeout;
  std::vector<double> influence;
  bool local;
  bool incremental;
  lo_address qtmtarget;
  std::mutex mtx;
  int32_t srv_port;
  std::string rigid;
  double last_prepared;
  TASCAR::c6dof_t transform;
};

int qualisys_tracker_t::qtmres(const char*, const char*, lo_arg**, int,
                               lo_message)
{
  return 0;
}

int qualisys_tracker_t::qtmxml(const char*, const char*, lo_arg**, int,
                               lo_message)
{
  lo_send(qtmtarget, "/qtm", "sss", "StreamFrames", "AllFrames", "6DEuler");
  return 0;
}

int qualisys_tracker_t::qtm6d(const char* path, const char*, lo_arg** argv, int,
                              lo_message)
{
  if(strncmp(path, "/qtm/6d_euler/", 14) == 0) {
    std::lock_guard<std::mutex> lock(mtx);
    std::string sentrigid(&(path[14]));
    if(rigid == sentrigid) {
      transform.position.x = 0.001 * influence[0] * (argv[0]->f);
      transform.position.y = 0.001 * influence[1] * (argv[1]->f);
      transform.position.z = 0.001 * influence[2] * (argv[2]->f);
      TASCAR::zyx_euler_t eul;
      eul.z = DEG2RAD * influence[3] * (argv[3]->f);
      eul.y = DEG2RAD * influence[4] * (argv[4]->f);
      eul.x = DEG2RAD * influence[5] * (argv[5]->f);
      transform.orientation = eul;
    }
  }
  return 0;
}

qualisys_tracker_t::qualisys_tracker_t(const TASCAR::module_cfg_t& cfg)
    : TASCAR::actor_module_t(cfg), qtmurl("osc.udp://localhost:22225"),
      timeout(1.0), local(false), incremental(false), srv_port(0),
      last_prepared(gettime())
{
  GET_ATTRIBUTE_(qtmurl);
  GET_ATTRIBUTE_(timeout);
  GET_ATTRIBUTE_(influence);
  GET_ATTRIBUTE_BOOL_(local);
  GET_ATTRIBUTE_BOOL_(incremental);
  GET_ATTRIBUTE_(rigid);
  for(uint32_t k = influence.size(); k < 6; ++k)
    influence.push_back(1.0);
  qtmtarget = lo_address_new_from_url(qtmurl.c_str());
  add_variables(session);
}

void qualisys_tracker_t::update(uint32_t, bool)
{
  if(incremental)
    add_transformation(transform, local);
  else
    set_transformation(transform, local);
}

void qualisys_tracker_t::configure()
{
  std::lock_guard<std::mutex> lock(mtx);
  last_prepared = gettime();
  lo_send(qtmtarget, "/qtm", "si", "Connect", srv_port);
  lo_send(qtmtarget, "/qtm", "ss", "GetParameters", "All");
}

void qualisys_tracker_t::release()
{
  std::lock_guard<std::mutex> lock(mtx);
  lo_send(qtmtarget, "/qtm", "s", "Disconnect");
}

qualisys_tracker_t::~qualisys_tracker_t()
{
  lo_address_free(qtmtarget);
}

void qualisys_tracker_t::add_variables(TASCAR::osc_server_t* srv)
{
  srv_port = srv->get_srv_port();
  srv->set_prefix("");
  srv->add_method("/qtm/cmd_res", "s", &qtmres, this);
  srv->add_method("/qtm/xml", "s", &qtmxml, this);
  srv->add_method("", "ffffff", &qtm6d, this);
}

REGISTER_MODULE(qualisys_tracker_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
