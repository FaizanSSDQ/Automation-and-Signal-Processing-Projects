function [rms_deg, err_deg] = tilt_error(q_est, q_true)
%TILT_ERROR  Angle between estimated and true gravity direction (degrees).
%   [rms_deg, err_deg] = tilt_error(q_est, q_true)
%
%   Tilt (roll and pitch together) is what an accelerometer-aided filter can
%   observe; comparing the gravity vectors avoids Euler-angle wrap-around
%   and is independent of yaw.
%
%   Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

    n = size(q_est, 1);
    err_deg = zeros(1, n);
    for k = 1:n
        ge = quat('rotate', quat('conj', q_est(k, :)), [0 0 1]);
        gt = quat('rotate', quat('conj', q_true(k, :)), [0 0 1]);
        err_deg(k) = acosd(max(-1, min(1, dot(ge, gt))));
    end
    rms_deg = sqrt(mean(err_deg.^2));
end
