function run_tests()
%RUN_TESTS  Checks for DFT-based ideal low-pass filtering. MATLAB and GNU Octave.
%   From the project folder:  addpath tests; run_tests
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

    % The course example: 1-s pulse, period T0 = 4 s, sampled at 8 Hz (B = 4 Hz), cut-off 2 Hz.
    T0 = 4; Ts = 1 / 8; N0 = 32; f0 = 1 / T0;
    [g, k] = pulse_samples(1, T0, Ts);
    expect = zeros(1, N0); expect([0:3 29:31] + 1) = 1; expect([4 28] + 1) = 0.5;
    check(isequal(g, expect), 'pulse samples match the textbook table');

    [H, fq] = lowpass_mask(N0, f0, 2);
    check(all(H(fq ~= 0) == fliplr(H(2:end))), 'mask is symmetric (H[q] = H[N0-q])');
    check(sum(H == 1) == 15 && sum(H == 0.5) == 2, 'pass band: 15 bins at 1, two at 1/2 (q = 8 and 24)');

    [y, G] = dft_filter(g, H);
    check(max(abs(imag(ifft(G .* H)))) < 1e-12, 'filtered signal is real');
    check(abs(G(1) - sum(g)) < 1e-12 && abs(sum(y) - sum(g)) < 1e-9, 'DC gain is 1: the pulse area is kept');
    check(abs(G(1) * Ts - 1) < 1e-12, 'G[0] Ts = pulse area (1 V s)');

    % ---- against the exact continuous result ----------------------------------------------------
    ref = lowpass_pulse_train(k * Ts, 1, T0, 2);
    err = max(abs(y - ref));
    check(err < 0.03, sprintf('DFT result within %.3f of the exact filtered pulse train (aliasing)', err));
    T0f = 4; Tsf = 1 / 64;
    [gf, kf] = pulse_samples(1, T0f, Tsf);
    yf = dft_filter(gf, lowpass_mask(numel(gf), 1 / T0f, 2));
    errf = max(abs(yf - lowpass_pulse_train(kf * Tsf, 1, T0f, 2)));
    check(errf < err / 4, sprintf('8x faster sampling shrinks the aliasing error (%.4f -> %.4f)', err, errf));

    % ---- the filtered pulse rings, and a wider filter makes the edges sharper ------------------
    t = linspace(-2, 2, 4001);
    y1 = lowpass_pulse_train(t, 1, T0, 1);
    y3 = lowpass_pulse_train(t, 1, T0, 3);
    check(max(y3) > 1.05, 'ideal filtering makes the pulse overshoot (Gibbs)');
    slope = @(y) max(abs(diff(y))) / (t(2) - t(1));
    check(slope(y3) > 2 * slope(y1), 'a 3 Hz filter gives edges more than twice as steep as 1 Hz');

    fprintf('%d checks, %d failures\n', checks, failures);
    if failures > 0
        error('run_tests:failed', '%d of %d checks failed', failures, checks);
    end
end
