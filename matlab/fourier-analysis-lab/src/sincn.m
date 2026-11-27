function y = sincn(x)
%SINCN  Normalised sinc, sin(pi x) / (pi x), with sincn(0) = 1.
%   Base MATLAB has no sinc() without the Signal Processing Toolbox.
%
%   Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

    y = ones(size(x));
    k = x ~= 0;
    y(k) = sin(pi * x(k)) ./ (pi * x(k));
end
