/*
 * This file is part of the TASCAR software, see <http://tascar.org/>
 *
 * Copyright (c) 2019 Giso Grimm
 * Copyright (c) 2020 Giso Grimm
 * Copyright (c) 2021 Giso Grimm
 * Copyright (c) 2022 Giso Grimm
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

/*
 * Each variable comes as a pair of an inlet (OSC receiver or LSL
 * stream inlet) and a recorder (type independent storage). The data
 * logger is controlling all recorder instances.
 */

#include "datalogging_glade.h"
#include "session.h"
#include <cmath>
#include <fstream>
#include <gtkmm.h>
#include <gtkmm/builder.h>
#include <gtkmm/window.h>
#ifdef HAS_LSL
#include <lsl_cpp.h>
#endif
#include <chrono>
#include <matio.h>
#include <mutex>
#include <sys/stat.h>
#include <sys/types.h>
#include <thread>

using std::isfinite;

#define STRBUFFER_SIZE 1024

class recorder_t;

/**
 * @brief Base class of all input variables
 */
class var_base_t : public TASCAR::xml_element_t {
public:
  var_base_t(tsccfg::node_t);
  virtual ~var_base_t(){};
  void set_recorder(recorder_t* rec);
  std::string get_name() const;
  size_t get_size() const;
  bool is_linked_with(recorder_t* rec) const;

protected:
  recorder_t* datarecorder = NULL;
};

#ifdef HAS_LSL

/**
 * @brief Receiver of LSL variable
 */
class lslvar_t : public var_base_t {
public:
  lslvar_t(tsccfg::node_t xmlsrc, double lsltimeout);
  ~lslvar_t();
  void set_delta(double deltatime);
  void poll_data();
  void get_stream_delta_start();
  void get_stream_delta_end();
  std::string get_xml();
  bool has_inlet();
  lsl::stream_info get_info();
  double stream_delta_start = 0.0;
  double stream_delta_end = 0.0;
  std::string name;
  uint32_t size = 0u;

private:
  double get_stream_delta();
  lsl::stream_inlet* inlet = NULL;
  std::string predicate;
  double delta;
  bool time_correction_failed = false;
  double tctimeout = 2.0;
  lsl::channel_format_t chfmt = lsl::cf_undefined;
  std::mutex inletlock;
  TASCAR::tictoc_t ts;
  void poll_lsl_data();
  std::thread lsl_poll_service;
  std::atomic_bool run_lsl_poll_service = true;
};

#endif

/**
 * @brief Receiver of OSC string variable (format "s")
 */
class oscsvar_t : public var_base_t {
public:
  oscsvar_t(tsccfg::node_t xmlsrc);
  virtual ~oscsvar_t(){};
  static int osc_receive_sample(const char* path, const char* types,
                                lo_arg** argv, int argc, lo_message msg,
                                void* user_data);
  std::string path;
  bool ds_format = false;

private:
  void osc_receive_sample(const char* path, double t, const char* msg);
};

/**
 * @brief Receiver of OSC float/double variable (format "ddd", dimension can be
 * configured)
 */
class oscvar_t : public var_base_t {
public:
  oscvar_t(tsccfg::node_t xmlsrc);
  virtual ~oscvar_t(){};
  std::string get_fmt();
  static int osc_receive_sample(const char* path, const char* types,
                                lo_arg** argv, int argc, lo_message msg,
                                void* user_data);
  std::string path;
  uint32_t size = 1u;
  bool ignorefirst = false;
  bool usedouble = true;

private:
  void osc_receive_sample(const char* path, uint32_t size, double* data);
};

typedef std::vector<oscvar_t*> oscvarlist_t;
typedef std::vector<oscsvar_t*> oscsvarlist_t;
#ifdef HAS_LSL
typedef std::vector<lslvar_t*> lslvarlist_t;
#endif

class dlog_vars_t : public TASCAR::module_base_t {
public:
  dlog_vars_t(const TASCAR::module_cfg_t& cfg);
  ~dlog_vars_t();
  void set_jack_client(jack_client_t* jc) { jc_ = jc; };
  void validate_attributes(std::string& msg) const;

protected:
  std::string multicast;
  std::string port;
  std::string srv_proto = "UDP";
  std::string fileformat = "matcell";
  std::string outputdir;
  bool displaydc = true;
  bool controltransport = true;
  bool usetransport = false;
#ifdef HAS_LSL
  double lsltimeout = 10.0;
#endif
  bool headless = false;
  oscvarlist_t oscvars;
  oscsvarlist_t oscsvars;
#ifdef HAS_LSL
  lslvarlist_t lslvars;
#endif
  jack_client_t* jc_ = NULL;
  std::atomic_bool is_rolling = false;
};

void dlog_vars_t::validate_attributes(std::string& msg) const
{
  TASCAR::module_base_t::validate_attributes(msg);
  for(auto pvar : oscvars)
    pvar->validate_attributes(msg);
  for(auto pvar : oscsvars)
    pvar->validate_attributes(msg);
#ifdef HAS_LSL
  for(auto pvar : lslvars)
    pvar->validate_attributes(msg);
#endif
}

class label_t {
public:
  label_t(double t1_, double t2_, const std::string& msg_)
      : t1(t1_), t2(t2_), msg(msg_){};
  double t1;
  double t2;
  std::string msg;
};

class data_draw_t : public Gtk::DrawingArea {
public:
  data_draw_t(bool ignore_first, size_t num_channels)
      : num_channels(num_channels), ignore_first_(ignore_first),
        vdc(num_channels, 0.0)
  {
    connection_timeout = Glib::signal_timeout().connect(
        sigc::mem_fun(*this, &data_draw_t::on_timeout), 200);
  };
  virtual ~data_draw_t()
  {
    connection_timeout.disconnect();
    drawlock.lock();
    drawlock.unlock();
    plotdatalock.lock();
    plotdatalock.unlock();
  };
  virtual bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr);
  void clear();
  void set_time(double ntime) { time = ntime; };
  bool on_timeout();
  void store_sample(uint32_t nch, double* data);
  void store_msg(double t1, double t2, const std::string& msg);
  void set_displaydc(bool displaydc) { displaydc_ = displaydc; };
  void set_alive() { timeout_cnt = 10u; };

private:
  void get_valuerange(const std::vector<double>& data, size_t channels,
                      size_t firstchannel, size_t n1, size_t n2, double& dmin,
                      double& dmax);
  std::mutex drawlock;
  std::mutex plotdatalock;
  std::vector<double> plotdata_;
  std::vector<label_t> plot_messages;
  bool b_textdata = false;
  std::atomic<double> time;
  size_t num_channels = 1u;
  bool ignore_first_ = false;
  bool displaydc_ = true;
  std::vector<double> vdc;
  uint32_t timeout_cnt = 10u;
  sigc::connection connection_timeout;
};

void data_draw_t::store_sample(uint32_t n, double* data)
{
  if(plotdatalock.try_lock()) {
    timeout_cnt = 10u;
    b_textdata = false;
    // todo: increase efficiency, add multi-frame addition
    for(uint32_t k = 0; k < n; k++)
      plotdata_.push_back(data[k]);
    plotdatalock.unlock();
  }
}

void data_draw_t::store_msg(double t1, double t2, const std::string& msg)
{
  if(plotdatalock.try_lock()) {
    timeout_cnt = 10u;
    b_textdata = true;
    plot_messages.emplace_back(t1, t2, msg);
    plotdatalock.unlock();
  }
}

