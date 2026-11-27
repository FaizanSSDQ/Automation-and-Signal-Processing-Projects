function run_tests()
%RUN_TESTS  Checks for the IMU orientation fusion project. Runs in MATLAB and GNU Octave.
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

    % ---- quaternion algebra -----------------------------------------------------------------
    q = quat('from_euler', 0.3, -0.2, 1.1);
    check(abs(norm(q) - 1) < 1e-12, 'Euler -> unit quaternion');
    [r, p, y] = quat('to_euler', q);
    check(max(abs([r p y] - [0.3 -0.2 1.1])) < 1e-12, 'Euler round trip');
    check(max(abs(quat('mul', q, quat('conj', q)) - [1 0 0 0])) < 1e-12, 'q * conj(q) = identity');
    v = quat('rotate', quat('from_euler', 0, 0, pi / 2), [1 0 0]);
    check(max(abs(v - [0 1 0])) < 1e-12, '90 deg yaw turns x into y');
    q = [1 0 0 0];
    for k = 1:100
        q = quat('integrate', q, [0 0 pi / 2], 0.01);       % 90 deg/s for 1 s
    end
    [~, ~, y] = quat('to_euler', q);
    check(abs(y - pi / 2) < 1e-9, 'integrating 90 deg/s for 1 s gives 90 deg');

    % ---- simulated IMU -----------------------------------------------------------------------
    fs = 100;
    sim = imu_synth(fs, 60);
    check(max(abs(mean(sim.acc(:, 1:100), 2) - [0; 0; 1])) < 0.01, 'flat and still: accelerometer reads +1 g on z');
    check(max(abs(mean(sim.gyro(:, 1:100), 2) - sim.bias(:))) < 0.003, 'still: gyroscope reads its bias');

    % ---- estimators ----------------------------------------------------------------------------
    e_gyro = tilt_error(fuse_orientation('gyro', sim.gyro, sim.acc, fs), sim.q_true);
    e_comp = tilt_error(fuse_orientation('complementary', sim.gyro, sim.acc, fs), sim.q_true);
    [qm, bias] = fuse_orientation('mahony', sim.gyro, sim.acc, fs);
    e_mah = tilt_error(qm, sim.q_true);
    e_mad = tilt_error(fuse_orientation('madgwick', sim.gyro, sim.acc, fs), sim.q_true);
    fprintf('  tilt RMS (deg): gyro %.2f  complementary %.2f  Mahony %.2f  Madgwick %.2f\n', ...
            e_gyro, e_comp, e_mah, e_mad);
    check(e_gyro > 10, 'gyro integration alone drifts (> 10 deg in 60 s)');
    check(e_comp < 4, 'complementary filter: tilt within 4 deg RMS');
    check(e_mah < 2, 'Mahony: tilt within 2 deg RMS');
    check(e_mad < 3, 'Madgwick: tilt within 3 deg RMS');
    check(max(abs(bias(1:2) - sim.bias(1:2))) < 0.006, sprintf('Mahony learns the x/y gyro bias (%.4f %.4f)', bias(1:2)));

    % ---- fall detection -----------------------------------------------------------------------
    fall = imu_synth(fs, 40, struct('fall_at', 20, 'seed', 2));
    ev = detect_fall(fall.acc, fall.gyro - fall.bias(:) * ones(1, size(fall.gyro, 2)), fs);
    check(numel(ev) == 1, sprintf('one fall detected (%d)', numel(ev)));
    if numel(ev) == 1
        check(abs(ev.impact - fall.fall_idx) <= 2, 'impact located within 20 ms');
    end
    walk = imu_synth(fs, 60, struct('seed', 3, 'lin_acc', 0.3));
    check(isempty(detect_fall(walk.acc, walk.gyro, fs)), 'no false alarm during movement');
    jump = walk;
    i0 = 20 * fs;
    jump.acc(:, i0:i0 + 34) = 0.02;                         % 0.35 s of free fall ...
    jump.acc(:, i0 + 35:i0 + 39) = [2; -1; 3] * ones(1, 5); % ... and a landing, but movement continues
    check(isempty(detect_fall(jump.acc, jump.gyro, fs)), 'a jump (no stillness afterwards) is not a fall');

    fprintf('%d checks, %d failures\n', checks, failures);
    if failures > 0
        error('run_tests:failed', '%d of %d checks failed', failures, checks);
    end
end
