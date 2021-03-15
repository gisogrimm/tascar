#include "jackiowav.h"

#include "windows_port.h"

jackio_t::jackio_t(const std::string& ifname,const std::string& ofname,
		   const std::vector<std::string>& ports, const std::string& jackname,int freewheel,int autoconnect,bool verbose)
  : jackc_transport_t(jackname),
    sf_in(NULL),
    sf_out(NULL),
    buf_in(NULL),
    buf_out(NULL),
    pos(0),
    b_quit(false),
    start(false),
    freewheel_(freewheel),
    use_transport(false),
    startframe(0),
    nframes_total(0),
    p(ports),
    b_cb(false),
    b_verbose(verbose), wait_(false), cpuload(0), xruns(0)
{
  memset(&sf_inf_in,0,sizeof(sf_inf_in));
  memset(&sf_inf_out,0,sizeof(sf_inf_out));
  log("opening input file "+ifname);
  if( !(sf_in = sf_open(ifname.c_str(),SFM_READ,&sf_inf_in)) )
    throw TASCAR::ErrMsg("unable to open input file \""+ifname+"\" for reading.");
  sf_inf_out = sf_inf_in;
  sf_inf_out.samplerate = get_srate();
  sf_inf_out.channels = std::max(0,(int)(p.size()) - (int)(sf_inf_in.channels));
  sf_inf_out.format = SF_FORMAT_WAV | SF_FORMAT_FLOAT | SF_ENDIAN_FILE;
  if( autoconnect ){
    p.clear();
    for(int ch=0;ch<sf_inf_in.channels; ch++){
      char ctmp[100];
      sprintf(ctmp,"system:playback_%d",ch+1);
      p.push_back(ctmp);
    }
    p.push_back("system:capture_1");
  }
  if( ofname.size() ){
    log("creating output file "+ofname);
    if( !(sf_out = sf_open(ofname.c_str(),SFM_WRITE,&sf_inf_out)) ){
      std::string errmsg("Unable to open output file \""+ofname+"\": ");
      errmsg += sf_strerror(NULL);
      throw TASCAR::ErrMsg(errmsg);
    }
  }
  char c_tmp[1024];
  nframes_total = sf_inf_in.frames;
  sprintf(c_tmp,"%d",nframes_total);
  log("allocating memory for "+std::string(c_tmp)+" audio frames");
  buf_in = new float[std::max((sf_count_t)1,sf_inf_in.channels * sf_inf_in.frames)];
  buf_out = new float[std::max((sf_count_t)1,sf_inf_out.channels * sf_inf_in.frames)];
  memset(buf_out,0,sizeof(float)*sf_inf_out.channels * sf_inf_in.frames);
  memset(buf_in,0,sizeof(float)*sf_inf_in.channels * sf_inf_in.frames);
  log("reading input file into memory");
  sf_readf_float(sf_in,buf_in,sf_inf_in.frames);
  for(unsigned int k=0;k<(unsigned int)sf_inf_out.channels;k++){
    sprintf(c_tmp,"in_%d",k+1);
    log("adding input port "+std::string(c_tmp));
    add_input_port(c_tmp);
  }
  for(unsigned int k=0;k<(unsigned int)sf_inf_in.channels;k++){
    sprintf(c_tmp,"out_%d",k+1);
    log("adding output port "+std::string(c_tmp));
    add_output_port(c_tmp);
  }
}

