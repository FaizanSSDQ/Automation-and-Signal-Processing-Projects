function run_tests()
%RUN_TESTS  Checks for the Fourier analysis lab. Runs in MATLAB and GNU Octave.
%   From the project folder:  addpath src tests; run_tests
%   Throws an error (non-zero exit in batch mode) if any check fails.
%
%   Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

    here = fileparts(mfilename('fullpath'));
    addpath(fullfile(here, '..', 'src'));
    checks = 0;
    failures = 0;

    function check(cond, name)
        checks = checks + 1;
        if ~cond
            failures = failures + 1;
            fprintf('  FAIL  %s\n', name);
        end
    end

    % ---- waveforms --------------------------------------------------------------------
    T = 2;
    check(waveform('square', 0.1, T) == 1 && waveform('square', 1.1, T) == -1, 'square levels');
    check(waveform('triangle', 0, T) == 1 && waveform('triangle', 1, T) == -1, 'triangle peaks');
    check(abs(waveform('sawtooth', 1, T)) < 1e-12, 'sawtooth mid-period is 0');
    check(waveform('pulse', 0.5, T, 0.3) == 1 && waveform('pulse', 0.7, T, 0.3) == 0, 'pulse duty');
    check(abs(waveform('square', 0.1 + 5 * T, T) - 1) < 1e-12, 'periodic');

    % ---- numeric coefficients agree with the closed forms ------------------------------
    kinds = {'square', 'triangle', 'sawtooth', 'pulse'};
    for k = 1:numel(kinds)
        kind = kinds{k};
        f = @(t) waveform(kind, t, T, 0.3);
        [c, n] = fs_coeffs(f, T, 25, 65536);
        ca = fs_analytic(kind, n, 0.3);
        check(max(abs(c - ca)) < 1e-4, ['coefficients match closed form: ' kind]);
    end

    % ---- Parseval: power in time = sum of |c_n|^2 ------------------------------------
    n = -4001:4001;
    check(abs(sum(abs(fs_analytic('square', n)).^2) - 1) < 2e-4, 'Parseval square (power 1)');
    check(abs(sum(abs(fs_analytic('triangle', n)).^2) - 1/3) < 1e-9, 'Parseval triangle (power 1/3)');
    check(abs(sum(abs(fs_analytic('sawtooth', n)).^2) - 1/3) < 1e-4, 'Parseval sawtooth (power 1/3)');

    % ---- conjugate symmetry of a real signal -------------------------------------------
    [c, n] = fs_coeffs(@(t) waveform('pulse', t, T, 0.3), T, 10);
    check(max(abs(c - conj(fliplr(c)))) < 1e-12, 'c(-n) = conj(c(n)) for a real signal');

    % ---- synthesis converges; corners converge faster than jumps -----------------------
    t = linspace(0, 2 * T, 2001);
    yt = fs_synth(fs_analytic('triangle', -201:201), T, t);
    check(max(abs(yt - waveform('triangle', t, T))) < 0.005, 'triangle reconstructed within 0.5 %');
    tm = [0.125 0.25 0.375 0.625 0.875] * T;      % away from the jumps at 0 and T/2
    ys = fs_synth(fs_analytic('square', -401:401), T, tm);
    check(max(abs(ys - waveform('square', tm, T))) < 0.01, 'square reconstructed away from the jumps');

    % ---- Gibbs phenomenon: about 8.95 % of the jump, whatever N --------------------------
    g = [gibbs_overshoot(25), gibbs_overshoot(101), gibbs_overshoot(401)];
    check(all(g > 8.8 & g < 9.1), sprintf('Gibbs overshoot ~8.95%% (got %.2f %.2f %.2f)', g));

    % ---- continuous-time transform of a rectangular pulse is a sinc ---------------------
    fs = 1000;
    tau = 0.5;
    tt = (0:20 * fs - 1) / fs;
    x = double(tt < tau);
    [X, f] = ctft_approx(x, fs);
    band = abs(f) <= 10;
    check(max(abs(abs(X(band)) - tau * abs(sincn(f(band) * tau)))) < 1e-3, 'rect pulse -> tau*sinc(f*tau)');
    check(sincn(0) == 1 && abs(sincn(1)) < 1e-15 && abs(sincn(0.5) - 2/pi) < 1e-15, 'sincn values');

    % ---- windows: the textbook side-lobe levels ----------------------------------------
    L = 64;
    expect = {'rect', -13.3, 1; 'hann', -31.5, 2; 'hamming', -42.7, 2; 'blackman', -58.1, 3};
    for k = 1:size(expect, 1)
        [psl, ml] = window_sidelobe(window_fn(expect{k, 1}, L));
        check(abs(psl - expect{k, 2}) < 0.6, sprintf('%s side lobe %.1f dB', expect{k, 1}, psl));
        check(abs(ml - expect{k, 3}) < 0.1, sprintf('%s main lobe %.2f bins', expect{k, 1}, ml));
    end
    check(all(abs(window_fn('hann', 9) - flipud(window_fn('hann', 9))) < 1e-15), 'windows are symmetric');

    fprintf('%d checks, %d failures\n', checks, failures);
    if failures > 0
        error('run_tests:failed', '%d of %d checks failed', failures, checks);
    end
end
