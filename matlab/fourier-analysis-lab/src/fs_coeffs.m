function [c, n] = fs_coeffs(f, T, N, M)
%FS_COEFFS  Complex Fourier series coefficients c(-N) .. c(N) of a T-periodic function.
%   [c, n] = fs_coeffs(f, T, N)      f is a function handle f(t)
%   [c, n] = fs_coeffs(f, T, N, M)   M samples per period (default 4096)
%
%   c_n = (1/T) * integral over one period of f(t) exp(-j 2 pi n t / T) dt
%
%   The integral is evaluated with the midpoint rule on M points. For a
%   periodic integrand this rule is extremely accurate, and sampling at the
%   midpoints never lands exactly on a jump of a square or pulse wave.
%
%   Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

    if nargin < 4
        M = 4096;
    end
    tk = ((0:M-1) + 0.5) * T / M;          % midpoints of M equal slices
    fk = f(tk);
    n = -N:N;
    E = exp(-1j * 2 * pi * n(:) * tk / T);   % (2N+1) x M
    c = (E * fk(:)).' / M;
end
