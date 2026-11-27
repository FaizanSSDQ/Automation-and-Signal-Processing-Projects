function y = ramp(t)
%RAMP  Unit ramp r(t) = t u(t): the running integral of the unit step.
%   y = ramp(t)
%
%   Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

    y = t .* (t >= 0);
end
