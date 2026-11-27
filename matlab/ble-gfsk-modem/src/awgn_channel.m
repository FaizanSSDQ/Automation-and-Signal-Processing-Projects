function r = awgn_channel(s, ebn0_db, sps, cfo, rng_seed)
%AWGN_CHANNEL  Add white Gaussian noise for a given Eb/N0, and an optional carrier offset.
%   r = awgn_channel(s, ebn0_db, sps)
%   r = awgn_channel(s, ebn0_db, sps, cfo)             cfo in cycles per symbol
%   r = awgn_channel(s, ebn0_db, sps, cfo, rng_seed)   repeatable noise
%
%   s has unit power and one bit per symbol, so Eb = sps samples of power 1.
%   Complex noise of variance sps / (Eb/N0) per sample gives the requested
%   Eb/N0 over the full sample bandwidth.
%
%   A frequency offset of 50 kHz at 1 Msym/s is cfo = 0.05.
%
%   Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

    if nargin < 4
        cfo = 0;
    end
    if nargin >= 5
        randn('state', rng_seed); %#ok<RAND>   works in MATLAB and Octave
    end
    n = numel(s);
    sigma2 = sps / 10^(ebn0_db / 10);
    noise = sqrt(sigma2 / 2) * (randn(1, n) + 1j * randn(1, n));
    r = s(:).' .* exp(1j * 2 * pi * cfo * (0:n-1) / sps) + noise;
end
