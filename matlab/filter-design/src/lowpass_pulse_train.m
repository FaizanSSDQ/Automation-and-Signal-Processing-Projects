function y = lowpass_pulse_train(t, width, T0, fc)
%LOWPASS_PULSE_TRAIN  Exact output of an ideal low-pass filter fed with a rectangular pulse train.
%   y = lowpass_pulse_train(t, width, T0, fc)
%
%   The pulse train's Fourier series is D_n = (width/T0) sinc(n width/T0).
%   The ideal filter keeps the harmonics below fc (and halves one that sits
%   exactly on fc), so the output is a finite Fourier sum: the reference the
%   DFT result is checked against.
%
%   Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

    f0 = 1 / T0;
    nmax = floor(fc / f0 + 1e-9);
    n = -nmax:nmax;
    d = width / T0;
    D = d * ones(size(n));
    nz = n ~= 0;
    D(nz) = sin(pi * n(nz) * d) ./ (pi * n(nz));
    w = ones(size(n));
    w(abs(abs(n) * f0 - fc) < 1e-9) = 0.5;
    y = real((D .* w) * exp(1j * 2 * pi * n(:) * f0 * t(:).'));
    y = reshape(y, size(t));
end
