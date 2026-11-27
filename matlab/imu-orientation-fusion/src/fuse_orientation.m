function [q, bias_est] = fuse_orientation(method, gyro, acc, fs, params)
%FUSE_ORIENTATION  Orientation from a 6-axis IMU with one of four estimators.
%   q = fuse_orientation(method, gyro, acc, fs)
%   [q, bias_est] = fuse_orientation(method, gyro, acc, fs, params)
%
%   method
%     'gyro'           integrate the gyroscope only (drifts with its bias)
%     'complementary'  high-pass gyro + low-pass accelerometer tilt, per axis;
%                      params.alpha (default 0.98)
%     'mahony'         nonlinear complementary filter on the rotation group:
%                      the error between measured and predicted gravity drives
%                      a PI correction of the gyro rate; params.kp (0.5),
%                      params.ki (0.05). The integral term learns the bias.
%     'madgwick'       gradient-descent step towards the accelerometer's
%                      gravity, blended with the gyro; params.beta (0.08)
%
%   The default gains were chosen by sweeping them on the simulated wrist
%   motion (examples/make_figures.m, gain_tuning.png).
%
%   gyro in rad/s and acc in g, both 3 x n. q is n x 4 ([w x y z]).
%   Initial orientation: tilt from the first accelerometer sample, yaw 0.
%   Without a magnetometer, yaw is unobservable: every method drifts in yaw.
%
%   Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

    if nargin < 5
        params = struct();
    end
    n = size(gyro, 2);
    dt = 1 / fs;
    a0 = acc(:, 1) / norm(acc(:, 1));
    q0 = quat('from_euler', atan2(a0(2), a0(3)), atan2(-a0(1), hypot(a0(2), a0(3))), 0);
    q = zeros(n, 4);
    q(1, :) = q0;
    bias_est = [0 0 0];

    switch method
        case 'gyro'
            for k = 2:n
                q(k, :) = quat('integrate', q(k - 1, :), gyro(:, k - 1), dt);
            end

        case 'complementary'
            alpha = get(params, 'alpha', 0.98);
            [r, p, y] = quat('to_euler', q0);
            for k = 2:n
                % propagate Euler angles with the gyro (small-angle body-rate mapping)
                wx = gyro(1, k - 1); wy = gyro(2, k - 1); wz = gyro(3, k - 1);
                r_dot = wx + sin(r) * tan(p) * wy + cos(r) * tan(p) * wz;
                p_dot = cos(r) * wy - sin(r) * wz;
                y_dot = (sin(r) * wy + cos(r) * wz) / cos(p);
                r = r + r_dot * dt; p = p + p_dot * dt; y = y + y_dot * dt;
                a = acc(:, k);
                r_acc = atan2(a(2), a(3));
                p_acc = atan2(-a(1), hypot(a(2), a(3)));
                r = alpha * r + (1 - alpha) * (r + wrap(r_acc - r));
                p = alpha * p + (1 - alpha) * p_acc;
                q(k, :) = quat('from_euler', r, p, y);
            end

        case 'mahony'
            kp = get(params, 'kp', 0.5);
            ki = get(params, 'ki', 0.05);
            integral = [0 0 0];
            for k = 2:n
                qk = q(k - 1, :);
                a = acc(:, k - 1).';
                w = gyro(:, k - 1).';
                if norm(a) > 0.5 && norm(a) < 1.5          % trust gravity only near 1 g
                    a = a / norm(a);
                    v = quat('rotate', quat('conj', qk), [0 0 1]);   % predicted gravity, body frame
                    e = cross(a, v);
                    integral = integral + ki * e * dt;
                    w = w + kp * e + integral;
                end
                q(k, :) = quat('integrate', qk, w, dt);
            end
            bias_est = -integral;

        case 'madgwick'
            beta = get(params, 'beta', 0.08);
            for k = 2:n
                qk = q(k - 1, :);
                a = acc(:, k - 1).';
                w = gyro(:, k - 1).';
                qdot = 0.5 * quat('mul', qk, [0 w]);
                if norm(a) > 0.5 && norm(a) < 1.5
                    a = a / norm(a);
                    [qw, qx, qy, qz] = deal(qk(1), qk(2), qk(3), qk(4));
                    f = [2 * (qx * qz - qw * qy) - a(1);
                         2 * (qw * qx + qy * qz) - a(2);
                         2 * (0.5 - qx^2 - qy^2) - a(3)];
                    J = [-2 * qy,  2 * qz, -2 * qw, 2 * qx;
                          2 * qx,  2 * qw,  2 * qz, 2 * qy;
                          0,      -4 * qx, -4 * qy, 0];
                    step = (J.' * f).';
                    if norm(step) > 0
                        qdot = qdot - beta * step / norm(step);
                    end
                end
                q(k, :) = quat('normalize', qk + qdot * dt);
            end

        otherwise
            error('fuse_orientation:method', 'unknown method "%s"', method);
    end
end

function v = get(s, name, default)
    if isfield(s, name)
        v = s.(name);
    else
        v = default;
    end
end

function a = wrap(a)
    a = mod(a + pi, 2 * pi) - pi;
end
