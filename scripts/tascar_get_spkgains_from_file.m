function [vF, vG] = tascar_get_spkgains_from_file(fileref, filetest, cfmin, cfmax, bpo, overlap, channel_ref, channel_test)
%TASCAR_GET_SPKGAINS_FROM_FILE Calculate speaker gains by comparing
%band levels of two audio files.
%
%   This function computes the frequency-dependent gain required to
%   match the spectrum of a test signal to a reference signal. It
%   calculates band levels for both files and derives the gain as the
%   difference (Reference - Test).
%
% Inputs:
%   fileref  - Path or name of the reference audio file.
%   filetest - Path or name of the test audio file.
%   cfmin    - Minimum center frequency for analysis (Hz).
%   cfmax    - Maximum center frequency for analysis (Hz).
%   bpo      - Bands per octave.
%   overlap  - Overlap factor for band analysis (typically 0.5).
%   channel_ref - Audio channel in reference file (1-based; optional)
%   channel_test - Audio channel in reference file (1-based; optional)
%
% Outputs:
%   vF       - Vector of center frequencies (Hz).
%   vG       - Vector of calculated gains (dB) corresponding to vF.
%
% Behavior:
%   If called with no output arguments, the function displays the
%   frequency vector and gain vector to the command window and clears
%   the variables from the workspace.
%
% Example:
%   [freqs, gains] = ...
%      tascar_get_spkgains_from_file('ref.wav', 'test.wav', 80, 20000, 12, 6);
%   tascar_get_spkgains_from_file('ref.wav', 'test.wav', 80, 20000, 12, 6);
%       % Displays results

    if nargin < 7
        channel_ref = 1;
    end
    if nargin < 8
        channel_test = 1;
    end

  [vF,vL1] = tascar_bandlevels(fileref, cfmin, cfmax, 0, bpo, overlap, channel_ref );
  [vF,vL2] = tascar_bandlevels(filetest, cfmin, cfmax, 0, bpo, overlap, channel_test );
  vG = vL2-vL1;
  if nargout == 0
    disp('vfreq:');
    disp(sprintf('%g ',vF));
    disp('vgain:');
    disp(sprintf('%g ',vG));
    clear('vF','vG');
  end
end
