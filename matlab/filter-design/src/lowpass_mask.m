function [H, fq] = lowpass_mask(N0, f0, fc)
%LOWPASS_MASK  Ideal low-pass response on the DFT frequency grid.
%   [H, fq] = lowpass_mask(N0, f0, fc)
%
%   Bin q stands for the frequency q*f0 for q < N0/2 and (q - N0)*f0 above
%   (the upper half of the DFT holds the negative frequencies). H is 1 where
%   |frequency| < fc, 1/2 exactly at fc and 0 above. Both halves are set
%   together, so H is symmetric and the filtered signal stays real; the
%   course script set the upper pass band to 2 and never filled most of the
%   mask because its loop ran only once.
%
%   Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

    q = 0:N0-1;
    fq = q * f0;
    fq(q >= N0 / 2) = (q(q >= N0 / 2) - N0) * f0;
    a = abs(fq);
    H = double(a < fc - 1e-12) + 0.5 * double(abs(a - fc) <= 1e-12);
end
