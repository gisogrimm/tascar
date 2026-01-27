/*
 * This file is part of the TASCAR software, see <http://tascar.org/>
 *
 * Copyright (c) 2018 Giso Grimm
 * Copyright (c) 2019 Giso Grimm
 * Copyright (c) 2020 Giso Grimm
 * Copyright (c) 2021 Giso Grimm
 * Copyright (c) 2026 Giso Grimm
 */
/* License (GPL)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */
#include "jackclient.h"
#include "defs.h"
#include "errorhandling.h"
#include "tscconfig.h"
#include <jack/thread.h>
#include <regex.h>
#include <set>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

using namespace std::chrono_literals;

static std::string errmsg("");

static void jack_report_error(const char* msg)
{
  std::cerr << msg << std::endl;
  if(errmsg.size())
    errmsg += "\n";
  errmsg += msg;
}

jackc_portless_t::jackc_portless_t(const std::string& clientname)
    : srate(0), active(false), xruns(0), xrun_latency(0), shutdown(false),
      clientname_(clientname)
{
  mtx_active.lock();
  errmsg = "";
  jack_set_error_function(&jack_report_error);
  jack_status_t jstat;
  // jack_options_t opt(JackUseExactName |JackNoStartServer);
  if((int)(clientname.size() + 1) > jack_client_name_size())
    throw TASCAR::ErrMsg(
        "unable to open jack client: Client name is too long. (\"" +
        clientname + "\" max " + TASCAR::to_string(jack_client_name_size()) +
        ")");
  jack_options_t opt((jack_options_t)(JackNoStartServer | JackUseExactName));
  jc = jack_client_open(clientname.c_str(), opt, &jstat);
  if(!jc) {
    std::string err("unable to open jack client: ");
    if(jstat & JackFailure)
      err += "Overall operation failed. ";
    if(jstat & JackInvalidOption)
      err += "The operation contained an invalid or unsupported option. ";
    if(jstat & JackNameNotUnique)
      err += "The desired client name was not unique. ";
    if(jstat & JackServerStarted)
      err += "The JACK server was started as a result of this operation. ";
    if(jstat & JackServerFailed)
      err += "Unable to connect to the JACK server. ";
    if(jstat & JackServerError)
      err += "Communication error with the JACK server. ";
    if(jstat & JackInitFailure)
      err += "Unable to initialize client. ";
    if(jstat & JackShmFailure)
      err += "Unable to access shared memory. ";
    if(jstat & JackVersionError)
      err += "Client's protocol version does not match. ";
    if(errmsg.size())
      err += "\n" + errmsg;
    throw TASCAR::ErrMsg(err);
  }
  srate = jack_get_sample_rate(jc);
  fragsize = jack_get_buffer_size(jc);
  rtprio = jack_client_real_time_priority(jc);
  jack_set_xrun_callback(jc, jackc_portless_t::xrun_callback, this);
  jack_on_shutdown(jc, jackc_portless_t::on_shutdown, this);
}

jackc_portless_t::~jackc_portless_t()
{
  if(!shutdown) {
    if(active)
      deactivate();
    int err(0);
    if((err = jack_client_close(jc)) != 0) {
      std::cerr << "Error: jack_client_close returned " << err << std::endl;
    }
  }
}

uint32_t jackc_portless_t::tp_get_frame() const
{
  if(shutdown)
    throw TASCAR::ErrMsg("Jack server has shut down");
  return jack_get_current_transport_frame(jc);
}

double jackc_portless_t::tp_get_time() const
{
  return (1.0 / (double)srate) * (double)tp_get_frame();
}

// work around for bug in jack library:
void assert_valid_regexp(const std::string& exp)
{
  regex_t reg;
  if(regcomp(&reg, exp.c_str(), REG_EXTENDED | REG_NOSUB) != 0)
    throw TASCAR::ErrMsg("Invalid regular expression \"" + exp + "\".");
  regfree(&reg);
}

std::vector<std::string> jackc_portless_t::get_all_port_names(int flags) const
{
  std::vector<std::string> ports;
  const char** pp_ports(
      jack_get_ports(jc, NULL, JACK_DEFAULT_AUDIO_TYPE, flags));
  if(pp_ports) {
    const char** p(pp_ports);
    while(*p) {
      ports.push_back(*p);
      ++p;
    }
    jack_free(pp_ports);
  }
  return ports;
}

