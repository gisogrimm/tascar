#include "audiochunks.h"
#include <string.h>
#include <algorithm>
#include "errorhandling.h"
#include "xmlconfig.h"
#include "amb33defs.h"
#include <samplerate.h>

#include "windows_port.h"

using namespace TASCAR;

wave_t::wave_t(uint32_t chunksize)
  : d(new float[std::max(1u,chunksize)]),
    n(chunksize), own_pointer(true),
    append_pos(0),
    rmsscale(1.0f)
{
  memset(d,0,sizeof(float)*std::max(1u,n));
  rmsscale = 1.0f/(float)n;
}


wave_t::wave_t(uint32_t chunksize,float* ptr)
  : d(ptr), n(chunksize), own_pointer(false), append_pos(0),
    rmsscale(1.0f)
{
  rmsscale = 1.0f/(float)n;
}

wave_t::wave_t(const std::vector<float>& src)
  : d(new float[std::max(1lu,(long unsigned int)(src.size()))]),
    n(src.size()), own_pointer(true), append_pos(0),
    rmsscale(1.0f/(float)n)
{
  memset(d,0,sizeof(float)*std::max(1lu,(long unsigned int)(src.size())));
  for(uint32_t k=0;k<src.size();++k)
    d[k] = src[k];
}

wave_t::wave_t(const std::vector<double>& src)
  : d(new float[std::max(1lu,(long unsigned int)(src.size()))]),
    n(src.size()), own_pointer(true), append_pos(0),
    rmsscale(1.0f/(float)n)
{
  memset(d,0,sizeof(float)*std::max(1lu,(long unsigned int)(src.size())));
  for(uint32_t k=0;k<src.size();++k)
    d[k] = src[k];
}

wave_t::wave_t(const wave_t& src)
  : d(new float[std::max(1u,src.n)]),
    n(src.n), own_pointer(true), append_pos(src.append_pos),
    rmsscale(1.0f)
{
  memset(d,0,sizeof(float)*std::max(1u,src.n));
  for(uint32_t k=0;k<n;++k)
    d[k] = src.d[k];
  rmsscale = 1.0f/(float)n;
}

wave_t::~wave_t()
{
  if( own_pointer )
    delete [] d;
}

void wave_t::use_external_buffer(uint32_t chunksize,float* ptr)
{
  if( chunksize != n )
    throw TASCAR::ErrMsg("Programming error: Invalid size of new buffer");
  if( own_pointer )
    delete [] d;
  d = ptr;
  own_pointer = false;
}

void wave_t::clear()
{
  memset(d,0,sizeof(float)*n);
}

uint32_t wave_t::copy(float* data,uint32_t cnt,float gain)
{
  uint32_t n_min(std::min(n,cnt));
  for( uint32_t k=0;k<n_min;++k)
    d[k] = data[k]*gain;
  if( n_min < n )
    memset(&(d[n_min]),0,sizeof(float)*(n-n_min));
  return n_min;
}

uint32_t wave_t::copy_stride(float* data,uint32_t cnt,uint32_t stride, float gain)
{
  uint32_t n_min(std::min(n,cnt));
  for( uint32_t k=0;k<n_min;++k){
    d[k] = *data*gain;
    data += stride;
  }
  if( n_min < n )
    memset(&(d[n_min]),0,sizeof(float)*(n-n_min));
  return n_min;
}

void wave_t::operator*=(double v)
{
  for( uint32_t k=0;k<n;++k)
    d[k] *= v;
}

void wave_t::operator*=(float v)
{
  for( uint32_t k=0;k<n;++k)
    d[k] *= v;
}

uint32_t wave_t::copy_to(float* data,uint32_t cnt,float gain)
{
  uint32_t n_min(std::min(n,cnt));
  for( uint32_t k=0;k<n_min;++k)
    data[k] = d[k]*gain;
  //memcpy(data,d,n_min*sizeof(float));
  if( n_min < cnt )
    memset(&(data[n_min]),0,sizeof(float)*(cnt-n_min));
  return n_min;
}

uint32_t wave_t::copy_to_stride(float* data,uint32_t cnt,uint32_t stride,float gain)
{
  uint32_t n_min(std::min(n,cnt));
  for( uint32_t k=0;k<n_min;++k){
    *data = d[k]*gain;
    data += stride;
  }
  for( uint32_t k=n_min; k < cnt; ++k){
    *data = 0;
    data += stride;
  }
  return n_min;
}

void wave_t::operator+=(const wave_t& o)
{
  for(uint32_t k=0;k<std::min(size(),o.size());++k)
    d[k]+=o[k];
}

