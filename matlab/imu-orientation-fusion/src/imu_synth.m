function sim = imu_synth(fs, duration, opts)
%IMU_SYNTH  Wrist-like motion with exact ground truth and a realistic 6-axis IMU.
%   sim = imu_synth(fs, duration)
%   sim = imu_synth(fs, duration, opts)
%
%   opts fields (all optional):
%     .gyro_bias   rad/s, 1x3 (default [0.02 -0.015 0.01], about 1 deg/s)
%     .gyro_noise  rad/s per sample (default 0.005)
%     .acc_noise   g per sample (default 0.01)
%     .lin_acc     g, size of linear accelerations from arm movement (default 0.15)
%     .fall_at     seconds: insert a fall (free fall, impact, lying still); [] = none
%     .seed
%
%   The true body rate is a mix of slow sinusoids on all three axes, with a
%   still period at the start for initialisation. The true orientation is
%   integrated exactly from it; the gyro adds bias and noise, and the
%   accelerometer measures gravity rotated into the body frame plus linear
%   acceleration and noise (in g, z up when the wrist is flat).
%
%   sim fields: t, gyro (3 x n, rad/s), acc (3 x n, g), q_true (n x 4),
%   euler_true (3 x n, rad), fall_idx (sample of impact or []).
%
%   Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

    if nargin < 3
        opts = struct();
    end
    bias = get(opts, 'gyro_bias', [0.02 -0.015 0.01]);
    gn = get(opts, 'gyro_noise', 0.005);
    an = get(opts, 'acc_noise', 0.01);
    la = get(opts, 'lin_acc', 0.15);
    fall_at = get(opts, 'fall_at', []);
    randn('state', get(opts, 'seed', 1)); %#ok<RAND>

    n = round(duration * fs);
    dt = 1 / fs;
    t = (0:n-1) * dt;
    move = min(1, max(0, (t - 2) / 2));          % still for 2 s, then ramp in
    w = [0.9 * sin(2 * pi * 0.31 * t) + 0.5 * sin(2 * pi * 0.83 * t);
         0.7 * sin(2 * pi * 0.23 * t + 1) + 0.4 * sin(2 * pi * 1.1 * t);
         0.6 * sin(2 * pi * 0.17 * t + 2)] .* [move; move; move];
    lin = la * [sin(2 * pi * 1.3 * t); sin(2 * pi * 0.9 * t + 0.5); 0.5 * sin(2 * pi * 1.7 * t)] .* [move; move; move];

    fall_idx = [];
    if ~isempty(fall_at)
        i0 = round(fall_at * fs);
        nf = round(0.35 * fs);                   % 0.35 s of free fall (about 60 cm)
        ni = round(0.05 * fs);                   % 50 ms impact
        w(:, i0:end) = 0;                        % afterwards: lying still
        lin(:, i0:end) = 0;
        fall_idx = i0 + nf;
    end

    q = zeros(n, 4);
    q(1, :) = [1 0 0 0];
    acc = zeros(3, n);
    eul = zeros(3, n);
    for k = 1:n
        if k > 1
            q(k, :) = quat('integrate', q(k - 1, :), w(:, k - 1), dt);
        end
        g_body = quat('rotate', quat('conj', q(k, :)), [0 0 1]);   % gravity seen by the sensor
        a = g_body(:) + lin(:, k);
        if ~isempty(fall_idx)
            if k >= fall_idx - nf && k < fall_idx
                a = [0; 0; 0] + 0.02 * [1; -1; 0.5];   % free fall: almost nothing measured
            elseif k >= fall_idx && k < fall_idx + ni
                a = g_body(:) + [1.5; -0.8; 3.2];      % impact spike, about 4 g
            end
        end
        acc(:, k) = a;
        [r, p, y] = quat('to_euler', q(k, :));
        eul(:, k) = [r; p; y];
    end
    gyro = w + bias(:) * ones(1, n) + gn * randn(3, n);
    acc = acc + an * randn(3, n);
    sim = struct('t', t, 'gyro', gyro, 'acc', acc, 'q_true', q, 'euler_true', eul, ...
                 'fall_idx', fall_idx, 'fs', fs, 'bias', bias);
end

function v = get(s, name, default)
    if isfield(s, name)
        v = s.(name);
    else
        v = default;
    end
end
