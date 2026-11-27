function X = ctft(x, f, a, b)
%CTFT  Continuous-time Fourier transform of a signal that is zero outside [a, b].
%   X = ctft(x, f, a, b)    x function handle, f frequencies in Hz
%
%       X(f) = integral_a^b x(t) exp(-j 2 pi f t) dt
%
%   evaluated with integral() at each frequency, so it can be compared with
%   the closed-form transforms in the tests.
%
%   Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

    X = zeros(size(f));
    for k = 1:numel(f)
        g = @(t) x(t) .* exp(-1j * 2 * pi * f(k) * t);
        X(k) = integral(g, a, b, 'AbsTol', 1e-12, 'RelTol', 1e-10);
    end
end
