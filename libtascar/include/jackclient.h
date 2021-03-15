/**
 * @file jackclient.h
 * @ingroup libtascar
 * @brief C++ wrapper for the JACK API.
 * @author Giso Grimm
 * @date 2011
 */
/* License (GPL)
 *
 * Copyright (C) 2018  Giso Grimm
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

#ifndef JACKCLIENT_H
#define JACKCLIENT_H

#include <stdint.h>
#include <jack/jack.h>
#include <string>
#include <vector>
#include <pthread.h>

class jackc_portless_t {
public:
  jackc_portless_t(const std::string& clientname);
  virtual ~jackc_portless_t();
  void tp_start();
  void tp_stop();
  void tp_locate(uint32_t p);
  void tp_locate(double t_sec);
  uint32_t tp_get_frame() const;
  double tp_get_time() const;
  void activate();
  void deactivate();
  std::string get_client_name();
  int get_srate() { return srate; };
  int get_fragsize() { return fragsize; };
  void connect(const std::string& src, const std::string& dest, bool btry=false, bool allowoutputsource=false, bool connectmulti=false);
  uint32_t get_xruns() const { return xruns;};
  uint32_t get_xrun_latency() const { return xrun_latency;};
  float get_cpu_load() const { return jack_cpu_load(jc);};
  std::vector<std::string> get_port_names_regexp( const std::string& name, int flags = 0 ) const;
  std::vector<std::string> get_port_names_regexp( const std::vector<std::string>& names, int flags = 0 ) const;
private:
  static int xrun_callback(void *arg);
  static void on_shutdown(void *arg);
public:
  jack_client_t* jc;
  int srate;
  int fragsize;
  int rtprio;
  bool active;
  uint32_t xruns;
  double xrun_latency;
};

/**
   \brief Jack client C++ wrapper
   \ingroup libtascar
*/
class jackc_t : public jackc_portless_t {
public:
  jackc_t(const std::string& clientname);
  virtual ~jackc_t();
  virtual void add_input_port(const std::string& name);
  virtual void add_output_port(const std::string& name);
  void connect_in(unsigned int port,const std::string& pname,bool btry=false, bool allowoutputsource=false);
  void connect_out(unsigned int port,const std::string& pname,bool btry=false);
  size_t get_num_input_ports() const {return inPort.size();};
  size_t get_num_output_ports() const {return outPort.size();};
  std::vector<std::string> get_input_ports() const { return input_port_names; };
  std::vector<std::string> get_output_ports() const { return output_port_names; };
protected:
  virtual int process(jack_nframes_t nframes,const std::vector<float*>& inBuffer,const std::vector<float*>& outBuffer) { return 0; };
private:
  static int process_(jack_nframes_t nframes, void *arg);
  int process_(jack_nframes_t nframes);
private:
  std::vector<jack_port_t*> inPort;
  std::vector<jack_port_t*> outPort;
  std::vector<float*> inBuffer;
  std::vector<float*> outBuffer;
  std::vector<std::string> input_port_names;
  std::vector<std::string> output_port_names;
};

class jackc_db_t : public jackc_t {
public:
  jackc_db_t(const std::string& clientname,jack_nframes_t fragsize);
  virtual ~jackc_db_t();
  virtual void add_input_port(const std::string& name);
  virtual void add_output_port(const std::string& name);
protected:
  virtual int inner_process(jack_nframes_t nframes,const std::vector<float*>& inBuffer,const std::vector<float*>& outBuffer) { return 0; };
  virtual int process(jack_nframes_t nframes,const std::vector<float*>& inBuffer,const std::vector<float*>& outBuffer);
private:
  static void * service(void* h);
  void service();
  std::vector<float*> dbinBuffer[2];
  std::vector<float*> dboutBuffer[2];
  jack_nframes_t inner_fragsize;
  bool inner_is_larger;
  uint32_t ratio;
  jack_native_thread_t inner_thread;
  pthread_mutex_t mutex[2];
  pthread_mutex_t mtx_inner_thread;
  bool buffer_filled[2];
  uint32_t current_buffer;
  bool b_exit_thread;
  uint32_t inner_pos;
};

class jackc_transport_t : public jackc_t {
public:
  jackc_transport_t(const std::string& clientname);
  void tp_playrange( double t1, double t2 );
protected:
  int process(jack_nframes_t nframes,const std::vector<float*>& inBuffer,const std::vector<float*>& outBuffer);
  virtual int process(jack_nframes_t nframes,const std::vector<float*>& inBuffer,const std::vector<float*>& outBuffer,uint32_t tp_frame, bool tp_rolling) = 0;
private:
  double stop_at_time;
};

#endif

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */

