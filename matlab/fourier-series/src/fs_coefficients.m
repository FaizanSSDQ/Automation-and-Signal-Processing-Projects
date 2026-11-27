function [D, n] = fs_coefficients(f, a, b, N)
%FS_COEFFICIENTS  Exponential Fourier series coefficients D(-N) .. D(N) over one period [a, b].
%   [D, n] = fs_coefficients(f, a, b, N)
%
%       D_n = (1/T) * integral_a^b f(t) exp(-j 2 pi n t / T) dt,   T = b - a
%
%   f is a function handle. Each coefficient is one call to integral(),
%   with the 1/T factor applied to every n (the course scripts applied it to
%   D_0 only, and took the period as a - b, which is negative).
%   D(k) holds the coefficient of harmonic n(k).
%
%   Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

    T = b - a;
    if T <= 0
        error('fs_coefficients:period', 'the period [a, b] needs b > a');
    end
    n = -N:N;
    D = zeros(size(n));
    for k = 1:numel(n)
        g = @(t) f(t) .* exp(-1j * 2 * pi * n(k) * t / T);
        D(k) = integral(g, a, b, 'AbsTol', 1e-12, 'RelTol', 1e-10) / T;
    end
end
