#include "levelmeter.h"
#include <algorithm>

#include "windows_port.h"

TASCAR::levelmeter_t::levelmeter_t(float fs, float tc, levelmeter::weight_t weight)
  : wave_t(fs*tc),
    w(weight),
    segment_length(fs*0.125),
    segment_shift(0.5*segment_length),
    num_segments(n/segment_shift-1),    
    i30(0.3*num_segments),
    i50(0.5*num_segments),
    i65(0.65*num_segments),
    i95(0.95*num_segments),
    i99(0.99*num_segments),
    bp(500.0,4000.0,fs),
    bp_C(62.5,4000.0,fs),
    flt_A(fs)
{
}

void TASCAR::levelmeter_t::update(const TASCAR::wave_t& src)
{
  switch( w ){
  case TASCAR::levelmeter::Z:
    append(src);
    break;
  case TASCAR::levelmeter::bandpass:
    for(uint32_t k=0;k<src.n;++k)
      append_sample( bp.filter( src.d[k] ) );
    break;
  case TASCAR::levelmeter::C:
    for(uint32_t k=0;k<src.n;++k)
      append_sample( bp_C.filter( src.d[k] ) );
    break;
  case TASCAR::levelmeter::A:
    for(uint32_t k=0;k<src.n;++k)
      append_sample( flt_A.filter( src.d[k] ) );
    break;
  }
}

void TASCAR::levelmeter_t::set_weight(levelmeter::weight_t weight)
{
  w = weight;
}

void TASCAR::levelmeter_t::get_rms_and_peak( float& rms, float& peak ) const
{
  rms = spldb();
  peak = maxabsdb();
}

void TASCAR::levelmeter_t::get_percentile_levels( float& q30, float& q50, float& q65, float& q95, float& q99 ) const
{
  if( num_segments ){
    std::vector<float> stlev(num_segments,0);
    float* dseg(d);
    for(std::vector<float>::iterator it=stlev.begin();it!=stlev.end();++it){
      TASCAR::wave_t wseg(segment_length,dseg);
      *it = std::max(wseg.rms(),1e-10f);
      dseg += segment_shift;
    }
    std::sort(stlev.begin(),stlev.end());
    q30 = 20.0*log10(stlev[i30])-SPLREF;
    q50 = 20.0*log10(stlev[i50])-SPLREF;
    q65 = 20.0*log10(stlev[i65])-SPLREF;
    q95 = 20.0*log10(stlev[i95])-SPLREF;
    q99 = 20.0*log10(stlev[i99])-SPLREF;
  }else{
    q30 = q50 = q65 = q95 = q99 = 0.0f;
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
