function q = quantize_q14(sos)
%QUANTIZE_Q14  Round biquad coefficients to Q14 integers (int16, range -2 .. 2).
%   q = quantize_q14(sos)   integer matrix, same shape; q / 2^14 is the stored value
%
%   Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

    q = round(sos * 2^14);
    if any(abs(q(:)) > 32767)
        error('quantize_q14:range', 'a coefficient is outside the Q14 range [-2, 2)');
    end
end
