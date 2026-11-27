function [hr_bpm, t_hr, S, f] = hr_spectral(x, fs, win_s, hop_s)
%HR_SPECTRAL  Heart rate from the dominant frequency of sliding PPG windows, with tracking.
%   [hr_bpm, t_hr] = hr_spectral(x, fs)                 8-s windows every 2 s
%   [hr_bpm, t_hr, S, f] = hr_spectral(x, fs, win_s, hop_s)
%
%   Each window is Hann-weighted and zero-padded to a fine frequency grid.
%   The search covers 0.7 to 3.5 Hz (42 to 210 bpm). After the first window,
%   the search is limited to +-15 bpm around the previous estimate: a heart
%   rate cannot jump further in two seconds, but a motion artefact can,
%   so tracking stops the estimate from locking onto the arm swing.
%   Sub-harmonic check: if the strongest peak has a peak of at least 40 %
%   of its height at half its frequency, the pulse's second harmonic was
%   found, and the half frequency is the heart rate.
%   S (frequency x window) and f are returned for spectrogram plots.
%
%   Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

    if nargin < 3
        win_s = 8;
    end
    if nargin < 4
        hop_s = 2;
    end
    x = x(:).';
    L = round(win_s * fs);
    hop = round(hop_s * fs);
    nfft = 2^nextpow2(16 * L);
    w = 0.5 - 0.5 * cos(2 * pi * (0:L-1) / (L - 1));
    f = (0:nfft/2) * fs / nfft;
    band = f >= 0.7 & f <= 3.5;
    starts = 1:hop:(numel(x) - L + 1);
    hr_bpm = zeros(1, numel(starts));
    S = zeros(nnz(band), numel(starts));
    fb = f(band);
    prev = NaN;
    for k = 1:numel(starts)
        seg = x(starts(k):starts(k) + L - 1);
        X = abs(fft((seg - mean(seg)) .* w, nfft));
        X = X(1:nfft/2 + 1);
        Sb = X(band);
        S(:, k) = Sb.';
        search = true(size(fb));
        if ~isnan(prev)
            search = abs(fb * 60 - prev) <= 15;
        end
        cand = Sb;
        cand(~search) = 0;
        [~, i] = max(cand);
        fi = fb(i);
        if isnan(prev)
            j = find(abs(fb - fi / 2) <= 0.05);
            if ~isempty(j) && max(Sb(j)) >= 0.4 * Sb(i)
                [~, jj] = max(Sb(j));
                fi = fb(j(jj));
            end
        end
        hr_bpm(k) = fi * 60;
        prev = hr_bpm(k);
    end
    t_hr = (starts - 1 + L / 2) / fs;
    f = fb;
end
