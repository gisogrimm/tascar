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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 3 for more details.
 *
 * You should have received a copy of the GNU General Public License,
 * Version 3 along with TASCAR. If not, see <http://www.gnu.org/licenses/>.
 */

#include "session.h"
#include "tictoctimer.h"
#include <thread>

class osclist_t : public TASCAR::module_base_t {
public:
  osclist_t(const TASCAR::module_cfg_t& cfg);
  ~osclist_t();
  static int osc_recv(const char* path, const char* types, lo_arg** argv,
                      int argc, lo_message msg, void* user_data);
  int osc_recv(const char* path, const char* fmt);
  std::string get_state_json();

private:
  void service();
  std::thread srv;
  std::atomic_bool run_service = true;
  std::map<std::string, TASCAR::tictoc_t> list;
  std::mutex mtx;
  float timeout = 60.0;
};

int osclist_t::osc_recv(const char* path, const char* fmt, lo_arg**, int,
                        lo_message, void* user_data)
{
  return ((osclist_t*)user_data)->osc_recv(path, fmt);
}

int osclist_t::osc_recv(const char* path, const char* fmt)
{
  std::string fpath = path;
  fpath += " ,";
  fpath += fmt;
  {
    std::lock_guard<std::mutex> lock(mtx);
    if(list.find(fpath) == list.end()) {
      std::cout << fpath << std::endl;
    }
    list[fpath].tic();
  }
  return 1;
}

std::string osclist_t::get_state_json()
{
  std::lock_guard<std::mutex> lock(mtx);
  std::string rv = "{";
  for(auto& osc : list)
    rv += "\"" + osc.first + "\":" + TASCAR::to_string(osc.second.toc()) + ",";
  if(rv[rv.size() - 1] == ',')
    rv.erase(rv.size() - 1);
  rv += "}";
  return rv;
}

osclist_t::osclist_t(const TASCAR::module_cfg_t& cfg) : module_base_t(cfg)
{
  GET_ATTRIBUTE(timeout, "s", "timeout after which messages are discarded");
  session->add_method("", NULL, &osclist_t::osc_recv, this);
  srv = std::thread(&osclist_t::service, this);
}

osclist_t::~osclist_t()
{
  run_service = false;
  srv.join();
}

void osclist_t::service()
{
  uint32_t cnt = 0;
  while(run_service) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    if(cnt == 0) {
      cnt = 1000;
      {
        std::lock_guard<std::mutex> lock(mtx);
        for(auto it = list.begin(); it != list.end();) {
          if(it->second.toc() >= timeout)
            it = list.erase(it);
          else
            ++it;
        }
      }
    } else {
      --cnt;
    }
  }
}

REGISTER_MODULE(osclist_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