std::vector<std::string> jackc_portless_t::get_all_client_names(int flags) const
{
  std::vector<std::string> ports = get_all_port_names(flags);
  std::set<std::string> clients_set;
  for(const auto& p : ports) {
    auto cp = p.find(":");
    if(cp == std::string::npos)
      clients_set.insert(p);
    else
      clients_set.insert(p.substr(0, cp));
  }
  std::vector<std::string> clients;
  for(const auto& c : clients_set)
    clients.push_back(c);
  return clients;
}

std::vector<std::string> get_port_names_regexp(jack_client_t* jc,
                                               std::string name, int flags)
{
  std::vector<std::string> ports;
  if(name.size() && (name[0] != '^'))
    name = "^" + name;
  if(name.size() && (name[name.size() - 1] != '$'))
    name = name + "$";
  assert_valid_regexp(name);
  const char** pp_ports(
      jack_get_ports(jc, name.c_str(), JACK_DEFAULT_AUDIO_TYPE, flags));
  if(pp_ports) {
    const char** p(pp_ports);
    while(*p) {
      ports.push_back(*p);
      ++p;
    }
    jack_free(pp_ports);
  }
  return ports;
}

std::vector<std::string>
jackc_portless_t::get_port_names_regexp(const std::vector<std::string>& name,
                                        int flags) const
{
  std::vector<std::string> ports;
  for(auto it = name.begin(); it != name.end(); ++it) {
    std::vector<std::string> lports(get_port_names_regexp(*it, flags));
    ports.insert(ports.end(), lports.begin(), lports.end());
  }
  return ports;
}

std::vector<std::string>
jackc_portless_t::get_port_names_regexp(const std::string& name,
                                        int flags) const
{
  if(shutdown)
    throw TASCAR::ErrMsg("Jack server has shut down");
  return ::get_port_names_regexp(jc, name, flags);
}

void jackc_portless_t::activate()
{
  if(shutdown)
    throw TASCAR::ErrMsg("Jack server has shut down");
  jack_activate(jc);
  active = true;
  mtx_active.unlock();
}

void jackc_portless_t::deactivate()
{
  mtx_active.lock();
  if(shutdown)
    return;
  if(active)
    jack_deactivate(jc);
  active = false;
}

/**
   \brief Connect JACK ports

   \param src Source port name (including client name)

   \param dest Destination port name (including client name)

   \param bwarn If true, report failure as warning, if false, throw
          an exception.

   \param allowoutputsource If true, then if src is an output port, a
          connection is made to all ports which connect to src.

   \param connectmulti Port names may contain globbing characters.
          The number of connections is the maximum of src and dest
          matches. If src contains fewer matches then dest, the src
          matches are cyclically repeated (and accoringly for
          destination matches).

   \param allowinputdest If true, then if dest is an input port, a
          connection is made to all ports which connect to dest.

   \param noconnecttoself If allowoutputsource or allowinputdest is
          true and noconnecttoself is true, then no connections to
          other own ports will be made

 */
