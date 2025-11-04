/*
 * This file is part of the TASCAR software, see <http://tascar.org/>
 *
 * Copyright (c) 2018 Giso Grimm
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

#include "serviceclass.h"
#include "session.h"
#include <unistd.h>

#define SNPRINTF(s, n, fmt, val)                                               \
  s[n - 1] = 0;                                                                \
  snprintf(s, n, fmt, val)

void operator*=(TASCAR::Scene::rgb_color_t& self, double s)
{
  self.r *= s;
  self.g *= s;
  self.b *= s;
}

void operator-=(TASCAR::Scene::rgb_color_t& self,
                const TASCAR::Scene::rgb_color_t& o)
{
  self.r -= o.r;
  self.g -= o.g;
  self.b -= o.b;
}

double coldist(TASCAR::Scene::rgb_color_t c1,
               const TASCAR::Scene::rgb_color_t& c2)
{
  c1 -= c2;
  return sqrt(c1.r * c1.r + c1.g * c1.g + c1.b * c1.b);
  // return fabs(c1.r) + fabs(c1.g) + fabs(c1.g);
}

std::string col2colname(TASCAR::Scene::rgb_color_t col)
{
  double dmax(std::max(col.r, std::max(col.g, col.b)));
  if(dmax > 0) {
    col *= 1.0 / dmax;
  } else {
    col.r = col.g = col.b = 1.0;
  }
  std::vector<TASCAR::Scene::rgb_color_t> cols;
  std::vector<std::string> names;
  cols.push_back(TASCAR::Scene::rgb_color_t("#ff281c"));
  names.push_back("red");
  cols.push_back(TASCAR::Scene::rgb_color_t("#75cc26"));
  names.push_back("green");
  // cols.push_back(TASCAR::Scene::rgb_color_t("#00c4a8"));
  cols.push_back(TASCAR::Scene::rgb_color_t("#0044a8"));
  names.push_back("blue");
  cols.push_back(TASCAR::Scene::rgb_color_t("#ffed00"));
  names.push_back("yellow");
  cols.push_back(TASCAR::Scene::rgb_color_t("#aa7faa"));
  names.push_back("purple");
  cols.push_back(TASCAR::Scene::rgb_color_t("#b2b2b2"));
  names.push_back("gray");
  cols.push_back(TASCAR::Scene::rgb_color_t("#f9a01c"));
  names.push_back("orange");
  cols.push_back(TASCAR::Scene::rgb_color_t("#826647"));
  names.push_back("brown");
  cols.push_back(TASCAR::Scene::rgb_color_t("#ff05f2"));
  names.push_back("pink");
  double dmin(coldist(cols[0], col));
  uint32_t kmin(0);
  for(uint32_t k = 1; k < cols.size(); ++k) {
    cols[k] *= 1.0 / std::max(cols[k].r, std::max(cols[k].g, cols[k].b));
    double cd(coldist(cols[k], col));
    if(cd < dmin) {
      dmin = cd;
      kmin = k;
    }
  }
  return names[kmin];
}

class connection_t {
public:
  connection_t(const std::string& host, uint32_t port, uint32_t channels,
               bool htmlcolors_, bool mutesounds_);
  void uploadsession(TASCAR::session_t* session,
                     std::vector<TASCAR::Scene::audio_port_t*>& routeports);
  void updatesession(TASCAR::session_t* session,
                     std::vector<TASCAR::Scene::audio_port_t*>& routeports,
                     bool force);
  void setvaluesession(TASCAR::session_t* session,
                       std::vector<TASCAR::Scene::audio_port_t*>& routeports,
                       uint32_t channel, float val);
  void setmutesession(TASCAR::session_t* session,
                      std::vector<TASCAR::Scene::audio_port_t*>& routeports,
                      uint32_t channel, bool val);
  ~connection_t();
  uint32_t scene;

private:
  connection_t(const connection_t&);
  lo_address target;
  uint32_t channels;
  std::vector<float> vals;
  std::vector<float> levels;
  std::vector<bool> mutestates;
  bool htmlcolors;
  bool mutesounds;
};

connection_t::connection_t(const std::string& host, uint32_t port,
                           uint32_t channels_, bool htmlcolors_,
                           bool mutesounds_)
    : scene(0), channels(channels_), htmlcolors(htmlcolors_),
      mutesounds(mutesounds_)
{
  vals.resize(channels);
  levels.resize(channels);
  mutestates.resize(channels);
  char ctmp[32];
  SNPRINTF(ctmp, 32, "%d", port);
  target = lo_address_new(host.c_str(), ctmp);
  if(!target)
    throw TASCAR::ErrMsg("Unable to create target adress \"" + host + "\".");
  lo_address_set_ttl(target, 1);
}

connection_t::~connection_t()
{
  lo_address_free(target);
}

void connection_t::uploadsession(
    TASCAR::session_t* session,
    std::vector<TASCAR::Scene::audio_port_t*>& routeports)
{
  uint32_t ch(0);
  if(scene < session->scenes.size()) {
    lo_send(target, "/touchosc/scene", "s",
            session->scenes[scene]->name.c_str());
    for(auto it : session->scenes[scene]->sounds) {
      if(ch < channels) {
        char cfader[1024];
        SNPRINTF(cfader, 1023, "/touchosc/fader%d", ch + 1);
        char clabel[1024];
        SNPRINTF(clabel, 1023, "/touchosc/label%d", ch + 1);
        char clevel[1024];
        SNPRINTF(clevel, 1023, "/touchosc/level%d", ch + 1);
        char cmute[1024];
        SNPRINTF(cmute, 1023, "/touchosc/mute%d", ch + 1);
        float v(it->get_gain_db());
        vals[ch] = v;
        float l(it->read_meter());
        if(!(l > 0.0f))
          l = 0.0f;
        levels[ch] = l;
        lo_send(target, cfader, "f", v);
        lo_send(target, clabel, "s", it->get_fullname().c_str());
        lo_send(target, clevel, "f", l);
        if(mutesounds) {
          lo_send(target, cmute, "i", it->get_mute());
        } else {
          if(it->parent)
            lo_send(target, cmute, "i", it->parent->get_mute());
        }
        std::string col;
        if(!htmlcolors)
          col = col2colname(it->get_color());
        else
          col = it->get_color().str();
        SNPRINTF(cfader, 1023, "/touchosc/fader%d/color", ch + 1);
        lo_send(target, cfader, "s", col.c_str());
        SNPRINTF(clabel, 1023, "/touchosc/label%d/color", ch + 1);
        lo_send(target, clabel, "s", col.c_str());
        ++ch;
      }
    }
    for(auto it : session->scenes[scene]->receivermod_objects) {
      if(ch < channels) {
        char cfader[1024];
        SNPRINTF(cfader, 1023, "/touchosc/fader%d", ch + 1);
        char clabel[1024];
        SNPRINTF(clabel, 1023, "/touchosc/label%d", ch + 1);
        char clevel[1024];
        SNPRINTF(clevel, 1023, "/touchosc/level%d", ch + 1);
        char cmute[1024];
        SNPRINTF(cmute, 1023, "/touchosc/mute%d", ch + 1);
        float v(it->get_gain_db());
        vals[ch] = v;
        float l(it->read_meter_max());
        if(!(l > 0.0f))
          l = 0.0f;
        levels[ch] = l;
        lo_send(target, cfader, "f", v);
        lo_send(target, clabel, "s", it->get_name().c_str());
        lo_send(target, clevel, "f", l);
        lo_send(target, cmute, "i", it->get_mute());
        std::string col;
        if(!htmlcolors)
          col = col2colname(it->color);
        else
          col = it->color.str();
        SNPRINTF(cfader, 1023, "/touchosc/fader%d/color", ch + 1);
        lo_send(target, cfader, "s", col.c_str());
        SNPRINTF(clabel, 1023, "/touchosc/label%d/color", ch + 1);
        lo_send(target, clabel, "s", col.c_str());
        ++ch;
      }
    }
    for(auto it : session->scenes[scene]->diffuse_reverbs) {
      if(ch < channels) {
        char cfader[1024];
        SNPRINTF(cfader, 1023, "/touchosc/fader%d", ch + 1);
        char clabel[1024];
        SNPRINTF(clabel, 1023, "/touchosc/label%d", ch + 1);
        char clevel[1024];
        SNPRINTF(clevel, 1023, "/touchosc/level%d", ch + 1);
        char cmute[1024];
        SNPRINTF(cmute, 1023, "/touchosc/mute%d", ch + 1);
        float v(it->get_gain_db());
        vals[ch] = v;
        float l(it->read_meter_max());
        if(!(l > 0.0f))
          l = 0.0f;
        levels[ch] = l;
        lo_send(target, cfader, "f", v);
        lo_send(target, clabel, "s", it->get_name().c_str());
        lo_send(target, clevel, "f", l);
        lo_send(target, cmute, "i", it->get_mute());
        std::string col;
        if(!htmlcolors)
          col = col2colname(it->color);
        else
          col = it->color.str();
        SNPRINTF(cfader, 1023, "/touchosc/fader%d/color", ch + 1);
        lo_send(target, cfader, "s", col.c_str());
        SNPRINTF(clabel, 1023, "/touchosc/label%d/color", ch + 1);
        lo_send(target, clabel, "s", col.c_str());
        ++ch;
      }
    }
  }
  // now add routes:
  for(auto rp : routeports) {
    if(ch < channels) {
      auto scenerp = dynamic_cast<TASCAR::Scene::route_t*>(rp);
      if(scenerp) {
        char cfader[1024];
        SNPRINTF(cfader, 1023, "/touchosc/fader%d", ch + 1);
        char clabel[1024];
        SNPRINTF(clabel, 1023, "/touchosc/label%d", ch + 1);
        char clevel[1024];
        SNPRINTF(clevel, 1023, "/touchosc/level%d", ch + 1);
        char cmute[1024];
        SNPRINTF(cmute, 1023, "/touchosc/mute%d", ch + 1);
        float v(rp->get_gain_db());
        vals[ch] = v;
        float l(scenerp->read_meter_max());
        if(!(l > 0.0f))
          l = 0.0f;
        levels[ch] = l;
        lo_send(target, cfader, "f", v);
        lo_send(target, clabel, "s", scenerp->get_name().c_str());
        lo_send(target, clevel, "f", l);
        lo_send(target, cmute, "i", scenerp->get_mute());
        std::string col("#777777");
        if(!htmlcolors)
          col = col2colname(col);
        SNPRINTF(clabel, 1023, "/touchosc/label%d/color", ch + 1);
        lo_send(target, clabel, "s", col.c_str());
        SNPRINTF(cfader, 1023, "/touchosc/fader%d/color", ch + 1);
        lo_send(target, cfader, "s", col.c_str());
        ++ch;
      }
    }
  }
  for(uint32_t k = ch; k < channels; ++k) {
    char cfader[1024];
    SNPRINTF(cfader, 1023, "/touchosc/fader%d", k + 1);
    char clabel[1024];
    SNPRINTF(clabel, 1023, "/touchosc/label%d", k + 1);
    char clevel[1024];
    SNPRINTF(clevel, 1023, "/touchosc/level%d", k + 1);
    lo_send(target, clabel, "s", "");
    lo_send(target, cfader, "f", -30.0f);
    lo_send(target, clevel, "f", 0.0f);
    SNPRINTF(cfader, 1023, "/touchosc/fader%d/color", k + 1);
    if(htmlcolors)
      lo_send(target, cfader, "s", "#000000");
    else
      lo_send(target, cfader, "s", "brown");
    SNPRINTF(clabel, 1023, "/touchosc/label%d/color", k + 1);
    if(htmlcolors)
      lo_send(target, cfader, "s", "#000000");
    else
      lo_send(target, cfader, "s", "brown");
  }
}

void connection_t::updatesession(
    TASCAR::session_t* session,
    std::vector<TASCAR::Scene::audio_port_t*>& routeports, bool force)
{
  uint32_t ch(0);
  if(scene < session->scenes.size()) {
    for(auto it : session->scenes[scene]->sounds) {
      if(ch < channels) {
        char cfader[1024];
        SNPRINTF(cfader, 1023, "/touchosc/fader%d", ch + 1);
        char clevel[1024];
        SNPRINTF(clevel, 1023, "/touchosc/level%d", ch + 1);
        char cmute[1024];
        SNPRINTF(cmute, 1023, "/touchosc/mute%d", ch + 1);
        float v(it->get_gain_db());
        if(force || (vals[ch] != v))
          lo_send(target, cfader, "f", v);
        vals[ch] = v;
        float l(it->read_meter());
        if(!(l > 0.0f))
          l = 0.0f;
        if(force || (fabsf(levels[ch] - l) > 0.1)) {
          lo_send(target, clevel, "f", l);
          levels[ch] = l;
        }
        bool vb = false;
        if(mutesounds) {
          vb = it->get_mute();
        } else {
          if(it->parent)
            vb = it->parent->get_mute();
        }
        if(force || (mutestates[ch] != vb)) {
          lo_send(target, cmute, "i", vb);
        }
        mutestates[ch] = vb;
        ++ch;
      }
    }
    for(auto it : session->scenes[scene]->receivermod_objects) {
      if(ch < channels) {
        char cfader[1024];
        SNPRINTF(cfader, 1023, "/touchosc/fader%d", ch + 1);
        char clevel[1024];
        SNPRINTF(clevel, 1023, "/touchosc/level%d", ch + 1);
        char cmute[1024];
        SNPRINTF(cmute, 1023, "/touchosc/mute%d", ch + 1);
        float v(it->get_gain_db());
        if(force || (vals[ch] != v))
          lo_send(target, cfader, "f", v);
        vals[ch] = v;
        float l(it->read_meter_max());
        if(!(l > 0.0f))
          l = 0.0f;
        if(force || (fabsf(levels[ch] - l) > 0.1)) {
          lo_send(target, clevel, "f", l);
          levels[ch] = l;
        }
        bool vb = false;
        vb = it->get_mute();
        if(force || (mutestates[ch] != vb)) {
          lo_send(target, cmute, "i", vb);
        }
        mutestates[ch] = vb;
        ++ch;
      }
    }
    for(auto it : session->scenes[scene]->diffuse_reverbs) {
      if(ch < channels) {
        char cfader[1024];
        SNPRINTF(cfader, 1024, "/touchosc/fader%d", ch + 1);
        char clevel[1024];
        SNPRINTF(clevel, 1024, "/touchosc/level%d", ch + 1);
        char cmute[1024];
        SNPRINTF(cmute, 1023, "/touchosc/mute%d", ch + 1);
        float v(it->get_gain_db());
        if(force || (vals[ch] != v))
          lo_send(target, cfader, "f", v);
        vals[ch] = v;
        float l(it->read_meter_max());
        if(!(l > 0.0f))
          l = 0.0f;
        if(force || (fabsf(levels[ch] - l) > 0.1)) {
          lo_send(target, clevel, "f", l);
          levels[ch] = l;
        }
        bool vb = false;
        vb = it->get_mute();
        if(force || (mutestates[ch] != vb)) {
          lo_send(target, cmute, "i", vb);
        }
        mutestates[ch] = vb;
        ++ch;
      }
    }
  }
  // now add routes:
  for(auto rp : routeports) {
    if(ch < channels) {
      auto scenerp = dynamic_cast<TASCAR::Scene::route_t*>(rp);
      if(scenerp) {
        char cfader[1024];
        SNPRINTF(cfader, 1024, "/touchosc/fader%d", ch + 1);
        char clevel[1024];
        SNPRINTF(clevel, 1024, "/touchosc/level%d", ch + 1);
        char cmute[1024];
        SNPRINTF(cmute, 1023, "/touchosc/mute%d", ch + 1);
        float v(rp->get_gain_db());
        if(force || (vals[ch] != v))
          lo_send(target, cfader, "f", v);
        vals[ch] = v;
        float l(scenerp->read_meter_max());
        if(!(l > 0.0f))
          l = 0.0f;
        if(force || (fabsf(levels[ch] - l) > 0.1)) {
          lo_send(target, clevel, "f", l);
          levels[ch] = l;
        }
        bool vb = false;
        vb = scenerp->get_mute();
        if(force || (mutestates[ch] != vb)) {
          lo_send(target, cmute, "i", vb);
        }
        mutestates[ch] = vb;
        ++ch;
      }
    }
  }
}

void connection_t::setvaluesession(
    TASCAR::session_t* session,
    std::vector<TASCAR::Scene::audio_port_t*>& routeports, uint32_t channel,
    float val)
{
  if(channel >= channels)
    return;
  vals[channel] = val;
  uint32_t ch(0);
  if(scene < session->scenes.size()) {
    for(auto it : session->scenes[scene]->sounds) {
      if(ch < channels) {
        if(ch == channel) {
          it->set_gain_db(val);
          return;
        }
        ++ch;
      }
    }
    for(auto it : session->scenes[scene]->receivermod_objects) {
      if(ch < channels) {
        if(ch == channel) {
          it->set_gain_db(val);
          return;
        }
        ++ch;
      }
    }
    for(auto it : session->scenes[scene]->diffuse_reverbs) {
      if(ch < channels) {
        if(ch == channel) {
          it->set_gain_db(val);
          return;
        }
        ++ch;
      }
    }
  }
  // now add routes:
  for(auto rp : routeports) {
    if(ch < channels) {
      auto scenerp = dynamic_cast<TASCAR::Scene::route_t*>(rp);
      if(scenerp) {
        if(ch == channel) {
          rp->set_gain_db(val);
          return;
        }
        ++ch;
      }
    }
  }
  if(ch < channels) {
    char cfader[1024];
    SNPRINTF(cfader, 1024, "/touchosc/fader%d", channel + 1);
    lo_send(target, cfader, "f", -30.0f);
  }
}

void connection_t::setmutesession(
    TASCAR::session_t* session,
    std::vector<TASCAR::Scene::audio_port_t*>& routeports, uint32_t channel,
    bool val)
{
  if(channel >= channels)
    return;
  mutestates[channel] = val;
  uint32_t ch(0);
  if(scene < session->scenes.size()) {
    for(auto it : session->scenes[scene]->sounds) {
      if(ch < channels) {
        if(ch == channel) {
          if(mutesounds) {
            it->set_mute(val);
          } else {
            if(it->parent)
              it->parent->set_mute(val);
          }
          return;
        }
        ++ch;
      }
    }
    for(auto it : session->scenes[scene]->receivermod_objects) {
      if(ch < channels) {
        if(ch == channel) {
          it->set_mute(val);
          return;
        }
        ++ch;
      }
    }
    for(auto it : session->scenes[scene]->diffuse_reverbs) {
      if(ch < channels) {
        if(ch == channel) {
          it->set_mute(val);
          return;
        }
        ++ch;
      }
    }
  }
  // now add routes:
  for(auto rp : routeports) {
    if(ch < channels) {
      auto scenerp = dynamic_cast<TASCAR::Scene::route_t*>(rp);
      if(scenerp) {
        if(ch == channel) {
          scenerp->set_mute(val);
          return;
        }
        ++ch;
      }
    }
  }
}

class touchosc_t : public TASCAR::module_base_t, TASCAR::service_t {
public:
  class chref_t {
  public:
    touchosc_t* t;
    uint32_t c;
  };
  touchosc_t(const TASCAR::module_cfg_t& cfg);
  ~touchosc_t();
  static int osc_connect(const char* path, const char* types, lo_arg** argv,
                         int argc, lo_message msg, void* user_data);
  static int osc_setfader(const char* path, const char* types, lo_arg** argv,
                          int argc, lo_message msg, void* user_data);
  static int osc_setmute(const char* path, const char* types, lo_arg** argv,
                         int argc, lo_message msg, void* user_data);
  static int osc_sceneinc(const char* path, const char* types, lo_arg** argv,
                          int argc, lo_message msg, void* user_data);
  static int osc_scenedec(const char* path, const char* types, lo_arg** argv,
                          int argc, lo_message msg, void* user_data);
  void connect(const std::string& host, uint32_t channels);
  void setfader(const std::string& host, uint32_t channel, float val);
  void setmute(const std::string& host, uint32_t channel, bool val);
  void setscene(const std::string& host, int32_t dscene);
  void service();

private:
  uint32_t port;
  bool htmlcolors;
  bool mutesounds = true;
  // std::vector<TASCAR::named_object_t> obj;
  std::vector<lo_message> vmsg;
  std::vector<lo_arg**> vargv;
  std::vector<std::string> vpath;
  std::map<std::string, connection_t*> connections;
  std::vector<chref_t> chrefs;
  pthread_mutex_t mtx;
  std::vector<TASCAR::Scene::audio_port_t*> routeports;
};

int touchosc_t::osc_connect(const char*, const char*, lo_arg** argv, int,
                            lo_message msg, void* user_data)
{
  lo_address src(lo_message_get_source(msg));
  touchosc_t* h((touchosc_t*)user_data);
  h->connect(lo_address_get_hostname(src), argv[0]->i);
  return 0;
}

int touchosc_t::osc_setfader(const char*, const char*, lo_arg** argv, int,
                             lo_message msg, void* user_data)
{
  lo_address src(lo_message_get_source(msg));
  touchosc_t::chref_t* h((touchosc_t::chref_t*)user_data);
  h->t->setfader(lo_address_get_hostname(src), h->c, argv[0]->f);
  return 0;
}

int touchosc_t::osc_setmute(const char*, const char*, lo_arg** argv, int,
                            lo_message msg, void* user_data)
{
  lo_address src(lo_message_get_source(msg));
  touchosc_t::chref_t* h((touchosc_t::chref_t*)user_data);
  h->t->setmute(lo_address_get_hostname(src), h->c, (argv[0]->i != 0));
  return 0;
}

int touchosc_t::osc_sceneinc(const char*, const char*, lo_arg** argv, int,
                             lo_message msg, void* user_data)
{
  lo_address src(lo_message_get_source(msg));
  touchosc_t* h((touchosc_t*)user_data);
  if(argv[0]->f > 0)
    h->setscene(lo_address_get_hostname(src), 1);
  return 0;
}

int touchosc_t::osc_scenedec(const char*, const char*, lo_arg** argv, int,
                             lo_message msg, void* user_data)
{
  lo_address src(lo_message_get_source(msg));
  touchosc_t* h((touchosc_t*)user_data);
  if(argv[0]->f > 0)
    h->setscene(lo_address_get_hostname(src), -1);
  return 0;
}

void touchosc_t::setscene(const std::string& host, int32_t dscene)
{
  if(dscene == 0)
    return;
  if(pthread_mutex_lock(&mtx) == 0) {
    std::map<std::string, connection_t*>::iterator it;
    if((it = connections.find(host)) != connections.end()) {
      if(it->second) {
        if(((int32_t)(it->second->scene) + dscene >= 0) &&
           ((int32_t)(it->second->scene) + dscene <
            (int32_t)(session->scenes.size()))) {
          it->second->scene += dscene;
          it->second->uploadsession(session, routeports);
        }
      }
    }
    pthread_mutex_unlock(&mtx);
  }
}

void touchosc_t::setfader(const std::string& host, uint32_t channel, float val)
{
  if(pthread_mutex_lock(&mtx) == 0) {
    std::map<std::string, connection_t*>::iterator it;
    if((it = connections.find(host)) != connections.end()) {
      if(it->second)
        it->second->setvaluesession(session, routeports, channel, val);
    }
    pthread_mutex_unlock(&mtx);
  }
}

void touchosc_t::setmute(const std::string& host, uint32_t channel, bool val)
{
  if(pthread_mutex_lock(&mtx) == 0) {
    std::map<std::string, connection_t*>::iterator it;
    if((it = connections.find(host)) != connections.end()) {
      if(it->second)
        it->second->setmutesession(session, routeports, channel, val);
    }
    pthread_mutex_unlock(&mtx);
  }
}

void touchosc_t::connect(const std::string& host, uint32_t channels)
{
  if(channels == 0)
    return;
  if(pthread_mutex_lock(&mtx) == 0) {
    // std::cout << "Adding touchosc host \"" + host + "\" with " << channels
    //          << " channels.\n";
    std::map<std::string, connection_t*>::iterator it;
    if((it = connections.find(host)) != connections.end()) {
      if(it->second)
        delete it->second;
      it->second = NULL;
    }
    connection_t* con(
        new connection_t(host, port, channels, htmlcolors, mutesounds));
    routeports = session->find_route_ports({"/*"});
    connections[host] = con;
    con->uploadsession(session, routeports);
    pthread_mutex_unlock(&mtx);
  }
}

touchosc_t::touchosc_t(const TASCAR::module_cfg_t& cfg)
    : module_base_t(cfg), port(9000), htmlcolors(false)
{
  GET_ATTRIBUTE(port, "", "Port number of OSC device");
  GET_ATTRIBUTE_BOOL(htmlcolors,
                     "User HTML colors instead of touchosc default colors");
  GET_ATTRIBUTE_BOOL(mutesounds,
                     "Mute individual sounds instead of parent source");
  pthread_mutex_init(&mtx, NULL);
  session->add_method("/touchosc/connect", "i", &touchosc_t::osc_connect, this);
  session->add_method("/touchosc/incscene", "f", &touchosc_t::osc_sceneinc,
                      this);
  session->add_method("/touchosc/decscene", "f", &touchosc_t::osc_scenedec,
                      this);
  chrefs.resize(16);
  for(std::vector<chref_t>::iterator it = chrefs.begin(); it != chrefs.end();
      ++it) {
    it->t = this;
    it->c = it - chrefs.begin();
    char ctmp[1024];
    SNPRINTF(ctmp, 1024, "/touchosc/fader%d", it->c + 1);
    session->add_method(ctmp, "f", &touchosc_t::osc_setfader, &(*it));
    SNPRINTF(ctmp, 1024, "/touchosc/mute%d", it->c + 1);
    session->add_method(ctmp, "i", &touchosc_t::osc_setmute, &(*it));
  }
  start_service();
}

void touchosc_t::service()
{
  uint32_t cnt(20);
  while(run_service) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    // usleep(100000);
    if(pthread_mutex_lock(&mtx) == 0) {
      for(std::map<std::string, connection_t*>::iterator it =
              connections.begin();
          it != connections.end(); ++it)
        if(it->second)
          it->second->updatesession(session, routeports, cnt == 0);
      if(cnt)
        --cnt;
      else
        cnt = 20;
      pthread_mutex_unlock(&mtx);
    }
  }
}

touchosc_t::~touchosc_t()
{
  stop_service();
  for(std::map<std::string, connection_t*>::iterator it = connections.begin();
      it != connections.end(); ++it)
    if(it->second)
      delete it->second;
  for(uint32_t k = 0; k < vmsg.size(); ++k)
    lo_message_free(vmsg[k]);
  pthread_mutex_destroy(&mtx);
}

REGISTER_MODULE(touchosc_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
