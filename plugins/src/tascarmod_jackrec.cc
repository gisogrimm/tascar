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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 3 for more details.
 *
 * You should have received a copy of the GNU General Public License,
 * Version 3 along with TASCAR. If not, see <http://www.gnu.org/licenses/>.
 */

#include "jackiowav.h"
#include "session.h"
#include "tascar_os.h"
#include <mutex>

#ifdef _WIN32
#include <stdio.h>
#include <windows.h>
#else
#include <dirent.h>
#endif

/**
 * \def OSC_VOID(x)
 * \brief Macro to define an OSC method wrapper for a void member function.
 *
 * This macro creates a static callback function compatible with liblo
 * and a corresponding void member function. The callback casts the user_data
 * pointer to jackrec_t* and invokes the member function.
 *
 * \param x The name of the member function to wrap.
 */
#define OSC_VOID(x)                                                            \
  static int x(const char*, const char*, lo_arg**, int, lo_message,            \
               void* user_data)                                                \
  {                                                                            \
    ((jackrec_t*)user_data)->x();                                              \
    return 1;                                                                  \
  };                                                                           \
  void x()

/**
 * \def OSC_STRING(x)
 * \brief Macro to define an OSC method wrapper for a member function taking a
 * string argument.
 *
 * This macro creates a static callback function compatible with liblo
 * and a corresponding member function accepting a const std::string&.
 * The callback extracts the string argument from the OSC message (argv[0]->s)
 * and invokes the member function.
 *
 * \param x The name of the member function to wrap.
 */
#define OSC_STRING(x)                                                          \
  static int x(const char*, const char*, lo_arg** argv, int, lo_message,       \
               void* user_data)                                                \
  {                                                                            \
    ((jackrec_t*)user_data)->x(&(argv[0]->s));                                 \
    return 1;                                                                  \
  };                                                                           \
  void x(const std::string&)

/**
 * \class jackrec_t
 * \brief JACK audio recorder module for TASCAR.
 *
 * This module connects to JACK audio ports and records the incoming audio
 * streams to disk. It supports various audio formats and can be controlled
 * via OSC (Open Sound Control). It also supports JACK transport
 * synchronization.
 */
class jackrec_t : public TASCAR::module_base_t {
public:
  /**
   * \brief Constructor.
   * \param cfg Module configuration loaded from the XML session file.
   */
  jackrec_t(const TASCAR::module_cfg_t& cfg);
  /**
   * \brief Destructor.
   * Stops recording, cleans up resources, and joins the service thread.
   */
  ~jackrec_t();
  /**
   * \brief Registers OSC variables and methods with the server.
   * \param srv Pointer to the OSC server instance.
   */
  void add_variables(TASCAR::osc_server_t* srv);

  // OSC Methods
  OSC_VOID(start);            ///< Start recording.
  OSC_VOID(stop);             ///< Stop recording.
  OSC_VOID(clearports);       ///< Clear the list of JACK ports to record.
  OSC_STRING(addport);        ///< Add a JACK port name to the recording list.
  OSC_STRING(delport);        ///< Add a JACK port name to the recording list.
  OSC_VOID(listports);        ///< Send a list of available JACK ports via OSC.
  OSC_VOID(listenabledports); ///< Send a list of enabled JACK ports via OSC.
  OSC_VOID(listfiles); ///< Send a list of existing recording files via OSC.
  OSC_STRING(rmfile);  ///< Remove a specific file from disk.

  /**
   * \brief Scans the configured directory for files matching the pattern.
   * \return A vector of filenames found.
   */
  std::vector<std::string> scan_dir();

private:
  // Configuration variables:
  std::string name = "jackrec"; ///< Name of the module (used for OSC prefix and
                                ///< JACK client name).
  double buflen = 10.0;  ///< Buffer length in seconds for the recording thread.
  std::string path = ""; ///< Directory path where audio files are stored.
  std::string pattern = "rec*.wav"; ///< File pattern for searching recordings.
  int format = 0; ///< Libsndfile format bitmask (container | codec).
  bool usetransport =
      false; ///< If true, record only when JACK transport is rolling.

  // OSC variables:
  std::string
      ofname; ///< Output filename (if empty, a timestamped name is generated).
  std::vector<std::string> ports; ///< List of JACK port names to connect to.