void data_draw_t::get_valuerange(const std::vector<double>& data,
                                 size_t channels, size_t firstchannel,
                                 size_t n1, size_t n2, double& dmin,
                                 double& dmax)
{
  dmin = std::numeric_limits<double>::max();
  dmax = std::numeric_limits<double>::lowest();
  for(size_t dim = firstchannel; dim < channels; dim++) {
    vdc[dim] = 0;
    if(!displaydc_) {
      size_t cnt(0);
      for(size_t k = n1; k < n2; k++) {
        double v(data[dim + k * channels]);
        if((v > std::numeric_limits<double>::lowest()) &&
           (v < std::numeric_limits<double>::max())) {
          vdc[dim] += v;
          cnt++;
        }
      }
      if(cnt)
        vdc[dim] /= (double)cnt;
    }
    for(size_t k = n1; k < n2; k++) {
      double v(data[dim + k * channels]);
      if((v > std::numeric_limits<double>::lowest()) &&
         (v < std::numeric_limits<double>::max())) {
        dmin = std::min(dmin, v - vdc[dim]);
        dmax = std::max(dmax, v - vdc[dim]);
      }
    }
  }
  if(dmax == dmin) {
    dmin -= 1.0;
    dmax += 1.0;
  }
  if(!(dmax > dmin)) {
    dmin = 1.0;
    dmax = 1.0;
  }
}

void data_draw_t::clear()
{
  std::lock_guard<std::mutex> lock(plotdatalock);
  plotdata_.clear();
  plot_messages.clear();
  b_textdata = false;
  timeout_cnt = 10u;
}

/**
 * @brief Data recorder for one variable
 */
class recorder_t {
public:
  recorder_t(uint32_t size, const std::string& name, std::atomic_bool& is_rec,
             std::atomic_bool& is_roll, jack_client_t* jc, double srate,
             bool ignore_first, bool headless);
  virtual ~recorder_t();
  /**
   * @brief Store a single data sample (can be multi-channel)
   *
   * @param n Number of channels including timestamps
   * @param data Sample data
   *
   * The data format of OSC variables is [t,d1,d2,d3,...], the data
   * format of LSL data is [t,tlsl,d1,d2,d3,...], where t is the local
   * session time at time of arrival (OSC) or time of measurement
   * (LSL), tlsl is the LSL time stamp, and d are the data points.
   */
  void store_sample(uint32_t nch, double* data);
  void store_msg(double t1, double t2, const std::string& msg);
  std::vector<double> get_data()
  {
    std::lock_guard<std::mutex> lock(dlock);
    return xdata_;
  };
  size_t get_size() const { return size_; };
  const std::string& get_name() const { return name_; };
  void clear();
  void set_textdata() { b_textdata = true; };
  bool is_textdata() const { return b_textdata; };
  std::vector<label_t> get_textdata()
  {
    std::lock_guard<std::mutex> lock(dlock);
    return xmessages;
  };
  double get_session_time() const;
  data_draw_t* drawer = NULL;

private:
  std::mutex dlock;
  // bool displaydc_;
  size_t size_;
  bool b_textdata;
  std::vector<double> xdata_;
  std::vector<label_t> xmessages;
  std::string name_;
  std::atomic_bool& is_rec_;
  std::atomic_bool& is_roll_;
  // pthread_mutex_t& record_mtx_;
  jack_client_t* jc_;
  double audio_sample_period_;
  // bool ignore_first_;
  size_t plotdata_cnt;
};

var_base_t::var_base_t(tsccfg::node_t xmlsrc) : TASCAR::xml_element_t(xmlsrc) {}

void var_base_t::set_recorder(recorder_t* rec)
{
  datarecorder = rec;
}

std::string var_base_t::get_name() const
{
  if(datarecorder)
    return datarecorder->get_name();
  return "";
}

size_t var_base_t::get_size() const
{
  if(datarecorder)
    return datarecorder->get_size();
  return 0u;
}

bool var_base_t::is_linked_with(recorder_t* rec) const
{
  if(datarecorder)
    return datarecorder == rec;
  return false;
}

/**
 * @brief Show error message on screen
 */
void error_message(const std::string& msg)
{
  std::cerr << "Error: " << msg << std::endl;
  Gtk::MessageDialog dialog("Error", false, Gtk::MESSAGE_ERROR);
  dialog.set_secondary_text(msg);
  dialog.run();
}

/**
 * @brief Return current date and time as string
 */
std::string datestr()
{
  time_t t(time(NULL));
  struct tm* ltm = localtime(&t);
  char ctmp[1024];
  strftime(ctmp, 1024, "%Y%m%d_%H%M%S", ltm);
  return ctmp;
}

#ifdef HAS_LSL
bool lslvar_t::has_inlet()
{
  std::lock_guard<std::mutex> lock(inletlock);
  if(inlet)
    return true;
  return false;
}

lsl::stream_info lslvar_t::get_info()
{
  std::lock_guard<std::mutex> lock(inletlock);
  if(inlet)
    return inlet->info();
  return lsl::stream_info("null", "none");
}

std::string lslvar_t::get_xml()
{
  std::lock_guard<std::mutex> lock(inletlock);
  if(!inlet)
    return "";
  return inlet->info().as_xml();
}

lslvar_t::lslvar_t(tsccfg::node_t xmlsrc, double) // lsltimeout
    : var_base_t(xmlsrc), delta(lsl::local_clock())
{
  GET_ATTRIBUTE(predicate, "",
                "LSL stream resolving predicate, e.g., \"name='EEG'\"");
  if(predicate.empty())
    throw TASCAR::ErrMsg("Invalid (empty) predicate.");
  GET_ATTRIBUTE(tctimeout, "s", "Time correction timeout");
  bool required(true);
  GET_ATTRIBUTE_BOOL(required, "Require this stream. If true, then loading "
                               "will fail if stream is not available.");
  std::vector<lsl::stream_info> results(lsl::resolve_stream(predicate, 1, 1));
  if(results.empty()) {
    if(required)
      throw TASCAR::ErrMsg("No matching LSL stream found (" + predicate + ").");
    else
      TASCAR::add_warning("No matching LSL stream found (" + predicate + ").",
                          e);
  }
  if(results.size() > 1)
    TASCAR::add_warning("More than one LSL stream found (" + predicate +
                            "), using first one.",
                        e);
  if(!results.empty()) {
    chfmt = results[0].channel_format();
    size = results[0].channel_count() + 1;
    name = results[0].name();
    inlet = new lsl::stream_inlet(results[0]);
    std::cerr << "created LSL inlet for predicate " << predicate << " ("
              << results[0].channel_count() << " channels, fmt " << chfmt << ")"
              << std::endl;
    std::cerr << "measuring LSL time correction: ";
    get_stream_delta_start();
    std::cerr << stream_delta_start << " s\n";
  }
  lsl_poll_service = std::thread(&lslvar_t::poll_lsl_data, this);
}

void lslvar_t::get_stream_delta_start()
{
  stream_delta_start = get_stream_delta();
}

void lslvar_t::get_stream_delta_end()
{
  stream_delta_end = get_stream_delta();
}

double lslvar_t::get_stream_delta()
{
  std::lock_guard<std::mutex> lock(inletlock);
  if(!inlet)
    return 0.0;
  double stream_delta(0);
  try {
    stream_delta = inlet->time_correction(tctimeout);
  }
  catch(const std::exception& e) {
    std::cerr << "Warning: " << e.what() << std::endl;
    stream_delta = 0;
    time_correction_failed = true;
  }
  return stream_delta;
}

lslvar_t::~lslvar_t()
{
  run_lsl_poll_service = false;
  if(lsl_poll_service.joinable())
    lsl_poll_service.join();
  if(inlet)
    delete inlet;
}

void lslvar_t::set_delta(double deltatime)
{
  delta = deltatime;
}
#endif

