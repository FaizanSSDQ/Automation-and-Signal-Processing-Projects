function [e, W] = nlms_cancel(d, refs, order, mu)
%NLMS_CANCEL  Adaptive noise cancellation with the normalised LMS algorithm.
%   [e, W] = nlms_cancel(d, refs)               order 8 per reference, mu 0.05
%   [e, W] = nlms_cancel(d, refs, order, mu)
%
%   d     the corrupted signal (here: PPG = pulse + motion artefact)
%   refs  one reference signal per row (here: accelerometer axes), which
%         carry the motion but not the pulse
%
%   An FIR filter per reference learns to turn the accelerometer into the
%   artefact it causes; its output is subtracted from d. Because the pulse
%   is uncorrelated with the motion, the filter cannot remove it, so the
%   error signal e is the cleaned PPG:
%       y = w' u,   e = d - y,   w <- w + mu e u / (eps + u' u)
%   W holds the final weights (order x references).
%
%   Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

    if nargin < 3
        order = 8;
    end
    if nargin < 4
        mu = 0.05;      % balances convergence speed (first seconds) and steady-state residual
    end
    d = d(:).';
    R = size(refs, 1);
    refs = refs - mean(refs, 2) * ones(1, size(refs, 2));   % remove gravity / DC
    w = zeros(order * R, 1);
    u = zeros(order * R, 1);
    e = zeros(size(d));
    for n = 1:numel(d)
        for r = 1:R
            idx = (r - 1) * order + (1:order);
            u(idx) = [refs(r, n); u(idx(1:end-1))];
        end
        y = w.' * u;
        e(n) = d(n) - y;
        w = w + mu * e(n) * u / (1e-6 + u.' * u);
    end
    W = reshape(w, order, R);
end
