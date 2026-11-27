function H = freq_resp(filt, f, fs)
%FREQ_RESP  Complex frequency response at frequencies f (Hz), no toolbox.
%   H = freq_resp(h, f, fs)        FIR taps h (row or column)
%   H = freq_resp(sos, f, fs)      second-order sections, L x 6
%   H = freq_resp({b, a}, f, fs)   transfer function, b and a in powers of z^-1
%
%   Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

    zi = exp(-1j * 2 * pi * f(:).' / fs);          % z^-1 on the unit circle
    if iscell(filt)
        H = poly_zinv(filt{1}, zi) ./ poly_zinv(filt{2}, zi);
    elseif size(filt, 2) == 6 && size(filt, 1) >= 1 && all(filt(:, 4) == 1)
        H = ones(size(zi));
        for k = 1:size(filt, 1)
            H = H .* poly_zinv(filt(k, 1:3), zi) ./ poly_zinv(filt(k, 4:6), zi);
        end
    else
        H = poly_zinv(filt, zi);
    end
    H = reshape(H, size(f));
end

function v = poly_zinv(c, zi)
    % c(1) + c(2) z^-1 + c(3) z^-2 + ...  (Horner's rule in z^-1)
    v = zeros(size(zi));
    for k = numel(c):-1:1
        v = v .* zi + c(k);
    end
end