void jackc_portless_t::connect(const std::string& src, const std::string& dest,
                               bool bwarn, bool allowoutputsource,
                               bool connectmulti, bool allowinputdest,
                               bool noconnecttoself)
{
  if(shutdown)
    throw TASCAR::ErrMsg("Jack server has shut down");
  if(connectmulti) {
    // connect multiple ports simultaneously using globbing:
    std::vector<std::string> ports_src(get_port_names_regexp(src));
    std::vector<std::string> ports_dest(get_port_names_regexp(dest));
    if((ports_src.size() > 0) && (ports_dest.size() > 0)) {
      for(uint32_t c = 0; c < std::max(ports_src.size(), ports_dest.size());
          ++c) {
        connect(ports_src[c % ports_src.size()],
                ports_dest[c % ports_dest.size()], bwarn, allowoutputsource,
                false, allowinputdest, noconnecttoself);
      }
    } else {
      if(bwarn)
        TASCAR::add_warning("No connection \"" + src + "\" to \"" + dest +
                            "\" found.");
      else
        throw TASCAR::ErrMsg("No connection \"" + src + "\" to \"" + dest +
                             "\" found.");
    }
    //
  } else {
    jack_port_t* srcport(jack_port_by_name(jc, src.c_str()));
    jack_port_t* destport(jack_port_by_name(jc, dest.c_str()));
    if(allowoutputsource && srcport &&
       (jack_port_flags(srcport) & JackPortIsInput)) {
      const char** cons(jack_port_get_all_connections(jc, srcport));
      const char** ocons(cons);
      if(cons) {
        while(*cons) {
          if((!noconnecttoself) ||
             (!jack_port_is_mine(jc, jack_port_by_name(jc, *cons)))) {
            if(jack_connect(jc, *cons, dest.c_str()) != 0) {
              errmsg = std::string("unable to connect port '") +
                       std::string(*cons) + "' to '" + dest + "'.";
              if(bwarn)
                TASCAR::add_warning(errmsg);
              else
                throw TASCAR::ErrMsg(errmsg.c_str());
            }
            //}else{
            //  std::cerr << "not connecting " << *cons << " to " << dest <<
            //  std::endl;
          }
          ++cons;
        }
        jack_free(ocons);
      }
    } else {
      if(allowinputdest && destport &&
         (jack_port_flags(destport) & JackPortIsOutput)) {
        const char** cons(jack_port_get_all_connections(jc, destport));
        const char** ocons(cons);
        if(cons) {
          while(*cons) {
            if((!noconnecttoself) ||
               (!jack_port_is_mine(jc, jack_port_by_name(jc, *cons)))) {
              if(jack_connect(jc, src.c_str(), *cons) != 0) {
                errmsg = std::string("unable to connect port '") + src +
                         "' to '" + std::string(*cons) + "'.";
                if(bwarn)
                  TASCAR::add_warning(errmsg);
                else
                  throw TASCAR::ErrMsg(errmsg.c_str());
              }
              //}else{
              //  std::cerr << "not connecting " << src << " to " << *cons <<
              //  std::endl;
            }
            ++cons;
          }
          jack_free(ocons);
        }
      } else {
        if(jack_connect(jc, src.c_str(), dest.c_str()) != 0) {
          errmsg = std::string("unable to connect port '") + src + "' to '" +
                   dest + "'.";
          if(bwarn)
            TASCAR::add_warning(errmsg);
          else
            throw TASCAR::ErrMsg(errmsg.c_str());
        }
      }
    }
  }
}

jackc_t::jackc_t(const std::string& clientname) : jackc_portless_t(clientname)
{
  jack_set_process_callback(jc, process_, this);
}

jackc_t::~jackc_t()
{
  if(active) {
    deactivate();
    for(unsigned int k = 0; k < inPort.size(); k++)
      jack_port_unregister(jc, inPort[k]);
    for(unsigned int k = 0; k < outPort.size(); k++)
      jack_port_unregister(jc, outPort[k]);
  }
}

int jackc_t::process(jack_nframes_t, const std::vector<float*>&,
                     const std::vector<float*>&)
{
  return 0;
}

int jackc_t::process_(jack_nframes_t nframes, void* arg)
{
  return ((jackc_t*)(arg))->process_(nframes);
}

int jackc_t::process_(jack_nframes_t nframes)
{
  if(!active)
    return 0;
  if(mtx_active.try_lock()) {
    for(unsigned int k = 0; k < inBuffer.size(); k++)
      inBuffer[k] = (float*)(jack_port_get_buffer(inPort[k], nframes));
    for(unsigned int k = 0; k < outBuffer.size(); k++)
      outBuffer[k] = (float*)(jack_port_get_buffer(outPort[k], nframes));
    int retv = process(nframes, inBuffer, outBuffer);
    mtx_active.unlock();
    return retv;
  }
  return 0;
}

