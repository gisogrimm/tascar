/**
 * @file osc_helper.h
 * @ingroup libtascar
 * @brief helper classes for OSC
 * @author Giso Grimm
 * @date 2012
 */
/* License (GPL)
 *
 * Copyright (C) 2018  Giso Grimm
 * Copyright (C) 2024  Giso Grimm
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

#ifndef OSC_HELPER_H
#define OSC_HELPER_H

#include "tscconfig.h"
#if defined(_WIN32) && !defined(WIN32)
#define WIN32 _WIN32 // liblo needs WIN32 defined in order to detect Windows
#endif
#include <atomic>
#include <condition_variable>
#include <lo/lo.h>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

typedef std::string(strcnvrt_t)(void*);

namespace TASCAR {

  class msg_t {
  public:
    msg_t(tsccfg::node_t);
    msg_t(const std::string&);
    msg_t(const msg_t&);
    ~msg_t();
    std::string path;
    lo_message msg;

  private:
  };

  /// OSC server
  class osc_server_t {
  public:
    class descriptor_t {
    public:
      std::string path;
      std::string relpath;
      std::string typespec;
      bool readable;
      std::string rangehint;
      std::string comment;
    };
    class data_element_t {
    public:
      data_element_t(const std::string& p, void* ptr_, strcnvrt_t* cnv_,
                     const std::string& type_);
      data_element_t();
      void* ptr;
      strcnvrt_t* cnv;
      std::string path;
      std::string name;
      std::string prefix;
      std::string type;
      std::string getstr() const { return cnv(ptr); };
    };
    osc_server_t(const std::string& multicast, const std::string& port,
                 const std::string& proto, bool verbose = true);
    ~osc_server_t();
    void set_prefix(const std::string& prefix);
    const std::string& get_prefix() const;
    /**
       \brief Register a method in the OSC server

       \param path OSC path (a prefix may be added internally, see
       \ref set_prefix() )

       \param typespec OSC types, a string consisting of the
       characters f(loat) d(ouble) i(nteger) s(tring)

       \param h Method handler

       \param user_data Pointer to user data

       \param visible Show method in variable list

       \param readable Indicate that a <path>/get method will be
       provided to read the value

       \param rangehint Range hint in the form [min,max], or [min,max[
       or ]min,max] or ]min,max[. Bool types use "bool", and string
       types "string" to indicate the range. The range hint is only
       used for visualization and does not imply an out-of-bounds
       checking.

       \param comment Help comment
     */
    void add_method(const std::string& path, const char* typespec,
                    lo_method_handler h, void* user_data, bool visible = true,
                    bool readable = false, const std::string& rangehint = "",
                    const std::string& comment = "");
    /** \brief Register a double variable for OSC access
        \param path OSC path
        \param data Pointer to data
     */
    void add_double(const std::string& path, double* data,
                    const std::string& range = "",
                    const std::string& comment = "");
    /** \brief Register a double variable for OSC access, convert from dB values

        In coming messages will be converted from dB to linear representation.

        \param path OSC path
        \param data Pointer to data
     */
    void add_double_db(const std::string& path, double* data,
                       const std::string& range = "[-40,10]",
                       const std::string& comment = "");
    /** \brief Register a double variable for OSC access, convert from dB SPL
       values

        In coming messages will be converted from dB SPL to Pascal
       representation.

        \param path OSC path
        \param data Pointer to data
     */
    void add_double_dbspl(const std::string& path, double* data,
                          const std::string& range = "[0,120]",
                          const std::string& comment = "");
    /** \brief Register a double variable for OSC access, convert from degree
       values

        In coming messages will be converted from degrees to radians.

        \param path OSC path
        \param data Pointer to data
     */
    void add_double_degree(const std::string& path, double* data,
                           const std::string& range = "[0,360]",
                           const std::string& comment = "");
    /** \brief Register a float variable for OSC access
        \param path OSC path
        \param data Pointer to data
     */
    void add_float(const std::string& path, float* data,
                   const std::string& range = "",
                   const std::string& comment = "");
    /** \brief Register a float variable for OSC access, convert from dB values

        In coming messages will be converted from dB to linear representation.

        \param path OSC path
        \param data Pointer to data
     */
    void add_float_db(const std::string& path, float* data,
                      const std::string& range = "[-40,10]",
                      const std::string& comment = "");
    void add_float_dbspl(const std::string& path, float* data,
                         const std::string& range = "[0,120]",
                         const std::string& comment = "");
    /** \brief Register a float variable for OSC access, convert from degree
       values

        In coming messages will be converted from degrees to radians.

        \param path OSC path
        \param data Pointer to data
     */
    void add_float_degree(const std::string& path, float* data,
                          const std::string& range = "[0,360]",
                          const std::string& comment = "");
    /** \brief Register a vector of floats variable for OSC access

        The dimension of the vector specifies the length of the
        message. Sending a message of different size will not have any
        effect, i.e., the callback handler will not change the size of
        the data.

        \param path OSC path
        \param data Pointer to data
     */
    void add_vector_float(const std::string& path, std::vector<float>* data,
                          const std::string& range = "",
                          const std::string& comment = "");
    /** \brief Register a vector of doubles variable for OSC access

        The dimension of the vector specifies the length of the
        message. Sending a message of different size will not have any
        effect, i.e., the callback handler will not change the size of
        the data.

        \param path OSC path
        \param data Pointer to data
     */
    void add_vector_double(const std::string& path, std::vector<double>* data,
                           const std::string& range = "",
                           const std::string& comment = "");
    /** \brief Register a vector of int32 variable for OSC access

        The dimension of the vector specifies the length of the
        message. Sending a message of different size will not have any
        effect, i.e., the callback handler will not change the size of
        the data.

        \param path OSC path
        \param data Pointer to data
     */
    void add_vector_int(const std::string& path, std::vector<int32_t>* data,
                        const std::string& range = "",
                        const std::string& comment = "");
    /** \brief Register a vector of floats variable for OSC access as dB SPL

        The dimension of the vector specifies the length of the
        message. Sending a message of different size will not have any
        effect, i.e., the callback handler will not change the size of
        the data.

        \param path OSC path
        \param data Pointer to data
     */
    void add_vector_float_dbspl(const std::string& path,
                                std::vector<float>* data,
                                const std::string& range = "[0,120]",
                                const std::string& comment = "");
    /** \brief Register a vector of floats variable for OSC access as dB (e.g.
       for gains)

        The dimension of the vector specifies the length of the
        message. Sending a message of different size will not have any
        effect, i.e., the callback handler will not change the size of
        the data.

        \param path OSC path
        \param data Pointer to data
     */
    void add_vector_float_db(const std::string& path, std::vector<float>* data,
                             const std::string& range = "[-40,10]",
                             const std::string& comment = "");
    void add_bool_true(const std::string& path, bool* data,
                       const std::string& comment = "");
    void add_bool_false(const std::string& path, bool* data,
                        const std::string& comment = "");
    void add_bool(const std::string& path, bool* data,
                  const std::string& comment = "");
    void add_int(const std::string& path, int32_t* data,
                 const std::string& range = "",
                 const std::string& comment = "");
    void add_uint(const std::string& path, uint32_t* data,
                  const std::string& range = "",
                  const std::string& comment = "");
    void add_string(const std::string& path, std::string* data,
                    const std::string& comment = "");
    void add_pos(const std::string& path, TASCAR::pos_t* data,
                 const std::string& range = "",
                 const std::string& comment = "");
    void activate();
    void deactivate();
    std::string list_variables() const;
    std::map<std::string, descriptor_t> get_variable_map() const;
    int dispatch_data(void* data, size_t size);
    int dispatch_data_message(const char* path, lo_message m);
    int get_srv_port() const { return lo_server_thread_get_port(lost); };
    std::vector<descriptor_t> variables;
    const std::string osc_srv_addr;
    const std::string osc_srv_port;
    const std::string& get_srv_url() const { return osc_srv_url; };
    void send_variable_list(const std::string& url, const std::string& path,
                            const std::string& prefix = "") const;
    /**
       @brief Return list of OSC variables with their current values
       as json expression


       @param prefix Optionally return only those variables with the
       path beginning with prefix
       @param asstring Return variable values as strings (default)
       @return json expression
     */
    std::string get_vars_as_json(std::string prefix = "",
                                 bool asstring = true) const;
    /**
       @brief Read a script file with OSC messages
     */
    void read_script_one(std::string filename);
    void read_script(const std::vector<std::string>& filenames);
    void read_script_async(const std::vector<std::string>& filenames);
    std::string scriptpath = "";
    std::string scriptext = "";
    bool scriptcancel = false;
    int scriptrunning = 0;
    /**
       @brief Dispatch all messages from the time interval tstart (included) to
       tend (excluded).
     */
    void timed_messages_process(double tstart, double tend);
    void timed_messages_clear();
    void timed_message_add(double time, const std::string& msgtext);

    void set_variable_owner(const std::string& owner);
    void unset_variable_owner();

    void generate_osc_documentation_files();

  private:
    void scriptthread_fun();
    std::string get_vars_as_json_rg(
        std::string prefix,
        std::map<std::string, data_element_t>::const_iterator& ibegin,
        std::map<std::string, data_element_t>::const_iterator iend,
        bool asstring) const;
    std::string osc_srv_url;
    std::string prefix;
    lo_server_thread lost;
    std::atomic_bool initialized;
    std::atomic_bool isactive;
    bool verbose = false;
    std::map<std::string, data_element_t> datamap;
    std::atomic<bool> runscriptthread;
    std::atomic<bool> cancelscript;
    std::thread scriptthread;
    std::mutex mtxscript;
    std::mutex mtxscriptnames;
    std::vector<std::string> nextscripts;
    std::condition_variable cond_var_script;
    std::mutex mtxdispatch;
    std::map<double, std::vector<msg_t>> timed_messages;
    std::mutex mtxtimedmessages;
    std::map<std::string, std::map<std::string, descriptor_t>> owned_vars;
    std::string varowner;
  };

}; // namespace TASCAR

#endif

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
