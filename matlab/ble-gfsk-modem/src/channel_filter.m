function y = channel_filter(x, fc, half)
%CHANNEL_FILTER  Zero-delay low-pass FIR for complex baseband (Hamming-windowed sinc).
%   y = channel_filter(x, fc, half)
%
%   fc is the cut-off in cycles per sample, the filter has 2*half+1 taps and
%   unit DC gain; its delay of half samples is removed so the output lines up
%   with the input. This is the receiver's channel (IF) filter: it keeps the
%   GFSK signal's band and rejects the noise outside it.
%
%   Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

    m = -half:half;
    h = 2 * fc * ones(size(m));
    k = m ~= 0;
    h(k) = sin(2 * pi * fc * m(k)) ./ (pi * m(k));
    h = h .* (0.54 + 0.46 * cos(pi * m / half));
    h = h / sum(h);
    y = filter(h, 1, [x(:).', zeros(1, half)]);
    y = y(half + 1:end);
end
