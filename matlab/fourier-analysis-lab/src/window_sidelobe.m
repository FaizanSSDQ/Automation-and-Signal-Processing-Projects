function [psl_db, mainlobe_bins] = window_sidelobe(w)
%WINDOW_SIDELOBE  Peak side-lobe level (dB) and main-lobe half width (bins) of a window.
%   [psl_db, mainlobe_bins] = window_sidelobe(w)
%
%   The window's spectrum is computed with heavy zero padding; the main lobe
%   ends at the first minimum, and the highest point beyond it is the peak
%   side lobe, relative to the main-lobe peak.
%
%   Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

    L = numel(w);
    pad = 64;
    W = abs(fft(w(:), pad * L));
    W = W(1:pad * L / 2) / W(1);
    d = diff(W);
    first_min = find(d(1:end-1) < 0 & d(2:end) >= 0, 1) + 1;
    psl_db = 20 * log10(max(W(first_min:end)));
    mainlobe_bins = (first_min - 1) / pad;
end