oscsvar_t::oscsvar_t(tsccfg::node_t xmlsrc) : var_base_t(xmlsrc)
{
  GET_ATTRIBUTE(path, "", "OSC path name, expecting messages with 's' format");
  GET_ATTRIBUTE_BOOL(
      ds_format, "Use ds format, i.e., a double in addition to the string.");
}

oscvar_t::oscvar_t(tsccfg::node_t xmlsrc) : var_base_t(xmlsrc)
{
  GET_ATTRIBUTE(path, "",
                "OSC path name, expecting messages with 'd' format "
                "(usedouble=true) or 'f' format.");
  GET_ATTRIBUTE(size, "", "Numer of double/float values per sample.");
  GET_ATTRIBUTE_BOOL(ignorefirst, "Ignore first value in visualization.");
  GET_ATTRIBUTE_BOOL(
      usedouble,
      "Use double precision OSC variable instead of single precision.");
}

std::string oscvar_t::get_fmt()
{
  if(usedouble)
    return std::string(size, 'd');
  return std::string(size, 'f');
}

dlog_vars_t::dlog_vars_t(const TASCAR::module_cfg_t& cfg) : module_base_t(cfg)
{
  GET_ATTRIBUTE(multicast, "", "OSC multicasting address");
  GET_ATTRIBUTE(port, "", "OSC port, or empty to use session server");
  GET_ATTRIBUTE(srv_proto, "", "Server protocol, UDP or TCP");
  GET_ATTRIBUTE(fileformat, "",
                "File format, can be either ``mat'', ``matcell'' or ``txt''");
  GET_ATTRIBUTE(outputdir, "", "Data output directory");
#ifdef HAS_LSL
  GET_ATTRIBUTE(lsltimeout, "s", "Number of seconds to scan for LSL streams");
#endif
  GET_ATTRIBUTE_BOOL(displaydc, "Display DC components");
  GET_ATTRIBUTE_BOOL(controltransport,
                     "Control transport with recording session control");
  GET_ATTRIBUTE_BOOL(usetransport, "Record only while transport is rolling");
  GET_ATTRIBUTE_BOOL(headless, "Use without GUI");
  if(fileformat.size() == 0)
    fileformat = "matcell";
  if((fileformat != "txt") && (fileformat != "mat") &&
     (fileformat != "matcell"))
    throw TASCAR::ErrMsg("Invalid file format \"" + fileformat + "\".");
}

dlog_vars_t::~dlog_vars_t() {}

std::string nice_name(std::string s, std::string extend = "")
{
  for(uint32_t k = 0; k < s.size(); k++)
    switch(s[k]) {
    case '/':
    case ':':
    case '.':
    case ' ':
    case '-':
    case '+':
      s[k] = '_';
    }
  while(s.size() && (s[0] == '_'))
    s.erase(0, 1);
  if(extend.size())
    return s + "." + extend;
  return s;
}

recorder_t::recorder_t(uint32_t size, const std::string& name,
                       std::atomic_bool& is_rec, std::atomic_bool& is_roll,
                       jack_client_t* jc, double srate, bool ignore_first,
                       bool headless)
    : size_(size), b_textdata(false), name_(name), is_rec_(is_rec),
      is_roll_(is_roll), jc_(jc), audio_sample_period_(1.0 / srate),
      // ignore_first_(ignore_first),
      plotdata_cnt(0)
{
  if(!headless)
    drawer = new data_draw_t(ignore_first, size);
  // pthread_mutex_init(&drawlock, NULL);
  // pthread_mutex_init(&plotdatalock, NULL);
}

double recorder_t::get_session_time() const
{
  return audio_sample_period_ * jack_get_current_transport_frame(jc_);
}

void recorder_t::clear()
{
  std::lock_guard<std::mutex> lock(dlock);
  xdata_.clear();
  xmessages.clear();
  b_textdata = false;
  if(drawer)
    drawer->clear();
  // pthread_mutex_lock(&plotdatalock);
  // plotdata_.clear();
  // plot_messages.clear();
  // pthread_mutex_unlock(&plotdatalock);
  plotdata_cnt = 0;
  // timeout_cnt = 10u;
}

recorder_t::~recorder_t()
{
  if(drawer)
    delete drawer;
}

double sqr(double x)
{
  return x * x;
}

/**
   \brief Find index range corresponding to time interval
   \param data Multichannel data vector, time is in first channel
   \param channels Number of channels
   \param t1 Start time, value included
   \param t2 End time, value excluded
   \retval n1 Start index, included
   \retval n2 End index, excluded
 */
void find_timeinterval(const std::vector<double>& data, size_t channels,
                       double t1, double t2, size_t& n1, size_t& n2)
{
  size_t N(data.size() / channels);
  n1 = 0;
  n2 = 0;
  if(N == 0)
    return;
  for(n2 = N - 1; (n2 > 0) && (data[n2 * channels] >= t2); --n2)
    ;
  ++n2;
  for(n1 = n2 - 1; (n1 > 0) && (data[n1 * channels] >= t1); --n1)
    ;
}

TASCAR::Scene::rgb_color_t set_hsv(float h, float s, float v)
{
  // const std::complex<float> i(0.0, 1.0);
  // h = (std::arg(std::exp(i * h * DEG2RADf)) * RAD2DEGf);
  // if(h < 0)
  //  h += 360.0;
  h = fmodf(h, 360.0f);
  float c(v * s);
  float x(c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f)));
  float m(v - c);
  TASCAR::Scene::rgb_color_t rgb_d;
  if(h < 60)
    rgb_d = TASCAR::Scene::rgb_color_t(c, x, 0);
  else if(h < 120)
    rgb_d = TASCAR::Scene::rgb_color_t(x, c, 0);
  else if(h < 180)
    rgb_d = TASCAR::Scene::rgb_color_t(0, c, x);
  else if(h < 240)
    rgb_d = TASCAR::Scene::rgb_color_t(0, x, c);
  else if(h < 300)
    rgb_d = TASCAR::Scene::rgb_color_t(x, 0, c);
  else
    rgb_d = TASCAR::Scene::rgb_color_t(c, 0, x);
  rgb_d.r += m;
  rgb_d.g += m;
  rgb_d.b += m;
  return rgb_d;
}

