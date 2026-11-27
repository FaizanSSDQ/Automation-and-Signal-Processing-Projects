function soft = gfsk_demod(r, sps, cutoff)
%GFSK_DEMOD  Channel filter + frequency discriminator: soft symbol values from GFSK.
%   soft = gfsk_demod(r, sps)
%   soft = gfsk_demod(r, sps, cutoff)    channel-filter cut-off in symbol rates (default 0.6)
%
%   1. Channel filter: a low-pass FIR (windowed sinc) keeps the signal's own
%      band (+-600 kHz at 1 Msym/s) and rejects the noise outside it, as the
%      IF filter of a real receiver does. Its delay is removed.
%   2. Discriminator: the phase step between consecutive samples,
%      angle(r[n] conj(r[n-1])), is the instantaneous frequency.
%   3. Integrate and dump over each symbol period, for every sample offset;
%      the caller picks the symbol timing. Positive means bit 1.
%
%   soft is a matrix: row k holds the symbol values for timing offset k-1.
%
%   Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

    if nargin < 3
        cutoff = 0.6;
    end
    r = r(:).';
    r = channel_filter(r, cutoff / sps, 4 * sps);   % src/channel_filter.m
    dphi = [0, angle(r(2:end) .* conj(r(1:end-1)))];
    acc = filter(ones(1, sps), 1, dphi);         % moving sum over one symbol
    nsym = floor(numel(acc) / sps) - 1;
    soft = zeros(sps, nsym);
    for k = 1:sps
        soft(k, :) = acc(k + sps * (1:nsym) - 1);
    end
end

