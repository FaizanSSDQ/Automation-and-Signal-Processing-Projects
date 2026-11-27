function y = pulse_shapes(kind, t)
%PULSE_SHAPES  Single unit pulses used as building blocks.
%   y = pulse_shapes('rect', t)      1 for |t| < 1/2, 1/2 at the edges, 0 elsewhere
%   y = pulse_shapes('tri', t)       1 - |t| for |t| < 1, 0 elsewhere
%   y = pulse_shapes('gauss', t)     exp(-pi t^2), the pulse that is its own transform
%
%   Scaling the argument scales the width: pulse_shapes('rect', t / 4) is
%   4 s wide. Replaces rectpuls(), which needs the Signal Processing Toolbox.
%
%   Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

    switch lower(kind)
        case 'rect'
            y = double(abs(t) < 0.5) + 0.5 * double(abs(t) == 0.5);
        case 'tri'
            y = max(0, 1 - abs(t));
        case 'gauss'
            y = exp(-pi * t.^2);
        otherwise
            error('pulse_shapes:kind', 'unknown pulse "%s"', kind);
    end
end
