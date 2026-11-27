function y = waveform(kind, t, T, duty)
%WAVEFORM  Periodic test signals with period T.
%   y = waveform('square',   t, T)        +1 on [0, T/2), -1 on [T/2, T)
%   y = waveform('triangle', t, T)        +1 at t = 0, -1 at t = T/2 (even)
%   y = waveform('sawtooth', t, T)        rises from -1 to +1 over each period
%   y = waveform('pulse',    t, T, duty)  1 on [0, duty*T), 0 elsewhere
%
%   Base MATLAB only (no Signal Processing Toolbox).
%
%   Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

    if nargin < 4
        duty = 0.5;
    end
    u = mod(t, T) / T;                 % position within the period, 0 .. 1
    switch lower(kind)
        case 'square'
            y = 1 - 2 * (u >= 0.5);
        case 'triangle'
            y = 1 - 4 * min(u, 1 - u);
        case 'sawtooth'
            y = 2 * u - 1;
        case 'pulse'
            y = double(u < duty);
        otherwise
            error('waveform:kind', 'unknown waveform "%s"', kind);
    end
end
