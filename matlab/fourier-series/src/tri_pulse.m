function y = tri_pulse(t)
%TRI_PULSE  Unit triangular pulse: 1 - |t| for |t| < 1, 0 elsewhere.
%   y = tri_pulse(t)
%
%   tri_pulse(t/2) is the pulse of the course assignment: peak 1 at t = 0,
%   zero at t = +-2, so one period of the periodic version spans [-2, 2].
%
%   Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

    y = max(0, 1 - abs(t));
end
