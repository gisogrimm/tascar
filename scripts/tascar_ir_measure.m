function [ir,fs,x,y,sCfg,sHelp] = tascar_ir_measure( varargin )
% TASCAR_IR_MEASURE - measure impulse responses via jack
%
% Usage:
% [ir,fs,x,y] = tascar_ir_measure( ... )
%
% Use key/value pairs to fine-control the program.
%
% Examples:
% - Show list of valid key/value pairs:
%   tascar_ir_measure help
%
% - Measure impulse response, playback via first hardware channel,
%   recorded from first two hardware inputs:
%   [ir,fs] = tascar_ir_measure( 'output', 1, 'input', 1:2 );
%
% - Measure impulse response from hardware inputs 1, 3 and 7, use
%   the third input (hardware channel 7) as a reference:
%   [ir,fs] = tascar_ir_measure( 'output', 1, 'input', [1,3,7], 'refchannel', 3 );
%
% The test signal is played once before the measurement phase.
%
% Author: Giso Grimm
% Date: 11/2015
  if nargin == 0
    varargin{end+1} = 'help';
    disp(help(mfilename));
  end
  sCfg = struct;
  sHelp = struct;
  sCfg.len = 2^10;
  sHelp.len = 'length of impulse response in samples';
  sCfg.nrep = 1;
  sHelp.nrep = 'number of repetitions in measurement phase';
  sCfg.gain = 0.1;
  sHelp.gain = 'scaling of test signal (peak amplitude re FS)';
  sCfg.output = {};
  sHelp.output = 'output ports';
  sCfg.input = {};
  sHelp.input = 'input ports';
  sCfg.refchannel = [];
  sHelp.refchannel = 'use a recorded channel as reference signal';
  sCfg.sigtype = 'farina';
  sHelp.sigtype = ['Measurement signal type: squarephase, farina,' ...
		   ' noise'];
  sCfg.regthr = -60;
  sHelp.regthr = 'Regularization threshold in dB (relative to median)';
  sCfg.fmin = 0.001;
  sHelp.fmin = 'Minimum frequency in Farina sigtype (1 = Nyquist frequency)';
  sCfg.fmax = 1;
  sHelp.fmax = 'Maximum frequency in Farina sigtype (1 = Nyquist frequency)';
  sCfg.ramplen = 0.001;
  sHelp.ramplen = ['Total ramp length in Farina sigtype (1 = full' ...
		   ' signal, corresponding to multiplication with' ...
		   ' raised cosine).'];
  sCfg.fweight = 0.25;
  sHelp.fweight = ['Frequency weighting exponent in Farina method.' ...
		   ' g ~ f^(-fweight)'];
  sCfg = tascar_parse_keyval( sCfg, sHelp, varargin{:} );
  if isempty(sCfg)
    return;
  end
  if nargin < 6
    refchannel = [];
  end
  if ischar(sCfg.output)
    nChannels = 1;
  else
    nChannels = numel(sCfg.output);
  end
  x = create_testsig( sCfg.len, sCfg.gain, sCfg.sigtype, sCfg );
  [y,fs,bufsize,load,xruns] = ...
      tascar_jackio( repmat(x,[sCfg.nrep+1,nChannels]), ...
		     'output',sCfg.output, ...
		     'input',sCfg.input );
  if xruns > 0
    warning(sprintf('%d xruns!',xruns));
  end
  y(1:sCfg.len,:) = [];
  y = reshape( y, [sCfg.len, sCfg.nrep, size(y,2)] );
  ymean = squeeze(mean( y, 2 ));
  if isempty(sCfg.refchannel)
    X = realfft(x);
    Y = realfft(ymean);
  else
    X = realfft(ymean(:,sCfg.refchannel));
    Y = realfft(ymean(:,setdiff(1:size(ymean,2),sCfg.refchannel)));
  end
  % regularization:
  X_med = median(abs(X));
  regthr = X_med * 10.^(0.05*sCfg.regthr);
  idx_reg = find(abs(X)<regthr);
  X(idx_reg) = regthr * exp(i*angle(X(idx_reg)));
  % end regularization
  Y = Y ./ repmat(X,[1,size(Y,2)]);
  ir = realifft(Y,sCfg.len);

function x = create_testsig( len, gain, stype, sCfg )
  nbins = floor(len/2)+1;
  vF = [1:nbins]';
  vF = vF / max(vF);
  switch stype
   case 'squarephase'
    X = exp(-len*i*(2*pi)*vF.^2)./max(0.1,vF.^0.5);
    x = realifft(X,len);
   case 'farina'
    T = ([1:len]'-1)/len;
    w1 = len*sCfg.fmin;
    w2 = len*sCfg.fmax;
    phase = pi*w1/log(w2/w1)*(exp(T*log(w2/w1))-1);
    x = sin(phase);
    f = diff(phase);
    f(end+1) = f(end);
    x = x.*f.^(-sCfg.fweight);
    rlen = round(0.5*sCfg.ramplen*len);
    wnd = hann(2*rlen);
    x(1:rlen) = x(1:rlen).*wnd(1:rlen);
    x(end+1-[1:rlen]) = x(end+1-[1:rlen]).*wnd(1:rlen);
   case 'noise'
    x = randn(len,1);
   otherwise
    error(['invalid type: ',stype]);
  end
  x = 0.5*x/max(abs(x))*gain;
