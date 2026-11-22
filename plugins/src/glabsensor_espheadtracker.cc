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

#include "glabsensorplugin.h"
#include <lsl_cpp.h>
#include <mutex>
#include <sys/time.h>
#include <thread>

using namespace TASCAR;

double gettime()
{
  struct timeval tv;
  memset(&tv, 0, sizeof(timeval));
  gettimeofday(&tv, NULL);
  return (double)(tv.tv_sec) + 0.000001 * tv.tv_usec;
}

class espdevice_t {
public:
  espdevice_t(const std::string& url, uint32_t caliblen_);
  ~espdevice_t();
  void push_eog(lo_arg** argv);
  void push_acceleration(lo_arg** argv);
  void push_rot(lo_arg** argv);
  void push_temp(lo_arg** argv);
  void send_calib();
  double get_temp() const { return data_temp[0]; };
  double get_eog() const { return data_eog[0]; };
  double get_rot(uint32_t k) const { return data_rot[std::min(k, 2u)]; };
  double last_call;

private:
  lo_address target;
  double dt;
  lsl::stream_info inf_eog;
  lsl::stream_info inf_acceleration;
  lsl::stream_info inf_rot;
  lsl::stream_info inf_temp;
  lsl::stream_info inf_delta;
  lsl::stream_outlet str_eog;
  lsl::stream_outlet str_acceleration;
  lsl::stream_outlet str_rot;
  lsl::stream_outlet str_temp;
  lsl::stream_outlet str_delta;
  std::vector<double> data_eog;
  std::vector<double> data_acceleration;
  std::vector<double> data_rot;
  std::vector<double> data_temp;
  std::vector<double> data_delta;
  uint32_t caliblen;
};

espdevice_t::espdevice_t(const std::string& url, uint32_t caliblen_)
    : last_call(gettime()), target(lo_address_new(url.c_str(), "9999")),
      dt(HUGE_VAL),
      inf_eog("esp_eog", "Gaze", 1, 100.0 / 3.0, lsl::cf_float32, url),
      inf_acceleration("esp_acceleration", "Motion", 3, 100.0 / 3.0,
                       lsl::cf_float32, url),
      inf_rot("esp_rot", "Motion", 3, 100.0 / 3.0, lsl::cf_float32, url),
      inf_temp("esp_temperature", "Temperature", 1, 100.0 / 3.0,
               lsl::cf_float32, url),
      inf_delta("esp_clock_delta", "Time", 1, 100.0 / 3.0, lsl::cf_float32,
                url),
      str_eog(inf_eog), str_acceleration(inf_acceleration), str_rot(inf_rot),
      str_temp(inf_temp), str_delta(inf_delta), data_eog(1, 0),
      data_acceleration(3, 0), data_rot(3, 0), data_temp(1, 0),
      data_delta(1, 0), caliblen(caliblen_)
{
  // DEBUG(this);
}

espdevice_t::~espdevice_t()
{
  lo_address_free(target);
  // DEBUG(this);
}

void espdevice_t::send_calib()
{
  lo_send(target, "/calib", "i", caliblen);
}

void espdevice_t::push_eog(lo_arg** argv)
{
  last_call = gettime();
  double lsl_dt(lsl::local_clock() - argv[0]->d);
  if(dt == HUGE_VAL)
    dt = lsl_dt;
  data_eog[0] = argv[1]->d;
  str_eog.push_sample(data_eog, argv[0]->d + dt);
  data_delta[0] = lsl_dt - dt;
  str_delta.push_sample(data_delta, argv[0]->d + dt);
}

void espdevice_t::push_acceleration(lo_arg** argv)
{
  data_acceleration[0] = argv[1]->d;
  data_acceleration[1] = argv[2]->d;
  data_acceleration[2] = argv[3]->d;
  str_acceleration.push_sample(data_acceleration, argv[0]->d + dt);
}

void espdevice_t::push_rot(lo_arg** argv)
{
  data_rot[0] = argv[1]->d;
  data_rot[1] = argv[2]->d;
  data_rot[2] = argv[3]->d;
  str_rot.push_sample(data_rot, argv[0]->d + dt);
}

