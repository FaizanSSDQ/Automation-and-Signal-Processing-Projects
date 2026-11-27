function c = fs_analytic(kind, n, duty)
%FS_ANALYTIC  Closed-form Fourier series coefficients of the waveform() signals.
%   c = fs_analytic('square', n)            -2j/(pi n) for odd n, 0 otherwise
%   c = fs_analytic('triangle', n)          4/(pi^2 n^2) for odd n, 0 otherwise
%   c = fs_analytic('sawtooth', n)          j/(pi n), c0 = 0
%   c = fs_analytic('pulse', n, duty)       duty for n = 0,
%                                           (1 - exp(-j 2 pi n duty)) / (j 2 pi n)
%
%   Used to check fs_coeffs() and to show how fast each spectrum decays:
%   jumps give 1/n, corners give 1/n^2.
%
%   Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

    if nargin < 3
        duty = 0.5;
    end
    c = zeros(size(n));
    odd = mod(n, 2) ~= 0;
    nz = n ~= 0;
    switch lower(kind)
        case 'square'
            c(odd) = -2j ./ (pi * n(odd));
        case 'triangle'
            c(odd) = 4 ./ (pi^2 * n(odd).^2);
        case 'sawtooth'
            c(nz) = 1j ./ (pi * n(nz));
        case 'pulse'
            c(~nz) = duty;
            c(nz) = (1 - exp(-1j * 2 * pi * n(nz) * duty)) ./ (1j * 2 * pi * n(nz));
        otherwise
            error('fs_analytic:kind', 'unknown waveform "%s"', kind);
    end
end