bool data_draw_t::on_draw(const Cairo::RefPtr<Cairo::Context>& cr)
{
  if(drawlock.try_lock()) {
    Glib::RefPtr<Gdk::Window> window = get_window();
    if(window) {
      Gtk::Allocation allocation = get_allocation();
      const int width = allocation.get_width();
      const int height = allocation.get_height();
      cr->save();
      cr->set_source_rgb(1.0, 1.0, 1.0);
      cr->paint();
      cr->restore();
      cr->save();
      cr->translate(width, 0);
      cr->set_line_width(1);
      // data plot:
      plotdatalock.lock();
      double ltime = time;
      double drange(2);
      double dscale(1);
      double dmin(-1);
      double dmax(1);
      if(b_textdata) {
        drange = dmax - dmin;
        dscale = height / drange;
        // ltime = audio_sample_period_ * jack_get_current_transport_frame(jc_);
        for(const auto& msg : plot_messages) {
          // for(auto it = plot_messages.begin(); it != plot_messages.end();
          // ++it) {
          if((msg.t1 >= ltime - 30) && (msg.t1 <= ltime)) {
            double t((msg.t1 - ltime) * width / 30.0);
            double v(height - (-0.8 - dmin) * dscale);
            double v2(height - (-0.85 - dmin) * dscale);
            double v1(height);
            cr->save();
            cr->set_source_rgb(0.4, 0, 0);
            cr->move_to(t, v2);
            cr->line_to(t, v1);
            cr->stroke();
            cr->move_to(t, v);
            cr->save();
            cr->rotate(-TASCAR_PI2);
            cr->show_text(msg.msg);
            cr->restore();
            cr->restore();
          }
        }
      } else {
        // N is number of (multi-dimensional) samples:
        size_t N(plotdata_.size() / num_channels);
        if(N > 1) {
          //        double tmax(0);
          size_t n1(0);
          size_t n2(0);
          ltime = plotdata_[(N - 1) * num_channels];
          find_timeinterval(plotdata_, num_channels, ltime - 30, ltime, n1, n2);
          get_valuerange(plotdata_, num_channels, 1 + ignore_first_, n1, n2,
                         dmin, dmax);
          size_t stepsize((n2 - n1) / 2048);
          ++stepsize;
          drange = dmax - dmin;
          dscale = height / drange;
          for(size_t dim = 1 + ignore_first_; dim < num_channels; ++dim) {
            cr->save();
            double size_norm(1.0 /
                             ((double)num_channels - 1.0 - ignore_first_));
            TASCAR::Scene::rgb_color_t rgb(
                set_hsv((float)(((double)dim - 1.0 - ignore_first_) *
                                size_norm * 360.0),
                        0.8f, 0.7f));
            cr->set_source_rgb(rgb.r, rgb.g, rgb.b);
            bool restart_line(true);
            // limit number of lines:
            for(size_t k = n1; k < n2; k += stepsize) {
              double t(plotdata_[k * num_channels] - ltime);
              double v(plotdata_[dim + k * num_channels]);
              if(std::isfinite(v)) {
                if(!displaydc_)
                  v -= vdc[dim];
                if((v > std::numeric_limits<double>::lowest()) &&
                   (v < std::numeric_limits<double>::max())) {
                  t *= width / 30.0;
                  v = height - (v - dmin) * dscale;
                  //++nlines;
                  if(restart_line) {
                    cr->move_to(t, v);
                    restart_line = false;
                  } else
                    cr->line_to(t, v);
                } else {
                  restart_line = true;
                }
              }
            }
            cr->stroke();
            cr->restore();
          }
        }
      }
      plotdatalock.unlock();
      // draw scale:
      cr->set_source_rgb(0, 0, 0);
      cr->move_to(-width, 0);
      cr->line_to(0, 0);
      cr->line_to(0, height);
      cr->line_to(-width, height);
      cr->line_to(-width, 0);
      cr->stroke();
      // x-grid, x-scale:
      for(int32_t k = 0; k < 30; k++) {
        double t(floor(ltime) - k);
        double x((t - ltime) * width / 30.0);
        if(t >= 0) {
          // cr->move_to(-(double)k*width/30.0,height);
          // cr->line_to(-(double)k*width/30.0,(double)height*(1-0.03));
          cr->move_to(x, height);
          double dx(0.02);
          if((int)t % 5 == 0)
            dx *= 2;
          cr->line_to(x, (double)height * (1 - dx));
          if((int)t % 5 == 0) {
            char ctmp[1024];
            ctmp[1023] = 0;
            snprintf(ctmp, 1023, "%d", (int)t);
            cr->show_text(ctmp);
          }
        }
      }
      cr->stroke();
      if(!b_textdata) {
        // y-grid & scale:
        double ystep(pow(10.0, floor(log10(0.5 * drange))));
        if(std::isfinite(dmax) && std::isfinite(dmin) && std::isfinite(ystep) &&
           isfinite(dscale)) {
          double ddmin(ystep * round(dmin / ystep));
          if(std::isfinite(dmax) && std::isfinite(ddmin) &&
             std::isfinite(ystep) && (dmax > ddmin)) {
            uint8_t ky(0);
            for(double dy = ddmin; (dy < dmax) && (ky < 20); dy += ystep) {
              ++ky;
              double v(height - (dy - dmin) * dscale);
              cr->move_to(-width, v);
              cr->line_to(-width * (1.0 - 0.01), v);
              char ctmp[1024];
              ctmp[1023] = 0;
              snprintf(ctmp, 1023, "%g", dy);
              cr->show_text(ctmp);
              cr->stroke();
            }
          }
        }
      }
      cr->restore();
      // draw green or red circle in left upper corner:
      cr->save();
      if(timeout_cnt > 0u) {
        cr->set_source_rgb(0, 0.7, 0);
        cr->move_to(40.0, 30.0);
        cr->arc(40.0, 30.0, 15.0, 0.0, TASCAR_2PI);
        cr->fill();
      } else {
        cr->set_source_rgb(0.7, 0, 0);
        cr->move_to(40.0, 30.0);
        cr->arc(40.0, 30.0, 15.0, 0.0, TASCAR_2PI);
        cr->fill();
        cr->set_source_rgb(1.0, 1.0, 1.0);
        cr->set_line_width(5);
        cr->move_to(30.0, 30.0);
        cr->line_to(50.0, 30.0);
        cr->stroke();
      }
      cr->restore();
    }
    drawlock.unlock();
  }
  return true;
}

bool data_draw_t::on_timeout()
{
  if(timeout_cnt)
    --timeout_cnt;
  Glib::RefPtr<Gdk::Window> win = get_window();
  if(win) {
    Gdk::Rectangle r(0, 0, get_allocation().get_width(),
                     get_allocation().get_height());
    win->invalidate_rect(r, false);
  }
  return true;
}

void recorder_t::store_sample(uint32_t n, double* data)
{
  // todo: increase efficiency, add multi-frame addition
  if(n != size_)
    throw TASCAR::ErrMsg("Invalid size (recorder_t::store)");
  if(is_rec_ && is_roll_) {
    std::lock_guard<std::mutex> lock(dlock);
    for(uint32_t k = 0; k < n; k++)
      xdata_.push_back(data[k]);
    if(drawer)
      drawer->store_sample(n, data);
  } else {
    if(drawer)
      drawer->set_alive();
  }
}

void recorder_t::store_msg(double t1, double t2, const std::string& msg)
{
  if(is_rec_ && is_roll_) {
    std::lock_guard<std::mutex> lock(dlock);
    b_textdata = true;
    xmessages.emplace_back(t1, t2, msg);
    if(drawer)
      drawer->store_msg(t1, t2, msg);
  } else {
    if(drawer)
      drawer->set_alive();
  }
}

#ifdef HAS_LSL
void lslvar_t::poll_data()
{
  std::lock_guard<std::mutex> lock(inletlock);
  if(!inlet)
    return;
  double recorder_buffer[size + 1];
  double* data_buffer(&(recorder_buffer[2]));
  double t(1);
  bool has_data = false;
  // char* strb(str_buffer.data());
  while(t != 0) {
    if(chfmt == lsl::cf_string) {
      std::string sample;
      t = inlet->pull_sample(&sample, 1u, 0.0);
      if((t != 0) && datarecorder) {
        datarecorder->set_textdata();
        datarecorder->store_msg(t - delta + stream_delta_start,
                                t + stream_delta_start, sample);
        has_data = true;
      }
    } else {
      try {
        t = inlet->pull_sample(data_buffer, size - 1, 0.0);
        if((t != 0) && datarecorder) {
          recorder_buffer[0] = t - delta + stream_delta_start;
          recorder_buffer[1] = t + stream_delta_start;
          datarecorder->store_sample(size + 1, recorder_buffer);
          has_data = true;
        }
      }
      catch(const std::exception& e) {
        TASCAR::add_warning(e.what());
      }
    }
  }
  if(has_data)
    ts.tic();
}
#endif

/**
 * @brief Plugin interface
 */
