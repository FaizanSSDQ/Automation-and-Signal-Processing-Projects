function [s, freq] = gfsk_mod(bits, sps, bt, h)
%GFSK_MOD  Gaussian frequency-shift keying, complex baseband (no toolbox).
%   [s, freq] = gfsk_mod(bits, sps)            Bluetooth LE: BT = 0.5, h = 0.5
%   [s, freq] = gfsk_mod(bits, sps, bt, h)
%
%   1. Map bits to +-1 and hold each for sps samples.
%   2. Smooth with a Gaussian pulse of bandwidth-time product BT, which
%      limits the occupied bandwidth.
%   3. Integrate the result as instantaneous frequency: the deviation is
%      +-h/2 of the symbol rate (250 kHz at 1 Msym/s for h = 0.5).
%   s has unit amplitude; freq is the instantaneous frequency in cycles per
%   symbol (+-0.25 for h = 0.5 after the filter settles).
%
%   Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

    if nargin < 3
        bt = 0.5;
    end
    if nargin < 4
        h = 0.5;
    end
    nrz = kron(2 * bits(:).' - 1, ones(1, sps));
    g = gaussian_pulse(bt, sps, 3);
    freq = conv(nrz, g, 'same') * h / 2;          % cycles per symbol
    phase = 2 * pi * cumsum(freq) / sps;
    s = exp(1j * phase);
end

function g = gaussian_pulse(bt, sps, span)
    % Impulse response of a Gaussian filter with 3-dB bandwidth B = BT / T,
    % truncated to +-span/2 symbols and normalised to unit DC gain.
    t = (-span * sps / 2 : span * sps / 2) / sps;   % in symbols
    sigma = sqrt(log(2)) / (2 * pi * bt);
    g = exp(-t.^2 / (2 * sigma^2));
    g = g / sum(g);
end