jackio_t::jackio_t(double duration,const std::string& ofname,
		   const std::vector<std::string>& ports, const std::string& jackname,int freewheel,int autoconnect,bool verbose)
  : jackc_transport_t(jackname),
    sf_in(NULL),
    sf_out(NULL),
    buf_in(NULL),
    buf_out(NULL),
    pos(0),
    b_quit(false),
    start(false),
    freewheel_(freewheel),
    use_transport(false),
    startframe(0),
    nframes_total(std::max(1u,uint32_t(get_srate()*duration))),
    p(ports),
    b_cb(false),
    b_verbose(verbose), wait_(false), cpuload(0), xruns(0)
{
  memset(&sf_inf_in,0,sizeof(sf_inf_in));
  memset(&sf_inf_out,0,sizeof(sf_inf_out));
  sf_inf_out.samplerate = get_srate();
  sf_inf_out.channels = std::max(1,(int)(p.size()));
  sf_inf_out.format = SF_FORMAT_WAV | SF_FORMAT_FLOAT | SF_ENDIAN_FILE;
  if( autoconnect ){
    p.clear();
    p.push_back("system:capture_1");
  }
  if( ofname.size() ){
    log("creating output file "+ofname);
    if( !(sf_out = sf_open(ofname.c_str(),SFM_WRITE,&sf_inf_out)) ){
      std::string errmsg("Unable to open output file \""+ofname+"\": ");
      errmsg += sf_strerror(NULL);
      throw TASCAR::ErrMsg(errmsg);
    }
  }
  char c_tmp[1024];
  sprintf(c_tmp,"%d",nframes_total);
  log("allocating memory for "+std::string(c_tmp)+" audio frames");
  buf_out = new float[sf_inf_out.channels * nframes_total];
  memset(buf_out,0,sizeof(float)*sf_inf_out.channels * nframes_total);
  for(unsigned int k=0;k<(unsigned int)sf_inf_out.channels;k++){
    sprintf(c_tmp,"in_%d",k+1);
    log("add input port "+std::string(c_tmp));
    add_input_port(c_tmp);
  }
}

jackio_t::~jackio_t()
{
  log("cleaning up file handles");
  if( sf_in ){
    sf_close(sf_in);
  }
  if( sf_out ){
    sf_writef_float(sf_out,buf_out,nframes_total);
    sf_close(sf_out);
  }
  log("deallocating memory");
  if( buf_in )
    delete [] buf_in;
  if( buf_out )
    delete [] buf_out;
}

int jackio_t::process(jack_nframes_t nframes,const std::vector<float*>& inBuffer,const std::vector<float*>& outBuffer,uint32_t tp_frame, bool tp_rolling)
{
  b_cb = true;
  bool record(start);
  if( use_transport )
    record &= tp_rolling;
  if( wait_ )
    record &= tp_frame >= startframe;
  for(unsigned int k=0;k<nframes;k++){
    if( record && (pos < nframes_total) ){
      if( buf_in )
        for(unsigned int ch=0;ch<outBuffer.size();ch++)
          outBuffer[ch][k] = buf_in[sf_inf_in.channels*pos+ch];
      for(unsigned int ch=0;ch<inBuffer.size();ch++)
        buf_out[sf_inf_out.channels*pos+ch] = inBuffer[ch][k];
      pos++;
    }else{
      if( pos >= nframes_total)
        b_quit = true;
      for(unsigned int ch=0;ch<outBuffer.size();ch++)
        outBuffer[ch][k] = 0;
    }
  }
  return 0;
}

void jackio_t::log(const std::string& msg)
{
  if( b_verbose )
    std::cerr << msg << std::endl;
}

void jackio_t::run()
{
  log("activating jack client");
  activate();
  for(unsigned int k=0;k<(unsigned int)sf_inf_in.channels;k++)
    if( k<p.size()){
      log("connecting output port to "+p[k]);
      connect_out(k,p[k]);
    }
  for(unsigned int k=0;k<(unsigned int)sf_inf_out.channels;k++)
    if( k+sf_inf_in.channels < p.size() ){
      log("connecting input port to "+p[k+sf_inf_in.channels]);
      connect_in(k,p[k+sf_inf_in.channels],false,true);
    }
  if( freewheel_ ){
    log("switching to freewheeling mode");
    jack_set_freewheel( jc, 1 );
  }
  if( use_transport && (!wait_) ){
    log("locating to startframe");
    tp_stop();
    tp_locate(startframe);
  }
  b_cb = false;
  while( !b_cb ){
    usleep( 5000 );
  }
  start = true;
  if( use_transport && (!wait_) ){
    log("starting transport");
    tp_start();
  }
  log("waiting for data to complete");
  while( !b_quit ){
    usleep( 5000 );
  }
  cpuload = get_cpu_load();
  xruns = get_xruns();
  if( use_transport && (!wait_) ){
    log("stopping transport");
    tp_stop();
  }
  if( freewheel_ ){
    log("deactivating freewheeling mode");
    jack_set_freewheel( jc, 0 );
  }
  log("deactivating jack client");
  deactivate();
}

