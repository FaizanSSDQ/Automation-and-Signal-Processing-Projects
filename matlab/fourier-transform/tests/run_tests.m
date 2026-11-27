function run_tests()
%RUN_TESTS  Checks for the Fourier transform exercises. MATLAB and GNU Octave.
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

    f = [-3.3 -1 -0.5 0 0.25 0.5 1 2 4.7];

    % ---- rectangular pulse of width tau, centred: X(f) = tau sinc(f tau) ------------------------
    for tau = [1 2]
        X = ctft(@(t) ones(size(t)), f, -tau / 2, tau / 2);
        check(max(abs(X - tau * sincn(f * tau))) < 1e-9, sprintf('rect, width %g: tau sinc(f tau)', tau));
    end
    X = ctft(@(t) ones(size(t)), [1 2 3], -0.5, 0.5);
    check(max(abs(X)) < 1e-9, 'width 1: zeros at 1, 2, 3 Hz');

    % ---- one-sided exponential e^{-at} u(t): X(f) = 1 / (a + j 2 pi f) -------------------------
    a = 1;
    X = ctft(@(t) exp(-a * t), f, 0, 40);                % e^{-40} is negligible
    Xa = 1 ./ (a + 1j * 2 * pi * f);
    check(max(abs(X - Xa)) < 1e-9, 'e^{-t}u(t) -> 1/(1 + j 2 pi f)');
    check(max(abs(abs(X) - 1 ./ sqrt(a^2 + (2 * pi * f).^2))) < 1e-9, '|X| = 1 / sqrt(a^2 + (2 pi f)^2)');
    check(max(abs(angle(X) + atan(2 * pi * f / a))) < 1e-9, 'phase = -atan(2 pi f / a)');

    % ---- properties ------------------------------------------------------------------------------
    Xs = ctft(@(t) ones(size(t)), f, 0.5, 1.5);           % the width-1 pulse delayed by 1 s
    X0 = ctft(@(t) ones(size(t)), f, -0.5, 0.5);
    check(max(abs(Xs - X0 .* exp(-1j * 2 * pi * f))) < 1e-9, 'time shift multiplies by exp(-j 2 pi f t0)');
    check(max(abs(abs(Xs) - abs(X0))) < 1e-9, 'a delay does not change |X|');
    x2 = @(t) exp(-abs(t));
    X2 = ctft(x2, f, -30, 0) + ctft(x2, f, 0, 30);       % split at the kink at t = 0
    check(max(abs(imag(X2))) < 1e-9, 'a real even signal has a real transform');
    check(max(abs(X2 - 2 ./ (1 + (2 * pi * f).^2))) < 1e-9, 'e^{-|t|} -> 2 / (1 + (2 pi f)^2)');
    % Parseval: energy in time = energy in frequency (e^{-t}u(t): 1/2)
    ff = linspace(-200, 200, 400001);
    Ef = sum(1 ./ (1 + (2 * pi * ff).^2)) * (ff(2) - ff(1));
    check(abs(Ef - 0.5) < 1e-3, sprintf('Parseval: integral |X|^2 df = 1/2 (%.4f)', Ef));

    fprintf('%d checks, %d failures\n', checks, failures);
    if failures > 0
        error('run_tests:failed', '%d of %d checks failed', failures, checks);
    end
end
