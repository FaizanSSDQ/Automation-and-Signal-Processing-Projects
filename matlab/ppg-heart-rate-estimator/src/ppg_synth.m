function sim = ppg_synth(fs, duration, hr_bpm, motion, seed)
%PPG_SYNTH  Synthetic wrist PPG with breathing, motion artefacts and a matching accelerometer.
%   sim = ppg_synth(fs, duration, hr_bpm)                       at rest
%   sim = ppg_synth(fs, duration, hr_bpm, motion)                with motion
%   sim = ppg_synth(fs, duration, hr_bpm, motion, seed)
%
%   hr_bpm   scalar, or [start end] for a linear ramp (exercise / recovery)
%   motion   struct with fields (all optional):
%              .cadence_hz  arm-swing frequency while walking or running (default 1.6)
%              .start, .stop  seconds of movement (default: the whole record)
%              .gain        artefact size relative to the pulse (default 3)
%
%   The pulse is a systolic wave plus a smaller dicrotic wave each beat,
%   shaped so the fundamental dominates the spectrum, as in wrist PPG.
%   Movement makes the sensor shift on the skin: the artefact is the
%   accelerometer signal passed through an unknown short filter, which is
%   exactly the situation an adaptive filter can undo. All signals are
%   synthetic; nothing here is measured data.
%
%   sim fields: t, ppg, clean, acc (3 x n, in g), hr_true (bpm per sample),
%   beats (sample index of each systolic peak), fs.
%
%   Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

    if nargin < 4 || isempty(motion)
        motion = struct('gain', 0);
    end
    if nargin < 5
        seed = 1;
    end
    randn('state', seed); %#ok<RAND>
    n = round(duration * fs);
    t = (0:n-1) / fs;
    if isscalar(hr_bpm)
        hr_bpm = [hr_bpm hr_bpm];
    end
    hr = hr_bpm(1) + (hr_bpm(2) - hr_bpm(1)) * t / duration;
    hr = hr .* (1 + 0.02 * sin(2 * pi * 0.25 * t));          % breathing modulates the rate slightly
    phase = cumsum(hr / 60) / fs;                            % beats elapsed

    % one beat: systolic wave at 0.25 of the cycle, smaller dicrotic wave at 0.6
    u = mod(phase, 1);
    clean = exp(-((u - 0.25) / 0.14).^2) + 0.3 * exp(-((u - 0.6) / 0.12).^2);
    clean = clean - mean(clean);
    beats = find(diff(floor(phase - 0.25)) > 0) + 1;

    baseline = 0.3 * sin(2 * pi * 0.25 * t);                 % respiration wander
    cadence = field_or(motion, 'cadence_hz', 1.6);
    on = true(1, n);
    if isfield(motion, 'start')
        on = t >= motion.start & t < field_or(motion, 'stop', duration);
    end
    gain = field_or(motion, 'gain', 3);
    env = conv(double(on), ones(1, round(fs)) / round(fs), 'same');   % 1-s fade in and out
    env = env * (gain > 0);                                  % at rest the wrist is still
    acc = zeros(3, n);
    acc(1, :) = env .* (0.6 * sin(2 * pi * cadence * t) + 0.25 * sin(2 * pi * 2 * cadence * t + 0.7));
    acc(2, :) = env .* (0.4 * sin(2 * pi * cadence * t + 1.1) + 0.15 * sin(2 * pi * 3 * cadence * t));
    acc(3, :) = 1 + env .* 0.3 .* sin(2 * pi * 2 * cadence * t + 0.3);
    acc = acc + 0.01 * randn(3, n);

    % artefact: each axis through a short unknown filter, summed
    hx = [0.6 0.3 -0.2 0.1];
    hy = [0.4 -0.25 0.15];
    hz = [0.3 0.2];
    art = filter(hx, 1, acc(1, :)) + filter(hy, 1, acc(2, :)) + filter(hz, 1, acc(3, :) - 1);
    art = gain * art / max(std(art), eps) * std(clean) * (gain > 0);

    ppg = clean + baseline + art + 0.03 * randn(1, n);
    sim = struct('t', t, 'ppg', ppg, 'clean', clean, 'acc', acc, 'hr_true', hr, ...
                 'beats', beats, 'fs', fs);
end

function v = field_or(s, name, default)
    if isfield(s, name)
        v = s.(name);
    else
        v = default;
    end
end