void jackio_t::set_transport_start(double start_, bool wait)
{
  use_transport = true;
  wait_ = wait;
  startframe = get_srate() * start_;
}

jackrec_async_t::jackrec_async_t(const std::string& ofname,
                                 const std::vector<std::string>& ports,
                                 const std::string& jackname, double buflen,
                                 int format)
    : jackc_transport_t(jackname), rectime(0), xrun(0), werror(0), sf_out(NULL),
      rb(NULL), run_service(true), tscale(1), recframes(0),
      channels(ports.size())
{
  if(!channels)
    throw TASCAR::ErrMsg("No sources selected.");
  memset(&sf_inf_out, 0, sizeof(sf_inf_out));
  sf_inf_out.samplerate = get_srate();
  sf_inf_out.channels = ports.size();
  sf_inf_out.format = format;
  if(ofname.size()) {
    if(!(sf_out = sf_open(ofname.c_str(), SFM_WRITE, &sf_inf_out))) {
      std::string errmsg("Unable to open output file \"" + ofname + "\": ");
      errmsg += sf_strerror(NULL);
      throw TASCAR::ErrMsg(errmsg);
    }
  }
  unsigned int k(0);
  char c_tmp[1024];
  for(auto p : ports) {
    ++k;
    sprintf(c_tmp, "in_%d", k);
    add_input_port(c_tmp);
  }
  if(buflen < 2.0)
    buflen = 2.0;
  rb = jack_ringbuffer_create(buflen * get_srate() * channels * sizeof(float));
  buf = new float[channels * get_fragsize()];
  rlen = channels * get_srate();
  rbuf = new float[rlen];
  activate();
  k = 0;
  for(auto p : ports) {
    connect_in(k, p, true, true);
    ++k;
  }
  tscale = 1.0 / get_srate();
  srv = std::thread(&jackrec_async_t::service, this);
}

jackrec_async_t::~jackrec_async_t()
{
  deactivate();
  run_service = false;
  srv.join();
  if(sf_out) {
    sf_close(sf_out);
  }
  if(rb)
    jack_ringbuffer_free(rb);
  delete[] buf;
  delete[] rbuf;
}

void jackrec_async_t::service()
{
  size_t rchunk(rlen * sizeof(float));
  while(run_service) {
    if(jack_ringbuffer_read_space(rb) >= rchunk) {
      size_t rcnt(jack_ringbuffer_read(rb, (char*)rbuf, rchunk));
      rcnt /= sizeof(float) * channels;
      size_t wcnt(sf_writef_float(sf_out, rbuf, rcnt));
      if(wcnt < rcnt)
        ++werror;
    }
    usleep(100);
  }
  size_t rcnt(0);
  do {
    rcnt = jack_ringbuffer_read(rb, (char*)rbuf, rchunk);
    rcnt /= sizeof(float) * channels;
    sf_writef_float(sf_out, rbuf, rcnt);
  } while(rcnt > 0);
}

int jackrec_async_t::process(jack_nframes_t nframes,
                             const std::vector<float*>& inBuffer,
                             const std::vector<float*>& outBuffer,
                             uint32_t tp_frame, bool tp_rolling)
{
  size_t ch(inBuffer.size());
  for(size_t k = 0; k < nframes; ++k)
    for(size_t c = 0; c < ch; ++c)
      buf[ch * k + c] = inBuffer[c][k];
  size_t wcnt(ch * nframes * sizeof(float));
  size_t cnt(jack_ringbuffer_write(rb, (const char*)buf, wcnt));
  if(cnt < wcnt)
    ++xrun;
  recframes += nframes;
  rectime = recframes * tscale;
  return 0;
}

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
