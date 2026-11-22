/*
 * This file is part of the TASCAR software, see <http://tascar.org/>
 *
 * Copyright (c) 2019 Giso Grimm
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

#include <opencv2/opencv.hpp>

#include "errorhandling.h"
#include "glabsensorplugin.h"
#include "linuxtrack.c"
#include "linuxtrack.h"
#include <algorithm>
#include <complex>
#include <fstream>
#include <lsl_cpp.h>
#include <mutex>
#include <numeric>
#include <thread>

using namespace TASCAR;
using namespace std::complex_literals;

#define NUM_MARKER 10
#define MAX_CENTER_DISTANCE 5
#define PLOT_AGELIMIT 5

zyx_euler_t rotmat2euler(cv::Mat& R)
{
  double sy(sqrt(R.at<double>(0, 0) * R.at<double>(0, 0) +
                 R.at<double>(1, 0) * R.at<double>(1, 0)));

  bool singular = sy < 1e-6; // If

  zyx_euler_t r;
  if(!singular) {
    r.x = atan2(R.at<double>(2, 1), R.at<double>(2, 2));
    r.y = atan2(-R.at<double>(2, 0), sy);
    r.z = atan2(R.at<double>(1, 0), R.at<double>(0, 0));
  } else {
    r.x = atan2(-R.at<double>(1, 2), R.at<double>(1, 1));
    r.y = atan2(-R.at<double>(2, 0), sy);
    r.z = 0;
  }
  return r;
}

void zyx_euler2mat(const TASCAR::zyx_euler_t& eul, cv::Mat& mat)
{
  mat =
      // rot_z:
      (cv::Mat_<double>(3, 3) << cos(eul.z), -sin(eul.z), 0, sin(eul.z),
       cos(eul.z), 0, 0, 0, 1) *
      // rot_y:
      (cv::Mat_<double>(3, 3) << cos(eul.y), 0, sin(eul.y), 0, 1, 0,
       -sin(eul.y), 0, cos(eul.y)) *
      // rot_x:
      (cv::Mat_<double>(3, 3) << 1, 0, 0, 0, cos(eul.x), -sin(eul.x), 0,
       sin(eul.x), cos(eul.x));
}

void zyx_euler_inv2mat(const TASCAR::zyx_euler_t& eul, cv::Mat& mat)
{
  mat =
      // rot_x:
      (cv::Mat_<double>(3, 3) << 1, 0, 0, 0, cos(eul.x), sin(eul.x), 0,
       -sin(eul.x), cos(eul.x)) *
      // rot_y:
      (cv::Mat_<double>(3, 3) << cos(eul.y), 0, -sin(eul.y), 0, 1, 0,
       sin(eul.y), 0, cos(eul.y)) *
      // rot_z:
      (cv::Mat_<double>(3, 3) << cos(eul.z), sin(eul.z), 0, -sin(eul.z),
       cos(eul.z), 0, 0, 0, 1);
}

/**
  \brief Return the distance from a line.
  \param vP list of points
  \param k1 index of first line point
  \param k2 index of second line point
  \param k3 index of the tested point
  \retval r resulting ratio of intersection point; values between 0 and 1 are
  within the line \return orthogonal distance from the line
 */
double distance_from_line(const std::vector<std::complex<double>>& vP,
                          uint32_t k1, uint32_t k2, uint32_t k3, double& r)
{
  std::complex<double> p2(vP[k2] - vP[k1]);
  double l(std::abs(p2));
  std::complex<double> p((vP[k3] - vP[k1]) * conj(p2 / l));
  r = std::real(p) / l;
  return std::abs(std::imag(p));
}

/**
   \brief Count the numbers of lines with three points starting at a given point
   \param vP list of points
   \param k index of starting point
   \param threshold threshold to be counted as straight line
   \retval idx_end list of end points of each line
   \retval idx_mid list of mid points of each line
   \retval vr Ratio of intersection
   \return Number of lines
 */
uint32_t num_lines(const std::vector<std::complex<double>>& vP, uint32_t k,
                   double threshold, std::vector<uint32_t>& idx_end,
                   std::vector<uint32_t>& idx_mid, std::vector<double>& vr)
{
  idx_end.resize(0);
  idx_mid.resize(0);
  vr.resize(0);
  uint32_t n(0);
  uint32_t nP(vP.size());
  for(uint32_t k1 = 0; k1 < nP; ++k1)
    if(k1 != k) {
      uint32_t nmatch(0);
      uint32_t kmatch(0);
      double rmatch(0);
      for(uint32_t k2 = 0; k2 < nP; ++k2)
        if((k2 != k1) && (k2 != k)) {
          double r(0);
          double d(distance_from_line(vP, k, k1, k2, r));
          if((d < threshold) && (r > 0.2) && (r < 0.8)) {
            nmatch++;
            rmatch = r;
            kmatch = k2;
          }
        }
      if(nmatch == 1) {
        n++;
        idx_end.push_back(k1);
        idx_mid.push_back(kmatch);
        vr.push_back(rmatch);
      }
    }
  return n;
}

class data_t {
public:
  data_t();
  void trycopydataraw(const data_t*);
  void trycopydatasolved(const data_t*);
  // camera coordinates of markers:
  std::vector<std::complex<double>> vP;
  std::vector<std::vector<uint32_t>> cIdx;
  std::vector<std::vector<uint32_t>> cIdx_mid;
  std::vector<cv::Point2f> imagePoints;
  std::vector<cv::Point2f> crown2D;
  // back reference from image point to crown marker number:
  std::vector<uint32_t> crownIndex;
  // current transformation in global coordinates:
  TASCAR::c6dof_t tf_global;
  // camera transformation:
  TASCAR::c6dof_t tf_cam;
  uint32_t ageraw;
  uint32_t agesolved;
  //
  std::mutex mtx;
};

data_t::data_t()
    : vP(NUM_MARKER, 0.0),
      cIdx(NUM_MARKER, std::vector<uint32_t>(NUM_MARKER, 0)),
      cIdx_mid(NUM_MARKER, std::vector<uint32_t>(NUM_MARKER, 0)), ageraw(-1),
      agesolved(-1)
{
}

