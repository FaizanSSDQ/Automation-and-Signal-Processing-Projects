function y = fs_reconstruct(D, T, t)
%FS_RECONSTRUCT  Signal rebuilt from its Fourier coefficients D(-N) .. D(N).
%   y = fs_reconstruct(D, T, t)
%
%       y(t) = sum_n D_n exp(j 2 pi n t / T)
%
%   One matrix product replaces the 23 hand-written terms of the course
%   script. The coefficients of a real signal come in conjugate pairs, so
%   the sum is real; the real part (not the magnitude) removes rounding
%   noise without folding negative values up.
%
%   Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

    N = (numel(D) - 1) / 2;
    n = -N:N;
    y = real(D(:).' * exp(1j * 2 * pi * n(:) * t(:).' / T));
    y = reshape(y, size(t));
end
