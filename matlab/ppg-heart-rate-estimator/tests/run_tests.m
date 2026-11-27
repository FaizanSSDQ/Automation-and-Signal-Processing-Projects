function run_tests()
%RUN_TESTS  Checks for the PPG heart-rate estimator. Runs in MATLAB and GNU Octave.
%   From the project folder:  addpath tests; run_tests
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

    fs = 50;

    % ---- synthesis ---------------------------------------------------------------------------
    sim = ppg_synth(fs, 60, 72);
    check(numel(sim.ppg) == 60 * fs && size(sim.acc, 1) == 3, 'signal sizes');
    check(abs(numel(sim.beats) - 72) <= 2, sprintf('72 bpm gives ~72 beats per minute (%d)', numel(sim.beats)));
    check(max(abs(sim.acc(1, :))) < 0.1, 'at rest the accelerometer is still');

    % ---- band-pass removes breathing wander --------------------------------------------------
    y = bandpass_fir(sim.ppg, fs, 0.5, 4);
    X = abs(fft(y)); fr = (0:numel(y) - 1) * fs / numel(y);
    check(max(X(fr > 0.2 & fr < 0.3)) < 0.1 * max(X(fr > 1 & fr < 1.4)), 'breathing (0.25 Hz) suppressed');
    check(numel(y) == numel(sim.ppg), 'zero-phase filter keeps the length');

    % ---- at rest: both estimators are accurate -----------------------------------------------
    [hp, tp] = hr_peaks(y, fs);
    check(hr_error(hp, tp, sim) < 2, sprintf('peaks at rest within 2 bpm (%.2f)', hr_error(hp, tp, sim)));
    [hs, ts] = hr_spectral(y, fs);
    check(hr_error(hs, ts, sim) < 2, sprintf('spectral at rest within 2 bpm (%.2f)', hr_error(hs, ts, sim)));

    % ---- a changing heart rate is followed ---------------------------------------------------
    sim = ppg_synth(fs, 90, [70 130], [], 2);
    [hs, ts] = hr_spectral(bandpass_fir(sim.ppg, fs, 0.5, 4), fs);
    check(hr_error(hs, ts, sim) < 3, sprintf('ramp 70 -> 130 bpm tracked within 3 bpm (%.2f)', hr_error(hs, ts, sim)));

    % ---- running: the artefact misleads, the adaptive filter restores ---------------------------
    motion = struct('cadence_hz', 2.6, 'gain', 4);
    sim = ppg_synth(fs, 120, 95, motion, 3);
    raw = bandpass_fir(sim.ppg, fs, 0.5, 4);
    [hs_raw, ts] = hr_spectral(raw, fs);
    clean = bandpass_fir(nlms_cancel(sim.ppg, sim.acc), fs, 0.5, 4);
    [hs_nlms, ts2] = hr_spectral(clean, fs);
    e_raw = hr_error(hs_raw, ts, sim);
    e_nlms = hr_error(hs_nlms, ts2, sim);
    check(e_raw > 10, sprintf('without cancellation the estimate is wrong (%.1f bpm)', e_raw));
    check(e_nlms < 3, sprintf('with NLMS within 3 bpm (%.2f)', e_nlms));

    % ---- NLMS on its own: learns a known filter --------------------------------------------------
    randn('state', 4); %#ok<RAND>
    u = randn(1, 5000);
    u = u - mean(u);                 % the canceller removes the mean of its references
    d = filter([0.5 -0.3 0.2], 1, u);
    [e, W] = nlms_cancel(d, u, 4, 0.5);
    check(max(abs(W(:).' - [0.5 -0.3 0.2 0])) < 1e-3, 'NLMS identifies a 3-tap filter');
    check(10 * log10(sum(d(4001:end).^2) / sum(e(4001:end).^2)) > 40, 'residual > 40 dB below the input');

    fprintf('%d checks, %d failures\n', checks, failures);
    if failures > 0
        error('run_tests:failed', '%d of %d checks failed', failures, checks);
    end
end
