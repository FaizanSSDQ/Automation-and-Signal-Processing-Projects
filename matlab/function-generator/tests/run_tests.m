function run_tests()
%RUN_TESTS  Checks for the waveform generator functions. MATLAB and GNU Octave.
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

    % ---- pulses ---------------------------------------------------------------------------------
    check(isequal(pulse_shapes('rect', [-1 -0.5 0 0.5 1]), [0 0.5 1 0.5 0]), 'rect: unit width, midpoint edges');
    check(isequal(pulse_shapes('tri', [-1 -0.5 0 0.5 2]), [0 0.5 1 0.5 0]), 'tri: unit half-width');
    t = linspace(-10, 10, 20001);
    check(abs(sum(pulse_shapes('rect', t / 4)) * (t(2) - t(1)) - 4) < 2e-3, 'rect(t/4) is 4 s wide');
    check(abs(sum(pulse_shapes('gauss', t)) * (t(2) - t(1)) - 1) < 1e-9, 'exp(-pi t^2) has unit area');

    % ---- periodic waveforms -----------------------------------------------------------------------
    T = 0.01;
    t = (0:9999) / 1e5;                                     % 100 kHz, exactly 10 periods of 100 Hz
    for kind = {'square', 'pulse', 'triangle', 'sawtooth', 'sine'}
        y = periodic_wave(kind{1}, t, T, 0.25);
        check(max(abs(y(1:1000) - y(1001:2000))) < 1e-12, ['periodic: ' kind{1}]);
    end
    check(abs(mean(periodic_wave('pulse', t, T, 0.25)) - 0.25) < 1e-12, 'pulse duty cycle 25 %');
    check(abs(mean(periodic_wave('square', t, T))) < 1e-12, 'square wave has no DC');
    check(abs(max(periodic_wave('triangle', t, T)) - 1) < 1e-9, 'triangle peaks at 1');

    % ---- harmonic content (what an oscilloscope's FFT shows) ----------------------------------------
    fs = 1e5; f0 = 100; K = 9;
    [A, ~] = harmonics(periodic_wave('square', t, T), fs, f0, K);
    k = 1:K;
    ref = 4 ./ (pi * k) .* mod(k, 2);
    check(max(abs(A - ref)) < 0.01, 'square: odd harmonics 4/(pi k)');
    [A, ~] = harmonics(periodic_wave('triangle', t, T), fs, f0, K);
    ref = 8 ./ (pi^2 * k.^2) .* mod(k, 2);
    check(max(abs(A - ref)) < 1e-3, 'triangle: odd harmonics 8/(pi^2 k^2)');
    [A, ~] = harmonics(periodic_wave('sawtooth', t, T), fs, f0, K);
    check(max(abs(A - 2 ./ (pi * k))) < 0.01, 'sawtooth: all harmonics 2/(pi k)');
    [A, ~] = harmonics(periodic_wave('pulse', t, T, 0.25), fs, f0, 8);
    check(A(4) < 1e-3 && A(8) < 1e-3 && A(1) > 0.4, '25 % pulse: every 4th harmonic missing');
    [A, ~] = harmonics(periodic_wave('sine', t, T), fs, f0, 5);
    check(abs(A(1) - 1) < 1e-9 && max(A(2:5)) < 1e-9, 'a sine has one harmonic only');

    fprintf('%d checks, %d failures\n', checks, failures);
    if failures > 0
        error('run_tests:failed', '%d of %d checks failed', failures, checks);
    end
end
