function [hr_bpm, t_hr, peaks] = hr_peaks(x, fs)
%HR_PEAKS  Heart rate from beat-to-beat intervals of a band-passed PPG.
%   [hr_bpm, t_hr, peaks] = hr_peaks(x, fs)
%
%   A sample is a beat when it is a local maximum, lies above an adaptive
%   threshold (half the running peak amplitude, which follows the signal as
%   contact changes) and comes at least 0.3 s after the previous beat
%   (no heart beats faster than 200 bpm). Each interval gives one heart-rate
%   value at the time of the second beat. Intervals outside 30 to 220 bpm
%   are dropped as artefacts.
%
%   Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

    x = x(:).';
    refractory = round(0.3 * fs);
    win = round(2 * fs);
    peaks = [];
    last = -Inf;
    for i = 2:numel(x) - 1
        if x(i) > x(i - 1) && x(i) >= x(i + 1) && i - last >= refractory
            lo = max(1, i - win);
            thr = 0.5 * max(x(lo:i));
            if x(i) >= thr && x(i) > 0
                peaks(end + 1) = i; %#ok<AGROW>
                last = i;
            end
        end
    end
    ibi = diff(peaks) / fs;
    hr_bpm = 60 ./ ibi;
    t_hr = peaks(2:end) / fs;
    ok = hr_bpm >= 30 & hr_bpm <= 220;
    hr_bpm = hr_bpm(ok);
    t_hr = t_hr(ok);
end