void data_t::trycopydatasolved(const data_t* src)
{
  if(mtx.try_lock()) {
    imagePoints = src->imagePoints;
    crownIndex = src->crownIndex;
    crown2D = src->crown2D;
    tf_global = src->tf_global;
    tf_cam = src->tf_cam;
    agesolved = 0;
    mtx.unlock();
  }
}

void data_t::trycopydataraw(const data_t* src)
{
  if(mtx.try_lock()) {
    vP = src->vP;
    cIdx = src->cIdx;
    cIdx_mid = src->cIdx_mid;
    ageraw = 0;
    mtx.unlock();
  }
}

class trackir_tracker_t : public sensorplugin_drawing_t, public data_t {
public:
  class error_t {
  public:
    error_t(double e_ = 0, uint32_t k0_ = 0, uint32_t k1_ = 0)
        : e(e_), k0(k0_), k1(k1_){};
    double e;
    uint32_t k0;
    uint32_t k1;
  };
  trackir_tracker_t(const sensorplugin_cfg_t& cfg);
  virtual ~trackir_tracker_t();

protected:
  virtual void solve() = 0;
  std::vector<std::complex<double>> vCrown;
  std::complex<double> rot_right;
  std::vector<error_t> vMap;
  std::vector<double> transform;
  std::vector<double> transform1;

protected:
  void service();
  void get_and_process_ltr_markers();
  void marker_match();
  void find_lines(double threshold = 1.0);
  lsl::stream_info info_marker;
  lsl::stream_outlet outlet_marker;
  lsl::stream_info info_transform;
  lsl::stream_outlet outlet_transform;
  lsl::stream_info info_transform1;
  lsl::stream_outlet outlet_transform1;
  std::vector<std::complex<double>> vPN;
  std::vector<uint32_t> vN;
  std::vector<std::vector<double>> cR;
  std::complex<double> pC;
  float blobs[3 * NUM_MARKER];
  uint32_t counter;
  std::vector<uint32_t> idxLines;
  std::vector<uint32_t> idx2Lines;
  std::vector<uint32_t> idxNLines;
  std::vector<double> vStd;
  double scale;
  std::complex<double> crot;
  double rot;
  double linethreshold;
  double maxdist;
  double margin;
  // current transformation based on pure sorting algorithm:
  TASCAR::c6dof_t tf_presolve;

protected:
  bool solved;
  std::thread srv;
  bool run_service;
  bool use_calib;
  data_t plot;
  bool flipx;
  bool flipy;
  float f;
};

class trackir_solver_t : public trackir_tracker_t {
public:
  trackir_solver_t(const sensorplugin_cfg_t& cfg);
  void solve();
  void load_camcalib();
  void load_crown();
  void current2camcalib();
  void save_camcalib();
  void clear_camcalib();

protected:
  std::vector<cv::Point3f> objectPoints;
  std::vector<cv::Point3f> crown3D;
  cv::Mat cameraMatrix;
  cv::Mat distCoeffs;
  // raw camera output:
  cv::Mat rvec;
  cv::Mat tvec;
  // current transformation in camera coordinates:
  TASCAR::c6dof_t tf_current;
  // previous (applied) transformation:
  TASCAR::pos_t prevpos;
  TASCAR::pos_t prevrot;
  // warning threshold of crown motion in one cycle:
  double maxframedist;
  std::string camcalibfile;
  std::string crownfile;
  cv::Mat cam_comp_mat;
  bool use_presolvepos;
};

class trackir_drawer_t : public trackir_solver_t {
public:
  trackir_drawer_t(const sensorplugin_cfg_t& cfg);
  ~trackir_drawer_t(){};
  void draw(const Cairo::RefPtr<Cairo::Context>& cr, double width,
            double height);
  void on_click_calib();
  void on_click_calib_save();
  void on_click_calib_load();
  void on_click_calib_reset();
  void on_click_calib_use();
  void on_click_drawraw();
  Gtk::Widget& get_gtkframe() { return hbox; };

protected:
  //
  bool camview;
  Gtk::HBox hbox;
  Gtk::VBox vbox;
  Gtk::Button b_calib;
  Gtk::Button b_calibsave;
  Gtk::Button b_calibload;
  Gtk::Button b_calibreset;
  Gtk::CheckButton b_calibuse;
  Gtk::CheckButton b_drawraw;
};

bool err_comp(const trackir_tracker_t::error_t& a,
              const trackir_tracker_t::error_t& b)
{
  return a.e < b.e;
}

trackir_tracker_t::trackir_tracker_t(const sensorplugin_cfg_t& cfg)
    : sensorplugin_drawing_t(cfg), vMap(NUM_MARKER, error_t(0, 0)),
      transform(6, 0.0), transform1(6, 0.0),
      info_marker(name + "marker", "motrack", 3 * NUM_MARKER, 120,
                  lsl::cf_double64,
                  name + cfg.hostname + "trackir5raw_t" + TASCARVER),
      outlet_marker(info_marker),
      info_transform(name, "motrack", 6, 120, lsl::cf_double64,
                     cfg.hostname + "trackir5tf_t" + TASCARVER),
      outlet_transform(info_transform),
      info_transform1(name + "presolve", "motrack", 6, 120, lsl::cf_double64,
                      cfg.hostname + "trackir5tf1_t" + TASCARVER),
      outlet_transform1(info_transform1), vPN(NUM_MARKER, 0.0),
      vN(NUM_MARKER, 0), cR(NUM_MARKER, std::vector<double>(NUM_MARKER, 0.0)),
      pC(HUGE_VAL), counter(0), idxLines(NUM_MARKER, 0),
      idx2Lines(NUM_MARKER, 0), idxNLines(NUM_MARKER, 0), vStd(NUM_MARKER, 0.0),
      scale(1.0), crot(1.0), rot(0.0), linethreshold(1.0), maxdist(0.05),
      margin(100), solved(false), run_service(true), use_calib(true),
      flipx(false), flipy(false), f(640)
{
  GET_ATTRIBUTE_(linethreshold);
  GET_ATTRIBUTE_(maxdist);
  GET_ATTRIBUTE_(margin);
  GET_ATTRIBUTE_BOOL_(use_calib);
  GET_ATTRIBUTE_BOOL_(flipx);
  GET_ATTRIBUTE_BOOL_(flipy);
  memset(blobs, 0, 3 * NUM_MARKER * sizeof(float));
  // crown shape:
  // front out: 0.105 0 0
  vCrown.push_back(0.105);
  // right out: -0.0525 -0.0909327 0
  vCrown.push_back(std::complex<double>(-0.0525, -0.0909327));
  // left out: -0.0525 0.0909327 0
  vCrown.push_back(std::complex<double>(-0.0525, 0.0909327));
  // center: 0 0 0
  vCrown.push_back(0);
  // front in: 0.035 0 0
  vCrown.push_back(0.035);
  // right in: -0.035 -0.0606218 0
  vCrown.push_back(std::complex<double>(-0.035, -0.0606218));
  //// correct for camera orientation:
  // for(uint32_t k=0;k<vCrown.size();++k)
  //  vCrown[k] = std::conj(-std::complex<double>(0.0,1.0)*vCrown[k]);
  da.set_size_request(360, 240);
  srv = std::thread(&trackir_tracker_t::service, this);
  rot_right = vCrown[1] - vCrown[3];
  rot_right /= std::abs(rot_right);
}

