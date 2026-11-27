function y = rect_pulse(t)
%RECT_PULSE  Unit rectangular pulse: 1 for |t| < 1/2, 1/2 at |t| = 1/2, 0 elsewhere.
%   y = rect_pulse(t)
%
%   The value 1/2 at the edges is the midpoint a Fourier series converges to.
%
%   Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

    y = double(abs(t) < 0.5) + 0.5 * double(abs(t) == 0.5);
end
