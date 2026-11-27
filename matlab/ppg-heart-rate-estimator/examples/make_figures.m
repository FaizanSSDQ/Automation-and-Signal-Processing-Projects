function make_figures()
%MAKE_FIGURES  Regenerate the README figures. Runs in MATLAB and GNU Octave.
%   From the project folder:  addpath examples; make_figures
%   All signals are synthetic (ppg_synth), not measured data.
%
%   Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

    here = fileparts(mfilename('fullpath'));
    addpath(fullfile(here, '..', 'src'));
    out = fullfile(here, '..', 'docs');
    set(0, 'defaultaxesfontsize', 13, 'defaulttextfontsize', 13, 'defaultlinelinewidth', 1.3);
    blue = [0.17 0.42 0.69]; orange = [0.87 0.42 0.13]; grey = [0.45 0.47 0.5]; green = [0.18 0.52 0.35];
    red = [0.75 0.15 0.15];
    fs = 50;

    % A 3-minute session that starts running at once (0 to 150 s, heart rate climbing), then cools down.
    % Starting in motion is the hard case: the tracker has no clean pulse to lock on to first.
    motion = struct('cadence_hz', 2.6, 'gain', 4, 'start', 0, 'stop', 150);
    sim = ppg_synth(fs, 180, [75 135], motion, 11);
    raw = bandpass_fir(sim.ppg, fs, 0.5, 4);
    cleaned = bandpass_fir(nlms_cancel(sim.ppg, sim.acc), fs, 0.5, 4);
    [hr_raw, t_raw, S_raw, f] = hr_spectral(raw, fs);
    [hr_cln, t_cln, S_cln] = hr_spectral(cleaned, fs);
    [hp, tp] = hr_peaks(cleaned, fs);

    % ---- 1. signals ---------------------------------------------------------------------------
    fig = figure('Visible', vis(), 'Position', [60 60 1300 676]);
    seg = sim.t >= 100 & sim.t < 108;
    subplot(3, 1, 1);
    plot(sim.t(seg), sim.clean(seg), 'Color', green, 'LineWidth', 1.4);
    title('True pulse (synthetic)'); grid on;
    subplot(3, 1, 2);
    plot(sim.t(seg), raw(seg), 'Color', red, 'LineWidth', 1.2);
    title('Measured PPG while running: the arm swing swamps the pulse'); grid on;
    subplot(3, 1, 3);
    plot(sim.t(seg), cleaned(seg), 'Color', blue, 'LineWidth', 1.4);
    title('After adaptive cancellation with the accelerometer (NLMS)'); grid on; xlabel('time (s)');
    save_fig(fig, fullfile(out, 'signals.png'));

    % ---- 2. spectrograms --------------------------------------------------------------------------
    fig = figure('Visible', vis(), 'Position', [60 60 1300 572]);
    subplot(1, 2, 1);
    imagesc(t_raw, f * 60, 20 * log10(S_raw ./ max(S_raw(:)) + 1e-6)); axis xy; caxis([-40 0]);
    hold on; plot(sim.t, sim.hr_true, 'w--', 'LineWidth', 1.2);
    xlabel('time (s)'); ylabel('bpm'); title('Before: arm swing at 156 per minute dominates');
    subplot(1, 2, 2);
    imagesc(t_cln, f * 60, 20 * log10(S_cln ./ max(S_cln(:)) + 1e-6)); axis xy; caxis([-40 0]);
    hold on; plot(sim.t, sim.hr_true, 'w--', 'LineWidth', 1.2);
    xlabel('time (s)'); title('After NLMS: the heart-rate line is clear');
    colormap(parula_like());
    save_fig(fig, fullfile(out, 'spectrograms.png'));

    % ---- 3. heart-rate tracks ---------------------------------------------------------------------
    fig = figure('Visible', vis(), 'Position', [60 60 1300 546]); hold on;
    patch([0 150 150 0], [40 40 200 200], [0.95 0.93 0.86], 'EdgeColor', 'none', 'HandleVisibility', 'off');
    plot(sim.t, sim.hr_true, 'Color', [0.1 0.1 0.1], 'LineWidth', 2.5);
    plot(t_raw, hr_raw, 'o-', 'Color', red, 'MarkerSize', 4);
    plot(t_cln, hr_cln, 's-', 'Color', blue, 'MarkerSize', 4, 'LineWidth', 1.4);
    plot(tp, hp, '.', 'Color', green);
    text(120, 192, 'running (0 to 150 s)');
    legend({'true heart rate', sprintf('spectral, raw (MAE %.1f bpm)', hr_error(hr_raw, t_raw, sim)), ...
            sprintf('spectral, NLMS (MAE %.1f bpm)', hr_error(hr_cln, t_cln, sim)), ...
            sprintf('beat-to-beat, NLMS (MAE %.1f bpm)', hr_error(hp, tp, sim))}, 'Location', 'northwest');
    legend boxoff;
    xlabel('time (s)'); ylabel('heart rate (bpm)'); grid on; ylim([40 200]);
    title('Running from the first second: without the accelerometer the tracker follows the arm swing');
    save_fig(fig, fullfile(out, 'hr_tracks.png'));

    % ---- 4. NLMS step size: fast convergence vs clean steady state ---------------------------
    run = ppg_synth(fs, 120, 95, struct('cadence_hz', 2.6, 'gain', 4), 3);
    ref = bandpass_fir(run.clean, fs, 0.5, 4);
    mus = [0.01 0.02 0.05 0.1 0.2 0.5];
    first = zeros(size(mus)); steady = zeros(size(mus));
    for i = 1:numel(mus)
        res = bandpass_fir(nlms_cancel(run.ppg, run.acc, 8, mus(i)), fs, 0.5, 4) - ref;
        first(i) = std(res(1:8 * fs)) / std(ref);
        steady(i) = std(res(20 * fs:end)) / std(ref);
    end
    fig = figure('Visible', vis(), 'Position', [60 60 1300 520]);
    semilogx(mus, 100 * first, 'o-', 'Color', orange, 'MarkerFaceColor', orange, 'LineWidth', 1.6); hold on;
    semilogx(mus, 100 * steady, 's-', 'Color', blue, 'MarkerFaceColor', blue, 'LineWidth', 1.6);
    top = 10 * ceil(max(100 * [first steady]) / 10 + 1);
    semilogx([0.05 0.05], [0 top], '--', 'Color', grey);
    text(0.052, 0.93 * top, 'chosen: \mu = 0.05', 'Color', grey);
    legend({'residual in the first 8 s (still converging)', 'residual after convergence'}, ...
           'Location', 'northoutside', 'Orientation', 'horizontal'); legend boxoff;
    xlabel('NLMS step size \mu'); ylabel('residual artefact (% of pulse RMS)'); grid on; ylim([0 top]);
    title('Step size: small \mu cancels cleanly but learns slowly, large \mu learns fast but stays noisy');
    save_fig(fig, fullfile(out, 'nlms_step_size.png'));
