function run_tests()
%RUN_TESTS  Checks for the elementary-signals functions. MATLAB and GNU Octave.
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

    % ---- elementary signals -----------------------------------------------------------------
    check(isequal(unit_step([-1 0 2]), [0 1 1]), 'u(t) is 0 before 0, 1 from 0');
    check(isequal(unit_impulse(-2:2), [0 0 1 0 0]), 'delta[n] is a single 1 at n = 0');
    check(isequal(ramp([-2 0 3]), [0 0 3]), 'r(t) = t u(t)');
    t = 0:0.5:60;
    w = unit_step(t - 20) - unit_step(t - 40);
    check(all(w(t >= 20 & t < 40) == 1) && all(w(t < 20 | t >= 40) == 0), 'u(t-20) - u(t-40) is a window on [20, 40)');
    n = -5:5;
    x = 3 * unit_impulse(n - 2) - unit_impulse(n + 1);
    check(x(n == 2) == 3 && x(n == -1) == -1 && sum(abs(x)) == 4, 'signals as sums of shifted impulses');
    dt = 1e-3;
    tr = 0:dt:5;
    check(abs(sum(unit_step(tr)) * dt - ramp(5)) < 2e-3, 'the ramp is the integral of the step');

    % ---- sinusoids: a phase shift in degrees ----------------------------------------------------
    t = linspace(0, 2, 1001);
    s = sin(2 * pi * t + pi / 2);
    check(max(abs(s - cos(2 * pi * t))) < 1e-12, 'cos(wt) = sin(wt + 90 deg)');

    % ---- energy and power ------------------------------------------------------------------------
    dt = 1e-4;
    t = 0:dt:40;
    [E, P] = energy_power(exp(-t), dt);
    check(abs(E - 0.5) < 1e-3, sprintf('energy of e^{-t}u(t) is 1/2 (%.4f)', E));
    check(P < 0.02, 'an energy signal has (almost) no average power');
    [~, P] = energy_power(sin(2 * pi * t), dt);
    check(abs(P - 0.5) < 1e-3, sprintf('power of a unit sinusoid is 1/2 (%.4f)', P));

    % The course example: x(t) = exp(-|t|/2) sin(2 pi t) [u(t) - u(t-4)], repeated every 6 s.
    dt = 0.002;
    t1 = 0:dt:6 - dt;
    x1 = exp(-abs(t1 / 2)) .* sin(2 * pi * t1) .* (unit_step(t1) - unit_step(t1 - 4));
    [E1, ~] = energy_power(x1, dt);
    % reference: the same integral, integral_0^4 e^{-t} sin^2(2 pi t) dt, on a 2000x finer grid
    f = @(t) exp(-t) .* sin(2 * pi * t).^2;
    E_ref = sum(f(0:1e-6:4 - 1e-6)) * 1e-6;
    check(abs(E1 - E_ref) < 1e-4, sprintf('pulse energy %.4f (reference %.4f)', E1, E_ref));
    [tp, xp] = periodic_extension(x1, dt, 6);
    [~, Pp] = energy_power(xp, dt);
    check(abs(Pp - E1 / 6) < 1e-12, 'periodic signal: power = energy of one period / period');
    check(abs(tp(1) + 18) < 1e-12 && numel(xp) == 6 * numel(x1), 'extension covers -18 s .. +18 s');

    fprintf('%d checks, %d failures\n', checks, failures);
    if failures > 0
        error('run_tests:failed', '%d of %d checks failed', failures, checks);
    end
end
