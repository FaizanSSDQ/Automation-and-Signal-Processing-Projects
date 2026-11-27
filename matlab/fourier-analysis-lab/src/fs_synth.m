function y = fs_synth(c, T, t)
%FS_SYNTH  Partial Fourier sum from coefficients c(-N) .. c(N).
%   y = fs_synth(c, T, t)   returns sum_n c_n exp(j 2 pi n t / T), real part
%
%   The coefficients of a real signal come in conjugate pairs, so the sum is
%   real; the real part only removes rounding noise.
%
%   Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

    N = (numel(c) - 1) / 2;
    n = -N:N;
    y = real(c(:).' * exp(1j * 2 * pi * n(:) * t(:).' / T));
    y = reshape(y, size(t));
end
