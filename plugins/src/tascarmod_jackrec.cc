/*
 * This file is part of the TASCAR software, see <http://tascar.org/>
 *
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

#include "jackiowav.h"
#include "session.h"
#include <mutex>

#ifdef _WIN32
#include <stdio.h>
#include <windows.h>
#else
#include <dirent.h>
#include <fnmatch.h>
#endif

#define OSC_VOID(x)                                                            \
  static int x(const char* path, const char* types, lo_arg** argv, int argc,   \
               lo_message msg, void* user_data)                                \
  {                                                                            \
    ((jackrec_t*)user_data)->x();                                              \
    return 0;                                                                  \
  };                                                                           \
  void x()

#define OSC_STRING(x)                                                          \
  static int x(const char* path, const char* types, lo_arg** argv, int argc,   \
               lo_message msg, void* user_data)                                \
  {                                                                            \
    ((jackrec_t*)user_data)->x(&(argv[0]->s));                                 \
    return 0;                                                                  \
  };                                                                           \
  void x(const std::string&)

class jackrec_t : public TASCAR::module_base_t {
public:
  jackrec_t(const TASCAR::module_cfg_t& cfg);
  ~jackrec_t();
  void add_variables(TASCAR::osc_server_t* srv);
  OSC_VOID(start);
  OSC_VOID(stop);
  OSC_VOID(clearports);
  OSC_STRING(addport);
  OSC_VOID(listports);
  OSC_VOID(listfiles);
  OSC_STRING(rmfile);
  std::vector<std::string> scan_dir();

private:
  // configuration variables:
  std::string name;
  double buflen;
  std::string path;
  std::string pattern;
  int format;
  // OSC variables:
  std::string ofname;
  std::vector<std::string> ports;
  // internal members:
  std::string prefix;
  jackrec_async_t* jr;
  std::mutex mtx;
  lo_address lo_addr;
  void service();
  std::thread srv;
  bool run_service;
};

jackrec_t::jackrec_t(const TASCAR::module_cfg_t& cfg)
    : module_base_t(cfg), name("jackrec"), buflen(10), path(""),
      pattern("rec*.wav"), format(0), jr(NULL), lo_addr(NULL), run_service(true)
{
  std::string url;
  // get configuration variables:
  GET_ATTRIBUTE_(name);
  GET_ATTRIBUTE_(buflen);
  GET_ATTRIBUTE_(url);
  GET_ATTRIBUTE_(path);
  GET_ATTRIBUTE_(pattern);
  int ifileformat(0);
  std::string fileformat("WAV");
  GET_ATTRIBUTE_(fileformat);
  std::string validformats;
#define ADD_FILEFORMAT(x)                                                      \
  if(fileformat == #x)                                                         \
    ifileformat = SF_FORMAT_##x;                                               \
  validformats += std::string(" ") + std::string(#x)
  ADD_FILEFORMAT(WAV);
  ADD_FILEFORMAT(AIFF);
  ADD_FILEFORMAT(AU);
  ADD_FILEFORMAT(RAW);
  ADD_FILEFORMAT(PAF);
  ADD_FILEFORMAT(SVX);
  ADD_FILEFORMAT(NIST);
  ADD_FILEFORMAT(VOC);
  ADD_FILEFORMAT(IRCAM);
  ADD_FILEFORMAT(W64);
  ADD_FILEFORMAT(MAT4);
  ADD_FILEFORMAT(MAT5);
  ADD_FILEFORMAT(PVF);
  ADD_FILEFORMAT(XI);
  ADD_FILEFORMAT(HTK);
  ADD_FILEFORMAT(SDS);
  ADD_FILEFORMAT(AVR);
  ADD_FILEFORMAT(WAVEX);
  ADD_FILEFORMAT(SD2);
  ADD_FILEFORMAT(FLAC);
  ADD_FILEFORMAT(CAF);
  ADD_FILEFORMAT(WVE);
  ADD_FILEFORMAT(OGG);
  ADD_FILEFORMAT(MPC2K);
  ADD_FILEFORMAT(RF64);
  if(ifileformat == 0)
    throw TASCAR::ErrMsg("Invalid file format \"" + fileformat +
                         "\". Valid formats are:" + validformats);
  validformats = "";
  std::string sampleformat("PCM_16");
  GET_ATTRIBUTE_(sampleformat);
  int isampleformat(0);
#define ADD_SAMPLEFORMAT(x)                                                    \
  if(sampleformat == #x)                                                       \
    isampleformat = SF_FORMAT_##x;                                             \
  validformats += std::string(" ") + std::string(#x)
  ADD_SAMPLEFORMAT(PCM_S8);
  ADD_SAMPLEFORMAT(PCM_16);
  ADD_SAMPLEFORMAT(PCM_24);
  ADD_SAMPLEFORMAT(PCM_32);
  ADD_SAMPLEFORMAT(PCM_U8);
  ADD_SAMPLEFORMAT(FLOAT);
  ADD_SAMPLEFORMAT(DOUBLE);
  ADD_SAMPLEFORMAT(ULAW);
  ADD_SAMPLEFORMAT(ALAW);
  ADD_SAMPLEFORMAT(IMA_ADPCM);
  ADD_SAMPLEFORMAT(MS_ADPCM);
  ADD_SAMPLEFORMAT(GSM610);
  ADD_SAMPLEFORMAT(VOX_ADPCM);
  ADD_SAMPLEFORMAT(G721_32);
  ADD_SAMPLEFORMAT(G723_24);
  ADD_SAMPLEFORMAT(G723_40);
  ADD_SAMPLEFORMAT(DWVW_12);
  ADD_SAMPLEFORMAT(DWVW_16);
  ADD_SAMPLEFORMAT(DWVW_24);
  ADD_SAMPLEFORMAT(DWVW_N);
  ADD_SAMPLEFORMAT(DPCM_8);
  ADD_SAMPLEFORMAT(DPCM_16);
  ADD_SAMPLEFORMAT(VORBIS);
  if(isampleformat == 0)
    throw TASCAR::ErrMsg("Invalid sample format \"" + sampleformat +
                         "\". Valid formats are:" + validformats);
  format = ifileformat | isampleformat;
  // register OSC variables:
  prefix = std::string("/") + name;
  add_variables(session);
  // optionally set OSC response target:
  if(!url.empty())
    lo_addr = lo_address_new_from_url(url.c_str());
  srv = std::thread(&jackrec_t::service, this);
  if(lo_addr)
    lo_send(lo_addr, (prefix + "/ready").c_str(), "");
}

jackrec_t::~jackrec_t()
{
  if(lo_addr)
    lo_send(lo_addr, (prefix + "/stop").c_str(), "");
  run_service = false;
  srv.join();
  std::lock_guard<std::mutex> lock(mtx);
  if(jr)
    delete jr;
  if(lo_addr)
    lo_address_free(lo_addr);
}

void jackrec_t::service()
{
  size_t xrun(0);
  size_t werror(0);
  while(run_service) {
    {
      std::lock_guard<std::mutex> lock(mtx);
      if(jr && lo_addr) {
        lo_send(lo_addr, (prefix + "/rectime").c_str(), "f",
                (float)(jr->rectime));
        if(jr->xrun > xrun) {
          xrun = jr->xrun;
          lo_send(lo_addr, (prefix + "/xrun").c_str(), "i", xrun);
        }
        if(jr->werror > werror) {
          if(werror == 0)
            lo_send(lo_addr, (prefix + "/error").c_str(), "s",
                    "Disk write error.");
          werror = jr->werror;
        }
      }
    }
    usleep(200000);
  }
}

void jackrec_t::clearports()
{
  ports.clear();
}

void jackrec_t::addport(const std::string& port)
{
  ports.push_back(port);
}

void jackrec_t::rmfile(const std::string& file)
{
  std::vector<std::string> dir(scan_dir());
  for(auto f : dir)
    if(f == file) {
      std::remove(f.c_str());
      return;
    }
  if(lo_addr)
    lo_send(
        lo_addr, (prefix + "/error").c_str(), "s",
        (std::string("Not removing file ") + file + std::string(".")).c_str());
}

void jackrec_t::start()
{
  std::lock_guard<std::mutex> lock(mtx);
  if(jr)
    delete jr;
  try {
    std::string ofname_(ofname);
    if(ofname.empty()) {
      time_t rawtime;
      struct tm* timeinfo;
      char buffer[80];
      time(&rawtime);
      timeinfo = localtime(&rawtime);
      strftime(buffer, sizeof(buffer), "%Y%m%d_%H%M%S", timeinfo);
      ofname_ = std::string("rec") + std::string(buffer) + std::string(".wav");
    }
    jr = new jackrec_async_t(ofname_, ports, name, buflen, format);
    if(lo_addr)
      lo_send(lo_addr, (prefix + "/start").c_str(), "");
  }
  catch(const std::exception& e) {
    std::string msg(e.what());
    msg = std::string("Failure: ") + msg;
    jr = NULL;
    TASCAR::add_warning(msg);
    if(lo_addr)
      lo_send(lo_addr, (prefix + "/error").c_str(), "s", msg.c_str());
  }
}

void jackrec_t::stop()
{
  std::lock_guard<std::mutex> lock(mtx);
  if(jr)
    delete jr;
  jr = NULL;
  if(lo_addr)
    lo_send(lo_addr, (prefix + "/stop").c_str(), "");
}

void jackrec_t::listports()
{
  jackc_portless_t jc(name + "_port");
  std::vector<std::string> lports(
      jc.get_port_names_regexp(".*", JackPortIsOutput));
  if(lo_addr) {
    lo_send(lo_addr, (prefix + "/portlist").c_str(), "");
    for(auto p : lports)
      if(p.find("sync_out") == std::string::npos)
        lo_send(lo_addr, (prefix + "/port").c_str(), "s", p.c_str());
  }
}

std::vector<std::string> jackrec_t::scan_dir()
{
  std::vector<std::string> res;
#ifdef _WIN32
  WIN32_FIND_DATA FindFileData;
  HANDLE hFind;
  hFind = FindFirstFile((path + pattern).c_str(), &FindFileData);
  if(hFind == INVALID_HANDLE_VALUE)
    return;
  do {
    res.push_back(FindFileData.cFileName);
  } while(FindNextFile(hFind, &FindFileData) != 0);
  FindClose(hFind);
#else
  struct dirent** namelist;
  int n;
  std::string dir(path);
  if(!dir.size())
    dir = ".";
  n = scandir(dir.c_str(), &namelist, NULL, alphasort);
  if(n >= 0) {
    for(int k = 0; k < n; k++) {
      if(fnmatch(pattern.c_str(), namelist[k]->d_name, 0) == 0)
        res.push_back(namelist[k]->d_name);
      free(namelist[k]);
    }
    free(namelist);
  }
#endif
  return res;
}

void jackrec_t::listfiles()
{
  std::vector<std::string> res(scan_dir());
  if(lo_addr) {
    lo_send(lo_addr, (prefix + "/filelist").c_str(), "");
    for(auto f : res)
      lo_send(lo_addr, (prefix + "/file").c_str(), "s", f.c_str());
  }
}

void jackrec_t::add_variables(TASCAR::osc_server_t* srv)
{
  std::string prefix_(srv->get_prefix());
  srv->set_prefix(prefix);
  srv->add_string("/name", &ofname);
  srv->add_method("/start", "", &jackrec_t::start, this);
  srv->add_method("/stop", "", &jackrec_t::stop, this);
  srv->add_method("/clear", "", &jackrec_t::clearports, this);
  srv->add_method("/addport", "s", &jackrec_t::addport, this);
  srv->add_method("/listports", "", &jackrec_t::listports, this);
  srv->add_method("/listfiles", "", &jackrec_t::listfiles, this);
  srv->add_method("/rmfile", "s", &jackrec_t::rmfile, this);
  srv->set_prefix(prefix_);
}

REGISTER_MODULE(jackrec_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