void trackir_tracker_t::get_and_process_ltr_markers()
{
  memset(blobs, 0, 3 * NUM_MARKER * sizeof(float));
  linuxtrack_pose_t pose;
  memset(&pose, 0, sizeof(pose));
  int blobs_read(0);
  int err(linuxtrack_get_pose_full(&pose, blobs, NUM_MARKER, &blobs_read));
  if(err != 0) {
    alive();
    if(counter != pose.counter) {
      std::lock_guard<std::mutex> lock(mtx);
      outlet_marker.push_sample(blobs);
      if(blobs_read > 3) {
        vP.resize(blobs_read);
        for(int32_t k = 0; k < blobs_read; ++k) {
          vP[k] =
              (1.0 - 2.0 * (double)flipx) * (double)(blobs[3 * k]) +
              1.0i * (1.0 - 2.0 * (double)flipy) * (double)(blobs[3 * k + 1]);
        }
        marker_match();
        if(transform[0] != HUGE_VAL) {
          outlet_transform.push_sample(transform);
          outlet_transform1.push_sample(transform1);
        } else
          solved = false;
      } else {
        solved = false;
      }
      if(!solved)
        add_warning("Unable to solve camera.");
      counter = pose.counter;
    }
  } else {
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    // usleep(2000);
  }
}

std::complex<double>
get_median_point(const std::vector<std::complex<double>>& vP)
{
  std::vector<double> tmp(vP.size());
  for(uint32_t k = 0; k < tmp.size(); ++k)
    tmp[k] = std::real(vP[k]);
  std::vector<double>::iterator med(tmp.begin() + tmp.size() / 2);
  std::nth_element(tmp.begin(), med, tmp.end());
  std::complex<double> r(*med);
  for(uint32_t k = 0; k < tmp.size(); ++k)
    tmp[k] = std::imag(vP[k]);
  med = tmp.begin() + tmp.size() / 2;
  std::nth_element(tmp.begin(), med, tmp.end());
  r += std::complex<double>(1.0i * (*med));
  return r;
}

