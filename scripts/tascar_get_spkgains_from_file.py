import sys
import argparse
import numpy as np
from scipy.io import wavfile

import numpy as np
from math import floor, log2, pow, cos, pi

def get_bandlevels(w, cfmin, cfmax, fs, bpo, overlap):
    """
    Calculate band levels of a waveform signal.
    Transcoded from C++ to match output exactly.
    """
    # Calculate number of bands
    numbands = int(np.floor(bpo * np.log2(cfmax / cfmin))) + 1

    # Recalculate bpo to ensure exact fit between cfmin and cfmax
    if numbands > 1:
        bpo = (numbands - 1) / np.log2(cfmax / cfmin)
    else:
        bpo = 1.0

    # Initialize frequency vector
    k_indices = np.arange(numbands)
    vF = cfmin * np.power(2.0, k_indices / bpo)

    # Get length of waveform
    w_n = len(w)

    # Execute FFT
    # We use standard FFT.
    fft_s = np.fft.fft(w)

    # Pre-calculate squared magnitude of FFT spectrum (Power)
    P = np.abs(fft_s)**2

    # Initialize level vector
    vL = np.zeros(numbands)

    # Nyquist frequency
    f_nyq = 0.5*fs

    # Iterate through each center frequency
    for i, f in enumerate(vF):
        # Calculate edge frequencies
        f1e = f * 2.0**(-0.5 / bpo)
        f2e = f * 2.0**(0.5 / bpo)
        f1  = f * 2.0**(-(0.5 + overlap) / bpo)
        f2  = f * 2.0**((0.5 + overlap) / bpo)

        # The C++ code calculates indices as: (uint32_t)((float)w.n * f / fs)
        # This maps frequency f directly to an index.
        # We must replicate this exactly to match the C++ output.
        idx1e = int(np.floor(w_n * f1e / f_nyq))
        idx2e = int(np.floor(w_n * f2e / f_nyq))
        idx1  = int(np.floor(w_n * f1 / f_nyq))
        idx2  = int(np.floor(w_n * f2 / f_nyq))

        # Clamp indices to valid range [0, w_n]
        # C++ uses std::min(..., fft.s.n_)
        idx1e = min(idx1e, w_n)
        idx2e = min(idx2e, w_n)
        idx1  = min(idx1, w_n)
        idx2  = min(idx2, w_n)

        # Initialize level accumulator
        l = 0.0

        # Lower overlap area (f1 to f1e)
        if idx1e > idx1:
            k_range = np.arange(idx1, idx1e)
            denom = (idx1e - idx1)
            # Avoid division by zero
            if denom > 0:
                norm_pos = (k_range - idx1) / denom
                w_win = 0.5 - 0.5 * np.cos(norm_pos * np.pi)
                l += np.sum(P[k_range] * (w_win**2))

        # Central area (f1e to idx2e)
        if idx2e > idx1e:
            k_range = np.arange(idx1e, idx2e)
            l += np.sum(P[k_range])

        # Upper overlap area (f2e to f2)
        if idx2 > idx2e:
            k_range = np.arange(idx2e, idx2)
            denom = (idx2 - idx2e)
            if denom > 0:
                norm_pos = (k_range - idx2e) / denom
                w_win = 0.5 + 0.5 * np.cos(norm_pos * np.pi)
                l += np.sum(P[k_range] * (w_win**2))

        # Scale to Pa^2
        # C++: l *= 5e4f * 5e4f * 2.0f;
        l = l * (5e4)**2 * 2.0

        # Normalize by N^2
        l = l / (w_n**2)

        # Convert to dB
        if l > 0:
            vL[i] = 10 * np.log10(l)
        else:
            vL[i] = -np.inf

    return vF, vL

def tascar_get_spkgains_from_file(fileref, filetest, cfmin, cfmax, bpo, overlap, channel=0):
    """
    Calculate speaker gains by comparing band levels of two audio files.
    """
    # Load Reference File
    try:
        fs_ref, data_ref = wavfile.read(fileref)
    except FileNotFoundError:
        print(f"Error: Reference file '{fileref}' not found.")
        sys.exit(1)

    # Load Test File
    try:
        fs_test, data_test = wavfile.read(filetest)
    except FileNotFoundError:
        print(f"Error: Test file '{filetest}' not found.")
        sys.exit(1)

    # Ensure sampling rates match
    if fs_ref != fs_test:
        print(f"Error: Sampling rate mismatch. Ref: {fs_ref}Hz, Test: {fs_test}Hz")
        sys.exit(1)

    # Convert to float and normalize if necessary
    if data_ref.dtype.kind == 'i':
        data_ref = data_ref.astype(np.float32) / np.iinfo(data_ref.dtype).max
    if data_test.dtype.kind == 'i':
        data_test = data_test.astype(np.float32) / np.iinfo(data_test.dtype).max

    # Handle multi-channel audio - select specific channel
    if len(data_ref.shape) > 1:
        if channel >= data_ref.shape[1]:
            print(f"Warning: Channel {channel} requested, but reference file has only {data_ref.shape[1]} channels. Using channel 0.")
            channel = 0
        data_ref = data_ref[:, channel]
    else:
        # Mono file, ignore channel parameter
        pass

    if len(data_test.shape) > 1:
        if channel >= data_test.shape[1]:
            print(f"Warning: Channel {channel} requested, but test file has only {data_test.shape[1]} channels. Using channel 0.")
            channel = 0
        data_test = data_test[:, channel]
    else:
        # Mono file, ignore channel parameter
        pass

    # Calculate Band Levels
    print(f"Analyzing Reference: {fileref} (Channel {channel})...")
    vF, vL1 = get_bandlevels(data_ref, cfmin, cfmax, fs_ref, bpo, overlap)

    print(f"Analyzing Test: {filetest} (Channel {channel})...")
    vF, vL2 = get_bandlevels(data_test, cfmin, cfmax, fs_ref, bpo, overlap)

    # Calculate Gains
    vG = vL1 - vL2

    return vF, vG

def main():
    parser = argparse.ArgumentParser(
        description="Calculate speaker gains by comparing band levels of two audio files."
    )
    parser.add_argument("fileref", help="Path to the reference audio file (WAV).")
    parser.add_argument("filetest", help="Path to the test audio file (WAV).")
    parser.add_argument("--cfmin", type=float, default=20.0, help="Minimum center frequency (Hz). Default: 20.0")
    parser.add_argument("--cfmax", type=float, default=20000.0, help="Maximum center frequency (Hz). Default: 20000.0")
    parser.add_argument("--bpo", type=float, default=3.0, help="Bands per octave. Default: 3.0")
    parser.add_argument("--overlap", type=float, default=0.5, help="Overlap factor. Default: 0.5")
    parser.add_argument("--channel", type=int, default=0, help="Input channel (0-based). Default: 0")

    args = parser.parse_args()

    # Run calculation
    vF, vG = tascar_get_spkgains_from_file(
        args.fileref,
        args.filetest,
        args.cfmin,
        args.cfmax,
        args.bpo,
        args.overlap,
        args.channel
    )

    # Output results
    print("\n--- Results ---")
    print("vfreq:")
    print(" ".join([f"{x:.2f}" for x in vF]))
    print("vgain:")
    print(" ".join([f"{x:.2f}" for x in vG]))

if __name__ == "__main__":
    main()