  // Internal members:
  std::string oscprefix; ///< OSC path prefix (e.g., "/jackrec").
  jackrec_async_t* jr =
      NULL;       ///< Pointer to the asynchronous recorder implementation.
  std::mutex mtx; ///< Mutex to protect access to the recorder object.
  lo_address lo_addr =
      NULL;        ///< OSC address for sending feedback/status messages.
  void service();  ///< Main loop for the service thread (sends status updates).
  std::thread srv; ///< The service thread.
  bool run_service = true; ///< Flag to control the service thread loop.
  std::string extension =
      ".wav";                 ///< File extension based on the selected format.
  std::string prefix = "rec"; ///< Prefix for auto-generated filenames.
  std::string tag = "";       ///< Optional tag to append to filenames.
};

/**
 * \brief Construct a new jackrec_t object.
 *
 * Initializes the module by reading attributes from the configuration object,
 * setting up file formats, and starting the service thread.
 *
 * \param cfg Module configuration containing attributes like name, path,
 * buflen, etc.
 */
jackrec_t::jackrec_t(const TASCAR::module_cfg_t& cfg)
    : module_base_t(cfg), name("jackrec"), path(""), pattern("rec*.wav"),
      format(0), jr(NULL), lo_addr(NULL), run_service(true)
{
  std::string url;
  // get configuration variables:
  GET_ATTRIBUTE(name, "", "Name used for OSC prefix and jack");
  GET_ATTRIBUTE(buflen, "s", "audio buffer length");
  GET_ATTRIBUTE(url, "", "URL of OSC controller interface");
  GET_ATTRIBUTE(path, "", "File path where to store and search for files");
  // append path delimiter if needed:
  if(!path.empty() && (path[path.size() - 1] != '/'))
    path += "/";
  GET_ATTRIBUTE(pattern, "", "search pattern");
  GET_ATTRIBUTE(prefix, "", "file prefix");
  GET_ATTRIBUTE_BOOL(usetransport, "Record only when transport is rolling");
  GET_ATTRIBUTE(ports, "", "List of ports to record");
  int ifileformat(0);
  std::string fileformat("WAV");
  GET_ATTRIBUTE(fileformat, "", "File format");
  std::string validformats;

  // Helper macro to map string format names to libsndfile constants
#define ADD_FILEFORMAT(x, y)                                                   \
  if(fileformat == #x) {                                                       \
    ifileformat = SF_FORMAT_##x;                                               \
    extension = y;                                                             \
  }                                                                            \
  validformats += std::string(" ") + std::string(#x)

  ADD_FILEFORMAT(WAV, ".wav");
  ADD_FILEFORMAT(AIFF, ".aif");
  ADD_FILEFORMAT(AU, ".au");
  ADD_FILEFORMAT(RAW, "");
  ADD_FILEFORMAT(PAF, ".paf");
  ADD_FILEFORMAT(SVX, ".svx");
  ADD_FILEFORMAT(NIST, ".nist");
  ADD_FILEFORMAT(VOC, ".voc");
  ADD_FILEFORMAT(IRCAM, ".ircam");
  ADD_FILEFORMAT(W64, ".wav");
  ADD_FILEFORMAT(MAT4, ".mat");
  ADD_FILEFORMAT(MAT5, ".mat");
  ADD_FILEFORMAT(PVF, ".pvf");
  ADD_FILEFORMAT(XI, ".xi");
  ADD_FILEFORMAT(HTK, ".htk");
  ADD_FILEFORMAT(SDS, ".sds");
  ADD_FILEFORMAT(AVR, ".avr");
  ADD_FILEFORMAT(WAVEX, ".wav");
  ADD_FILEFORMAT(SD2, ".sd2");
  ADD_FILEFORMAT(FLAC, ".flac");
  ADD_FILEFORMAT(CAF, ".caf");
  ADD_FILEFORMAT(WVE, ".wav");
  ADD_FILEFORMAT(OGG, ".ogg");
  ADD_FILEFORMAT(MPC2K, ".mpc2k");
  ADD_FILEFORMAT(RF64, ".rf64");
  GET_ATTRIBUTE(fileformat, validformats, "File format");
  if(ifileformat == 0)
    throw TASCAR::ErrMsg("Invalid file format \"" + fileformat +
                         "\". Valid formats are:" + validformats);
  validformats = "";
  std::string sampleformat("PCM_16");
  GET_ATTRIBUTE(sampleformat, "", "Audio sample format");
  int isampleformat(0);

  // Helper macro to map string sample format names to libsndfile constants
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
  GET_ATTRIBUTE(sampleformat, validformats, "Audio sample format");
  if(isampleformat == 0)
    throw TASCAR::ErrMsg("Invalid sample format \"" + sampleformat +
                         "\". Valid formats are:" + validformats);
  format = ifileformat | isampleformat;
  // register OSC variables:
  oscprefix = std::string("/") + name;
  add_variables(session);
  // optionally set OSC response target:
  if(!url.empty())
    lo_addr = lo_address_new_from_url(url.c_str());
  srv = std::thread(&jackrec_t::service, this);
  if(lo_addr)
    lo_send(lo_addr, (oscprefix + "/ready").c_str(), "");
}

/**
 * \brief Destroy the jackrec_t object.
 *
 * Stops the service thread, deletes the recorder object if active,
 * and frees the OSC address resources.
 */
jackrec_t::~jackrec_t()
{
  if(lo_addr)
    lo_send(lo_addr, (oscprefix + "/stop").c_str(), "");
  run_service = false;
  {
    std::lock_guard<std::mutex> lock(mtx);
    if(jr)
      delete jr;
    jr = NULL;
  }
  srv.join();
  if(lo_addr)
    lo_address_free(lo_addr);
}

/**
 * \brief Service thread main loop.
 *
 * Periodically checks the status of the recorder and sends OSC messages
 * regarding recording time, xruns (buffer underruns), and write errors.
 */
void jackrec_t::service()
{
  size_t xrun(0);
  size_t werror(0);
  while(run_service) {
    {
      std::lock_guard<std::mutex> lock(mtx);
      if(jr && lo_addr) {
        lo_send(lo_addr, (oscprefix + "/rectime").c_str(), "f",
                (float)(jr->rectime));
        if(jr->xrun > xrun) {
          xrun = jr->xrun;
          lo_send(lo_addr, (oscprefix + "/xrun").c_str(), "i", xrun);
        }
        if(jr->werror > werror) {
          if(werror == 0)
            lo_send(lo_addr, (oscprefix + "/error").c_str(), "s",
                    "Disk write error.");
          werror = jr->werror;
        }
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }
}

/**
 * \brief Clears the list of JACK ports to be recorded.
 */
void jackrec_t::clearports()
{
  ports.clear();
}

/**
 * \brief Adds a JACK port to the recording list.
 * \param port The name of the JACK port to add.
 */
void jackrec_t::addport(const std::string& port)
{
  if(std::find(ports.begin(), ports.end(), port) == ports.end())
    ports.push_back(port);
}

/**
 * \brief Remove a JACK port from the recording list.
 * \param port The name of the JACK port to remove.
 */
void jackrec_t::delport(const std::string& port)
{
  auto it = std::find(ports.begin(), ports.end(), port);
  if(it != ports.end())
    ports.erase(it);
}

/**
 * \brief Removes a file from the recording directory.
 * \param file The filename to remove.
 */
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
        lo_addr, (oscprefix + "/error").c_str(), "s",
        (std::string("Not removing file ") + file + std::string(".")).c_str());
}

/**
 * \brief Starts the audio recording.
 *
 * If no filename is set via OSC, a timestamped filename is generated.
 * Stops any existing recording before starting a new one.
 */
void jackrec_t::start()
{
  TASCAR::tictoc_t tictoc;
  tictoc.tic();
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
      ofname_ = path + prefix + tag + std::string(buffer) + extension;
    }
    jr =
        new jackrec_async_t(ofname_, ports, name, buflen, format, usetransport);
    if(lo_addr)
      lo_send(lo_addr, (oscprefix + "/start").c_str(), "");
  }
  catch(const std::exception& e) {
    std::string msg(e.what());
    msg = std::string("Failure: ") + msg;
    jr = NULL;
    TASCAR::add_warning(msg);
    if(lo_addr)
      lo_send(lo_addr, (oscprefix + "/error").c_str(), "s", msg.c_str());
  }
}

