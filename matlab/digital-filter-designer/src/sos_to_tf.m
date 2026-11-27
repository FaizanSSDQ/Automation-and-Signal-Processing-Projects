function [b, a] = sos_to_tf(sos)
%SOS_TO_TF  Multiply second-order sections out into one transfer function.
%   [b, a] = sos_to_tf(sos)   coefficients in powers of z^-1
%
%   Mathematically identical, numerically fragile: a high-order direct-form
%   filter is far more sensitive to coefficient rounding than the cascade
%   (see coeff_sensitivity in the examples).
%
%   Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

    b = 1;
    a = 1;
    for k = 1:size(sos, 1)
        b = conv(b, sos(k, 1:3));
        a = conv(a, sos(k, 4:6));
    end
end
