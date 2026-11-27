function sos = iir_butter(order, fc, fs, type)
%IIR_BUTTER  Butterworth IIR filter as second-order sections (no toolbox).
%   sos = iir_butter(order, fc, fs)           low-pass
%   sos = iir_butter(order, fc, fs, 'high')   high-pass
%
%   Steps, all written out:
%     1. Analog Butterworth poles on the unit circle (left half-plane).
%     2. Pre-warp the cut-off, Wc = 2 fs tan(pi fc / fs), so that the
%        digital filter is exactly -3 dB at fc.
%     3. Bilinear transform each pole, z = (1 + s/(2fs)) / (1 - s/(2fs)).
%     4. Pair conjugate poles into biquads; zeros go to z = -1 (low-pass)
%        or z = +1 (high-pass); scale each section to unity gain in the
%        pass band.
%
%   sos is an L x 6 matrix, one section per row: [b0 b1 b2 1 a1 a2], with
%   H(z) = (b0 + b1 z^-1 + b2 z^-2) / (1 + a1 z^-1 + a2 z^-2).
%   Odd orders end with a first-order section (b2 = a2 = 0).
%
%   Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

    if nargin < 4
        type = 'low';
    end
    high = strcmpi(type, 'high');
    Wc = 2 * fs * tan(pi * fc / fs);                       % pre-warped, rad/s
    k = 1:order;
    p_analog = Wc * exp(1j * pi * (2 * k + order - 1) / (2 * order));
    if high
        p_analog = Wc^2 ./ p_analog;                        % low-pass -> high-pass
    end
    p = (1 + p_analog / (2 * fs)) ./ (1 - p_analog / (2 * fs));

    upper = p(imag(p) > 1e-12);                             % one of each conjugate pair
    sos = zeros(0, 6);
    zero_at = 1 - 2 * ~high;                                % -1 (low) or +1 (high)
    for i = 1:numel(upper)
        a = real(poly([upper(i), conj(upper(i))]));
        b = real(poly([zero_at, zero_at]));
        sos(end + 1, :) = [b * section_gain(b, a, high), a]; %#ok<AGROW>
    end
    if mod(order, 2) == 1
        pr = real(p(abs(imag(p)) <= 1e-12));
        a = [1, -pr, 0];
        b = [1, -zero_at, 0];
        sos(end + 1, :) = [b * section_gain(b, a, high), a];
    end
end

function g = section_gain(b, a, high)
    z = 1 - 2 * high;                                       % evaluate at DC or Nyquist
    g = abs(polyval(fliplr(a), 1 / z) / polyval(fliplr(b), 1 / z));
    if ~isfinite(g)
        g = 1;
    end
end
