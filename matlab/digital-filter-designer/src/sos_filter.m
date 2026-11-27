function y = sos_filter(sos, x)
%SOS_FILTER  Run a biquad cascade in double precision (direct form II transposed).
%   y = sos_filter(sos, x)
%
%   Each section, per sample:
%       y  = b0 x + s1
%       s1 = b1 x - a1 y + s2
%       s2 = b2 x - a2 y
%
%   Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

    y = x(:);
    for k = 1:size(sos, 1)
        b = sos(k, 1:3);
        a = sos(k, 4:6);
        s1 = 0;
        s2 = 0;
        out = zeros(size(y));
        for i = 1:numel(y)
            xi = y(i);
            yi = b(1) * xi + s1;
            s1 = b(2) * xi - a(2) * yi + s2;
            s2 = b(3) * xi - a(3) * yi;
            out(i) = yi;
        end
        y = out;
    end
    y = reshape(y, size(x));
end
