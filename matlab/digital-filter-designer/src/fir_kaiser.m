function [h, info] = fir_kaiser(type, f_edges, fs, atten_db)
%FIR_KAISER  Linear-phase FIR filter by the Kaiser window method (no toolbox).
%   [h, info] = fir_kaiser('low',  [f_pass f_stop], fs, atten_db)
%   [h, info] = fir_kaiser('high', [f_stop f_pass], fs, atten_db)
%   [h, info] = fir_kaiser('band', [f_stop1 f_pass1 f_pass2 f_stop2], fs, atten_db)
%
%   The ideal response (a sinc for each band edge, placed in the middle of the
%   transition band) is multiplied by a Kaiser window. Kaiser's formulas give
%   the window shape (beta) and the length that meet the stop-band
%   attenuation over the narrowest transition band:
%       N    = (A - 8) / (2.285 * dw)          dw = transition width, rad/sample
%       beta = 0.1102 (A - 8.7)                for A > 50 dB, and so on
%   The length is made odd (type I filter), so every response type works.
%
%   info.N, info.beta, info.delay (samples) describe the result.
%
%   Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

    A = atten_db;
    widths = abs(diff(f_edges));
    switch lower(type)
        case {'low', 'high'}
            dw = 2 * pi * widths(1) / fs;
        case 'band'
            dw = 2 * pi * min(widths([1 3])) / fs;
        otherwise
            error('fir_kaiser:type', 'type must be low, high or band');
    end
    if A > 50
        beta = 0.1102 * (A - 8.7);
    elseif A >= 21
        beta = 0.5842 * (A - 21)^0.4 + 0.07886 * (A - 21);
    else
        beta = 0;
    end
    N = ceil((A - 8) / (2.285 * dw));
    N = N + mod(N, 2);                       % even order -> odd length, symmetric
    m = (0:N) - N / 2;                       % centred sample index

    lp = @(fc) 2 * fc / fs * sinc_(2 * fc / fs * m);   % ideal low-pass, cut-off fc
    switch lower(type)
        case 'low'
            hd = lp(mean(f_edges));
        case 'high'
            hd = double(m == 0) - lp(mean(f_edges));
        case 'band'
            hd = lp(mean(f_edges(3:4))) - lp(mean(f_edges(1:2)));
    end
    w = besseli(0, beta * sqrt(1 - (2 * m / N).^2)) / besseli(0, beta);
    h = hd .* w;
    info = struct('N', N, 'beta', beta, 'delay', N / 2);
end

function y = sinc_(x)
    y = ones(size(x));
    k = x ~= 0;
    y(k) = sin(pi * x(k)) ./ (pi * x(k));
end
