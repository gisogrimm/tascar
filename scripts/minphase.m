function [x_min, X_spec, phase] = minphase(x)
    %% Compute the minimum phase signal from the given input signal.
    %
    % Inputs:
    %   x       - Input signal (vector)
    %
    % Outputs:
    %   x_min   - Minimum phase signal corresponding to the input magnitude
    %   X_spec  - Complex spectrum of the minimum phase signal
    %   phase   - Phase of the minimum phase signal
    %
    % Description:
    % This function computes the minimum phase signal corresponding to the given
    % input signal using the Cepstral method.

    % Ensure input is a column vector
    x = x(:);
    N = length(x);

    % 1. Compute FFT of the input signal
    X = fft(x);

    % 2. Compute Magnitude
    mag = abs(X);

    % 3. Compute Real Cepstrum
    % Add small epsilon to avoid log(0)
    log_mag = log(max(mag, 1e-10));
    cepstrum = ifft(log_mag);

    % 4. Create Minimum Phase Window (Causal Window)
    % Set negative time components to zero and double non-zero (except DC and Nyquist)
    win = ones(N, 1);
    if mod(N, 2) == 0
        win(2:N/2) = 2;
        win(N/2+2:end) = 0;
    else
        win(2:(N+1)/2) = 2;
        win((N+3)/2:end) = 0;
    end

    % 5. Compute Minimum Phase Spectrum
    min_phase_cepstrum = cepstrum .* win;
    X_spec = exp(fft(min_phase_cepstrum));

    % 6. Extract Phase and Time Domain Signal
    phase = angle(X_spec);
    x_min = real(ifft(X_spec));
end