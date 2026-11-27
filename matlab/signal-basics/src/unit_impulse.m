function y = unit_impulse(n)
%UNIT_IMPULSE  Discrete-time unit impulse delta[n]: 1 at n = 0, 0 elsewhere.
%   y = unit_impulse(n)     n integer sample indices
%
%   The continuous-time Dirac delta has no finite value at t = 0; in
%   discrete time the impulse is a single sample of height 1, and any signal
%   is a sum of shifted, scaled impulses: x[n] = sum_k x[k] delta[n - k].
%
%   Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

    y = double(n == 0);
end
