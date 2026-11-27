function y = sincn(x)
%SINCN  Normalised sinc, sin(pi x) / (pi x), with sincn(0) = 1 (no toolbox needed).
%
%   Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

    y = ones(size(x));
    k = x ~= 0;
    y(k) = sin(pi * x(k)) ./ (pi * x(k));
end