end

function c = parula_like()
    % A perceptually ordered dark-to-light map that exists in MATLAB and Octave alike.
    k = linspace(0, 1, 64).';
    c = [0.2 + 0.8 * k.^1.5, 0.1 + 0.85 * k, 0.45 + 0.35 * sin(pi * k)];
    c = min(max(c, 0), 1);
end

function save_fig(fig, path)
    % MATLAB prints off-screen. Octave's FLTK toolkit can only render a visible
    % figure, so there the window is captured pixel for pixel instead.
    if exist('OCTAVE_VERSION', 'builtin')
        simplify_lines(fig);
        drawnow;
        frame = getframe(fig);
        imwrite(frame.cdata, path);
    else
        set(fig, 'PaperPositionMode', 'auto');
        print(fig, path, '-dpng', '-r110');
    end
    close(fig);
    fprintf('wrote %s\n', path);
end

function v = vis()
    % Figures are hidden in MATLAB, shown briefly in Octave (see save_fig).
    if exist('OCTAVE_VERSION', 'builtin')
        v = 'on';
    else
        v = 'off';
    end
end

function simplify_lines(fig)
    % Octave's FLTK renderer drops long runs of nearly collinear points (flat
    % pass bands, constant delays). Ramer-Douglas-Peucker simplification with a
    % tolerance below one pixel of the axes removes those runs while keeping
    % every visible detail; lines with markers keep all their points.
    h = findall(fig, 'type', 'line');
    for k = 1:numel(h)
        if strcmp(get(h(k), 'LineStyle'), 'none') || ~strcmp(get(h(k), 'Marker'), 'none')
            continue;
        end
        x = get(h(k), 'XData'); y = get(h(k), 'YData');
        if numel(x) < 20 || any(~isfinite(y))
            continue;
        end
        ax = get(h(k), 'Parent');
        xl = get(ax, 'XLim'); yl = get(ax, 'YLim');
        u = x; v = y;
        if strcmp(get(ax, 'XScale'), 'log'), u = log10(max(x, realmin)); xl = log10(xl); end
        if strcmp(get(ax, 'YScale'), 'log'), v = log10(max(y, realmin)); yl = log10(yl); end
        u = (u - xl(1)) / diff(xl); v = (v - yl(1)) / diff(yl);   % in axes units, 0..1
        keep = rdp(u, v, 4e-4);
        set(h(k), 'XData', x(keep), 'YData', y(keep));
    end
end

function keep = rdp(u, v, tol)
    n = numel(u);
    keep = false(1, n); keep([1 n]) = true;
    stack = [1 n];
    while ~isempty(stack)
        a = stack(end, 1); b = stack(end, 2); stack(end, :) = [];
        if b - a < 2
            continue;
        end
        i = a + 1:b - 1;
        dx = u(b) - u(a); dy = v(b) - v(a); L = hypot(dx, dy) + eps;
        d = abs(dy * (u(i) - u(a)) - dx * (v(i) - v(a))) / L;
        [dmax, j] = max(d);
        if dmax > tol
            m = i(j);
            keep(m) = true;
            stack = [stack; a m; m b]; %#ok<AGROW>
        end
    end
end
