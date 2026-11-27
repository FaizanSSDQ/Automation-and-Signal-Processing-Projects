function y = unit_step(t)
%UNIT_STEP  u(t): 1 for t >= 0, 0 otherwise.
%   y = unit_step(t)
%
%   Shifted and subtracted steps build every rectangular window:
%   u(t - t1) - u(t - t2) is 1 on [t1, t2).
%
%   Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

    y = double(t >= 0);
end
