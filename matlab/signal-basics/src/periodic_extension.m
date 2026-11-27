function [tp, xp] = periodic_extension(x, dt, periods)
%PERIODIC_EXTENSION  Repeat one period of samples to build a periodic signal.
%   [tp, xp] = periodic_extension(x, dt, periods)
%
%   x holds one period starting at t = 0, sampled every dt. The result runs
%   over periods/2 periods either side of t = 0 (periods must be even).
%   Replaces the growing-array loop of the course script.
%
%   Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

    L = numel(x);
    T = L * dt;
    xp = repmat(x(:).', 1, periods);
    tp = (0:L * periods - 1) * dt - periods / 2 * T;
end