void espdevice_t::push_temp(lo_arg** argv)
{
  data_temp[0] = argv[1]->d;
  str_temp.push_sample(data_temp, argv[0]->d + dt);
}

class espheadtracker_t : public sensorplugin_drawing_t {
public:
  espheadtracker_t(const TASCAR::sensorplugin_cfg_t& cfg);
  ~espheadtracker_t();
  void add_variables(TASCAR::osc_server_t* srv);
  void draw(const Cairo::RefPtr<Cairo::Context>& cr, double width,
            double height);
  void set_eog(lo_message msg, lo_arg** argv);
  void set_acceleration(lo_message msg, lo_arg** argv);
  void set_rot(lo_message msg, lo_arg** argv);
  void set_temp(lo_message msg, lo_arg** argv);
  Gtk::Widget& get_gtkframe() { return hbox; };
  void on_click_calib();

private:
  espdevice_t* getdev(lo_message msg);
  // list of outlets, one for each source IP address:
  std::map<std::string, espdevice_t*> devices;
  std::mutex devmtx;
  Gtk::HBox hbox;
  Gtk::HBox vbox;
  Gtk::Button b_calib;
  std::thread srv;
  bool run_service;
  void service();
  double timeout;
  uint32_t caliblen;
};

espdevice_t* espheadtracker_t::getdev(lo_message msg)
{
  lo_address srcaddr(lo_message_get_source(msg));
  std::string host(lo_address_get_hostname(srcaddr));
  // DEBUG(host);
  espdevice_t* dev(NULL);
  std::lock_guard<std::mutex> lock(devmtx);
  if(devices.find(host) == devices.end()) {
    std::cerr << "new ESP sensor at " << host << std::endl;
    dev = new espdevice_t(host, caliblen);
    devices[host] = dev;
  } else {
    dev = devices[host];
  }
  return dev;
}

int osc_acceleration(const char*, const char* types, lo_arg** argv, int argc,
                     lo_message msg, void* user_data)
{
  if(user_data && (argc == 4) && (types[0] == 'd') && (types[1] == 'd') &&
     (types[2] == 'd') && (types[3] == 'd'))
    ((espheadtracker_t*)user_data)->set_acceleration(msg, argv);
  return 0;
}

void espheadtracker_t::set_acceleration(lo_message msg, lo_arg** argv)
{
  getdev(msg)->push_acceleration(argv);
}

void espheadtracker_t::service()
{
  while(run_service) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    // usleep(1000);
    double ct(gettime());
    for(std::map<std::string, espdevice_t*>::iterator it = devices.begin();
        it != devices.end(); ++it) {
      // DEBUG(it->second->last_call);
      // DEBUG(ct);
      if((it->second->last_call != HUGE_VAL) &&
         (ct > it->second->last_call + timeout)) {
        // DEBUG(1);
        std::cerr << "removing ESP sensor at " << it->first << std::endl;
        espdevice_t* tmp(it->second);
        std::lock_guard<std::mutex> lock(devmtx);
        // DEBUG(1);
        devices.erase(it);
        // DEBUG(1);
        delete tmp;
        // DEBUG(1);
        break;
      }
    }
  }
}

int osc_eog(const char*, const char* types, lo_arg** argv, int argc,
            lo_message msg, void* user_data)
{
  if(user_data && (argc == 2) && (types[0] == 'd') && (types[1] == 'd'))
    ((espheadtracker_t*)user_data)->set_eog(msg, argv);
  return 0;
}

void espheadtracker_t::set_eog(lo_message msg, lo_arg** argv)
{
  alive();
  getdev(msg)->push_eog(argv);
}

int osc_rot(const char*, const char* types, lo_arg** argv, int argc,
            lo_message msg, void* user_data)
{
  if(user_data && (argc == 4) && (types[0] == 'd') && (types[1] == 'd') &&
     (types[2] == 'd') && (types[3] == 'd'))
    ((espheadtracker_t*)user_data)->set_rot(msg, argv);
  return 0;
}

