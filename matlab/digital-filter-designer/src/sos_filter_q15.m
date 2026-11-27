function [y, sat] = sos_filter_q15(sos, x)
%SOS_FILTER_Q15  Bit-exact model of a Q15 biquad cascade, as it would run on an MCU.
%   [y, sat] = sos_filter_q15(sos, x)
%
%   x and y are int16-range integers (Q15 samples, -32768 .. 32767).
%   Coefficients are stored as Q14 (int16, range -2 .. 2), because a1 of a
%   low-frequency section is close to -2. Each section is direct form I:
%       acc = b0 x[n] + b1 x[n-1] + b2 x[n-2] - a1 y[n-1] - a2 y[n-2]    (64-bit)
%       y   = saturate16( acc >> 14 )        arithmetic shift: rounds toward -inf
%   which is what CMSIS-DSP's arm_biquad_cascade_df1_q15 computes with a
%   postShift of 1. sat counts samples clipped to the int16 range.
%
%   Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

    q = quantize_q14(sos);
    y = double(x(:));
    sat = 0;
    for k = 1:size(q, 1)
        b = q(k, 1:3);
        a = q(k, 4:6);
        x1 = 0; x2 = 0; y1 = 0; y2 = 0;
        out = zeros(size(y));
        for i = 1:numel(y)
            acc = b(1) * y(i) + b(2) * x1 + b(3) * x2 - a(2) * y1 - a(3) * y2;
            v = floor(acc / 2^14);                     % arithmetic shift right by 14
            if v > 32767
                v = 32767; sat = sat + 1;
            elseif v < -32768
                v = -32768; sat = sat + 1;
            end
            x2 = x1; x1 = y(i);
            y2 = y1; y1 = v;
            out(i) = v;
        end
        y = out;
    end
    y = reshape(y, size(x));
end