/**
   \brief Try to align camera marker points with the corresponding crown markers
*/
void trackir_tracker_t::marker_match()
{
  uint32_t kC(-1);
  // detect lines of three markers, allow 1px off-axis distance:
  find_lines(linethreshold);
  // find markers which are line endpoints:
  idxLines.resize(0);
  idx2Lines.resize(0);
  idxNLines.resize(0);
  for(uint32_t k = 0; k < vN.size(); ++k) {
    if(vN[k] > 0)
      idxLines.push_back(k);
    if(vN[k] == 2)
      idx2Lines.push_back(k);
    if(vN[k] > 2)
      idxNLines.push_back(k);
  }
  // at least one line is required to identify crown:
  if(idxLines.empty()) {
    return;
  }
  if(idx2Lines.size() == 1) {
    // ideal case is to have a single marker which is connected with
    // two lines. Then this is the central marker:
    kC = idx2Lines[0];
  } else {
    // non-ideal case.
    if(idx2Lines.size() == 0) {
      // no marker with two lines, so lets see if there are markers
      // with more than two lines:
      if(idxNLines.size() == 1) {
        // exactly one marker with more than two lines, this is
        // probably the central marker:
        kC = idxNLines[0];
      } else {
        if(idxNLines.size() > 0) {
          // we have more than one marker connected to more than two
          // lines. Most likely, the marker is the central marker
          // for which the line length has least standard deviation:
          vStd.resize(idxNLines.size());
          for(uint32_t l = 0; l < idxNLines.size(); ++l) {
            std::vector<double> acc(cIdx[idxNLines[l]].size());
            for(uint32_t m = 0; m < cIdx[idxNLines[l]].size(); ++m)
              acc[m] = std::abs(vP[cIdx[idxNLines[l]][m]] - vP[idxNLines[l]]);
            double mean(std::accumulate(acc.begin(), acc.end(), 0.0) /
                        acc.size());
            vStd[l] = std::sqrt(
                std::inner_product(acc.begin(), acc.end(), acc.begin(), 0.0) /
                    acc.size() -
                mean * mean);
          }
          kC = idxNLines[std::min_element(vStd.begin(), vStd.end()) -
                         vStd.begin()];
        } else {
          // no marker with two or more lines, i.e., only markers with one
          // line. We take the marker (of a line end) which is nearest
          // to the previous central marker, given that the distance is
          // below a threshold. Otherwise we select the line end marker
          // which is closest to the median point.
          std::vector<double> vDist(idxLines.size());
          bool use_med(pC == HUGE_VAL);
          std::vector<double>::iterator dmini(vDist.begin());
          if(!use_med) {
            for(uint32_t k = 0; k < idxLines.size(); ++k)
              vDist[k] = std::abs(vP[idxLines[k]] - pC);
            dmini = std::min_element(vDist.begin(), vDist.end());
            kC = idxLines[dmini - vDist.begin()];
            if(*dmini > MAX_CENTER_DISTANCE)
              use_med = true;
          }
          if(use_med) {
            // above threshold, thus test for median point:
            std::complex<double> pMed(get_median_point(vP));
            vDist.resize(idxLines.size());
            for(uint32_t k = 0; k < idxLines.size(); ++k)
              vDist[k] = std::abs(vP[idxLines[k]] - pMed);
            std::vector<double>::iterator dmini(
                std::min_element(vDist.begin(), vDist.end()));
            kC = idxLines[dmini - vDist.begin()];
          }
        }
      }
    } else {
      // we have more than one marker with two lines, so we chose
      // the one with lowest standard deviation of line length:
      vStd.resize(idx2Lines.size());
      for(uint32_t l = 0; l < idx2Lines.size(); ++l) {
        std::vector<double> acc(cIdx[idx2Lines[l]].size());
        for(uint32_t m = 0; m < cIdx[idx2Lines[l]].size(); ++m)
          acc[m] = std::abs(vP[cIdx[idx2Lines[l]][m]] - vP[idx2Lines[l]]);
        double mean(std::accumulate(acc.begin(), acc.end(), 0.0) / acc.size());
        vStd[l] = std::sqrt(
            std::inner_product(acc.begin(), acc.end(), acc.begin(), 0.0) /
                acc.size() -
            mean * mean);
      }
      kC = idx2Lines[std::min_element(vStd.begin(), vStd.end()) - vStd.begin()];
    }
  }
  pC = vP[kC];
  // test for out-of-range:
  if((std::abs(std::real(pC)) > (320 - margin)) ||
     (std::abs(std::imag(pC)) > (240 - margin)))
    add_warning("Center marker is almost out of range.");
  // estimate scaling factor, and normalise points:
  std::vector<double> acc(cIdx[kC].size());
  for(uint32_t k = 0; k < acc.size(); ++k)
    acc[k] = std::abs(vP[cIdx[kC][k]] - pC);
  std::vector<double>::iterator med(acc.begin() + acc.size() / 2);
  std::nth_element(acc.begin(), med, acc.end());
  scale = (*med) / 0.105;
  vPN.resize(vP.size());
  for(uint32_t k = 0; k < vPN.size(); ++k)
    vPN[k] = (vP[k] - pC) / scale;
  // estimate rotation:
  crot = vPN[cIdx[kC][0]] / std::abs(vPN[cIdx[kC][0]]);
  if(cR[kC][0] > 0.5) {
    // if the first line is not the frontal one (thus it is the right
    // wing), then rotate by 1/3:
    crot /= rot_right;
    // DEBUG(cR[kC][0]);
  }
  // compensate rotation:
  crot = std::conj(crot);
  for(uint32_t k = 0; k < vPN.size(); ++k)
    vPN[k] *= crot;
  rot = std::arg(crot);
  // DEBUG(cR[kC].size());
  // for(uint32_t k=0;k<cR[kC].size();++k){
  //  DEBUG(cR[kC][k]);
  //}
  // now do the comparison with the real crown:
  vMap.resize(0);
  // add central marker:
  vMap.push_back(error_t(0, 3, kC));
  uint32_t kFront(-1);
  uint32_t kRight(-1);
  if(cR[kC][0] < 0.5) {
    // add front marker:
    vMap.push_back(error_t(0, 0, cIdx[kC][0]));
    vMap.push_back(error_t(0, 4, cIdx_mid[kC][0]));
    kFront = cIdx[kC][0];
  } else {
    // add right wing marker:
    vMap.push_back(error_t(0, 1, cIdx[kC][0]));
    vMap.push_back(error_t(0, 5, cIdx_mid[kC][0]));
    kRight = cIdx[kC][0];
  }
  if(cIdx[kC].size() > 1) {
    if(cR[kC][1] < 0.5) {
      // add front marker:
      vMap.push_back(error_t(0, 0, cIdx[kC][1]));
      vMap.push_back(error_t(0, 4, cIdx_mid[kC][1]));
      kFront = cIdx[kC][1];
    } else {
      // add right wing marker:
      vMap.push_back(error_t(0, 1, cIdx[kC][1]));
      vMap.push_back(error_t(0, 5, cIdx_mid[kC][1]));
      kRight = cIdx[kC][1];
    }
  }
  for(uint32_t k = 0; k < vPN.size(); ++k) {
    if((k != kC) && (k != kFront) && (k != kRight)) {
      // for each tested marker find the best-matching real one:
      std::vector<double> acc(vCrown.size());
      for(uint32_t l = 0; l < acc.size(); ++l) {
        acc[l] = std::abs(vPN[k] - vCrown[l]);
        if((l == 3) || ((kFront != -1u) && ((l == 0) || (l == 4))) ||
           ((kRight != -1u) && ((l == 1) || (l == 5))))
          acc[l] = HUGE_VAL;
      }
      std::vector<double>::iterator dmin(min_element(acc.begin(), acc.end()));
      error_t err(*dmin, dmin - acc.begin(), k);
      if(err.e < maxdist)
        vMap.push_back(err);
    }
  }
  // sort by distance, select the 6 best ones, if they are below 1cm
  // threshold:
  std::sort(vMap.begin(), vMap.end(), err_comp);
  if(vMap.size() > 6)
    vMap.resize(6);
  // transform complex rotation value to angle:
  if(cIdx[kC].size() > 0) {
    double lscale(0);
    for(uint32_t k = 0; k < cIdx[kC].size(); ++k)
      lscale += std::abs(vP[cIdx[kC][k]] - pC);
    lscale /= (cIdx[kC].size() * f * 0.105);
    TASCAR::zyx_euler_t eul(-rot, 0, 0);
    tf_presolve.orientation = eul;
    tf_presolve.position.z = -1.0 / lscale;
    tf_presolve.position.x = -std::real(pC) * tf_presolve.position.z / f;
    tf_presolve.position.y = -std::imag(pC) * tf_presolve.position.z / f;
  }
  solve();
}

/**
\brief Find all lines consisting of three points
\param threshold Distance threshold to count a line as straight

Resulting numbers are stored in vN, resulting end point lists are stored in
cIdx.
*/
void trackir_tracker_t::find_lines(double threshold)
{
  vN.resize(vP.size());
  cIdx.resize(vP.size());
  cIdx_mid.resize(vP.size());
  cR.resize(vP.size());
  for(uint32_t k = 0; k < vP.size(); ++k)
    vN[k] = num_lines(vP, k, threshold, cIdx[k], cIdx_mid[k], cR[k]);
}