void espheadtracker_t::set_rot(lo_message msg, lo_arg** argv)
{
  getdev(msg)->push_rot(argv);
}

int osc_temp(const char*, const char* types, lo_arg** argv, int argc,
             lo_message msg, void* user_data)
{
  if(user_data && (argc == 2) && (types[0] == 'd') && (types[1] == 'd'))
    ((espheadtracker_t*)user_data)->set_temp(msg, argv);
  return 0;
}

void espheadtracker_t::set_temp(lo_message msg, lo_arg** argv)
{
  getdev(msg)->push_temp(argv);
}

espheadtracker_t::espheadtracker_t(const sensorplugin_cfg_t& cfg)
    : sensorplugin_drawing_t(cfg), b_calib("calibrate"), run_service(true),
      timeout(8), caliblen(400)
{
  GET_ATTRIBUTE_(timeout);
  GET_ATTRIBUTE_(caliblen);
  remove();
  da.set_size_request(360, 120);
  vbox.pack_start(b_calib, Gtk::PACK_EXPAND_WIDGET);
  hbox.pack_start(vbox, Gtk::PACK_SHRINK);
  hbox.pack_start(da, Gtk::PACK_EXPAND_WIDGET);
  b_calib.signal_clicked().connect(
      sigc::mem_fun(*this, &espheadtracker_t::on_click_calib));
  hbox.show_all();
  // DEBUG(1);
  srv = std::thread(&espheadtracker_t::service, this);
  // DEBUG(2);
}

void espheadtracker_t::on_click_calib()
{
  try {
    // DEBUG(1);
    std::lock_guard<std::mutex> lock(devmtx);
    for(std::map<std::string, espdevice_t*>::iterator it = devices.begin();
        it != devices.end(); ++it)
      it->second->send_calib();
  }
  catch(const std::exception& e) {
    add_warning(e.what());
  }
}

espheadtracker_t::~espheadtracker_t()
{
  run_service = false;
  srv.join();
  // DEBUG(devices.size());
  for(std::map<std::string, espdevice_t*>::iterator it = devices.begin();
      it != devices.end(); ++it) {
    // DEBUG(it->second);
    delete it->second;
  }
}

void espheadtracker_t::add_variables(TASCAR::osc_server_t* srv)
{
  srv->set_prefix("");
  srv->add_method("/eog", "dd", &osc_eog, this);
  srv->add_method("/acceleration", "dddd", &osc_acceleration, this);
  srv->add_method("/rot", "dddd", &osc_rot, this);
  srv->add_method("/temperature", "dd", &osc_temp, this);
}

void espheadtracker_t::draw(const Cairo::RefPtr<Cairo::Context>& cr,
                            double width, double height)
{
  double y(0.05);
  std::lock_guard<std::mutex> lock(devmtx);
  for(std::map<std::string, espdevice_t*>::iterator it = devices.begin();
      it != devices.end(); ++it) {
    y += 0.12;
    cr->move_to(0.05 * width, y * height);
    cr->show_text(it->first);
    cr->stroke();
    cr->move_to(0.3 * width, y * height);
    char ctmp[256];
    ctmp[255] = 0;
    snprintf(ctmp, 255, "%1.2f deg. C", it->second->get_temp());
    cr->show_text(ctmp);
    cr->stroke();
    cr->move_to(0.55 * width, y * height);
    snprintf(ctmp, 255, "%1.2f mV", 1000.0 * it->second->get_eog());
    cr->show_text(ctmp);
    cr->stroke();
    // rotation:
    cr->save();
    cr->set_line_width(1);
    for(uint32_t c = 0; c < 3; ++c) {
      double lx((0.75 + 0.05 * c) * width);
      double ly((y - 0.04) * height);
      double r(0.055 * height);
      cr->arc(lx, ly, r, 0, TASCAR_2PI);
      cr->stroke();
      double lrot(it->second->get_rot(c));
      cr->move_to(lx, ly);
      cr->line_to(lx + r * cos(lrot), ly + r * sin(lrot));
      cr->stroke();
    }
    cr->restore();
  }
}

REGISTER_SENSORPLUGIN(espheadtracker_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