class datalogging_t : public dlog_vars_t, public TASCAR::osc_server_t {

public:
  datalogging_t(const TASCAR::module_cfg_t& cfg);
  ~datalogging_t();
  virtual void update(uint32_t frame, bool running);
  void start_trial(const std::string& name);
  void stop_trial();
  void save_text(const std::string& filename);
  void save_mat(const std::string& filename);
  void save_matcell(const std::string& filename);
  void save_session_related_meta_data(mat_t* matfp, const std::string& fname);
  void on_osc_set_trialid();
  void on_ui_showdc();
  void on_ui_start();
  void on_ui_stop();
  bool on_100ms();
  bool is_headless() const { return headless; };
  static int osc_session_start(const char* path, const char* types,
                               lo_arg** argv, int argc, lo_message msg,
                               void* user_data);
  static int osc_session_stop(const char* path, const char* types,
                              lo_arg** argv, int argc, lo_message msg,
                              void* user_data);
  static int osc_session_set_trialid(const char* path, const char* types,
                                     lo_arg** argv, int argc, lo_message msg,
                                     void* user_data);
  void configure();
  void release();

private:
  // void poll_lsl_data();
  std::vector<recorder_t*> recorder;
  std::atomic_bool is_recording = false;
  // pthread_mutex_t record_mtx;
  bool b_recording;
  std::string filename;
  // GUI:
  Glib::RefPtr<Gtk::Builder> refBuilder;
  Gtk::Window* win = NULL;
  Gtk::Entry* trialid = NULL;
  Gtk::Label* datelabel = NULL;
  Gtk::Label* jacktime = NULL;
  Gtk::Grid* draw_grid = NULL;
  Gtk::Label* rec_label = NULL;
  Gtk::Entry* outputdirentry = NULL;
  Gtk::CheckButton* showdc = NULL;
  sigc::connection connection_timeout;
  Glib::Dispatcher osc_start;
  Glib::Dispatcher osc_stop;
  Glib::Dispatcher osc_set_trialid;
  std::string osc_trialid;
  // double audio_sample_period_;
  uint32_t fragsize;
  double srate;
};