/**
   \brief root-mean-square
*/
float wave_t::rms() const
{
  float rv(0.0f);
  for(uint32_t k=0;k<size();++k)
    rv += d[k]*d[k];
  rv *= rmsscale;
  return sqrt(rv);
}

/**
   \brief mean-square
*/
float wave_t::ms() const
{
  float rv(0.0f);
  for(uint32_t k=0;k<size();++k)
    rv += d[k]*d[k];
  rv *= rmsscale;
  return rv;
}

float wave_t::maxabs() const
{
  float rv(0.0f);
  for(uint32_t k=0;k<size();++k)
    rv = std::max(rv,fabsf(d[k]));
  return rv;
}

float wave_t::spldb() const
{
  return 10.0*log10(ms())-SPLREF;
}

float wave_t::maxabsdb() const
{
  return 20.0*log10(maxabs())-SPLREF;
}

amb1wave_t::amb1wave_t(uint32_t chunksize)
  : wxyz(4,wave_t(chunksize)),
    w_(chunksize,wxyz[0].d),
    x_(chunksize,wxyz[1].d),
    y_(chunksize,wxyz[2].d),
    z_(chunksize,wxyz[3].d)
{
}

void amb1wave_t::print_levels()
{
  std::cout << this << " wxyz" << 
    " " << w_.spldb() <<
    " " << y_.spldb() <<
    " " << z_.spldb() <<
    " " << x_.spldb() << std::endl;
}

wave_t& amb1wave_t::operator[](uint32_t acn)
{
  switch( acn ){
  case AMB11::idx::w :
    return w_;
  case AMB11::idx::y :
    return y_;
  case AMB11::idx::z :
    return z_;
  case AMB11::idx::x :
    return x_;
  }
  throw TASCAR::ErrMsg( "Invalid acn "+TASCAR::to_string(acn)+" for first order ambisonics.");
}

void amb1wave_t::clear()
{
  w_.clear();
  x_.clear();
  y_.clear();
  z_.clear();
}

void amb1wave_t::operator+=(const amb1wave_t& v)
{
  w_+=v.w();
  x_+=v.x();
  y_+=v.y();
  z_+=v.z();
}

void amb1wave_t::operator*=(double v)
{
  w_*=v;
  x_*=v;
  y_*=v;
  z_*=v;
}

void amb1wave_t::add_panned( pos_t p, const wave_t& v, float g )
{
  p.normalize();
  w_.add( v, g*MIN3DB );
  x_.add( v, g*p.x );
  y_.add( v, g*p.y );
  z_.add( v, g*p.z );
}

SF_INFO sndfile_handle_t::sf_info_configurator(int samplerate,int channels)
{
  SF_INFO inf;
  memset(&inf,0,sizeof(inf));
  inf.samplerate = samplerate;
  inf.channels = channels;
  inf.format = SF_FORMAT_WAV|SF_FORMAT_FLOAT;
  return inf;
}


sndfile_handle_t::sndfile_handle_t(const std::string& fname,int samplerate,int channels)
  : sf_inf(sf_info_configurator(samplerate,channels)),
    sfile(sf_open(TASCAR::env_expand(fname).c_str(),SFM_WRITE,&sf_inf))
{
  if( !sfile )
    throw TASCAR::ErrMsg("Unable to open sound file \""+fname+"\" for writing ("+TASCAR::to_string(samplerate)+" Hz, "+TASCAR::to_string(channels)+" channels).");
}

sndfile_handle_t::sndfile_handle_t(const std::string& fname)
  : sf_inf(sf_info_configurator(1,1)),
    sfile(sf_open(TASCAR::env_expand(fname).c_str(),SFM_READ,&sf_inf))
{
  if( !sfile )
    throw TASCAR::ErrMsg("Unable to open sound file \""+fname+"\" for reading.");
}

sndfile_handle_t::~sndfile_handle_t()
{
  sf_close(sfile);
}

uint32_t sndfile_handle_t::readf_float( float* buf, uint32_t frames )
{
  return sf_readf_float( sfile, buf, frames );
}

uint32_t sndfile_handle_t::writef_float( float* buf, uint32_t frames )
{
  return sf_writef_float( sfile, buf, frames );
}

uint64_t get_chunklen(uint64_t getframes,uint64_t start,uint64_t length)
{
  if( length > 0 )
    return length;
  return std::max(getframes,start) - start;
}

