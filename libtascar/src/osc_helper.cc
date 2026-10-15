/*
 * This file is part of the TASCAR software, see <http://tascar.org/>
 *
 * Copyright (c) 2018 Giso Grimm
 * Copyright (c) 2019 Giso Grimm
 * Copyright (c) 2021 Giso Grimm
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

#include "osc_helper.h"
#include "defs.h"
#include "errorhandling.h"
#include <fstream>
#include <map>
#include <math.h>
#include <unistd.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib") // Tells linker to use Winsock library
typedef SOCKET socket_t;
#define CLOSE_SOCKET closesocket
#else
#include <netinet/in.h>
#include <sys/socket.h>
typedef int socket_t;
#define CLOSE_SOCKET close
#endif

using namespace TASCAR;

static bool liblo_errflag;

std::string str_get_null(void*)
{
  return "null";
}

void osc_server_t::start_tcp_script_server(int port)
{
  run_tcp_server = true;
  tcp_server_thread =
      std::thread(&osc_server_t::tcp_script_thread_fun, this, port);
}

void osc_server_t::stop_tcp_script_server()
{
  run_tcp_server = false;
  if(tcp_server_thread.joinable())
    tcp_server_thread.join();
}

void osc_server_t::tcp_script_thread_fun(int port)
{
#ifdef _WIN32
  WSADATA wsaData;
  if(WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    return;
#endif

  socket_t server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if(server_fd == (socket_t)-1) {
#ifdef _WIN32
    WSACleanup();
#endif
    return;
  }

  int opt = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

  sockaddr_in address;
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(port);

  if(bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
    TASCAR::add_warning("bind failed while setting up TOSC TCP server on port " + std::to_string(port));
    CLOSE_SOCKET(server_fd); // Use the macro we defined
#ifdef _WIN32
    WSACleanup();
#endif
    return;
  }
  listen(server_fd, 3);

  while(run_tcp_server) {
    struct timeval tv{0, 100000}; // 1 second timeout to check run_tcp_server
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(server_fd, &readfds);

    if(select(server_fd + 1, &readfds, NULL, NULL, &tv) > 0) {
      socket_t client_socket = accept(server_fd, NULL, NULL);
      if(client_socket < 0)
        continue;

      // --- START TIMEOUT CONFIGURATION ---
      // Set a receive timeout so recv() doesn't block forever
      // This allows the loop to check 'run_tcp_server' every 1 second
#ifdef _WIN32
      DWORD timeout = 100; // Milliseconds
      setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
#else
      struct timeval timeout_tv{0, 100000}; // 1 second
      setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout_tv, sizeof(timeout_tv));
#endif
      // --- END TIMEOUT CONFIGURATION ---

      char buffer[4096];
      std::string line_acc;
      tictoc_t tictoc;

      while(run_tcp_server) {
        ssize_t bytes = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        
        if(bytes > 0) {
          buffer[bytes] = '\0';
          line_acc += buffer;
          size_t pos;
          while((pos = line_acc.find('\n')) != std::string::npos) {
            std::string line = line_acc.substr(0, pos);
            if(!line.empty() && line.back() == '\r')
              line.pop_back();
            process_script_line(line, tictoc);
            line_acc.erase(0, pos + 1);
          }
        } else if (bytes == 0) {
          // Client closed connection
          break;
        } else {
          // bytes < 0: could be a real error or just a timeout
#ifdef _WIN32
          int err = WSAGetLastError();
          if (err != WSAETIMEDOUT) break; 
#else
          if (errno != EAGAIN && errno != EWOULDBLOCK) break;
#endif
          // If it was just a timeout, the loop continues and checksL 
          // checks 'run_tcp_server' again.
        }
      }
      CLOSE_SOCKET(client_socket);
    }
  }
  CLOSE_SOCKET(server_fd);
#ifdef _WIN32
  WSACleanup();
#endif
}

TASCAR::osc_server_t::data_element_t::data_element_t()
    : ptr(NULL), cnv(str_get_null)
{
}

TASCAR::osc_server_t::data_element_t::data_element_t(const std::string& p,
                                                     void* ptr_,
                                                     strcnvrt_t* cnv_,
                                                     const std::string& type_)
    : ptr(ptr_), cnv(cnv_), path(p), type(type_)
{
  size_t pos = path.rfind("/");
  if(pos != std::string::npos) {
    name = path.substr(pos + 1);
    prefix = path.substr(0, pos);
  } else {
    prefix = "";
    name = path;
  }
}

void err_handler(int num, const char* msg, const char* where)
{
  liblo_errflag = true;
  std::string smsg;
  if(msg)
    smsg = msg;
  std::string swhere = "null";
  if(where)
    swhere = where;
  std::cout << "liblo error " << num << ": " << smsg << "\n(" << swhere
            << ")\n";
}

int osc_set_bool_true(const char*, const char*, lo_arg**, int, lo_message,
                      void* user_data)
{
  if(user_data)
    *(bool*)(user_data) = true;
  return 1;
}

int osc_set_bool_false(const char*, const char*, lo_arg**, int, lo_message,
                       void* user_data)
{
  if(user_data)
    *(bool*)(user_data) = false;
  return 1;
}

int osc_set_float(const char*, const char* types, lo_arg** argv, int argc,
                  lo_message, void* user_data)
{
  if(user_data && (argc == 1) && (types[0] == 'f'))
    *(float*)(user_data) = argv[0]->f;
  return 1;
}

int osc_set_pos(const char*, const char* types, lo_arg** argv, int argc,
                lo_message, void* user_data)
{
  if(user_data && (argc == 3) && (types[0] == 'f') && (types[1] == 'f') &&
     (types[2] == 'f')) {
    TASCAR::pos_t* p = (TASCAR::pos_t*)(user_data);
    p->x = argv[0]->f;
    p->y = argv[1]->f;
    p->z = argv[2]->f;
  }
  return 1;
}

std::string str_get_float(void* data)
{
  return TASCAR::to_string(*(float*)(data));
}

std::string str_get_pos(void* data)
{
  return TASCAR::to_string(*(TASCAR::pos_t*)(data));
}

int osc_get_float(const char* path, const char* types, lo_arg** argv, int argc,
                  lo_message, void* user_data)
{
  if(user_data && (argc == 2) && (types[0] == 's') && (types[1] == 's')) {
    lo_address target = lo_address_new_from_url(&(argv[0]->s));
    if(target) {
      std::string npath(path);
      if(npath.size() > 4)
        npath = npath.substr(0, npath.size() - 4);
      float* p = (float*)(user_data);
      lo_send(target, &(argv[1]->s), "sf", npath.c_str(), *p);
      lo_address_free(target);
    }
  }
  return 1;
}

int osc_get_pos(const char* path, const char* types, lo_arg** argv, int argc,
                lo_message, void* user_data)
{
  if(user_data && (argc == 2) && (types[0] == 's') && (types[1] == 's')) {
    lo_address target = lo_address_new_from_url(&(argv[0]->s));
    if(target) {
      std::string npath(path);
      if(npath.size() > 4)
        npath = npath.substr(0, npath.size() - 4);
      TASCAR::pos_t* pos = (TASCAR::pos_t*)(user_data);
      lo_send(target, &(argv[1]->s), "sfff", npath.c_str(), pos->x, pos->y,
              pos->z);
      lo_address_free(target);
    }
  }
  return 1;
}

int osc_set_float_db(const char*, const char* types, lo_arg** argv, int argc,
                     lo_message, void* user_data)
{
  if(user_data && (argc == 1) && (types[0] == 'f'))
    *(float*)(user_data) = powf(10.0f, 0.05 * argv[0]->f);
  return 1;
}

int osc_get_float_db(const char* path, const char* types, lo_arg** argv,
                     int argc, lo_message, void* user_data)
{
  if(user_data && (argc == 2) && (types[0] == 's') && (types[1] == 's')) {
    lo_address target = lo_address_new_from_url(&(argv[0]->s));
    if(target) {
      std::string npath(path);
      if(npath.size() > 4)
        npath = npath.substr(0, npath.size() - 4);
      lo_send(target, &(argv[1]->s), "sf", npath.c_str(),
              20.0f * log10f(*(float*)(user_data)));
      lo_address_free(target);
    }
  }
  return 1;
}

std::string str_get_float_db(void* data)
{
  return TASCAR::to_string(20.0f * log10f(*(float*)(data)));
}

int osc_set_float_dbspl(const char*, const char* types, lo_arg** argv, int argc,
                        lo_message, void* user_data)
{
  if(user_data && (argc == 1) && (types[0] == 'f'))
    *(float*)(user_data) = powf(10.0f, 0.05 * argv[0]->f) * 2e-5f;
  return 1;
}

std::string str_get_float_dbspl(void* data)
{
  return TASCAR::to_string(20.0f * log10f(*(float*)(data) * 5.0e4f));
}

int osc_get_float_dbspl(const char* path, const char* types, lo_arg** argv,
                        int argc, lo_message, void* user_data)
{
  if(user_data && (argc == 2) && (types[0] == 's') && (types[1] == 's')) {
    lo_address target = lo_address_new_from_url(&(argv[0]->s));
    if(target) {
      std::string npath(path);
      if(npath.size() > 4)
        npath = npath.substr(0, npath.size() - 4);
      lo_send(target, &(argv[1]->s), "sf", npath.c_str(),
              20.0f * log10f(*(float*)(user_data) * 5.0e4f));
      lo_address_free(target);
    }
  }
  return 1;
}

int osc_set_vector_float(const char*, const char*, lo_arg** argv, int argc,
                         lo_message, void* user_data)
{
  if(user_data) {
    std::vector<float>* data((std::vector<float>*)user_data);
    if(argc == (int)(data->size()))
      for(int k = 0; k < argc; ++k)
        (*data)[k] = argv[k]->f;
  }
  return 1;
}

int osc_set_vector_float_dbspl(const char*, const char*, lo_arg** argv,
                               int argc, lo_message, void* user_data)
{
  if(user_data) {
    std::vector<float>* data((std::vector<float>*)user_data);
    if(argc == (int)(data->size()))
      for(int k = 0; k < argc; ++k)
        (*data)[k] = powf(10.0f, 0.05f * argv[k]->f) * 2e-5f;
  }
  return 1;
}

int osc_set_vector_float_db(const char*, const char*, lo_arg** argv, int argc,
                            lo_message, void* user_data)
{
  if(user_data) {
    std::vector<float>* data((std::vector<float>*)user_data);
    if(argc == (int)(data->size()))
      for(int k = 0; k < argc; ++k)
        (*data)[k] = powf(10.0f, 0.05f * argv[k]->f);
  }
  return 1;
}

int osc_set_vector_double(const char*, const char*, lo_arg** argv, int argc,
                          lo_message, void* user_data)
{
  if(user_data) {
    std::vector<double>* data((std::vector<double>*)user_data);
    if(argc == (int)(data->size()))
      for(int k = 0; k < argc; ++k)
        (*data)[k] = argv[k]->f;
  }
  return 1;
}

int osc_set_vector_int(const char*, const char*, lo_arg** argv, int argc,
                       lo_message, void* user_data)
{
  if(user_data) {
    std::vector<int32_t>* data((std::vector<int32_t>*)user_data);
    if(argc == (int)(data->size()))
      for(int k = 0; k < argc; ++k)
        (*data)[k] = argv[k]->i;
  }
  return 1;
}

int osc_set_double_db(const char*, const char* types, lo_arg** argv, int argc,
                      lo_message, void* user_data)
{
  if(user_data && (argc == 1) && (types[0] == 'f'))
    *(double*)(user_data) = pow(10.0, 0.05 * argv[0]->f);
  return 1;
}

std::string str_get_double_db(void* data)
{
  return TASCAR::to_string(20.0 * log10(*(double*)(data)));
}

int osc_get_double_db(const char* path, const char* types, lo_arg** argv,
                      int argc, lo_message, void* user_data)
{
  if(user_data && (argc == 2) && (types[0] == 's') && (types[1] == 's')) {
    lo_address target = lo_address_new_from_url(&(argv[0]->s));
    if(target) {
      std::string npath(path);
      if(npath.size() > 4)
        npath = npath.substr(0, npath.size() - 4);
      lo_send(target, &(argv[1]->s), "sf", npath.c_str(),
              20.0f * log10f(*(double*)(user_data)));
      lo_address_free(target);
    }
  }
  return 1;
}

std::string str_get_double_dbspl(void* data)
{
  return TASCAR::to_string(20.0 * log10(*(double*)(data) * 5e4));
}

int osc_set_double_dbspl(const char*, const char* types, lo_arg** argv,
                         int argc, lo_message, void* user_data)
{
  if(user_data && (argc == 1) && (types[0] == 'f'))
    *(double*)(user_data) = pow(10.0, 0.05 * argv[0]->f) * 2e-5;
  return 1;
}

int osc_get_double_dbspl(const char* path, const char* types, lo_arg** argv,
                         int argc, lo_message, void* user_data)
{
  if(user_data && (argc == 2) && (types[0] == 's') && (types[1] == 's')) {
    lo_address target = lo_address_new_from_url(&(argv[0]->s));
    if(target) {
      std::string npath(path);
      if(npath.size() > 4)
        npath = npath.substr(0, npath.size() - 4);
      lo_send(target, &(argv[1]->s), "sf", npath.c_str(),
              20.0f * log10f(*(double*)(user_data) * 5.0e4f));
      lo_address_free(target);
    }
  }
  return 1;
}

int osc_set_float_degree(const char*, const char* types, lo_arg** argv,
                         int argc, lo_message, void* user_data)
{
  if(user_data && (argc == 1) && (types[0] == 'f'))
    *(float*)(user_data) = DEG2RADf * argv[0]->f;
  return 1;
}

std::string str_get_float_degree(void* data)
{
  return TASCAR::to_string(RAD2DEGf * *(float*)(data));
}

int osc_get_float_degree(const char* path, const char* types, lo_arg** argv,
                         int argc, lo_message, void* user_data)
{
  if(user_data && (argc == 2) && (types[0] == 's') && (types[1] == 's')) {
    lo_address target = lo_address_new_from_url(&(argv[0]->s));
    if(target) {
      std::string npath(path);
      if(npath.size() > 4)
        npath = npath.substr(0, npath.size() - 4);
      lo_send(target, &(argv[1]->s), "sf", npath.c_str(),
              RAD2DEGf * *(float*)(user_data));
      lo_address_free(target);
    }
  }
  return 1;
}

int osc_set_double_degree(const char*, const char* types, lo_arg** argv,
                          int argc, lo_message, void* user_data)
{
  if(user_data && (argc == 1) && (types[0] == 'f'))
    *(double*)(user_data) = DEG2RAD * argv[0]->f;
  return 1;
}

std::string str_get_double_degree(void* data)
{
  return TASCAR::to_string(RAD2DEG * *(double*)(data));
}

int osc_get_double_degree(const char* path, const char* types, lo_arg** argv,
                          int argc, lo_message, void* user_data)
{
  if(user_data && (argc == 2) && (types[0] == 's') && (types[1] == 's')) {
    lo_address target = lo_address_new_from_url(&(argv[0]->s));
    if(target) {
      std::string npath(path);
      if(npath.size() > 4)
        npath = npath.substr(0, npath.size() - 4);
      lo_send(target, &(argv[1]->s), "sf", npath.c_str(),
              RAD2DEGf * *(double*)(user_data));
      lo_address_free(target);
    }
  }
  return 1;
}

int osc_set_double(const char*, const char* types, lo_arg** argv, int argc,
                   lo_message, void* user_data)
{
  if(user_data && (argc == 1) && (types[0] == 'f'))
    *(double*)(user_data) = argv[0]->f;
  return 1;
}

std::string str_get_double(void* data)
{
  return TASCAR::to_string(*(double*)(data));
}

int osc_get_double(const char* path, const char* types, lo_arg** argv, int argc,
                   lo_message, void* user_data)
{
  if(user_data && (argc == 2) && (types[0] == 's') && (types[1] == 's')) {
    lo_address target = lo_address_new_from_url(&(argv[0]->s));
    if(target) {
      std::string npath(path);
      if(npath.size() > 4)
        npath = npath.substr(0, npath.size() - 4);
      lo_send(target, &(argv[1]->s), "sf", npath.c_str(),
              *(double*)(user_data));
      lo_address_free(target);
    }
  }
  return 1;
}

int osc_set_int32(const char*, const char* types, lo_arg** argv, int argc,
                  lo_message, void* user_data)
{
  if(user_data && (argc == 1) && (types[0] == 'i'))
    *(int32_t*)(user_data) = argv[0]->i;
  return 1;
}

int osc_get_int32(const char* path, const char* types, lo_arg** argv, int argc,
                  lo_message, void* user_data)
{
  if(user_data && (argc == 2) && (types[0] == 's') && (types[1] == 's')) {
    lo_address target = lo_address_new_from_url(&(argv[0]->s));
    if(target) {
      std::string npath(path);
      if(npath.size() > 4)
        npath = npath.substr(0, npath.size() - 4);
      lo_send(target, &(argv[1]->s), "si", npath.c_str(),
              *(int32_t*)(user_data));
      lo_address_free(target);
    }
  }
  return 1;
}

int osc_set_uint32(const char*, const char* types, lo_arg** argv, int argc,
                   lo_message, void* user_data)
{
  if(user_data && (argc == 1) && (types[0] == 'i'))
    *(uint32_t*)(user_data) = (uint32_t)(argv[0]->i);
  return 1;
}

std::string str_get_uint(void* data)
{
  return TASCAR::to_string(*(uint32_t*)(data));
}

std::string str_get_int(void* data)
{
  return TASCAR::to_string(*(int32_t*)(data));
}

std::string str_get_string(void* data)
{
  return *(std::string*)(data);
}

int osc_get_uint32(const char* path, const char* types, lo_arg** argv, int argc,
                   lo_message, void* user_data)
{
  if(user_data && (argc == 2) && (types[0] == 's') && (types[1] == 's')) {
    lo_address target = lo_address_new_from_url(&(argv[0]->s));
    if(target) {
      std::string npath(path);
      if(npath.size() > 4)
        npath = npath.substr(0, npath.size() - 4);
      lo_send(target, &(argv[1]->s), "si", npath.c_str(),
              *(uint32_t*)(user_data));
      lo_address_free(target);
    }
  }
  return 1;
}

int osc_set_bool(const char*, const char* types, lo_arg** argv, int argc,
                 lo_message, void* user_data)
{
  if(user_data && (argc == 1) && (types[0] == 'i'))
    *(bool*)(user_data) = (argv[0]->i != 0);
  return 1;
}

std::string str_get_bool(void* data)
{
  return TASCAR::to_string_bool(*(bool*)(data));
}

int osc_get_bool(const char* path, const char* types, lo_arg** argv, int argc,
                 lo_message, void* user_data)
{
  if(user_data && (argc == 2) && (types[0] == 's') && (types[1] == 's')) {
    lo_address target = lo_address_new_from_url(&(argv[0]->s));
    if(target) {
      std::string npath(path);
      if(npath.size() > 4)
        npath = npath.substr(0, npath.size() - 4);
      lo_send(target, &(argv[1]->s), "si", npath.c_str(), *(bool*)(user_data));
      lo_address_free(target);
    }
  }
  return 1;
}

int osc_set_string(const char*, const char* types, lo_arg** argv, int argc,
                   lo_message, void* user_data)
{
  if(user_data && (argc == 1) && (types[0] == 's'))
    *(std::string*)(user_data) = &(argv[0]->s);
  return 1;
}

int osc_get_string(const char* path, const char* types, lo_arg** argv, int argc,
                   lo_message, void* user_data)
{
  if(user_data && (argc == 2) && (types[0] == 's') && (types[1] == 's')) {
    lo_address target = lo_address_new_from_url(&(argv[0]->s));
    if(target) {
      std::string npath(path);
      if(npath.size() > 4)
        npath = npath.substr(0, npath.size() - 4);
      lo_send(target, &(argv[1]->s), "ss", npath.c_str(),
              ((std::string*)(user_data))->c_str());
      lo_address_free(target);
    }
  }
  return 1;
}

int osc_send_variables(const char*, const char* types, lo_arg** argv, int argc,
                       lo_message, void* user_data)
{
  if(user_data && (argc == 2) && (types[0] == 's') && (types[1] == 's')) {
    osc_server_t* srv(reinterpret_cast<osc_server_t*>(user_data));
    srv->send_variable_list(&(argv[0]->s), &(argv[1]->s));
  }
  if(user_data && (argc == 3) && (types[0] == 's') && (types[1] == 's') &&
     (types[2] == 's')) {
    osc_server_t* srv(reinterpret_cast<osc_server_t*>(user_data));
    srv->send_variable_list(&(argv[0]->s), &(argv[1]->s), &(argv[2]->s));
  }
  return 1;
}

int osc_tm_add(const char*, const char* types, lo_arg** argv, int argc,
               lo_message, void* user_data)
{
  if(user_data && (argc == 2) && (types[0] == 'f') && (types[1] == 's')) {
    osc_server_t* srv(reinterpret_cast<osc_server_t*>(user_data));
    srv->timed_message_add(argv[0]->f, &(argv[1]->s));
  }
  return 1;
}

int osc_tm_clear(const char*, const char*, lo_arg**, int argc, lo_message,
                 void* user_data)
{
  if(user_data && (argc == 0)) {
    osc_server_t* srv(reinterpret_cast<osc_server_t*>(user_data));
    srv->timed_messages_clear();
  }
  return 1;
}

int string2proto(const std::string& proto)
{
  if(proto == "UDP")
    return LO_UDP;
  if(proto == "TCP")
    return LO_TCP;
  if(proto == "UNIX")
    return LO_UNIX;
  throw TASCAR::ErrMsg("Invalid OSC protocol name \"" + proto + "\".");
}

osc_server_t::osc_server_t(const std::string& multicast,
                           const std::string& port, const std::string& proto,
                           bool verbose_)
    : osc_srv_addr(multicast), osc_srv_port(port), verbose(verbose_)
{
  initialized = false;
  isactive = false;
  runscriptthread = true;
  cancelscript = false;
  scriptthread = std::thread(&osc_server_t::scriptthread_fun, this);
  lost = NULL;
  liblo_errflag = false;
  if(port.size() && (port != "none")) {
    if(multicast.size()) {
      if(port == "auto")
        lost = lo_server_thread_new_multicast(multicast.c_str(), NULL,
                                              err_handler);
      else
        lost = lo_server_thread_new_multicast(multicast.c_str(), port.c_str(),
                                              err_handler);
      initialized = true;
    } else {
      if(port == "auto")
        lost = lo_server_thread_new_with_proto(NULL, string2proto(proto),
                                               err_handler);
      else
        lost = lo_server_thread_new_with_proto(
            port.c_str(), string2proto(proto), err_handler);
      initialized = true;
    }
    if((!lost) || liblo_errflag) {
      throw ErrMsg("liblo error (srv_addr: \"" + multicast + "\" srv_port: \"" +
                   port + "\" " + proto + ").");
    }
  }
  if(lost) {
    char* ctmp(lo_server_thread_get_url(lost));
    if(ctmp) {
      osc_srv_url = ctmp;
      free(ctmp);
    }
    if(verbose)
      std::cerr << "listening on \"" << osc_srv_url << "\"" << std::endl;
  }
  set_variable_owner("session_t");
  add_method("/sendvarsto", "ss", osc_send_variables, this);
  add_method("/sendvarsto", "sss", osc_send_variables, this);
  add_method("/timedmessages/add", "fs", osc_tm_add, this);
  add_method("/timedmessages/clear", "", osc_tm_clear, this);
  unset_variable_owner();
}

int osc_server_t::dispatch_data(void* data, size_t size)
{
  if(!isactive)
    return 0;
  // std::lock_guard<std::mutex> lk{mtxdispatch};
  lo_server srv(lo_server_thread_get_server(lost));
  return lo_server_dispatch_data(srv, data, size);
}

int osc_server_t::dispatch_data_message(const char* path, lo_message m)
{
  if(!isactive)
    return 0;

  size_t len = lo_message_length(m, path);

  // Lock the buffer to ensure thread safety
  std::lock_guard<std::mutex> lock(mtx_dispatch_buffer);

  // Only resize if the current buffer is too small.
  // This avoids repeated allocations.
  if(dispatch_buffer.size() < (len + 256)) {
    dispatch_buffer.resize(len + 256);
  }

  size_t sdatasize = 0;
  char* sdata = static_cast<char*>(
      lo_message_serialise(m, path, dispatch_buffer.data(), &sdatasize));
  // char* sdata = lo_message_serialise(m, path, dispatch_buffer.data(),
  // &sdatasize);

  if(!sdata)
    return -1;

  return dispatch_data(sdata, sdatasize);
}

// int osc_server_t::dispatch_data_message(const char* path, lo_message m)
//{
//   if(!isactive)
//     return 0;
//   size_t len(lo_message_length(m, path));
//   char data[len + 256];
//   size_t sdatasize = 0;
//   char* sdata = (char*)lo_message_serialise(m, path, data, &sdatasize);
//   return dispatch_data(sdata, sdatasize);
// }

osc_server_t::~osc_server_t()
{
  // first stop all running scripts:
  runscriptthread = false;
  {
    std::lock_guard<std::mutex> lk{mtxscriptnames};
    nextscripts.clear();
  }
  cond_var_script.notify_one();
  if(scriptthread.joinable())
    scriptthread.join();
  if(isactive) {
    deactivate();
  }
  stop_tcp_script_server();
  if(initialized) {
    lo_server_thread_free(lost);
  }
}

void osc_server_t::set_prefix(const std::string& prefix_)
{
  prefix = prefix_;
}

void osc_server_t::add_method(const std::string& path, const char* typespec,
                              lo_method_handler h, void* user_data,
                              bool visible, bool readable,
                              const std::string& rangehint,
                              const std::string& comment)
{
  if(initialized) {
    std::string sPath(prefix + path);
    if(verbose && visible) {
      std::cerr << "added handler " << sPath;
      if(typespec)
        std::cerr << " with typespec \"" << typespec << "\"";
      std::cerr << std::endl;
    }
    if(sPath.empty())
      lo_server_thread_add_method(lost, NULL, typespec, h, user_data);
    else
      lo_server_thread_add_method(lost, sPath.c_str(), typespec, h, user_data);
    if(visible) {
      descriptor_t d;
      d.relpath = path;
      d.path = sPath;
      if(typespec)
        d.typespec = typespec;
      else
        d.typespec = "(any)";
      d.readable = readable;
      d.rangehint = rangehint;
      d.comment = comment;
      variables.push_back(d);
      if(!varowner.empty())
        owned_vars[varowner][path + d.typespec] = d;
      else {
        owned_vars["undocumented"][path + d.typespec] = d;
      }
    }
  }
}

void osc_server_t::add_float(const std::string& path, float* data,
                             const std::string& range,
                             const std::string& comment)
{
  add_method(path, "f", osc_set_float, data, true, true, range, comment);
  add_method(path + "/get", "ss", osc_get_float, data, false);
  datamap[prefix + path] =
      data_element_t(prefix + path, data, str_get_float, "float");
}

void osc_server_t::add_double(const std::string& path, double* data,
                              const std::string& range,
                              const std::string& comment)
{
  add_method(path, "f", osc_set_double, data, true, true, range, comment);
  add_method(path + "/get", "ss", osc_get_double, data, false);
  datamap[prefix + path] =
      data_element_t(prefix + path, data, str_get_double, "double");
}

void osc_server_t::add_pos(const std::string& path, TASCAR::pos_t* data,
                           const std::string& range, const std::string& comment)
{
  add_method(path, "fff", osc_set_pos, data, true, true, range, comment);
  add_method(path + "/get", "ss", osc_get_pos, data, false);
  datamap[prefix + path] =
      data_element_t(prefix + path, data, str_get_pos, "pos");
}

void osc_server_t::add_float_db(const std::string& path, float* data,
                                const std::string& range,
                                const std::string& comment)
{
  add_method(path, "f", osc_set_float_db, data, true, true, range, comment);
  add_method(path + "/get", "ss", osc_get_float_db, data, false);
  datamap[prefix + path] =
      data_element_t(prefix + path, data, str_get_float_db, "float");
}

void osc_server_t::add_float_dbspl(const std::string& path, float* data,
                                   const std::string& range,
                                   const std::string& comment)
{
  add_method(path, "f", osc_set_float_dbspl, data, true, true, range, comment);
  add_method(path + "/get", "ss", osc_get_float_dbspl, data, false);
  datamap[prefix + path] =
      data_element_t(prefix + path, data, str_get_float_dbspl, "float");
}

void osc_server_t::add_vector_float_dbspl(const std::string& path,
                                          std::vector<float>* data,
                                          const std::string& range,
                                          const std::string& comment)
{
  add_method(path, std::string(data->size(), 'f').c_str(),
             osc_set_vector_float_dbspl, data, true, false, range, comment);
}

void osc_server_t::add_vector_float_db(const std::string& path,
                                       std::vector<float>* data,
                                       const std::string& range,
                                       const std::string& comment)
{
  add_method(path, std::string(data->size(), 'f').c_str(),
             osc_set_vector_float_db, data, true, false, range, comment);
}

void osc_server_t::add_vector_float(const std::string& path,
                                    std::vector<float>* data,
                                    const std::string& range,
                                    const std::string& comment)
{
  add_method(path, std::string(data->size(), 'f').c_str(), osc_set_vector_float,
             data, true, false, range, comment);
}

void osc_server_t::add_vector_double(const std::string& path,
                                     std::vector<double>* data,
                                     const std::string& range,
                                     const std::string& comment)
{
  add_method(path, std::string(data->size(), 'f').c_str(),
             osc_set_vector_double, data, true, false, range, comment);
}

void osc_server_t::add_vector_int(const std::string& path,
                                  std::vector<int32_t>* data,
                                  const std::string& range,
                                  const std::string& comment)
{
  add_method(path, std::string(data->size(), 'i').c_str(), osc_set_vector_int,
             data, true, false, range, comment);
}

void osc_server_t::add_double_db(const std::string& path, double* data,
                                 const std::string& range,
                                 const std::string& comment)
{
  add_method(path, "f", osc_set_double_db, data, true, true, range, comment);
  add_method(path + "/get", "ss", osc_get_double_db, data, false);
  datamap[prefix + path] =
      data_element_t(prefix + path, data, str_get_double_db, "double");
}

void osc_server_t::add_double_dbspl(const std::string& path, double* data,
                                    const std::string& range,
                                    const std::string& comment)
{
  add_method(path, "f", osc_set_double_dbspl, data, true, true, range, comment);
  add_method(path + "/get", "ss", osc_get_double_dbspl, data, false);
  datamap[prefix + path] =
      data_element_t(prefix + path, data, str_get_double_dbspl, "double");
}

void osc_server_t::add_float_degree(const std::string& path, float* data,
                                    const std::string& range,
                                    const std::string& comment)
{
  add_method(path, "f", osc_set_float_degree, data, true, true, range, comment);
  add_method(path + "/get", "ss", osc_get_float_degree, data, false);
  datamap[prefix + path] =
      data_element_t(prefix + path, data, str_get_float_degree, "float");
}

void osc_server_t::add_double_degree(const std::string& path, double* data,
                                     const std::string& range,
                                     const std::string& comment)
{
  add_method(path, "f", osc_set_double_degree, data, true, true, range,
             comment);
  add_method(path + "/get", "ss", osc_get_double_degree, data, false);
  datamap[prefix + path] =
      data_element_t(prefix + path, data, str_get_double_degree, "double");
}

void osc_server_t::add_bool_true(const std::string& path, bool* data,
                                 const std::string& comment)
{
  add_method(path, "", osc_set_bool_true, data, true, false, "", comment);
}

void osc_server_t::add_bool_false(const std::string& path, bool* data,
                                  const std::string& comment)
{
  add_method(path, "", osc_set_bool_false, data, true, false, "", comment);
}

void osc_server_t::add_bool(const std::string& path, bool* data,
                            const std::string& comment)
{
  add_method(path, "i", osc_set_bool, data, true, true, "bool", comment);
  add_method(path + "/get", "ss", osc_get_bool, data, false);
  datamap[prefix + path] =
      data_element_t(prefix + path, data, str_get_bool, "bool");
}

void osc_server_t::add_int(const std::string& path, int32_t* data,
                           const std::string& range, const std::string& comment)
{
  add_method(path, "i", osc_set_int32, data, true, true, range, comment);
  add_method(path + "/get", "ss", osc_get_int32, data, false);
  datamap[prefix + path] =
      data_element_t(prefix + path, data, str_get_int, "int");
}

void osc_server_t::add_uint(const std::string& path, uint32_t* data,
                            const std::string& range,
                            const std::string& comment)
{
  add_method(path, "i", osc_set_uint32, data, true, true, range, comment);
  add_method(path + "/get", "ss", osc_get_uint32, data, false);
  datamap[prefix + path] =
      data_element_t(prefix + path, data, str_get_uint, "uint");
}

void osc_server_t::add_string(const std::string& path, std::string* data,
                              const std::string& comment)
{
  add_method(path, "s", osc_set_string, data, true, true, "string", comment);
  add_method(path + "/get", "ss", osc_get_string, data, false);
  datamap[prefix + path] =
      data_element_t(prefix + path, data, str_get_string, "string");
}

void osc_server_t::activate()
{
  if(initialized) {
    int result = lo_server_thread_start(lost);
    if(result < 0)
      std::cerr << "lo_server_thread_start failed\n";
    else
      isactive = true;
    if(verbose)
      std::cerr << "server active\n";
  }
}

void osc_server_t::deactivate()
{
  if(initialized) {
    isactive = false;
    int result = lo_server_thread_stop(lost);
    if(result < 0)
      std::cerr << "lo_server_thread_stop failed\n";
    if(verbose)
      std::cerr << "server inactive\n";
  }
}

std::map<std::string, osc_server_t::descriptor_t>
osc_server_t::get_variable_map() const
{
  std::map<std::string, descriptor_t> vars;
  for(const auto& var : variables)
    vars[var.path + var.typespec] = var;
  return vars;
}

std::string osc_server_t::list_variables() const
{
  auto vars = get_variable_map();
  std::string rv;
  for(const auto& v : vars)
    rv += v.second.path + "  (" + v.second.typespec + ")" +
          (v.second.readable ? " r " : " ") + v.second.rangehint + " " +
          v.second.comment + "\n";
  return rv;
}

const std::string& osc_server_t::get_prefix() const
{
  return prefix;
}

void osc_server_t::send_variable_list(const std::string& url,
                                      const std::string& path,
                                      const std::string& prefix) const
{
  lo_address target = lo_address_new_from_url(url.c_str());
  if(!target)
    return;
  lo_send(target, (path + "/begin").c_str(), "");
  for(const auto& var : variables)
    if(prefix.empty() || (var.path.find(prefix) == 0))
      lo_send(target, path.c_str(), "ssiss", var.path.c_str(),
              var.typespec.c_str(), var.readable, var.rangehint.c_str(),
              var.comment.c_str());
  lo_send(target, (path + "/end").c_str(), "");
  lo_address_free(target);
}

TASCAR::msg_t::msg_t(tsccfg::node_t e) : msg(lo_message_new())
{
  TASCAR::xml_element_t elem(e);
  elem.GET_ATTRIBUTE(path, "", "OSC path name");
  for(auto& sne : tsccfg::node_get_children(e, "f")) {
    TASCAR::xml_element_t tsne(sne);
    double v(0);
    tsne.GET_ATTRIBUTE(v, "", "float value");
    lo_message_add_float(msg, v);
  }
  for(auto& sne : tsccfg::node_get_children(e, "i")) {
    TASCAR::xml_element_t tsne(sne);
    int32_t v(0);
    tsne.GET_ATTRIBUTE(v, "", "int value");
    lo_message_add_int32(msg, v);
  }
  for(auto& sne : tsccfg::node_get_children(e, "s")) {
    TASCAR::xml_element_t tsne(sne);
    std::string v("");
    tsne.GET_ATTRIBUTE(v, "", "string value");
    lo_message_add_string(msg, v.c_str());
  }
}

msg_t::msg_t(const std::string& src)
{
  msg = lo_message_new();
  std::vector<std::string> args(TASCAR::str2vecstr(src));
  if(args.size()) {
    path = args[0];
    for(size_t n = 1; n < args.size(); ++n) {
      char* p(NULL);
      float val(strtof(args[n].c_str(), &p));
      if(*p) {
        lo_message_add_string(msg, args[n].c_str());
      } else {
        lo_message_add_float(msg, val);
      }
    }
  }
}

msg_t::msg_t(const msg_t& src) : path(src.path), msg(lo_message_clone(src.msg))
{
}

TASCAR::msg_t::~msg_t()
{
  lo_message_free(msg);
}

std::string osc_server_t::get_vars_as_json_rg(
    std::string prefix,
    std::map<std::string, data_element_t>::const_iterator& ibegin,
    std::map<std::string, data_element_t>::const_iterator iend,
    bool asstring) const
{
  std::string r = "{";
  std::string lastprefix;
  if((prefix.size() > 0) && (prefix[prefix.size() - 1] == '/'))
    prefix.erase(prefix.size() - 1);
  for(auto it = ibegin; it != iend; ++it) {
    if(prefix.empty() || (it->second.path.find(prefix) == 0)) {
      std::string vpf = it->second.prefix;
      size_t pos = vpf.find(prefix);
      if(pos == 0)
        vpf.erase(0, prefix.size());
      if(vpf[0] == '/')
        vpf.erase(0, 1);
      if(!vpf.empty()) {
        r += "\"" + vpf + "\":" +
             get_vars_as_json_rg(it->second.prefix, it, iend, asstring) + ",";
      } else {
        if(asstring || (it->second.type == "string"))
          r += "\"" + it->second.name + "\":\"" + it->second.getstr() + "\",";
        else
          r += "\"" + it->second.name + "\":" + it->second.getstr() + ",";
      }
      ibegin = it;
      lastprefix = vpf;
    }
  }
  if(r[r.size() - 1] == ',')
    r.erase(r.size() - 1);
  r += "}";
  return r;
}

std::string osc_server_t::get_vars_as_json(std::string prefix,
                                           bool asstring) const
{
  auto it = datamap.begin();
  return get_vars_as_json_rg(prefix, it, datamap.end(), asstring);
}

void osc_server_t::read_script_one(std::string filename)
{
  std::string called_filename = filename;
  tictoc_t tictoc;
  char rbuf[0x4000];
  if(!filename.empty()) {
    if(!scriptpath.empty()) {
      if(filename[0] != '/') {
        if(scriptpath[scriptpath.size() - 1] != '/')
          filename = scriptpath + "/" + filename;
        else
          filename = scriptpath + filename;
      }
    }
    FILE* fh = fopen((filename + scriptext).c_str(), "r");
    if(!fh) {
      TASCAR::add_warning("Cannot open file \"" + filename + scriptext + "\".");
      return;
    }
    ++scriptrunning;
    while(!feof(fh)) {
      memset(rbuf, 0, 0x4000);
      if(cancelscript) {
        fclose(fh);
        --scriptrunning;
        return;
      }
      char* s = fgets(rbuf, 0x4000 - 1, fh);
      if(s) {
        rbuf[0x4000 - 1] = 0;
        if(rbuf[0] == '#')
          rbuf[0] = 0;
        // remove newline characters at end:
        if(strlen(rbuf))
          if(rbuf[strlen(rbuf) - 1] == 10)
            rbuf[strlen(rbuf) - 1] = 0;
        // remove carriage returns at end:
        if(strlen(rbuf))
          if(rbuf[strlen(rbuf) - 1] == 13)
            rbuf[strlen(rbuf) - 1] = 0;
        if(strlen(rbuf)) {
          if(rbuf[0] == '<') {
            std::string newfname(&rbuf[1]);
            if(newfname != called_filename)
              read_script_one(newfname);
            else
              TASCAR::add_warning("Not calling tosc script \"" + newfname +
                                  "\" recursively.");
          } else {
            process_script_line(rbuf, tictoc);
            // if(rbuf[0] == ',') {
            //   double val(0.0f);
            //   sscanf(&rbuf[1], "%lg", &val);
            //   tictoc.tic();
            //   while(tictoc.toc() < val) {
            //     if(cancelscript) {
            //       fclose(fh);
            //       --scriptrunning;
            //       return;
            //     }
            //     usleep(100);
            //   }
            // } else {
            //   std::vector<std::string> args(TASCAR::str2vecstr(rbuf));
            //   if(args.size()) {
            //     if(args[0].size() && (args[0][0] == '@')) {
            //       auto stime = args[0];
            //       stime.erase(stime.begin());
            //       args.erase(args.begin());
            //       char* p(NULL);
            //       double ftime(strtod(stime.c_str(), &p));
            //       if(!*p)
            //         timed_message_add(ftime, TASCAR::vecstr2str(args));
            //     } else {
            //       auto msg(lo_message_new());
            //       for(size_t n = 1; n < args.size(); ++n) {
            //         char* p(NULL);
            //         float val(strtof(args[n].c_str(), &p));
            //         if(*p) {
            //           lo_message_add_string(msg, args[n].c_str());
            //         } else {
            //           lo_message_add_float(msg, val);
            //         }
            //       }
            //       dispatch_data_message(args[0].c_str(), msg);
            //       lo_message_free(msg);
            //     }
            //   }
            // }
          }
        }
      }
    }
    fclose(fh);
    --scriptrunning;
  }
}

void osc_server_t::read_script(const std::vector<std::string>& filenames)
{
  cancelscript = true;
  std::lock_guard<std::mutex> lk(mtxscript);
  if(filenames.empty())
    return;
  cancelscript = false;
  for(auto filename : filenames) {
    read_script_one(filename);
  }
}

void osc_server_t::process_script_line(const std::string& line,
                                       tictoc_t& tictoc)
{
  if(line.empty() || line[0] == '#')
    return;

  if(line[0] == ',') {
    double val(0.0f);
    sscanf(line.c_str() + 1, "%lg", &val);
    tictoc.tic();
    while(tictoc.toc() < val) {
      if(cancelscript)
        return;
      std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
  } else {
    std::vector<std::string> args(TASCAR::str2vecstr(line));
    if(args.empty())
      return;

    if(args[0][0] == '@') {
      std::string stime = args[0].substr(1);
      double ftime = std::stod(stime);
      std::vector<std::string> msg_args(args.begin() + 1, args.end());
      timed_message_add(ftime, TASCAR::vecstr2str(msg_args));
    } else {
      auto msg = lo_message_new();
      for(size_t n = 1; n < args.size(); ++n) {
        char* p;
        float val = strtof(args[n].c_str(), &p);
        if(*p)
          lo_message_add_string(msg, args[n].c_str());
        else
          lo_message_add_float(msg, val);
      }
      dispatch_data_message(args[0].c_str(), msg);
      lo_message_free(msg);
    }
  }
}

void osc_server_t::scriptthread_fun()
{
  while(runscriptthread) {
    std::vector<std::string> scripts;
    {
      std::unique_lock<std::mutex> lock{mtxscriptnames};
      bool needtowait = true;
      if(nextscripts.size() > 0) {
        needtowait = false;
      }
      if(needtowait) {
        cond_var_script.wait(lock);
      }
      scripts = nextscripts;
      nextscripts.clear();
      lock.unlock();
    }
    if(runscriptthread && scripts.size()) {
      cancelscript = false;
      read_script(scripts);
      cancelscript = false;
    }
  }
}

void osc_server_t::read_script_async(const std::vector<std::string>& filenames)
{
  if(scriptcancel)
    cancelscript = true;
  {
    std::lock_guard<std::mutex> lk{mtxscriptnames};
    nextscripts = filenames;
  }
  cond_var_script.notify_one();
}

void osc_server_t::timed_messages_process(double tstart, double tend)
{
  if(mtxtimedmessages.try_lock()) {
    for(const auto& vmsg : timed_messages)
      if((tstart <= vmsg.first) && (vmsg.first < tend))
        for(const auto& msg : vmsg.second) {
          dispatch_data_message(msg.path.c_str(), msg.msg);
        }
    mtxtimedmessages.unlock();
  }
}

void osc_server_t::timed_messages_clear()
{
  std::lock_guard<std::mutex> lk{mtxtimedmessages};
  timed_messages.clear();
}

void osc_server_t::timed_message_add(double time, const std::string& msgtext)
{
  std::lock_guard<std::mutex> lk{mtxtimedmessages};
  timed_messages[time].push_back(msg_t(msgtext));
}

void osc_server_t::set_variable_owner(const std::string& owner)
{
  varowner = owner;
}

void osc_server_t::unset_variable_owner()
{
  varowner = "";
}

void osc_server_t::generate_osc_documentation_files()
{
  // auto vmap = get_variable_map();
  for(const auto& owner : owned_vars) {
    std::vector<std::string> fullpath;
    bool first = true;
    size_t kmax = fullpath.size();
    bool path_is_relpath = true;
    for(const auto& varpath : owner.second) {
      if(varpath.second.relpath != varpath.second.path)
        path_is_relpath = false;
      std::vector<std::string> fullpath_local =
          str2vecstr(varpath.second.relpath, "/");
      if(first) {
        fullpath = fullpath_local;
        kmax = fullpath.size();
      }
      kmax = std::min(kmax, fullpath_local.size());
      if(fullpath_local.size() == 1)
        kmax = 0;
      for(size_t k = 0; k < std::min(kmax, fullpath_local.size()); ++k)
        if(fullpath[k] != fullpath_local[k])
          kmax = k;
      fullpath.erase(fullpath.begin() + kmax, fullpath.end());
      kmax = fullpath.size();
      first = false;
    }
    std::string pat = TASCAR::vecstr2str(fullpath, "/");
    std::string rep = "...";
    std::string pref = "";
    if(owner.second.size() < 2)
      pat = "";
    if((pat.size() == 0) && (fullpath.size() == 1)) {
      pat = "/";
      rep = "/.../";
    }
    if(fullpath.size() == 0) {
      pat = "";
      pref = "/...";
    }
    if(path_is_relpath)
      pref = "";
    std::ofstream ofh("oscdoc_" + owner.first + ".tex");
    ofh << "\\definecolor{shadecolor}{RGB}{236,236,255}\\begin{snugshade}\n{"
           "\\footnotesize\n";
    ofh << "\\label{osctab:" << TASCAR::strrep(owner.first, "_", "") << "}\n";
    ofh << "OSC variables:\n";
    ofh << "\\nopagebreak\n\n";
    ofh << "\\begin{tabularx}{\\textwidth}{llllX}\n";
    ofh << "\\hline\n";
    ofh << "path & fmt. & range & readable & description\\\\\n\\hline\n";
    for(const auto& varpath : owner.second) {
      ofh << "\\attr{"
          << TASCAR::to_latex(pref +
                              TASCAR::strrep(varpath.second.relpath, pat, rep))
          << "} & " << varpath.second.typespec << " & "
          << TASCAR::to_latex(varpath.second.rangehint) << " & "
          << (varpath.second.readable ? "yes" : "no") << " & "
          << varpath.second.comment << "\\\\" << std::endl;
    }
    ofh << "\\hline\n\\end{tabularx}\n";
    ofh << "}\n\\end{snugshade}\n\\definecolor{shadecolor}{RGB}{255,230,204}\n";
  }
}

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