#define GET_WIDGET(x)                                                          \
  refBuilder->get_widget(#x, x);                                               \
  if(!x)                                                                       \
  throw TASCAR::ErrMsg(std::string("No widget \"") + #x +                      \
                       std::string("\" in builder."))

datalogging_t::datalogging_t(const TASCAR::module_cfg_t& cfg)
    : dlog_vars_t(cfg), TASCAR::osc_server_t(multicast, port, srv_proto),
      b_recording(false), // audio_sample_period_(1.0 / cfg.session->srate),
      fragsize(cfg.session->fragsize), srate(cfg.session->srate)
{
  TASCAR::osc_server_t* osc(this);
  if(port.empty())
    osc = session;
  osc->set_variable_owner(
      TASCAR::strrep(TASCAR::tscbasename(__FILE__), ".cc", ""));
  osc->add_method("/session_trialid", "s",
                  &datalogging_t::osc_session_set_trialid, this, true, false,
                  "string", "Set the new trial ID");
  osc->add_method("/session_start", "", &datalogging_t::osc_session_start, this,
                  true, false, "", "Start the recording of a session");
  osc->add_method("/session_stop", "", &datalogging_t::osc_session_stop, this,
                  true, false, "",
                  "Stop the recording of a session and save data to the file");
  osc->add_string("/session_outputdir", &outputdir, "Set the output directory");
  osc->unset_variable_owner();
  set_jack_client(session->jc);
  TASCAR::osc_server_t::activate();
  if(!headless) {
    refBuilder = Gtk::Builder::create_from_string(ui_datalogging);
    GET_WIDGET(win);
    GET_WIDGET(trialid);
    GET_WIDGET(datelabel);
    GET_WIDGET(jacktime);
    GET_WIDGET(draw_grid);
    GET_WIDGET(rec_label);
    GET_WIDGET(outputdirentry);
    GET_WIDGET(showdc);
    rec_label->set_text("");
  }
  outputdir = TASCAR::env_expand(outputdir);
  if(!headless) {
    outputdirentry->set_text(outputdir);
    win->set_title("tascar datalogging - " + session->name);
    int32_t dlogwin_x(0);
    int32_t dlogwin_y(0);
    int32_t dlogwin_width(300);
    int32_t dlogwin_height(300);
    win->get_size(dlogwin_width, dlogwin_height);
    win->get_position(dlogwin_x, dlogwin_y);
    get_attribute_value(e, "w", dlogwin_width);
    get_attribute_value(e, "h", dlogwin_height);
    get_attribute_value(e, "x", dlogwin_x);
    get_attribute_value(e, "y", dlogwin_y);
    win->resize(dlogwin_width, dlogwin_height);
    win->move(dlogwin_x, dlogwin_y);
    win->resize(dlogwin_width, dlogwin_height);
    win->move(dlogwin_x, dlogwin_y);
    win->show();
    // Create actions for menus and toolbars:
    Glib::RefPtr<Gio::SimpleActionGroup> refActionGroupMain =
        Gio::SimpleActionGroup::create();
    refActionGroupMain->add_action(
        "start", sigc::mem_fun(*this, &datalogging_t::on_ui_start));
    refActionGroupMain->add_action(
        "stop", sigc::mem_fun(*this, &datalogging_t::on_ui_stop));
    refActionGroupMain->add_action(
        "showdc", sigc::mem_fun(*this, &datalogging_t::on_ui_showdc));
    win->insert_action_group("datalog", refActionGroupMain);
  }
  osc_start.connect(sigc::mem_fun(*this, &datalogging_t::on_ui_start));
  osc_stop.connect(sigc::mem_fun(*this, &datalogging_t::on_ui_stop));
  osc_set_trialid.connect(
      sigc::mem_fun(*this, &datalogging_t::on_osc_set_trialid));
  for(auto sne : tsccfg::node_get_children(e, "variable"))
    oscvars.push_back(new oscvar_t(sne));
  for(auto sne : tsccfg::node_get_children(e, "osc"))
    oscvars.push_back(new oscvar_t(sne));
  for(auto sne : tsccfg::node_get_children(e, "oscs"))
    oscsvars.push_back(new oscsvar_t(sne));
#ifdef HAS_LSL
  for(auto sne : tsccfg::node_get_children(e, "lsl"))
    lslvars.push_back(new lslvar_t(sne, lsltimeout));
#endif
}

datalogging_t::~datalogging_t()
{
  //  for(auto pvar : oscvars)
  //    delete pvar;
  //  for(auto pvar : oscsvars)
  //    delete pvar;
  // #ifdef HAS_LSL
  //  for(auto pvar : lslvars)
  //    delete pvar;
  // #endif
  //  for(auto prec : recorder)
  //    delete prec;
  // if(!headless) {
  //   delete win;
  //   delete trialid;
  //   delete datelabel;
  //   delete jacktime;
  //   delete draw_grid;
  //   delete rec_label;
  // }
}

void datalogging_t::configure()
{
  TASCAR::module_base_t::configure();
  TASCAR::osc_server_t* osc(this);
  if(port.empty())
    osc = session;
  // first, add all regular OSC variables:
  for(auto var : oscvars) {
    recorder.push_back(new recorder_t(var->size + 1, var->path, is_recording,
                                      is_rolling, session->jc, session->srate,
                                      var->ignorefirst, headless));
    var->set_recorder(recorder.back());
    osc->add_method(var->path, var->get_fmt().c_str(),
                    &oscvar_t::osc_receive_sample, var);
  }
  // second, add all string OSC variables:
  for(auto var : oscsvars) {
    recorder.push_back(new recorder_t(2, var->path, is_recording, is_rolling,
                                      session->jc, session->srate, false,
                                      headless));
    var->set_recorder(recorder.back());
    if(var->ds_format)
      osc->add_method(var->path, "ds", &oscsvar_t::osc_receive_sample, var);
    else
      osc->add_method(var->path, "s", &oscsvar_t::osc_receive_sample, var);
  }
#ifdef HAS_LSL
  // finally, add all LSL variables:
  for(auto var : lslvars) {
    recorder.push_back(new recorder_t(var->size + 1, var->name, is_recording,
                                      is_rolling, session->jc, session->srate,
                                      true, headless));
    var->set_recorder(recorder.back());
  }
#endif
  if(!headless) {
    // update display of DC:
    for(auto rec : recorder)
      if(rec->drawer)
        rec->drawer->set_displaydc(displaydc);
    //
    connection_timeout = Glib::signal_timeout().connect(
        sigc::mem_fun(*this, &datalogging_t::on_100ms), 100);
    uint32_t num_cols((uint32_t)(ceil(sqrt(recorder.size()))));
    uint32_t num_rows(
        (uint32_t)(ceil(recorder.size() / std::max(1u, num_cols))));
    for(uint32_t k = 0; k < num_cols; k++)
      draw_grid->insert_column(0);
    for(uint32_t k = 0; k < num_rows; k++)
      draw_grid->insert_row(0);
    for(uint32_t k = 0; k < recorder.size(); k++) {
      uint32_t kc(k % num_cols);
      uint32_t kr(k / num_cols);
      Gtk::Box* box = new Gtk::Box(Gtk::ORIENTATION_VERTICAL);
      draw_grid->attach(*box, kc, kr, 1, 1);
      Gtk::Label* label = new Gtk::Label(recorder[k]->get_name());
      box->pack_start(*label, Gtk::PACK_SHRINK);
      box->pack_start(*(recorder[k]->drawer));
    }
    draw_grid->show_all();
    showdc->set_active(displaydc);
  }
}

void datalogging_t::release()
{
  try {
    stop_trial();
  }
  catch(const std::exception& e) {
    std::cerr << "Warning: " << e.what() << "\n";
  }
  connection_timeout.disconnect();
  TASCAR::osc_server_t::deactivate();
  for(auto prec : recorder)
    delete prec;
  recorder.clear();
#ifdef HAS_LSL
  for(auto var : lslvars)
    delete var;
  lslvars.clear();
#endif
  for(auto var : oscsvars)
    delete var;
  oscsvars.clear();
  for(auto var : oscvars)
    delete var;
  oscvars.clear();
  TASCAR::module_base_t::release();
}

#ifdef HAS_LSL
void lslvar_t::poll_lsl_data()
{
  while(run_lsl_poll_service) {
    poll_data();
    std::this_thread::sleep_for(std::chrono::microseconds(1000));
  }
}
#endif

int datalogging_t::osc_session_start(const char*, const char*, lo_arg**, int,
                                     lo_message, void* user_data)
{
  if(((datalogging_t*)user_data)->is_headless())
    ((datalogging_t*)user_data)->on_ui_start();
  else
    ((datalogging_t*)user_data)->osc_start.emit();
  return 1;
}

int datalogging_t::osc_session_stop(const char*, const char*, lo_arg**, int,
                                    lo_message, void* user_data)
{
  if(((datalogging_t*)user_data)->is_headless())
    ((datalogging_t*)user_data)->on_ui_stop();
  else
    ((datalogging_t*)user_data)->osc_stop.emit();
  return 1;
}

int datalogging_t::osc_session_set_trialid(const char*, const char*,
                                           lo_arg** argv, int, lo_message,
                                           void* user_data)
{
  ((datalogging_t*)user_data)->osc_trialid = std::string(&(argv[0]->s));
  if(!((datalogging_t*)user_data)->is_headless())
    ((datalogging_t*)user_data)->osc_set_trialid.emit();
  return 1;
}

void datalogging_t::on_osc_set_trialid()
{
  if(!headless)
    trialid->set_text(osc_trialid);
}

void datalogging_t::on_ui_showdc()
{
  displaydc = showdc->get_active();
  for(auto rec : recorder)
    if(rec->drawer)
      rec->drawer->set_displaydc(displaydc);
}

void datalogging_t::on_ui_start()
{
  try {
    std::string trialidtext;
    if(headless)
      trialidtext = osc_trialid;
    else
      trialidtext = trialid->get_text();
    if(trialidtext.empty()) {
      trialidtext = "unnamed";
      if(!headless)
        trialid->set_text(trialidtext);
    }
    std::string datestr_(datestr());
    if(!headless)
      datelabel->set_text(datestr_);
    // throw TASCAR::ErrMsg("Empty trial ID string.");
    trialidtext += "_" + datestr_;
    if(!headless)
      win->set_title("tascar datalogging - " + session->name + " [" +
                     trialidtext + "]");
    start_trial(trialidtext);
    if(!headless)
      rec_label->set_text(" REC ");
  }
  catch(const std::exception& e) {
    std::string tmp(e.what());
    tmp += " (start)";
    if(headless)
      std::cerr << "Error: " << tmp << std::endl;
    else
      error_message(tmp);
  }
}

void datalogging_t::on_ui_stop()
{
  try {
    stop_trial();
    if(!headless) {
      rec_label->set_text("");
      win->set_title("tascar datalogging - " + session->name);
    }
  }
  catch(const std::exception& e) {
    std::string tmp(e.what());
    tmp += " (stop)";
    if(headless)
      std::cerr << "Error: " << tmp << std::endl;
    else
      error_message(tmp);
  }
}

bool datalogging_t::on_100ms()
{
  if(!headless) {
    // update labels:
    if(!b_recording)
      datelabel->set_text(datestr());
    char ctmp[1024];
    ctmp[1023] = 0;
    snprintf(ctmp, 1023, "%1.1f s", session->tp_get_time());
    jacktime->set_text(ctmp);
  }
  return true;
}

int oscvar_t::osc_receive_sample(const char* path, const char* types,
                                 lo_arg** argv, int argc, lo_message,
                                 void* user_data)
{
  double data[argc + 1];
  data[0] = 0;
  for(uint32_t k = 0; k < (uint32_t)argc; k++) {
    switch(types[k]) {
    case 'f':
      data[k + 1] = argv[k]->f;
      break;
    case 'd':
      data[k + 1] = argv[k]->d;
      break;
    case 'i':
      data[k + 1] = argv[k]->i;
      break;
    }
  }
  ((oscvar_t*)user_data)->osc_receive_sample(path, argc + 1, data);
  return 1;
}

void oscvar_t::osc_receive_sample(const char*, uint32_t size, double* data)
{
  if(datarecorder) {
    data[0] = datarecorder->get_session_time();
    datarecorder->store_sample(size, data);
  }
}

int oscsvar_t::osc_receive_sample(const char* path, const char*, lo_arg** argv,
                                  int argc, lo_message, void* user_data)
{
  if(argc == 1)
    ((oscsvar_t*)user_data)->osc_receive_sample(path, 0.0, &(argv[0]->s));
  else
    ((oscsvar_t*)user_data)
        ->osc_receive_sample(path, argv[0]->d, &(argv[1]->s));
  return 1;
}

void oscsvar_t::osc_receive_sample(const char*, double t, const char* msg)
{
  if(datarecorder) {
    datarecorder->store_msg(datarecorder->get_session_time(), t, msg);
  }
}

void datalogging_t::start_trial(const std::string& name)
{
  stop_trial();
  if(name.empty())
    throw TASCAR::ErrMsg("Empty trial name.");
  if(controltransport) {
    session->tp_stop();
    session->tp_locate(0u);
    uint32_t timeout(1000);
    while(timeout && (session->tp_get_time() > 0)) {
      std::this_thread::sleep_for(std::chrono::microseconds(1000));
      timeout--;
    }
  }
  for(uint32_t k = 0; k < recorder.size(); k++)
    recorder[k]->clear();
    // lsl re-sync:
#ifdef HAS_LSL
  for(lslvarlist_t::iterator it = lslvars.begin(); it != lslvars.end(); ++it) {
    (*it)->get_stream_delta_start();
  }
#endif
  filename = name;
  b_recording = true;
  // pthread_mutex_unlock(&record_mtx);
  if(controltransport)
    session->tp_start();
  is_recording = true;
}

void datalogging_t::stop_trial()
{
  is_recording = false;
  if(controltransport)
    session->tp_stop();
#ifdef HAS_LSL
  // lsl re-sync:
  for(lslvarlist_t::iterator it = lslvars.begin(); it != lslvars.end(); ++it) {
    (*it)->get_stream_delta_end();
  }
#endif
  if(!headless)
    outputdir = outputdirentry->get_text();
  if(!outputdir.empty()) {
    if(outputdir[outputdir.size() - 1] != '/')
      outputdir = outputdir + "/";
    struct stat info;
    if(stat(outputdir.c_str(), &info) != 0)
      throw TASCAR::ErrMsg("Cannot access output directory \"" + outputdir +
                           "\".");
    if(!(info.st_mode & S_IFDIR)) // S_ISDIR() doesn't exist on my windows
      throw TASCAR::ErrMsg("\"" + outputdir + "\" is not a directory.");
  }
  if(!b_recording)
    return;
  // pthread_mutex_lock(&record_mtx);
  b_recording = false;
  if(fileformat == "txt")
    save_text(filename);
  else if(fileformat == "mat")
    save_mat(filename);
  else if(fileformat == "matcell")
    save_matcell(filename);
}

void datalogging_t::save_text(const std::string& filename)
{
  std::string fname(outputdir + nice_name(filename, "txt"));
  std::ofstream ofs(fname.c_str());
  if(!ofs.good())
    throw TASCAR::ErrMsg("Unable to create file \"" + fname + "\".");
  ofs << "# tascar datalogging\n";
  ofs << "# version: " << TASCARVERSION << "\n";
  ofs << "# trialid: " << filename << "\n";
  ofs << "# filename: " << fname << "\n";
  ofs << "# savedat: " << datestr() << "\n";
  ofs << "# tscfilename: " << session->get_file_name() << "\n";
  ofs << "# tscpath: " << session->get_session_path() << "\n";
  ofs << "# srate: " << srate << "\n";
  ofs << "# fragsize: " << fragsize << "\n";
  for(auto rec : recorder) {
    ofs << "# " << rec->get_name() << std::endl;
    if(rec->is_textdata()) {
      std::vector<label_t> data(rec->get_textdata());
      for(auto it = data.begin(); it != data.end(); ++it)
        ofs << it->t1 << " " << it->t2 << " \"" << it->msg << "\"\n";
    } else {
      std::vector<double> data(rec->get_data());
      size_t N(rec->get_size());
      size_t M(data.size());
      size_t cnt(N - 1);
      for(size_t l = 0; l < M; l++) {
        ofs << data[l];
        if(cnt--) {
          ofs << " ";
        } else {
          ofs << "\n";
          cnt = N - 1;
        }
      }
    }
  }
#ifdef HAS_LSL
  for(auto var : lslvars) {
    ofs << "# " << var->get_name()
        << "_stream_delta_start: " << var->stream_delta_start << "\n";
    ofs << "# " << var->get_name()
        << "_stream_delta_end: " << var->stream_delta_end << "\n";
  }
#endif
}

void mat_add_strvar(mat_t* matfb, const std::string& name,
                    const std::string& str)
{
  size_t dims[2];
  dims[0] = 1;
  dims[1] = str.size();
  char* v((char*)str.c_str());
  matvar_t* mStr(
      Mat_VarCreate(name.c_str(), MAT_C_CHAR, MAT_T_INT8, 2, dims, v, 0));
  if(mStr == NULL)
    throw TASCAR::ErrMsg("Unable to create variable \"" + name + "\".");
  Mat_VarWrite(matfb, mStr, MAT_COMPRESSION_NONE);
  Mat_VarFree(mStr);
}

void mat_add_double(mat_t* matfb, const std::string& name, double v)
{
  size_t dims[2];
  dims[0] = 1;
  dims[1] = 1;
  matvar_t* mVar(
      Mat_VarCreate(name.c_str(), MAT_C_DOUBLE, MAT_T_DOUBLE, 2, dims, &v, 0));
  if(mVar == NULL)
    throw TASCAR::ErrMsg("Unable to create variable \"" + name + "\".");
  Mat_VarWrite(matfb, mVar, MAT_COMPRESSION_NONE);
  Mat_VarFree(mVar);
}

void mat_set_double_field(matvar_t* s, const std::string& name, double v,
                          size_t idx)
{
  size_t dims[2];
  dims[0] = 1;
  dims[1] = 1;
  matvar_t* mVar(
      Mat_VarCreate(NULL, MAT_C_DOUBLE, MAT_T_DOUBLE, 2, dims, &v, 0));
  if(mVar == NULL)
    throw TASCAR::ErrMsg("Unable to create variable \"" + name + "\".");
  Mat_VarSetStructFieldByName(s, name.c_str(), idx, mVar);
}

void mat_add_double_field(matvar_t* s, const std::string& name, double v)
{
  Mat_VarAddStructField(s, name.c_str());
  mat_set_double_field(s, name, v, 0);
}

void mat_set_char_field(matvar_t* s, const std::string& name,
                        const std::string& str, size_t idx)
{
  size_t dims[2];
  dims[0] = 1;
  dims[1] = str.size();
  char* v((char*)str.c_str());
  matvar_t* mStr(Mat_VarCreate(NULL, MAT_C_CHAR, MAT_T_INT8, 2, dims, v, 0));
  if(mStr == NULL)
    throw TASCAR::ErrMsg("Unable to create variable \"" + name + "\".");
  Mat_VarSetStructFieldByName(s, name.c_str(), idx, mStr);
}

void mat_add_char_field(matvar_t* s, const std::string& name,
                        const std::string& str)
{
  Mat_VarAddStructField(s, name.c_str());
  mat_set_char_field(s, name, str, 0);
}

void datalogging_t::update(uint32_t, bool running)
{
  is_rolling = running || (!usetransport);
  if(jc_) {
    // ltime = audio_sample_period_ * jack_get_current_transport_frame(jc_);
    double time = jack_get_current_transport_frame(jc_) * t_sample;
#ifdef HAS_LSL
    double delta = lsl::local_clock() - time;
    for(auto plslvar : lslvars)
      plslvar->set_delta(delta);
#endif
    for(auto prec : recorder)
      if(prec->drawer)
        prec->drawer->set_time(time);
  }
}

void datalogging_t::save_session_related_meta_data(mat_t* matfp,
                                                   const std::string& fname)
{
  mat_add_strvar(matfp, "tascarversion", TASCARVERSION);
  mat_add_strvar(matfp, "trialid", filename);
  mat_add_strvar(matfp, "filename", fname);
  mat_add_strvar(matfp, "savedat", datestr());
  mat_add_strvar(matfp, "tscfilename", session->get_file_name());
  mat_add_strvar(matfp, "tscpath", session->get_session_path());
  mat_add_strvar(matfp, "sourcexml", session->save_to_string());
  mat_add_double(matfp, "fragsize", fragsize);
  mat_add_double(matfp, "srate", srate);
}

matvar_t* create_message_struct(const std::vector<label_t>& msg,
                                const std::string& name)
{
  size_t dims[2];
  dims[0] = msg.size();
  dims[1] = 1;
  const char* fieldnames[3] = {"t_tascar", "t_lsl", "message"};
  matvar_t* matDataStruct(
      Mat_VarCreateStruct(name.c_str(), 2, dims, fieldnames, 3));
  if(matDataStruct == NULL)
    throw TASCAR::ErrMsg("Unable to create message variable.");
  for(uint32_t c = 0; c < msg.size(); ++c) {
    mat_set_double_field(matDataStruct, "t_tascar", msg[c].t1, c);
    mat_set_double_field(matDataStruct, "t_lsl", msg[c].t2, c);
    mat_set_char_field(matDataStruct, "message", msg[c].msg, c);
  }
  return matDataStruct;
}

void datalogging_t::save_mat(const std::string& filename)
{
  // first create mat file:
  mat_t* matfp;
  std::string fname(outputdir + nice_name(filename, "mat"));
  matfp = Mat_CreateVer(fname.c_str(), NULL, MAT_FT_MAT5);
  if(NULL == matfp)
    throw TASCAR::ErrMsg("Unable to create file \"" + fname + "\".");
  // if anything fails, try to close file.
  try {
    // now, store session-related meta data:
    save_session_related_meta_data(matfp, fname);
    // store recorded variables:
    size_t dims[2];
    for(uint32_t k = 0; k < recorder.size(); k++) {
      std::string name(nice_name(recorder[k]->get_name()));
      matvar_t* mvar(NULL);
      if(recorder[k]->is_textdata()) {
        mvar = create_message_struct(recorder[k]->get_textdata(), name);
      } else {
        std::vector<double> data(recorder[k]->get_data());
        size_t N(recorder[k]->get_size());
        size_t M(data.size());
        dims[1] = M / N;
        dims[0] = N;
        double* x(&(data[0]));
        mvar = Mat_VarCreate(name.c_str(), MAT_C_DOUBLE, MAT_T_DOUBLE, 2, dims,
                             x, 0);
      }
      if(mvar == NULL)
        throw TASCAR::ErrMsg("Unable to create variable \"" + name + "\".");
      Mat_VarWrite(matfp, mvar, MAT_COMPRESSION_NONE);
      Mat_VarFree(mvar);
    }
#ifdef HAS_LSL
    // add LSL variable meta data:
    for(auto var : lslvars) {
      mat_add_double(matfp, var->get_name() + "_stream_delta_start",
                     var->stream_delta_start);
      mat_add_double(matfp, var->get_name() + "_stream_delta_end",
                     var->stream_delta_end);
      mat_add_strvar(matfp, var->get_name() + "_info", var->get_xml().c_str());
    }
#endif
  }
  catch(...) {
    // close mat file, to have as much written as possible.
    Mat_Close(matfp);
    throw;
  }
  Mat_Close(matfp);
}

void datalogging_t::save_matcell(const std::string& filename)
{
  // first create mat file:
  mat_t* matfp;
  std::string fname(outputdir + nice_name(filename, "mat"));
  matfp = Mat_CreateVer(fname.c_str(), NULL, MAT_FT_MAT5);
  if(NULL == matfp)
    throw TASCAR::ErrMsg("Unable to create file \"" + fname + "\".");
  // if anything fails, try to close file.
  try {
    // now, store session-related meta data:
    save_session_related_meta_data(matfp, fname);
    // store recorded variables:
    size_t dims[2];
    dims[0] = recorder.size();
    dims[1] = 1;
    matvar_t* cell_array(
        Mat_VarCreate("data", MAT_C_CELL, MAT_T_CELL, 2, dims, NULL, 0));
    if(cell_array == NULL)
      throw TASCAR::ErrMsg("Unable to create data cell array.");
    matvar_t* cell_array_names(
        Mat_VarCreate("names", MAT_C_CELL, MAT_T_CELL, 2, dims, NULL, 0));
    if(cell_array == NULL)
      throw TASCAR::ErrMsg("Unable to create data cell array.");
    for(uint32_t k = 0; k < recorder.size(); k++) {
      std::string name(nice_name(recorder[k]->get_name()));
      // create data cell entry as struct:
      const char* fieldnames[2] = {"data", "name"};
      dims[0] = 1;
      dims[1] = 1;
      matvar_t* matDataStruct(
          Mat_VarCreateStruct(NULL, 2, dims, fieldnames, 2));
      if(matDataStruct == NULL)
        throw TASCAR::ErrMsg("Unable to create variable \"" + name + "\".");
      matvar_t* mData(NULL);
      if(recorder[k]->is_textdata()) {
        mData = create_message_struct(recorder[k]->get_textdata(), name);
      } else {
        std::vector<double> data(recorder[k]->get_data());
        size_t N(recorder[k]->get_size());
        size_t M(data.size());
        dims[1] = M / N;
        dims[0] = N;
        double* x(&(data[0]));
        mData = Mat_VarCreate(NULL, MAT_C_DOUBLE, MAT_T_DOUBLE, 2, dims, x, 0);
      }
      if(mData == NULL)
        throw TASCAR::ErrMsg("Unable to create variable \"" + name + "\".");
      Mat_VarSetStructFieldByName(matDataStruct, "data", 0, mData);
      dims[0] = 1;
      dims[1] = name.size();
      char* v((char*)name.c_str());
      matvar_t* mStr(
          Mat_VarCreate(NULL, MAT_C_CHAR, MAT_T_INT8, 2, dims, v, 0));
      if(mStr == NULL)
        throw TASCAR::ErrMsg("Unable to create variable \"" + name + "\".");
      Mat_VarSetStructFieldByName(matDataStruct, "name", 0, mStr);
#ifdef HAS_LSL
      // test if this is an LSL variable, if yes, provide some header
      // information
      for(auto var : lslvars)
        if(var->is_linked_with(recorder[k]) && var->has_inlet()) {
          mat_add_char_field(matDataStruct, "lsl_name", var->get_info().name());
          mat_add_char_field(matDataStruct, "lsl_type", var->get_info().type());
          mat_add_double_field(matDataStruct, "lsl_srate",
                               var->get_info().nominal_srate());
          mat_add_char_field(matDataStruct, "lsl_source_id",
                             var->get_info().source_id());
          mat_add_char_field(matDataStruct, "lsl_hostname",
                             var->get_info().hostname());
          mat_add_double_field(matDataStruct, "lsl_protocolversion",
                               var->get_info().version());
          mat_add_double_field(matDataStruct, "lsl_libraryversion",
                               lsl::library_version());
          mat_add_double_field(matDataStruct, "stream_delta_start",
                               var->stream_delta_start);
          mat_add_double_field(matDataStruct, "stream_delta_end",
                               var->stream_delta_end);
          mat_add_char_field(matDataStruct, "lsl_info", var->get_xml());
        }
#endif
      // here add var to cell array!
      Mat_VarSetCell(cell_array, k, matDataStruct);
      matvar_t* mStrName(
          Mat_VarCreate(NULL, MAT_C_CHAR, MAT_T_INT8, 2, dims, v, 0));
      if(mStrName == NULL)
        throw TASCAR::ErrMsg("Unable to create variable \"" + name + "\".");
      Mat_VarSetCell(cell_array_names, k, mStrName);
    }
    // add data cell array to Mat file:
    Mat_VarWrite(matfp, cell_array, MAT_COMPRESSION_NONE);
    Mat_VarWrite(matfp, cell_array_names, MAT_COMPRESSION_NONE);
    Mat_VarFree(cell_array);
    Mat_VarFree(cell_array_names);
  }
  catch(...) {
    Mat_Close(matfp);
    throw;
  }
  Mat_Close(matfp);
}

REGISTER_MODULE(datalogging_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
