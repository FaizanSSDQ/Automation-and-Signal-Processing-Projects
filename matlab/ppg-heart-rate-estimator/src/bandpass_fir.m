function y = bandpass_fir(x, fs, f_lo, f_hi, taps)
%BANDPASS_FIR  Zero-phase band-pass filter (windowed sinc, Hamming), no toolbox.
%   y = bandpass_fir(x, fs, f_lo, f_hi)          default 10 s of taps
%   y = bandpass_fir(x, fs, f_lo, f_hi, taps)    odd number of taps
%
%   The heart-rate band (about 0.5 to 4 Hz, i.e. 30 to 240 bpm) is kept;
%   breathing wander below and noise above are removed. The filter delay is
%   removed afterwards, so peaks stay where they were (zero phase).
%
%   Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

    if nargin < 5
        taps = 10 * round(fs) + 1;      % transition band ~0.33 Hz: separates breathing (0.25 Hz)
    end
    m = (0:taps-1) - (taps - 1) / 2;
    lp = @(fc) 2 * fc / fs * sinc_(2 * fc / fs * m);
    w = 0.54 - 0.46 * cos(2 * pi * (0:taps-1) / (taps - 1));
    h = (lp(f_hi) - lp(f_lo)) .* w;
    d = (taps - 1) / 2;
    xp = [x(:).', zeros(1, d)];
    y = filter(h, 1, xp);
    y = y(d + 1:end);
    y = reshape(y, size(x));
end

function y = sinc_(x)
    y = ones(size(x));
    k = x ~= 0;
    y(k) = sin(pi * x(k)) ./ (pi * x(k));
end