trackir_solver_t::trackir_solver_t(const sensorplugin_cfg_t& cfg)
    : trackir_tracker_t(cfg),
      distCoeffs(cv::Mat::zeros(4, 1, cv::DataType<double>::type)),
      maxframedist(0.05), use_presolvepos(false)
{
  zyx_euler_inv2mat(tf_cam.orientation.to_euler(), cam_comp_mat);
  GET_ATTRIBUTE_(f);
  GET_ATTRIBUTE_(maxframedist);
  GET_ATTRIBUTE_(camcalibfile);
  GET_ATTRIBUTE_(crownfile);
  GET_ATTRIBUTE_BOOL_(use_presolvepos);
  if(camcalibfile.empty())
    camcalibfile = "${HOME}/tascartrackircamcalib.txt";
  if(crownfile.empty())
    crownfile = "${HOME}/tascartrackircrown.txt";
  cameraMatrix.create(3, 3, CV_32F);
  cameraMatrix.at<float>(0, 0) = f;
  cameraMatrix.at<float>(1, 1) = f;
  cameraMatrix.at<float>(2, 2) = 1;
  imagePoints.resize(6);
  objectPoints.resize(6);
  crown2D.resize(vCrown.size());
  crown3D.resize(vCrown.size());
  for(uint32_t k = 0; k < vCrown.size(); ++k)
    crown3D[k] = cv::Point3f(std::real(vCrown[k]), std::imag(vCrown[k]), 0.0);
  try {
    load_crown();
  }
  catch(const std::exception& e) {
    add_warning(e.what());
  }
  try {
    load_camcalib();
  }
  catch(const std::exception& e) {
    add_warning(e.what());
  }
}

void trackir_solver_t::current2camcalib()
{
  std::lock_guard<std::mutex> lock(mtx);
  if(solved) {
    tf_cam = tf_current;
    zyx_euler_inv2mat(tf_cam.orientation.to_euler(), cam_comp_mat);
    // cv::Mat mat;
    // zyx_euler2mat( tf_cam.orientation, mat );
    // DEBUG(tf_cam.orientation.print());
    // DEBUG(rotmat2euler(cam_comp_mat).print());
    // DEBUG(rotmat2euler(mat).print());
  } else
    throw TASCAR::ErrMsg("Currently no data is available.");
}

void trackir_solver_t::load_camcalib()
{
  std::ifstream calibfile(TASCAR::env_expand(camcalibfile).c_str());
  if(!calibfile.good())
    throw TASCAR::ErrMsg("Unable to open calib file \"" +
                         TASCAR::env_expand(camcalibfile) + "\".");
  std::vector<double> calibvals;
  while(!calibfile.eof()) {
    std::string calibline;
    getline(calibfile, calibline, '\n');
    size_t commentpos(calibline.find("#"));
    if(commentpos < std::string::npos)
      calibline.erase(calibline.begin() + commentpos, calibline.end());
    if(!calibline.empty()) {
      std::vector<double> linevals(TASCAR::str2vecdouble(calibline));
      calibvals.insert(calibvals.end(), linevals.begin(), linevals.end());
    }
  }
  if(calibvals.size() != 6) {
    DEBUG(calibvals.size());
    throw TASCAR::ErrMsg("Invalid number of calibration values in file " +
                         camcalibfile);
  }
  std::lock_guard<std::mutex> lock(mtx);
  tf_cam.position = TASCAR::pos_t(calibvals[0], calibvals[1], calibvals[2]);
  tf_cam.orientation = TASCAR::zyx_euler_t(
      DEG2RAD * calibvals[3], DEG2RAD * calibvals[4], DEG2RAD * calibvals[5]);
  zyx_euler_inv2mat(tf_cam.orientation.to_euler(), cam_comp_mat);
}

void trackir_solver_t::load_crown()
{
  std::ifstream fcrownfile(TASCAR::env_expand(crownfile).c_str());
  if(!fcrownfile.good())
    throw TASCAR::ErrMsg("Unable to open crown file \"" +
                         TASCAR::env_expand(crownfile) + "\".");
  std::vector<double> crown_coordinates;
  while(!fcrownfile.eof()) {
    std::string calibline;
    getline(fcrownfile, calibline, '\n');
    size_t commentpos(calibline.find("#"));
    if(commentpos < std::string::npos)
      calibline.erase(calibline.begin() + commentpos, calibline.end());
    if(!calibline.empty()) {
      std::vector<double> linevals(TASCAR::str2vecdouble(calibline));
      crown_coordinates.insert(crown_coordinates.end(), linevals.begin(),
                               linevals.end());
    }
  }
  if(crown_coordinates.size() != 18) {
    DEBUG(crown_coordinates.size());
    throw TASCAR::ErrMsg("Invalid number of values in file " + crownfile);
  }
  std::lock_guard<std::mutex> lock(mtx);
  crown3D.resize(6);
  crown2D.resize(6);
  vCrown.resize(6);
  for(uint32_t k = 0; k < 6; ++k) {
    crown3D[k] =
        cv::Point3f(crown_coordinates[3 * k], crown_coordinates[3 * k + 1],
                    crown_coordinates[3 * k + 2]);
    crown2D[k] =
        cv::Point2f(crown_coordinates[3 * k], crown_coordinates[3 * k + 1]);
    vCrown[k] = crown_coordinates[3 * k] + 1.0i * crown_coordinates[3 * k + 1];
  }
  rot_right = vCrown[1] - vCrown[3];
  rot_right /= std::abs(rot_right);
}

void trackir_solver_t::save_camcalib()
{
  std::ofstream calibfile(TASCAR::env_expand(camcalibfile).c_str());
  if(!calibfile.good())
    throw TASCAR::ErrMsg("Unable to open calib file \"" +
                         TASCAR::env_expand(camcalibfile) + "\".");
  std::lock_guard<std::mutex> lock(mtx);
  auto eul = tf_cam.orientation.to_euler();
  calibfile << "# camera calibration" << std::endl
            << "# pos.x pos.y pos.z (meter)" << std::endl
            << "# rot.z rot.y rot.x (degree)" << std::endl
            << tf_cam.position.x << " " << tf_cam.position.y << " "
            << tf_cam.position.z << std::endl
            << RAD2DEG * eul.z << " " << RAD2DEG * eul.y << " "
            << RAD2DEG * eul.x << std::endl;
}

