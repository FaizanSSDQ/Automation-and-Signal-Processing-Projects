function y = periodic_wave(kind, t, T, duty)
%PERIODIC_WAVE  Function-generator waveforms with period T.
%   y = periodic_wave('square',   t, T)          +1 / -1, 50 % duty
%   y = periodic_wave('pulse',    t, T, duty)    1 for the first duty*T of each period, else 0
%   y = periodic_wave('triangle', t, T)          -1 .. +1 .. -1, peak at T/2
%   y = periodic_wave('sawtooth', t, T)          -1 rising to +1 over each period
%   y = periodic_wave('sine',     t, T)
%
%   Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

    if nargin < 4
        duty = 0.5;
    end
    u = mod(t / T, 1);                          % position in the period, 0 .. 1
    u = round(u * 1e12) / 1e12;                 % samples on an edge must not fall either side by rounding
    u(u >= 1) = 0;
    switch lower(kind)
        case 'square'
            y = 1 - 2 * (u >= 0.5);
        case 'pulse'
            y = double(u < duty);
        case 'triangle'
            y = 1 - 4 * abs(u - 0.5);
        case 'sawtooth'
            y = 2 * u - 1;
        case 'sine'
            y = sin(2 * pi * u);
        otherwise
            error('periodic_wave:kind', 'unknown waveform "%s"', kind);
    end
end