jack_port_t* jackc_t::add_input_port(const std::string& name, bool is_midi)
{
  if(shutdown)
    throw TASCAR::ErrMsg("Jack server has shut down");
  if((int)(strlen(jack_get_client_name(jc)) + name.size() + 2) >=
     jack_port_name_size())
    throw TASCAR::ErrMsg(std::string("Port name \"" + get_client_name() + ":" +
                                     name + "\" is too long."));
  jack_port_t* p;
  if(is_midi)
    p = jack_port_register(jc, name.c_str(), JACK_DEFAULT_MIDI_TYPE,
                           JackPortIsInput, 0);
  else
    p = jack_port_register(jc, name.c_str(), JACK_DEFAULT_AUDIO_TYPE,
                           JackPortIsInput, 0);
  if(!p) {
    p = jack_port_by_name(jc, name.c_str());
    if(p)
      throw TASCAR::ErrMsg(
          std::string("Unable to register input port \"" + get_client_name() +
                      ":" + name + "\": A port of same name already exists."));
    throw TASCAR::ErrMsg(std::string("Unable to register input port \"" +
                                     get_client_name() + ":" + name + "\"."));
  }
  inPort.push_back(p);
  inBuffer.push_back(NULL);
  input_port_names.push_back(std::string(jack_get_client_name(jc)) + ":" +
                             name);
  return p;
}

jack_port_t* jackc_t::add_output_port(const std::string& name, bool is_midi)
{
  if(shutdown)
    throw TASCAR::ErrMsg("Jack server has shut down");
  if((int)(strlen(jack_get_client_name(jc)) + name.size() + 2) >=
     jack_port_name_size())
    throw TASCAR::ErrMsg(std::string("Port name \"" + get_client_name() + ":" +
                                     name + "\" is too long."));
  jack_port_t* p;
  if(is_midi)
    p = jack_port_register(jc, name.c_str(), JACK_DEFAULT_MIDI_TYPE,
                           JackPortIsOutput, 0);
  else
    p = jack_port_register(jc, name.c_str(), JACK_DEFAULT_AUDIO_TYPE,
                           JackPortIsOutput, 0);
  if(!p) {
    p = jack_port_by_name(jc, name.c_str());
    if(p)
      throw TASCAR::ErrMsg(
          std::string("Unable to register output port \"" + get_client_name() +
                      ":" + name + "\": A port of same name already exists."));
    throw TASCAR::ErrMsg(std::string("Unable to register output port \"" +
                                     get_client_name() + ":" + name + "\"."));
  }
  outPort.push_back(p);
  outBuffer.push_back(NULL);
  output_port_names.push_back(std::string(jack_get_client_name(jc)) + ":" +
                              name);
  return p;
}

int jackc_portless_t::xrun_callback(void* arg)
{
  ((jackc_portless_t*)arg)->xruns++;
  return 0;
}

void jackc_portless_t::on_shutdown(void* arg)
{
  ((jackc_portless_t*)arg)->active = false;
  ((jackc_portless_t*)arg)->shutdown = true;
}

void jackc_t::connect_in(unsigned int port, const std::string& pname,
                         bool bwarn, bool allowoutputsource,
                         bool noconnecttoself)
{
  if(inPort.size() <= port) {
    DEBUG(port);
    DEBUG(inPort.size());
    throw TASCAR::ErrMsg("Input port number not available (connect_in).");
  }
  connect(pname, jack_port_name(inPort[port]), bwarn, allowoutputsource, true,
          false, noconnecttoself);
}

void jackc_t::connect_out(unsigned int port, const std::string& pname,
                          bool bwarn, bool allowinputdest, bool noconnecttoself)
{
  if(outPort.size() <= port) {
    DEBUG(port);
    DEBUG(outPort.size());
    throw TASCAR::ErrMsg("Output port number not available (connect_out).");
  }
  connect(jack_port_name(outPort[port]), pname, bwarn, false, true,
          allowinputdest, noconnecttoself);
}

int jackc_t::disconnect_in(unsigned int port)
{
  if(inPort.size() <= port) {
    DEBUG(port);
    DEBUG(inPort.size());
    throw TASCAR::ErrMsg("Input port number not available (disconnect_in).");
  }
  return jack_port_disconnect(jc, inPort[port]);
}