looped_wave_t::looped_wave_t(uint32_t length)
  : wave_t(length),
    looped_t(0),
    looped_gain(0),
    iposition_(0),
    loop_(0)
{
}

sndfile_t::sndfile_t(const std::string& fname,uint32_t channel,double start,double length)
  : sndfile_handle_t(fname),
    looped_wave_t(get_chunklen(get_frames(),get_srate()*start,get_srate()*length))
{
  uint32_t ch(get_channels());
  int64_t istart(start*get_srate());
  int64_t ilength(length*get_srate());
  // if requested channel is not available return zeros, otherwise:
  if( channel < ch ){
    // if no samples remain then just return zeros, otherwise:
    if( istart < get_frames() ){
      if( ilength == 0 )
        ilength = get_frames()-istart;
      // trim "start" samples:
      if( istart > 0 ){
        wave_t chbuf_skip(istart*ch);
        readf_float(chbuf_skip.d,istart);
      }
      // this is the minimum of available and requested number of samples:
      uint32_t N(std::min(get_frames() - istart,ilength));
      // temporary data to read all channels:
      wave_t chbuf(N*ch);
      readf_float(chbuf.d,N);
      // select requested audio channel:
      for(uint32_t k=0;k<N;++k)
        d[k] = chbuf[k*ch+channel];
    }
  }
}

void sndfile_t::set_position(double position)
{
  set_iposition(get_srate()*position);
}

void looped_wave_t::set_iposition(int64_t position)
{
  iposition_ = position;
}


void looped_wave_t::set_loop(uint32_t loop)
{
  loop_ = loop;
}

void looped_wave_t::add_to_chunk(int64_t chunktime,wave_t& chunk)
{
  for(uint32_t k=0;k<chunk.n;++k){
    int64_t trel(chunktime+k-iposition_);
    if( (trel > 0) && (n > 0) ){
      ldiv_t dv(ldiv(trel,n));
      if( (loop_ == 0) || (dv.quot < (int64_t)loop_) )
        chunk.d[k] += d[dv.rem];
    }
  }
}

void looped_wave_t::add_chunk(int32_t chunk_time, int32_t start_time,float gain,wave_t& chunk)
{
  for(int32_t k=std::max(start_time,chunk_time);k < std::min(start_time+(int32_t)(size()),chunk_time+(int32_t)(chunk.size()));++k)
    chunk[k-chunk_time] += gain*d[k-start_time];
}

void looped_wave_t::add_chunk_looped(float gain,wave_t& chunk)
{
  float dg((gain-looped_gain)/chunk.n);
  float* pdN(chunk.d+chunk.n);
  for(float* pd=chunk.d;pd<pdN;++pd){
    *pd += (looped_gain += dg)*d[looped_t];
    ++looped_t;
    if( looped_t >= n )
      looped_t = 0;
  }
}

void wave_t::copy(const wave_t& src,float gain)
{
  memmove(d,src.d,std::min(n,src.n)*sizeof(float));
  if( gain != 1.0f )
    operator*=(gain);
}

void wave_t::add(const wave_t& src,float gain)
{
  uint32_t N(std::min(n,src.n));
  for( uint32_t k=0;k<N;++k)
    d[k] += gain*src.d[k];
}

void wave_t::operator*=(const wave_t& o)
{
  for(unsigned int k=0;k<std::min(o.n,n);++k){
    d[k] *= o.d[k];
  }
}

void wave_t::append(const wave_t& src)
{
  if((src.n == 0) || (n == 0))
    return;
  if(src.n < n) {
    // copy from append_pos to end:
    uint32_t n1(std::min(n - append_pos, src.n));
    memmove(&(d[append_pos]), src.d, n1 * sizeof(float));
    // if remainder then copy rest to beginning:
    if(n1 < src.n)
      memmove(d, &(src.d[n1]), (src.n - n1) * sizeof(float));
    append_pos = (append_pos + src.n) % n;
  } else {
    memmove(d, &(src.d[src.n - n]), n * sizeof(float));
    append_pos = 0;
  }
}

void wave_t::resample(double ratio)
{
  if(ratio == 1)
    return;
  uint32_t n_new(ratio * n);
  float* d_new(new float[std::max(1u, n_new)]);
  memset(d_new, 0, sizeof(float) * std::max(1u, n_new));
  // do the actual resampling:
  SRC_DATA srcd;
  srcd.data_in = d;
  srcd.data_out = d_new;
  srcd.input_frames = n;
  srcd.output_frames = n_new;
  srcd.src_ratio = ratio;
  src_simple(&srcd, SRC_SINC_MEDIUM_QUALITY, 1);
  //
  // clean-up and re-assign:
  if(own_pointer)
    delete[] d;
  d = d_new;
  n = n_new;
  own_pointer = true;
  rmsscale = 1.0f / (float)n;
}

