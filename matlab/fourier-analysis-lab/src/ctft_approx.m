function [X, f] = ctft_approx(x, fs)
%CTFT_APPROX  Continuous-time Fourier transform estimated with the FFT.
%   [X, f] = ctft_approx(x, fs)
%
%   x holds samples of a signal that is (close to) zero outside the record,
%   taken at rate fs, with the first sample at t = 0. Then
%       X(f) = integral x(t) exp(-j 2 pi f t) dt  ~  (1/fs) * DFT{x}
%   on the frequency grid f = (-L/2 .. L/2-1) * fs / L, centred with fftshift.
%
%   Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

    L = numel(x);
    X = fftshift(fft(x(:).')) / fs;
    f = (-floor(L/2) : ceil(L/2) - 1) * fs / L;
end