void trackir_solver_t::clear_camcalib()
{
  std::lock_guard<std::mutex> lock(mtx);
  tf_cam.position = pos_t();
  tf_cam.orientation = zyx_euler_t();
  zyx_euler_inv2mat(tf_cam.orientation.to_euler(), cam_comp_mat);
}

trackir_drawer_t::trackir_drawer_t(const sensorplugin_cfg_t& cfg)
    : trackir_solver_t(cfg), camview(true), b_calib("re-calibrate"),
      b_calibsave("save calibration"), b_calibload("load calibration"),
      b_calibreset("reset calibration"), b_calibuse("use calibration"),
      b_drawraw("camera view")
{
  GET_ATTRIBUTE_BOOL_(camview);
  remove();
  vbox.pack_start(b_calib, Gtk::PACK_EXPAND_WIDGET);
  vbox.pack_start(b_calibuse, Gtk::PACK_EXPAND_WIDGET);
  vbox.pack_start(b_calibsave, Gtk::PACK_EXPAND_WIDGET);
  vbox.pack_start(b_calibload, Gtk::PACK_EXPAND_WIDGET);
  vbox.pack_start(b_calibreset, Gtk::PACK_EXPAND_WIDGET);
  vbox.pack_start(b_drawraw, Gtk::PACK_EXPAND_WIDGET);
  hbox.pack_start(vbox, Gtk::PACK_SHRINK);
  hbox.pack_start(da, Gtk::PACK_EXPAND_WIDGET);
  b_calib.signal_clicked().connect(
      sigc::mem_fun(*this, &trackir_drawer_t::on_click_calib));
  b_calibsave.signal_clicked().connect(
      sigc::mem_fun(*this, &trackir_drawer_t::on_click_calib_save));
  b_calibload.signal_clicked().connect(
      sigc::mem_fun(*this, &trackir_drawer_t::on_click_calib_load));
  b_calibreset.signal_clicked().connect(
      sigc::mem_fun(*this, &trackir_drawer_t::on_click_calib_reset));
  b_calibuse.signal_clicked().connect(
      sigc::mem_fun(*this, &trackir_drawer_t::on_click_calib_use));
  b_drawraw.signal_clicked().connect(
      sigc::mem_fun(*this, &trackir_drawer_t::on_click_drawraw));
  b_drawraw.set_active(camview);
  b_calibuse.set_active(use_calib);
  hbox.show_all();
}

void trackir_drawer_t::on_click_calib()
{
  try {
    current2camcalib();
  }
  catch(const std::exception& e) {
    add_warning(e.what());
  }
}

void trackir_drawer_t::on_click_calib_reset()
{
  try {
    clear_camcalib();
  }
  catch(const std::exception& e) {
    add_warning(e.what());
  }
}

void trackir_drawer_t::on_click_calib_save()
{
  try {
    save_camcalib();
  }
  catch(const std::exception& e) {
    add_warning(e.what());
  }
}

void trackir_drawer_t::on_click_calib_load()
{
  try {
    load_camcalib();
  }
  catch(const std::exception& e) {
    add_warning(e.what());
  }
}

void trackir_drawer_t::on_click_drawraw()
{
  camview = b_drawraw.get_active();
}

void trackir_drawer_t::on_click_calib_use()
{
  use_calib = b_calibuse.get_active();
}

void c6dof_to_vec(std::vector<double>& self, const TASCAR::c6dof_t& tf)
{
  self.resize(6);
  self[0] = tf.position.x;
  self[1] = tf.position.y;
  self[2] = tf.position.z;
  auto eul = tf.orientation.to_euler();
  self[3] = eul.z;
  self[4] = eul.y;
  self[5] = eul.x;
}

void trackir_solver_t::solve()
{
  plot.trycopydataraw(this);
  imagePoints.resize(vMap.size());
  objectPoints.resize(vMap.size());
  crownIndex.resize(vMap.size());
  for(uint32_t k = 0; k < vMap.size(); ++k) {
    imagePoints[k] =
        cv::Point2f(std::real(vP[vMap[k].k1]), std::imag(vP[vMap[k].k1]));
    objectPoints[k] = crown3D[vMap[k].k0];
    crownIndex[k] = vMap[k].k0;
  }
  transform.resize(6);
  for(uint32_t k = 0; k < transform.size(); ++k)
    transform[k] = HUGE_VAL;
  if(vMap.size() < 4)
    return;
  try {
    // find a crown transformation:
    bool res(cv::solvePnP(objectPoints, imagePoints, cameraMatrix, distCoeffs,
                          rvec, tvec));
    // bool res(false);
    if(res) {
      // re-project full crown for visualization:
      cv::projectPoints(crown3D, rvec, tvec, cameraMatrix, distCoeffs, crown2D);
      // correct axis orientation to TASCAR conventions:
      rvec.at<double>(0, 1) *= -1.0;
      rvec.at<double>(0, 0) *= -1.0;
      // rvec.at<double>(0,2) *= -1.0;
      // tvec.at<double>(0,1) *= -1.0;
      tvec.at<double>(0, 2) *= -1.0;
      // convert to Euler angles:
      cv::Mat rot_mat;
      cv::Rodrigues(rvec, rot_mat);
      // store current transformation in camera coordinate system:
      tf_current.position.x = tvec.at<double>(0, 0);
      tf_current.position.y = tvec.at<double>(0, 1);
      tf_current.position.z = tvec.at<double>(0, 2);
      if(use_presolvepos)
        tf_current.position = tf_presolve.position;
      tf_current.orientation = rotmat2euler(rot_mat);
      // now compensate camera :
      // first, compensate camera orientation:
      if(use_calib) {
        rot_mat = rot_mat * cam_comp_mat;
        tf_global = tf_current;
        tf_global.orientation = rotmat2euler(rot_mat);
        // now compensate translation:
        tf_global.position -= tf_cam.position;
        tf_global.position *= tf_cam.orientation;
        // calibration of pre-solved estimation:
        cv::Mat presolve_rotmat;
        zyx_euler2mat(tf_presolve.orientation.to_euler(), presolve_rotmat);
        presolve_rotmat = presolve_rotmat * cam_comp_mat;
        tf_presolve.orientation = rotmat2euler(presolve_rotmat);
        tf_presolve.position -= tf_cam.position;
        tf_presolve.position *= tf_cam.orientation;
      } else {
        tf_global = tf_current;
      }
      // store in vector for sending:
      c6dof_to_vec(transform1, tf_presolve);
      c6dof_to_vec(transform, tf_global);
      // check for discontinuities:
      TASCAR::pos_t arot(0.1, 0, 0);
      arot *= tf_current.orientation;
      if(solved) {
        if(TASCAR::distance(tf_current.position, prevpos) > maxframedist)
          add_warning("Jump of crown position detected.");
        if(TASCAR::distance(arot, prevrot) > maxframedist)
          add_warning("Jump of crown orientation detected.");
      }
      prevpos = tf_current.position;
      prevrot = arot;
      plot.trycopydatasolved(this);
      // end.
      solved = true;
    } else
      solved = false;
  }
  catch(const std::exception& e) {
    add_critical(e.what());
  }
}

