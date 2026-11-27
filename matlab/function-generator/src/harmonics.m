function [A, f] = harmonics(x, fs, f0, K)
%HARMONICS  Amplitudes of the first K harmonics of a periodic signal, from the FFT.
%   [A, f] = harmonics(x, fs, f0, K)
%
%   x must hold a whole number of periods of fundamental f0, sampled at fs,
%   so every harmonic falls exactly on an FFT bin (no leakage). A(k) is the
%   peak amplitude of the k-th harmonic (one-sided, so a sine of amplitude 1
%   gives A = 1); f(k) = k * f0.
%
%   Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

    L = numel(x);
    X = fft(x(:).') / L;
    bins = round((1:K) * f0 * L / fs);
    A = 2 * abs(X(bins + 1));
    f = (1:K) * f0;
end
