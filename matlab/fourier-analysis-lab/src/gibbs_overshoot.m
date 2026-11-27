function pct = gibbs_overshoot(N)
%GIBBS_OVERSHOOT  Overshoot of the N-harmonic partial sum of a square wave, in percent of the jump.
%   pct = gibbs_overshoot(N)
%
%   The square wave jumps from -1 to +1 (a jump of 2). However many harmonics
%   are added, the partial sum overshoots by about 8.95 % of the jump next
%   to the discontinuity: the Gibbs phenomenon. Adding harmonics makes the
%   ripple narrower, not lower.
%
%   Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

    T = 1;
    n = -N:N;
    c = fs_analytic('square', n);
    % The first peak sits near t = T / (2N); search finely around it.
    t = linspace(0, 3 * T / max(N, 1), 20000);
    y = fs_synth(c, T, t);
    pct = (max(y) - 1) / 2 * 100;
end
