function [g, k] = pulse_samples(width, T0, Ts)
%PULSE_SAMPLES  One period of a centred rectangular pulse train, sampled for the DFT.
%   [g, k] = pulse_samples(width, T0, Ts)
%
%   N0 = T0 / Ts samples, k = 0 .. N0-1, with the pulse centred on k = 0 and
%   wrapping around the end of the record (the DFT treats the record as one
%   period). A sample that falls exactly on an edge gets 1/2, the midpoint of
%   the jump. For width 1 s, T0 = 4 s, Ts = 1/8 s this is the course example:
%   g = 1 for k = 0..3 and 29..31, 1/2 for k = 4 and 28, 0 otherwise.
%
%   Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

    N0 = round(T0 / Ts);
    k = 0:N0-1;
    t = k * Ts;
    t(t >= T0 / 2) = t(t >= T0 / 2) - T0;          % wrap to [-T0/2, T0/2)
    half = width / 2;
    g = double(abs(t) < half - 1e-12) + 0.5 * double(abs(abs(t) - half) <= 1e-12);
end