/**
 * \brief Stops the audio recording.
 *
 * Deletes the recorder object, which closes the audio file.
 */
void jackrec_t::stop()
{
  std::lock_guard<std::mutex> lock(mtx);
  if(jr)
    delete jr;
  jr = NULL;
  if(lo_addr)
    lo_send(lo_addr, (oscprefix + "/stop").c_str(), "");
}

/**
 * \brief Lists available JACK output ports.
 *
 * Sends the list of ports via OSC messages to the configured controller URL.
 */
void jackrec_t::listports()
{
  jackc_portless_t jc(name + "_port");
  std::vector<std::string> lports(
      jc.get_port_names_regexp(".*", JackPortIsOutput));
  if(lo_addr) {
    lo_send(lo_addr, (oscprefix + "/portlist").c_str(), "");
    for(auto p : lports)
      if(p.find("sync_out") == std::string::npos)
        lo_send(lo_addr, (oscprefix + "/port").c_str(), "s", p.c_str());
  }
}

/**
 * \brief Lists enabled JACK output ports.
 *
 * Sends the list of enabled ports via OSC messages to the configured controller
 * URL.
 */
void jackrec_t::listenabledports()
{
  if(lo_addr) {
    lo_send(lo_addr, (oscprefix + "/enabledports/start").c_str(), "");
    for(auto p : ports)
      lo_send(lo_addr, (oscprefix + "/enabledport").c_str(), "s", p.c_str());
    lo_send(lo_addr, (oscprefix + "/enabledports/end").c_str(), "");
  }
}

