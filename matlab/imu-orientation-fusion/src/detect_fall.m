function ev = detect_fall(acc, gyro, fs, cfg)
%DETECT_FALL  Free fall -> impact -> lying still: the classic wearable fall pattern.
%   ev = detect_fall(acc, gyro, fs)
%   ev = detect_fall(acc, gyro, fs, cfg)
%
%   cfg (defaults in brackets):
%     .ff_g     free fall when |a| < this [0.35 g] ...
%     .ff_ms    ... for at least this long [80 ms]
%     .imp_g    impact when |a| > this [2.5 g] ...
%     .imp_win  ... within this time after the free fall [1.0 s]
%     .still_s  then the wearer must lie still for this long [2.0 s]:
%               |a| within 0.15 g of 1 g and |w| < 0.3 rad/s
%
%   The three stages are what separates a fall from a jump (no stillness
%   afterwards) or a hand slapped on a table (no free fall before).
%   ev is a struct array with fields free_fall, impact, confirmed (sample
%   indices), empty when nothing was detected.
%
%   Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

    if nargin < 4
        cfg = struct();
    end
    ff_g = get(cfg, 'ff_g', 0.35);
    ff_n = round(get(cfg, 'ff_ms', 80) / 1000 * fs);
    imp_g = get(cfg, 'imp_g', 2.5);
    imp_n = round(get(cfg, 'imp_win', 1.0) * fs);
    still_n = round(get(cfg, 'still_s', 2.0) * fs);

    amag = sqrt(sum(acc.^2, 1));
    wmag = sqrt(sum(gyro.^2, 1));
    n = numel(amag);
    ev = struct('free_fall', {}, 'impact', {}, 'confirmed', {});
    run = 0;
    k = 1;
    while k <= n
        if amag(k) < ff_g
            run = run + 1;
        else
            if run >= ff_n
                ff_start = k - run;
                hit = find(amag(k:min(n, k + imp_n)) > imp_g, 1);
                if ~isempty(hit)
                    imp = k + hit - 1;
                    s0 = imp + round(0.5 * fs);                 % let the impact ring down
                    s1 = s0 + still_n - 1;
                    if s1 <= n && all(abs(amag(s0:s1) - 1) < 0.15) && all(wmag(s0:s1) < 0.3)
                        ev(end + 1) = struct('free_fall', ff_start, 'impact', imp, 'confirmed', s1); %#ok<AGROW>
                        k = s1;
                    end
                end
            end
            run = 0;
        end
        k = k + 1;
    end
end

function v = get(s, name, default)
    if isfield(s, name)
        v = s.(name);
    else
        v = default;
    end
end