int jackc_t::disconnect_out(unsigned int port)
{
  if(outPort.size() <= port) {
    DEBUG(port);
    DEBUG(outPort.size());
    throw TASCAR::ErrMsg("Output port number not available (disconnect_out).");
  }
  return jack_port_disconnect(jc, outPort[port]);
}

jackc_transport_t::jackc_transport_t(const std::string& clientname)
    : jackc_t(clientname), stop_at_time(0)
{
}

int jackc_transport_t::process(jack_nframes_t nframes,
                               const std::vector<float*>& inBuffer,
                               const std::vector<float*>& outBuffer)
{
  if(shutdown)
    return -1;
  jack_position_t pos;
  jack_transport_state_t jstate;
  jstate = jack_transport_query(jc, &pos);
  if((stop_at_time > 0) && ((double)pos.frame / srate >= stop_at_time)) {
    tp_stop();
    stop_at_time = 0;
  }
  return process(nframes, inBuffer, outBuffer, pos.frame,
                 jstate == JackTransportRolling);
}

void jackc_transport_t::tp_playrange(double t1, double t2)
{
  tp_stop();
  stop_at_time = 0;
  tp_locate(t1);
  // wait for one block:
  usleep((useconds_t)(1.0e6 * (double)fragsize / srate));
  stop_at_time = t2;
  tp_start();
}

void jackc_portless_t::tp_locate(double p)
{
  if(shutdown)
    throw TASCAR::ErrMsg("Jack server has shut down");
  jack_transport_locate(jc, (jack_nframes_t)(p * srate));
}

void jackc_portless_t::tp_locate(uint32_t p)
{
  if(shutdown)
    throw TASCAR::ErrMsg("Jack server has shut down");
  jack_transport_locate(jc, p);
}

void jackc_portless_t::tp_start()
{
  if(shutdown)
    throw TASCAR::ErrMsg("Jack server has shut down");
  jack_transport_start(jc);
}

void jackc_portless_t::tp_stop()
{
  if(shutdown)
    throw TASCAR::ErrMsg("Jack server has shut down");
  jack_transport_stop(jc);
}

std::string jackc_portless_t::get_client_name()
{
  if(shutdown)
    throw TASCAR::ErrMsg("Jack server has shut down");
  return jack_get_client_name(jc);
}

void* jackc_db_t::service(void* h)
{
  ((jackc_db_t*)h)->service();
  return NULL;
}

void jackc_db_t::service()
{
  mtx_inner_thread.lock();
  std::unique_lock<std::mutex> lk(mtx);
  while(!b_exit_thread) {
    cond.wait_for(lk, 100ms);
    if(active) {
      for(uint32_t kb = 0; kb < 2; kb++) {
        if(mutex_buffer[kb].try_lock()) {
          if(buffer_filled[kb]) {
            inner_process(inner_fragsize, dbinBuffer[kb], dboutBuffer[kb]);
            buffer_filled[kb] = false;
          }
          mutex_buffer[kb].unlock();
        }
      }
    }
  }
  mtx_inner_thread.unlock();
}

jackc_db_t::jackc_db_t(const std::string& clientname, jack_nframes_t infragsize)
    : jackc_t(clientname), inner_fragsize(infragsize),
      inner_is_larger(inner_fragsize > (jack_nframes_t)fragsize),
      current_buffer(0), b_exit_thread(false), inner_pos(0)
{
  buffer_filled[0] = false;
  buffer_filled[1] = false;
  if(inner_is_larger) {
    // check for integer ratio:
    ratio = inner_fragsize / fragsize;
    if(ratio * (jack_nframes_t)fragsize != inner_fragsize)
      throw TASCAR::ErrMsg(
          "Inner fragsize is not an integer multiple of fragsize.");
    // create extra thread:
    mutex_buffer[0].lock();
    if(0 != jack_client_create_thread(jc, &inner_thread,
                                      std::max(-1, rtprio - 1), (rtprio > 0),
                                      service, this))
      throw TASCAR::ErrMsg(
          "Unable to create inner processing thread with priority " +
          TASCAR::to_string(std::max(-1, rtprio - 1)) +
          " - check realtime settings.");
  } else {
    // check for integer ratio:
    ratio = fragsize / inner_fragsize;
    if((jack_nframes_t)fragsize != ratio * inner_fragsize)
      throw TASCAR::ErrMsg(
          "Fragsize is not an integer multiple of inner fragsize.");
  }
}