/**
 * \brief Scans the configured directory for files matching the pattern.
 * \return Vector of filenames.
 */
std::vector<std::string> jackrec_t::scan_dir()
{
  std::vector<std::string> res;
#ifdef _WIN32
  WIN32_FIND_DATA FindFileData;
  HANDLE hFind;
  hFind = FindFirstFile((path + pattern).c_str(), &FindFileData);
  if(hFind == INVALID_HANDLE_VALUE)
    return res;
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
      if(TASCAR::fnmatch(pattern.c_str(), namelist[k]->d_name, false) == 0)
        res.push_back(namelist[k]->d_name);
      free(namelist[k]);
    }
    free(namelist);
  }
#endif
  return res;
}

/**
 * \brief Lists files in the recording directory.
 *
 * Sends the list of files via OSC messages to the configured controller URL.
 */
void jackrec_t::listfiles()
{
  std::vector<std::string> res(scan_dir());
  if(lo_addr) {
    lo_send(lo_addr, (oscprefix + "/filelist").c_str(), "");
    for(auto f : res)
      lo_send(lo_addr, (oscprefix + "/file").c_str(), "s", f.c_str());
  }
}

/**
 * \brief Registers OSC variables and methods.
 * \param srv Pointer to the OSC server.
 */
void jackrec_t::add_variables(TASCAR::osc_server_t* srv)
{
  srv->set_variable_owner(
      TASCAR::strrep(TASCAR::tscbasename(__FILE__), ".cc", ""));
  std::string prefix_(srv->get_prefix());
  srv->set_prefix(oscprefix);
  srv->add_string("/name", &ofname,
                  "Output file name, leave empty for automatic file names");
  srv->add_method(
      "/start", "", &jackrec_t::start, this, true, false, "",
      "Start recording (or recording standby when usetransport is set)");
  srv->add_method("/stop", "", &jackrec_t::stop, this, true, false, "",
                  "Stop recording and close output file");
  srv->add_method("/clear", "", &jackrec_t::clearports, this, true, false, "",
                  "Clear list of ports");
  srv->add_method("/addport", "s", &jackrec_t::addport, this, true, false, "",
                  "Add the given port to the list of recorder input ports");
  srv->add_method(
      "/delport", "s", &jackrec_t::delport, this, true, false, "",
      "Remove the given port from the list of recorder input ports");
  srv->add_method("/listports", "", &jackrec_t::listports, this, true, false,
                  "", "List all available jack ports");
  srv->add_method("/listenabledports", "", &jackrec_t::listenabledports, this,
                  true, false, "", "List enabled jack ports");
  srv->add_method(
      "/listfiles", "", &jackrec_t::listfiles, this, true, false, "",
      "Send list of sound files (matching pattern provided in XML)");
  srv->add_method("/rmfile", "s", &jackrec_t::rmfile, this, true, false, "",
                  "Remove a file on disk");
  srv->add_string("/tag", &tag, "Set tag of output file");
  srv->add_bool("/usetransport", &usetransport,
                "Control wether to use jack transport during recording when "
                "started next");
  srv->set_prefix(prefix_);
  srv->unset_variable_owner();
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
