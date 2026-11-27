function run_tests()
%RUN_TESTS  Checks for the Fourier series assignment. MATLAB and GNU Octave.
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

    check(isequal(tri_pulse([-1 -0.5 0 0.5 1 2]), [0 0.5 1 0.5 0 0]), 'triangular pulse values');
    check(isequal(rect_pulse([-1 -0.5 0 0.5 1]), [0 0.5 1 0.5 0]), 'rectangular pulse with midpoint edges');

    % ---- the assignment: triangle(t/2), one period on [-2, 2] -------------------------------
    % Closed form: D_0 = 1/2, D_n = 2 / (pi^2 n^2) for odd n, 0 for even n (and real: the pulse is even).
    f = @(t) tri_pulse(t / 2);
    [D, n] = fs_coefficients(f, -2, 2, 17);
    Dref = zeros(size(n));
    Dref(n == 0) = 0.5;
    odd = mod(n, 2) ~= 0;
    Dref(odd) = 2 ./ (pi^2 * n(odd).^2);
    check(max(abs(D - Dref)) < 1e-9, sprintf('D_n match the closed form (max error %.1e)', max(abs(D - Dref))));
    check(max(abs(imag(D))) < 1e-12, 'an even signal has real coefficients');
    check(max(abs(D - conj(fliplr(D)))) < 1e-12, 'D(-n) = conj(D(n))');

    t = linspace(-2, 2, 2001);
    err = zeros(1, 3);
    Ns = [7 11 17];
    for k = 1:3
        [Dk, ~] = fs_coefficients(f, -2, 2, Ns(k));
        err(k) = max(abs(fs_reconstruct(Dk, 4, t) - f(t)));
    end
    check(all(diff(err) < 0), sprintf('more harmonics, smaller error: %.4f %.4f %.4f', err));
    check(err(3) < 0.015, 'N = 17 reconstructs within 1.5 %');
    check(abs(fs_reconstruct(D, 4, 4) - fs_reconstruct(D, 4, 0)) < 1e-12, 'the reconstruction repeats every 4 s');

    % ---- a rectangular pulse train (period 4, pulse width 1) ----------------------------------
    [Dr, n] = fs_coefficients(@(t) rect_pulse(t), -2, 2, 12);
    Dref = 0.25 * ones(size(n));
    nz = n ~= 0;
    Dref(nz) = sin(pi * n(nz) / 4) ./ (pi * n(nz));     % (1/T) * width * sinc(n * width / T)
    check(max(abs(Dr - Dref)) < 1e-6, 'rectangular pulse train: D_n = (1/4) sinc(n/4)');
    check(max(abs(Dr(n == 4 | n == 8 | n == -4))) < 1e-6, 'every 4th harmonic vanishes (duty cycle 1/4)');

    check(~isempty(lasterror_safe(@() fs_coefficients(f, 2, -2, 3))), 'a negative period is rejected');

    fprintf('%d checks, %d failures\n', checks, failures);
    if failures > 0
        error('run_tests:failed', '%d of %d checks failed', failures, checks);
    end
end

function msg = lasterror_safe(fn)
    msg = '';
    try
        fn();
    catch err
        msg = err.message;
    end
end
