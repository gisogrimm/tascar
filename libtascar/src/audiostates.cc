#include "audiostates.h"
#include "errorhandling.h"
#include "defs.h"
#include "xmlconfig.h"

#include "windows_port.h"

chunk_cfg_t::chunk_cfg_t( double f_sample_, uint32_t n_fragment_, uint32_t n_channels_ )
  :  f_sample(f_sample_),
     n_fragment(n_fragment_),
     n_channels(n_channels_)
{
  update();
}

void chunk_cfg_t::update()
{
  f_fragment = f_sample/n_fragment;
  t_sample = 1.0/std::max(EPS,f_sample);
  t_fragment = 1.0/std::max(EPS,f_fragment);
  t_inc = 1.0/std::max(EPS,(double)n_fragment);
  for( uint32_t ch=labels.size(); ch<n_channels; ++ch )
    labels.push_back( "."+TASCAR::to_string(ch) );
  // check for unique channel suffix:
  for( uint32_t ch1=0;ch1<labels.size();++ch1 )
    for( uint32_t ch2=0;ch2<labels.size();++ch2 )
      if( (ch1!=ch2) && (labels[ch1]==labels[ch2]) )
        throw TASCAR::ErrMsg("Identical channel label in channels "+TASCAR::to_string(ch1)
                             + " and "+TASCAR::to_string(ch2)+".");
}

audiostates_t::audiostates_t()
  : is_prepared_(false),
    preparecount(0)
{
}

void audiostates_t::prepare( chunk_cfg_t& cf_ )
{
  preparecount++;
#ifdef TSCDEBUG
  DEBUG(this);
  if( is_prepared_ )
    throw TASCAR::ErrMsg("Already in prepared-state in prepare callback");
#endif
  if( is_prepared_ )
    TASCAR::add_warning("Programming error: Already in prepared-state in prepare callback");
  *(chunk_cfg_t*)this = cf_;
  inputcfg_ = cf_;
  inputcfg_.update();
  configure();
  cf_ = *(chunk_cfg_t*)this;
  update();
  is_prepared_ = true;
}

void audiostates_t::release( )
{
#ifdef TSCDEBUG
  DEBUG(this);
  if( !is_prepared_ ){
    DEBUG(preparecount);
    throw TASCAR::ErrMsg("Release called without prepare");
  }
#endif
  if( !is_prepared_ )
    TASCAR::add_warning("Programming error: Release called without prepare ("+TASCAR::to_string(preparecount)+")");
  is_prepared_ = false;
}

audiostates_t::~audiostates_t()
{
#ifdef TSCDEBUG
  DEBUG(this);
  if( is_prepared_ )
    throw TASCAR::ErrMsg("still in prepared state at end.");
#endif
  if( is_prepared_ )
    TASCAR::add_warning("Programming error: still in prepared state at end.");
}

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */

