function varargout = quat(op, varargin)
%QUAT  Unit-quaternion helpers, q = [w x y z] (scalar first), body-to-world rotation.
%   q = quat('mul', p, q)            Hamilton product p * q
%   q = quat('conj', q)              inverse of a unit quaternion
%   v = quat('rotate', q, v)         rotate 3-vector v from body to world frame
%   q = quat('from_euler', r, p, y)  ZYX Euler angles (rad) -> quaternion
%   [r, p, y] = quat('to_euler', q)  quaternion -> ZYX Euler angles (rad)
%   q = quat('integrate', q, w, dt)  advance by body rate w (rad/s) for dt seconds
%   q = quat('normalize', q)
%
%   One file keeps the conventions in one place. Base MATLAB only.
%
%   Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

    switch op
        case 'mul'
            varargout{1} = mul(varargin{1}, varargin{2});
        case 'conj'
            q = varargin{1};
            varargout{1} = [q(1), -q(2:4)];
        case 'rotate'
            q = varargin{1};
            v = varargin{2};
            r = mul(mul(q, [0, v(:).']), [q(1), -q(2:4)]);
            varargout{1} = r(2:4);
        case 'from_euler'
            [r, p, y] = deal(varargin{:});
            cr = cos(r / 2); sr = sin(r / 2);
            cp = cos(p / 2); sp = sin(p / 2);
            cy = cos(y / 2); sy = sin(y / 2);
            varargout{1} = [cr*cp*cy + sr*sp*sy, sr*cp*cy - cr*sp*sy, ...
                            cr*sp*cy + sr*cp*sy, cr*cp*sy - sr*sp*cy];
        case 'to_euler'
            q = varargin{1};
            [w, x, y, z] = deal(q(1), q(2), q(3), q(4));
            roll = atan2(2 * (w*x + y*z), 1 - 2 * (x^2 + y^2));
            pitch = asin(max(-1, min(1, 2 * (w*y - z*x))));
            yaw = atan2(2 * (w*z + x*y), 1 - 2 * (y^2 + z^2));
            varargout = {roll, pitch, yaw};
        case 'integrate'
            [q, w, dt] = deal(varargin{:});
            angle = norm(w) * dt;
            if angle < 1e-12
                dq = [1 0 0 0];
            else
                ax = w(:).' / norm(w);
                dq = [cos(angle / 2), sin(angle / 2) * ax];
            end
            varargout{1} = normalize_q(mul(q, dq));
        case 'normalize'
            varargout{1} = normalize_q(varargin{1});
        otherwise
            error('quat:op', 'unknown operation "%s"', op);
    end
end

function r = mul(p, q)
    r = [p(1)*q(1) - p(2)*q(2) - p(3)*q(3) - p(4)*q(4), ...
         p(1)*q(2) + p(2)*q(1) + p(3)*q(4) - p(4)*q(3), ...
         p(1)*q(3) - p(2)*q(4) + p(3)*q(1) + p(4)*q(2), ...
         p(1)*q(4) + p(2)*q(3) - p(3)*q(2) + p(4)*q(1)];
end

function q = normalize_q(q)
    q = q / norm(q);
    if q(1) < 0
        q = -q;          % keep one of the two equivalent signs
    end
end
