function [vF, vL] = tascar_bandlevels(w, cfmin, cfmax, fs, bpo, overlap)
% GET_BANDLEVELS Calculate band levels of a waveform signal.
%
% Inputs:
%   w      - Input waveform vector (time domain), or file name
%   cfmin  - Minimum center frequency
%   cfmax  - Maximum center frequency
%   fs     - Sampling frequency (if a file name is provided and fs is zero, the file fs is taken)
%   bpo    - Bands per octave
%   overlap - Overlap factor (typically 0.5 for 50% overlap, measured in bands)
%
% Outputs:
%   vF     - Vector of center frequencies
%   vL     - Vector of band levels in dB

    % Calculate number of bands
    numbands = floor(bpo * log2(cfmax / cfmin)) + 1;

    % Recalculate bpo to ensure exact fit between cfmin and cfmax
    bpo = (numbands - 1) / log2(cfmax / cfmin);

    % Initialize frequency vector
    vF = zeros(1, numbands);

    % Calculate center frequencies
    for k = 0:numbands-1
        f = cfmin * 2.0^(k / bpo);
        vF(k+1) = f;
    end

    % Optionally read w from sound file
    if ischar(w)
      [w,file_fs] = audioread(w);
      w = w(:,1);
      if isempty(fs) || (fs==0)
        fs = file_fs;
      end
    end

    % Get length of waveform
    w_n = length(w);

    % Execute FFT
    fft_s = fft(w);

    % Initialize level vector
    vL = zeros(1, numbands);

    % Pre-calculate squared magnitude of FFT spectrum
    P = abs(fft_s).^2;

    % Iterate through each center frequency
    for i = 1:length(vF)
        f = vF(i);

        % Calculate edge frequencies
        f1e = f * 2.0^(-0.5 / bpo);
        f2e = f * 2.0^(0.5 / bpo);
        f1  = f * 2.0^(-(0.5 + overlap) / bpo);
        f2  = f * 2.0^((0.5 + overlap) / bpo);

        % Calculate indices
        idx1e = min(floor(w_n * f1e / fs), w_n) + 1;
        idx2e = min(floor(w_n * f2e / fs), w_n) + 1;
        idx1  = min(floor(w_n * f1 / fs), w_n) + 1;
        idx2  = min(floor(w_n * f2 / fs), w_n) + 1;

        % Initialize level accumulator
        l = 0.0;

        % Lower overlap area (f1 to f1e)
        if idx1e > idx1
            k_range = idx1:(idx1e-1);
            % Normalized position:
            norm_pos = (k_range - idx1) / (idx1e - idx1);
            w_win = 0.5 - 0.5 * cos(norm_pos * pi);
            l = l + sum(P(k_range) .* (w_win.^2));
        end

        % Central area (f1e to f2e)
        if idx2e > idx1e
            k_range = idx1e:(idx2e-1);
            l = l + sum(P(k_range));
        end

        % Upper overlap area (f2e to f2)
        if idx2 > idx2e
            k_range = idx2e:(idx2-1);
            % C++: (k - idx2e) / (idx2 - idx2e)
            norm_pos = (k_range - idx2e) / (idx2 - idx2e);
            w_win = 0.5 + 0.5 * cos(norm_pos * pi);
            l = l + sum(P(k_range) .* (w_win.^2));
        end

        % Scale to Pa^2
        l = l * (5e4)^2 * 2.0;

        % Normalize by N^2
        l = l / (w_n^2);

        % Convert to dB
        vL(i) = 10 * log10(l);
    end
end