trackir_tracker_t::~trackir_tracker_t()
{
  run_service = false;
  srv.join();
}

void trackir_tracker_t::service()
{
  int err(-1);
  while((err < 0) && run_service) {
    err = linuxtrack_init(NULL);
    if(err < 0)
      sleep(1);
  }
  err = -1;
  while((err != 2) && run_service) {
    err = linuxtrack_get_tracking_state();
    if(err != 2)
      sleep(1);
  }
  while(run_service) {
    get_and_process_ltr_markers();
  }
  linuxtrack_shutdown();
}

double pos2x(int32_t ax, const std::string& axes, const pos_t& pos)
{
  char c(axes[2 * ax]);
  return 2.0 * ax - 2.0 + (c == 'x') * pos.x + (c == 'y') * pos.y +
         (c == 'z') * pos.z;
}

double pos2y(int32_t ax, const std::string& axes, const pos_t& pos)
{
  char c(axes[2 * ax + 1]);
  return -((c == 'x') * pos.x + (c == 'y') * pos.y + (c == 'z') * pos.z);
}

void trackir_drawer_t::draw(const Cairo::RefPtr<Cairo::Context>& cr,
                            double width, double height)
{
  std::lock_guard<std::mutex> lock(plot.mtx);
  if(camview) {
    // plot camera view:
    cr->translate(0.5 * width, 0.5 * height);
    double scale(std::max(480.0 / height, 640.0 / width));
    cr->scale(1.0 / scale, 1.0 / scale);
    cr->rectangle(-320, -240, 640, 480);
    cr->stroke();
    // draw warning box:
    cr->save();
    std::vector<double> dash(2);
    dash[0] = 15;
    dash[1] = 15;
    cr->set_dash(dash, 0);
    cr->set_line_width(5.0);
    cr->set_source_rgba(1, 0.5, 0, 0.5);
    cr->rectangle(-320 + margin, -240 + margin, 640 - 2 * margin,
                  480 - 2 * margin);
    cr->stroke();
    cr->restore();
    cr->save();
    cr->set_line_width(2.0);
    cr->set_source_rgb(1, 0, 0);
    cr->move_to(0, 0);
    cr->line_to(320, 0);
    cr->stroke();
    cr->set_source_rgb(0, 1, 0);
    cr->move_to(0, 0);
    cr->line_to(0, -240);
    cr->stroke();
    cr->restore();
    // first draw all detected markers:
    if(plot.ageraw < PLOT_AGELIMIT) {
      cr->set_source_rgba(0, 0, 0, 0.2);
      for(std::vector<std::complex<double>>::const_iterator ip =
              plot.vP.begin();
          ip != plot.vP.end(); ++ip) {
        cr->arc(std::real(*ip), -std::imag(*ip), 8, 0, TASCAR_2PI);
        cr->fill();
      }
      // now draw all detected lines:
      cr->set_line_width(7);
      cr->set_source_rgba(0.4, 0, 0.4, 0.4);
      for(uint32_t k0 = 0; k0 < plot.cIdx.size(); ++k0) {
        if(plot.vP.size() > k0) {
          std::complex<double> p0(plot.vP[k0]);
          if(plot.cIdx[k0].size()) {
            for(uint32_t k1 = 0; k1 < plot.cIdx[k0].size(); ++k1) {
              std::complex<double> p1(plot.vP[plot.cIdx[k0][k1]]);
              cr->move_to(std::real(p0), -std::imag(p0));
              cr->line_to(std::real(p1), -std::imag(p1));
              cr->stroke();
            }
          }
        }
      }
    }
    if(plot.agesolved < PLOT_AGELIMIT) {
      cr->set_line_width(5);
      // highlight all markers, which were used for solving:
      cr->set_source_rgba(0, 0.8, 0, 0.5);
      for(uint32_t k = 0; k < plot.imagePoints.size(); ++k) {
        cr->arc(plot.imagePoints[k].x, -plot.imagePoints[k].y, 12, 0,
                TASCAR_2PI);
        cr->stroke();
      }
      cr->set_source_rgb(0, 0, 0);
      cr->set_font_size(20);
      for(uint32_t k = 0;
          k < std::min(plot.imagePoints.size(), plot.crownIndex.size()); ++k) {
        cr->move_to(plot.imagePoints[k].x + 10, -plot.imagePoints[k].y + 10);
        char ctmp[32];
        ctmp[31] = 0;
        snprintf(ctmp, 31, "%d", plot.crownIndex[k] + 1);
        cr->show_text(ctmp);
        cr->stroke();
      }
      // if a solution was found, show solution:
      // if( solved ){
      cr->set_line_width(3);
      cr->set_source_rgb(1, 0, 0);
      if(plot.crown2D.size() > 3) {
        cr->move_to(plot.crown2D[3].x, -plot.crown2D[3].y);
        cr->line_to(plot.crown2D[0].x, -plot.crown2D[0].y);
        cr->stroke();
        cr->set_line_width(2);
        cr->set_source_rgb(0, 0, 1);
        cr->move_to(plot.crown2D[3].x, -plot.crown2D[3].y);
        cr->line_to(plot.crown2D[1].x, -plot.crown2D[1].y);
        cr->stroke();
        cr->move_to(plot.crown2D[3].x, -plot.crown2D[3].y);
        cr->line_to(plot.crown2D[2].x, -plot.crown2D[2].y);
        cr->stroke();
        //}
      }
    }
  } else {
    // solved view view:
    cr->translate(0.5 * width, 0.5 * height);
    double scale(std::max(2.0 / height, 6.0 / width));
    cr->scale(1.0 / scale, 1.0 / scale);
    cr->set_line_width(0.02);
    cr->set_source_rgb(0, 0, 0);
    cr->rectangle(-1, -1, 2, 2);
    cr->rectangle(-3, -1, 2, 2);
    cr->rectangle(1, -1, 2, 2);
    cr->stroke();
    cr->set_font_size(0.2);
    if(plot.agesolved < PLOT_AGELIMIT) {
      pos_t dirx(0.5, 0, 0);
      dirx *= plot.tf_global.orientation;
      dirx += plot.tf_global.position;
      pos_t diry(0, 0.5, 0);
      diry *= plot.tf_global.orientation;
      diry += plot.tf_global.position;
      pos_t dirz(0, 0, 0.5);
      dirz *= plot.tf_global.orientation;
      dirz += plot.tf_global.position;
      std::vector<pos_t> pcam;
      pcam.push_back(pos_t(0.12, 0.09, -0.2));
      pcam.push_back(pos_t(0, 0.15, -0.2));
      pcam.push_back(pos_t(-0.12, 0.09, -0.2));
      pcam.push_back(pos_t(-0.12, -0.09, -0.2));
      pcam.push_back(pos_t(0.12, -0.09, -0.2));
      pos_t campos(plot.tf_cam.position);
      campos *= -1.0;
      for(std::vector<pos_t>::iterator it = pcam.begin(); it != pcam.end();
          ++it) {
        *it /= plot.tf_cam.orientation;
        *it -= plot.tf_cam.position;
      }
      std::string axes("xyyzzx");
      for(int32_t kax = 0; kax < 3; ++kax) {
        double x0(pos2x(kax, axes, plot.tf_global.position));
        double y0(pos2y(kax, axes, plot.tf_global.position));
        cr->set_source_rgb(0, 0, 0);
        cr->set_line_width(0.02);
        cr->move_to(-2.9 + 2 * kax, 0.9);
        cr->show_text(axes.substr(2 * kax, 2));
        cr->stroke();
        cr->set_source_rgba((axes[2 * kax] == 'x'), (axes[2 * kax] == 'y'),
                            (axes[2 * kax] == 'z'), 0.5);
        cr->move_to(-2 + 2 * kax, 0);
        cr->line_to(-1 + 2 * kax, 0);
        cr->stroke();
        cr->set_source_rgba((axes[2 * kax + 1] == 'x'),
                            (axes[2 * kax + 1] == 'y'),
                            (axes[2 * kax + 1] == 'z'), 0.5);
        cr->move_to(-2 + 2 * kax, 0);
        cr->line_to(-2 + 2 * kax, -1);
        cr->stroke();
        if(use_calib) {
          // draw camera:
          cr->set_source_rgb(0, 0, 0);
          cr->set_line_width(0.015);
          double xcam(pos2x(kax, axes, campos));
          double ycam(pos2y(kax, axes, campos));
          cr->move_to(xcam, ycam);
          cr->line_to(pos2x(kax, axes, pcam[0]), pos2y(kax, axes, pcam[0]));
          cr->line_to(pos2x(kax, axes, pcam[2]), pos2y(kax, axes, pcam[2]));
          cr->line_to(pos2x(kax, axes, pcam[3]), pos2y(kax, axes, pcam[3]));
          cr->line_to(pos2x(kax, axes, pcam[4]), pos2y(kax, axes, pcam[4]));
          cr->line_to(pos2x(kax, axes, pcam[0]), pos2y(kax, axes, pcam[0]));
          cr->move_to(xcam, ycam);
          cr->line_to(pos2x(kax, axes, pcam[2]), pos2y(kax, axes, pcam[2]));
          cr->move_to(xcam, ycam);
          cr->line_to(pos2x(kax, axes, pcam[3]), pos2y(kax, axes, pcam[3]));
          cr->move_to(xcam, ycam);
          cr->line_to(pos2x(kax, axes, pcam[4]), pos2y(kax, axes, pcam[4]));
          cr->stroke();
          cr->move_to(pos2x(kax, axes, pcam[0]), pos2y(kax, axes, pcam[0]));
          cr->line_to(pos2x(kax, axes, pcam[1]), pos2y(kax, axes, pcam[1]));
          cr->line_to(pos2x(kax, axes, pcam[2]), pos2y(kax, axes, pcam[2]));
          cr->line_to(pos2x(kax, axes, pcam[0]), pos2y(kax, axes, pcam[0]));
          cr->fill();
          cr->move_to(pos2x(kax, axes, pcam[0]), pos2y(kax, axes, pcam[0]));
          cr->line_to(pos2x(kax, axes, pcam[1]), pos2y(kax, axes, pcam[1]));
          cr->line_to(pos2x(kax, axes, pcam[2]), pos2y(kax, axes, pcam[2]));
          cr->line_to(pos2x(kax, axes, pcam[0]), pos2y(kax, axes, pcam[0]));
          cr->stroke();
        }
        cr->set_line_width(0.03);
        cr->set_source_rgb(1, 0, 0);
        cr->move_to(x0, y0);
        cr->line_to(pos2x(kax, axes, dirx), pos2y(kax, axes, dirx));
        cr->stroke();
        cr->set_source_rgb(0, 1, 0);
        cr->move_to(x0, y0);
        cr->line_to(pos2x(kax, axes, diry), pos2y(kax, axes, diry));
        cr->stroke();
        cr->set_source_rgb(0, 0, 1);
        cr->move_to(x0, y0);
        cr->line_to(pos2x(kax, axes, dirz), pos2y(kax, axes, dirz));
        cr->stroke();
      }
    }
  }
  if(plot.ageraw < PLOT_AGELIMIT)
    plot.ageraw++;
  if(plot.agesolved < PLOT_AGELIMIT)
    plot.agesolved++;
}

REGISTER_SENSORPLUGIN(trackir_drawer_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