amb1rotator_t::amb1rotator_t(uint32_t chunksize)
  : amb1wave_t(chunksize),
    wxx(1), wxy(0), wxz(0), wyx(0), wyy(1), wyz(0), wzx(0), wzy(0), wzz(1), 
    dt(1.0/(double)chunksize)
{
}

amb1rotator_t& amb1rotator_t::rotate(const amb1wave_t& src,const zyx_euler_t& o,bool invert)
{
  float dxx;
  float dxy;
  float dxz;
  float dyx;
  float dyy;
  float dyz;
  float dzx;
  float dzy;
  float dzz;
  if( invert ){
    double cosy(cos(o.y));
    double siny(sin(-o.y));
    double cosz(cos(o.z));
    double sinz(sin(-o.z));
    double sinx(sin(-o.x));
    double cosx(cos(o.x));
    double sinxsiny(sinx*siny);
    double cosxsiny(cosx*siny);
    dxx = (cosz*cosy - wxx)*dt;
    dxy = (sinz*cosy - wxy)*dt;
    dxz = (siny - wxz)*dt;
    dyx = (-cosz*sinxsiny-sinz*cosx - wyx)*dt;
    dyy = (cosz*cosx-sinz*sinxsiny - wyy)*dt;
    dyz = (cosy*sinx - wyz)*dt;
    dzx = (-cosz*cosxsiny+sinz*sinx - wzx)*dt;
    dzy = (-cosz*sinx-sinz*cosxsiny - wzy)*dt;
    dzz = (cosy*cosx - wzz)*dt;
  }else{
    // 1, 0, 0
    // rot_z: cosz, sinz, 0
    // rot_y: cosy*cosz, sinz, siny*cosz
    // rot_x: cosy*cosz, cosx*sinz-sinx*siny*cosz, cosx*siny*cosz+sinx*sinz

    // 0, 1, 0
    // rot_z: -sinz, cosz, 0
    // rot_y: -cosy*sinz, cosz, -siny*sinz
    // rot_x: -cosy*sinz, cosx*cosz+sinx*siny*sinz, -cosx*siny*sinz + sinx*cosz

    // 0, 0, 1
    // rot_z: 0, 0, 1
    // rot_y: -siny, 0, cosy
    // rot_x: -siny, -sinx*cosy, cosx*cosy
    double cosy(cos(o.y));
    double cosz(cos(o.z));
    double cosx(cos(o.x));
    double sinz(sin(o.z));
    double siny(sin(o.y));
    double sinx(sin(o.x));
    double sinxsiny(sinx*siny);
    dxx = (cosy*cosz - wxx)*dt;
    dxy = (cosx*sinz-sinxsiny*cosz - wxy)*dt;
    dxz = (cosx*siny*cosz+sinx*sinz - wxz)*dt;
    dyx = (-cosy*sinz - wyx)*dt;
    dyy = (cosx*cosz+sinxsiny*sinz - wyy)*dt;
    dyz = (-cosx*siny*sinz+sinx*cosz - wyz)*dt;
    dzx = (-siny - wzx)*dt;
    dzy = (-sinx*cosy - wzy)*dt;
    dzz = (cosx*cosy - wzz)*dt;
  }
  w_.copy(src.w());
  float *p_src_x(src.x().d);
  float *p_src_y(src.y().d);
  float *p_src_z(src.z().d);
  float *p_x(x_.d);
  float *p_y(y_.d);
  float *p_z(z_.d);
  for(uint32_t k=0;k<w_.n;++k){
    *p_x = *p_src_x*(wxx+=dxx) + *p_src_y*(wxy+=dxy) + *p_src_z*(wxz+=dxz);
    *p_y = *p_src_x*(wyx+=dyx) + *p_src_y*(wyy+=dyy) + *p_src_z*(wyz+=dyz);
    *p_z = *p_src_x*(wzx+=dzx) + *p_src_y*(wzy+=dzy) + *p_src_z*(wzz+=dzz);
    ++p_x;
    ++p_y;
    ++p_z;
    ++p_src_x;
    ++p_src_y;
    ++p_src_z;
  }
  return *this;
}

void sndfile_t::resample(double ratio)
{
  wave_t::resample(ratio);
  sf_inf.frames*=ratio;
  sf_inf.samplerate*=ratio;
}


/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