jackc_db_t::~jackc_db_t()
{
  b_exit_thread = true;
  if(inner_is_larger) {
    mtx_inner_thread.lock();
    mtx_inner_thread.unlock();
    for(uint32_t kb = 0; kb < 2; kb++) {
      mutex_buffer[kb].try_lock();
      mutex_buffer[kb].unlock();
      for(uint32_t k = 0; k < dbinBuffer[kb].size(); k++)
        delete[] dbinBuffer[kb][k];
      for(uint32_t k = 0; k < dboutBuffer[kb].size(); k++)
        delete[] dboutBuffer[kb][k];
    }
  }
}

jack_port_t* jackc_db_t::add_input_port(const std::string& name, bool is_midi)
{
  if(inner_is_larger) {
    // allocate buffer:
    for(uint32_t k = 0; k < 2; k++) {
      auto buf = new float[inner_fragsize];
      for(uint32_t k = 0; k < inner_fragsize; ++k)
        buf[k] = 0.0f;
      dbinBuffer[k].push_back(buf);
    }
  } else {
    for(uint32_t k = 0; k < 2; k++)
      dbinBuffer[k].push_back(NULL);
  }
  return jackc_t::add_input_port(name, is_midi);
}

jack_port_t* jackc_db_t::add_output_port(const std::string& name, bool is_midi)
{
  if(inner_is_larger) {
    // allocate buffer:
    for(uint32_t k = 0; k < 2; k++) {
      auto buf = new float[inner_fragsize];
      for(uint32_t k = 0; k < inner_fragsize; ++k)
        buf[k] = 0.0f;
      dboutBuffer[k].push_back(buf);
    }
  } else {
    for(uint32_t k = 0; k < 2; k++)
      dboutBuffer[k].push_back(NULL);
  }
  return jackc_t::add_output_port(name, is_midi);
}

int jackc_db_t::process(jack_nframes_t, const std::vector<float*>& inBuffer,
                        const std::vector<float*>& outBuffer)
{
  if(!active)
    return 0;
  int rv(0);
  if(inner_is_larger) {
    // copy data to buffer
    for(uint32_t k = 0; k < inBuffer.size(); k++)
      memcpy(&(dbinBuffer[current_buffer][k][inner_pos]), inBuffer[k],
             sizeof(float) * fragsize);
    // copy data from buffer
    for(uint32_t k = 0; k < outBuffer.size(); k++)
      memcpy(outBuffer[k], &(dboutBuffer[current_buffer][k][inner_pos]),
             sizeof(float) * fragsize);
    inner_pos += fragsize;
    // if buffer is full, lock other buffer and unlock current buffer
    if(inner_pos >= inner_fragsize) {
      uint32_t next_buffer((current_buffer + 1) % 2);
      // pthread_mutex_lock(&(mutex[next_buffer]));
      mutex_buffer[next_buffer].lock();
      buffer_filled[current_buffer] = true;
      mutex_buffer[current_buffer].unlock();
      // pthread_mutex_unlock(&(mutex[current_buffer]));
      current_buffer = next_buffer;
      inner_pos = 0;
      if(mtx.try_lock()) {
        mtx.unlock();
        cond.notify_one();
      }
    }
  } else {
    for(uint32_t kr = 0; kr < ratio; kr++) {
      for(uint32_t k = 0; k < inBuffer.size(); k++)
        dbinBuffer[0][k] = &(inBuffer[k][kr * fragsize]);
      for(uint32_t k = 0; k < outBuffer.size(); k++)
        dboutBuffer[0][k] = &(outBuffer[k][kr * fragsize]);
      rv = inner_process(inner_fragsize, dbinBuffer[0], dboutBuffer[0]);
    }
  }
  return rv;
}

bool test_for_jack_server()
{
  bool isrunning = true;
  try {
    jackc_portless_t jc("test_for_jack_server");
    jc.activate();
  }
  catch(...) {
    isrunning = false;
  }
  return isrunning;
}

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
