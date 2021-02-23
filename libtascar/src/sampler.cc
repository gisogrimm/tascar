#include "sampler.h"
#include <string.h>
#include <fstream>
#include "errorhandling.h"
#include <unistd.h>
#include <math.h>

TASCAR::loop_event_t::loop_event_t()
  : tsample(0),tloop(1),loopgain(1.0)
{
}

TASCAR::loop_event_t::loop_event_t(int32_t cnt,float gain)
  : tsample(0),tloop(cnt),loopgain(gain)
{
}

bool TASCAR::loop_event_t::valid() const 
{ 
  return tsample || tloop;
}

void TASCAR::loop_event_t::process(wave_t& out_chunk,const wave_t& in_chunk)
{
  uint32_t n_(in_chunk.size());
  for( uint32_t k=0;k<out_chunk.size();k++){
    if( tloop == -2 ){
      tsample = 0;
      tloop = 0;
    }
    if( tsample )
      tsample--;
    else{
      if( tloop ){
        if( tloop > 0 )
          tloop--;
        tsample = n_ - 1;
      }
    }
    if( tsample || tloop )
      out_chunk[k] += loopgain*in_chunk[n_-1 - tsample];
  }
}

TASCAR::looped_sample_t::looped_sample_t(const std::string& fname,uint32_t channel)
  : sndfile_t(fname,channel)
{
  loop_event.reserve(1024);
  pthread_mutex_init( &mutex, NULL );
}

TASCAR::looped_sample_t::~looped_sample_t()
{
  pthread_mutex_trylock( &mutex );
  pthread_mutex_unlock(  &mutex );
  pthread_mutex_destroy( &mutex );
}

void TASCAR::looped_sample_t::loop(wave_t& chunk)
{
  pthread_mutex_lock( &mutex );
  uint32_t kle(loop_event.size());
  while(kle){
    kle--;
    if( loop_event[kle].valid() ){
      loop_event[kle].process(chunk,*this);
    }else{
      loop_event.erase(loop_event.begin()+kle);
    }
  }
  pthread_mutex_unlock( &mutex );
}

void TASCAR::looped_sample_t::add(loop_event_t le)
{
  pthread_mutex_lock( &mutex );
  loop_event.push_back(le);
  pthread_mutex_unlock( &mutex );
}

void TASCAR::looped_sample_t::clear()
{
  pthread_mutex_lock( &mutex );
  loop_event.clear();
  pthread_mutex_unlock( &mutex );
}

void TASCAR::looped_sample_t::stop()
{
  pthread_mutex_lock( &mutex );
  for(uint32_t kle=0;kle<loop_event.size();kle++)
    loop_event[kle].tloop = 0;
  pthread_mutex_unlock( &mutex );
}

TASCAR::sampler_t::sampler_t(const std::string& jname,const std::string& srv_addr,const std::string& srv_port)
  : jackc_t(jname),
    osc_server_t(srv_addr,srv_port,"UDP"),
    b_quit(false)
{
  set_prefix("/"+jname);
  add_method("/quit","",sampler_t::osc_quit,this);
}

int TASCAR::sampler_t::osc_addloop(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
  if( (user_data) && (argc == 2) && (types[0]=='i') && (types[1]=='f') ){
    ((looped_sample_t*)user_data)->add(loop_event_t(argv[0]->i,argv[1]->f));
  }
  return 0;
}

int TASCAR::sampler_t::osc_stoploop(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
  if( (user_data) && (argc == 0) ){
    ((looped_sample_t*)user_data)->stop();
  }
  return 0;
}

int TASCAR::sampler_t::osc_clearloop(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
  if( (user_data) && (argc == 0) ){
    ((looped_sample_t*)user_data)->clear();
  }
  return 0;
}

int TASCAR::sampler_t::osc_quit(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
  if( user_data )
    ((sampler_t*)user_data)->quit();
  return 0;
}

TASCAR::sampler_t::~sampler_t()
{
  for( unsigned int k=0;k<sounds.size();k++)
    delete sounds[k];
}

int TASCAR::sampler_t::process(jack_nframes_t n, const std::vector<float*>& sIn, const std::vector<float*>& sOut)
{
  for(uint32_t k=0;k<sOut.size();k++)
    memset(sOut[k],0,n*sizeof(float));
  for(uint32_t k=0;k<sounds.size();k++){
    wave_t wout(n,sOut[k]);
    sounds[k]->loop(wout);
  }
  return 0;
}

void TASCAR::sampler_t::add_sound(const std::string& fname,double gain)
{
  looped_sample_t* sf(new looped_sample_t(fname,0));
  if( gain != 0 ){
    gain = pow(10.0,0.05*gain);
    *sf *= gain;
  }
  sounds.push_back(sf);
  soundnames.push_back(fname);
  uint32_t k(sounds.size()-1);
  char ctmp[1024];
  sprintf(ctmp,"%d",k+1);
  std::string sname(soundnames[k]);
  size_t p(sname.rfind("/"));
  if( p < sname.size() )
    sname.erase(0,p+1);
  p = sname.rfind(".");
  if( p < sname.size() )
    sname.erase(p,sname.size()-p);
  add_output_port(sname);
  add_method("/"+std::string(ctmp)+"/add","if",sampler_t::osc_addloop,sounds[k]);
  add_method("/"+std::string(ctmp)+"/stop","",sampler_t::osc_stoploop,sounds[k]);
  add_method("/"+std::string(ctmp)+"/clear","",sampler_t::osc_clearloop,sounds[k]);
  add_method("/"+sname+"/add","if",sampler_t::osc_addloop,sounds[k]);
  add_method("/"+sname+"/stop","",sampler_t::osc_stoploop,sounds[k]);
  add_method("/"+sname+"/clear","",sampler_t::osc_clearloop,sounds[k]);
}

void TASCAR::sampler_t::open_sounds(const std::string& fname)
{
  std::ifstream fh(fname.c_str());
  if( fh.fail() )
    throw TASCAR::ErrMsg("Unable to open sound font file \""+fname+"\".");
  while(!fh.eof() ){
    char ctmp[1024];
    memset(ctmp,0,1024);
    fh.getline(ctmp,1023);
    std::string fname(ctmp);
    if(fname.size()){
      add_sound(fname);
    }
  }
}

void TASCAR::sampler_t::start()
{
  jackc_t::activate();
  TASCAR::osc_server_t::activate();
}

void TASCAR::sampler_t::stop()
{
  TASCAR::osc_server_t::deactivate();
  jackc_t::deactivate();
}

void TASCAR::sampler_t::run()
{
  start();
  while( !b_quit ){
    usleep(1 * 1000 * 1000);
  }
  stop();
}

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */

