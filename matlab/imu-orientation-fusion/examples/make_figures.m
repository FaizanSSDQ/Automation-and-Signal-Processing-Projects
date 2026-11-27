function make_figures()
%MAKE_FIGURES  Regenerate the README figures. Runs in MATLAB and GNU Octave.
%   From the project folder:  addpath examples; make_figures
%   All data is simulated (imu_synth) with exact ground truth.
%
%   Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

    here = fileparts(mfilename('fullpath'));
    addpath(fullfile(here, '..', 'src'));
    out = fullfile(here, '..', 'docs');
    set(0, 'defaultaxesfontsize', 13, 'defaulttextfontsize', 13, 'defaultlinelinewidth', 1.3);
    blue = [0.17 0.42 0.69]; orange = [0.87 0.42 0.13]; grey = [0.45 0.47 0.5]; green = [0.18 0.52 0.35];
    red = [0.75 0.15 0.15];
    fs = 100;
    sim = imu_synth(fs, 60);

    methods = {'gyro', 'complementary', 'mahony', 'madgwick'};
    labels = {'gyro only', 'complementary', 'Mahony', 'Madgwick'};
    cols = {red, grey, blue, orange};
    E = cell(1, 4); R = zeros(1, 4); P = cell(1, 4);
    for i = 1:4
        q = fuse_orientation(methods{i}, sim.gyro, sim.acc, fs);
        [R(i), E{i}] = tilt_error(q, sim.q_true);
        p = zeros(1, size(q, 1));
        for k = 1:size(q, 1)
            [~, p(k)] = quat('to_euler', q(k, :));
        end
        P{i} = p;
    end

    % ---- 1. pitch tracking ------------------------------------------------------------------
    fig = figure('Visible', vis(), 'Position', [60 60 1300 546]); hold on;
    plot(sim.t, rad2deg_(sim.euler_true(2, :)), 'Color', [0.1 0.1 0.1], 'LineWidth', 3);
    for i = 1:4
        plot(sim.t, rad2deg_(P{i}), 'Color', cols{i}, 'LineWidth', 1.2);
    end
    legend([{'true pitch'}, labels], 'Location', 'southwest'); legend boxoff;
    xlabel('time (s)'); ylabel('pitch (deg)'); grid on; xlim([0 60]);
    title('Pitch of a moving wrist: the gyro alone drifts away, fusion stays on the truth');
    save_fig(fig, fullfile(out, 'pitch_tracking.png'));

    % ---- 2. tilt error over time ------------------------------------------------------------
    fig = figure('Visible', vis(), 'Position', [60 60 1300 520]);
    semilogy(sim.t, max(E{1}, 1e-3), 'Color', red, 'LineWidth', 1.2); hold on;
    for i = 2:4
        semilogy(sim.t, max(E{i}, 1e-3), 'Color', cols{i}, 'LineWidth', 1.2);
    end
    leg = cell(1, 4);
    for i = 1:4
        leg{i} = sprintf('%s (RMS %.2f deg)', labels{i}, R(i));
    end
    legend(leg, 'Location', 'southeast'); legend boxoff;
    xlabel('time (s)'); ylabel('tilt error (deg)'); grid on; ylim([0.01 100]);
    title('Tilt error: 1 deg/s of gyro bias against three ways of using gravity');
    save_fig(fig, fullfile(out, 'tilt_error.png'));

    % ---- 3. fall detection ---------------------------------------------------------------------
    fall = imu_synth(fs, 30, struct('fall_at', 15, 'seed', 2));
    ev = detect_fall(fall.acc, fall.gyro - fall.bias(:) * ones(1, size(fall.gyro, 2)), fs);
    amag = sqrt(sum(fall.acc.^2, 1));
    fig = figure('Visible', vis(), 'Position', [60 60 1300 520]); hold on;
    plot(fall.t, amag, 'Color', blue, 'LineWidth', 1.2);
    plot(fall.t([1 end]), [0.35 0.35], '--', 'Color', grey);
    plot(fall.t([1 end]), [2.5 2.5], '--', 'Color', grey);
    text(0.5, 0.5, 'free-fall threshold 0.35 g', 'Color', grey);
    text(0.5, 2.65, 'impact threshold 2.5 g', 'Color', grey);
    if ~isempty(ev)
        e = ev(1);
        patch(fall.t([e.free_fall e.impact e.impact e.free_fall]), [0 0 4.5 4.5], [0.98 0.9 0.8], ...
              'EdgeColor', 'none', 'FaceAlpha', 0.6);
        plot(fall.t(e.impact), amag(e.impact), 'v', 'Color', red, 'MarkerFaceColor', red, 'MarkerSize', 9);
        plot(fall.t(e.confirmed), 1, 's', 'Color', green, 'MarkerFaceColor', green, 'MarkerSize', 9);
        text(fall.t(e.impact) + 0.4, 4.1, 'impact', 'Color', red);
        text(fall.t(e.confirmed) - 2.2, 1.35, 'confirmed: lying still', 'Color', green);
    end
    xlabel('time (s)'); ylabel('|a| (g)'); grid on; ylim([0 4.6]);
    title('Fall = free fall, then impact, then stillness (a jump fails the last test)');
    save_fig(fig, fullfile(out, 'fall_detection.png'));

    % ---- 4. gain tuning: too little correction lets the bias win, too much lets motion in ------
    betas = [0.01 0.02 0.04 0.08 0.15 0.3 0.6];
    kps = [0.1 0.25 0.5 1 2 4 8];
    eb = zeros(size(betas)); ek = zeros(size(kps));
    for i = 1:numel(betas)
        eb(i) = tilt_error(fuse_orientation('madgwick', sim.gyro, sim.acc, fs, struct('beta', betas(i))), sim.q_true);
        ek(i) = tilt_error(fuse_orientation('mahony', sim.gyro, sim.acc, fs, struct('kp', kps(i))), sim.q_true);
    end
    fig = figure('Visible', vis(), 'Position', [60 60 1300 520]);
    subplot(1, 2, 1);
    semilogx(betas, eb, 'o-', 'Color', orange, 'MarkerFaceColor', orange, 'LineWidth', 1.6); hold on;
    semilogx([0.08 0.08], [0 max(eb) * 1.1], '--', 'Color', grey);
    xlabel('eta'); ylabel('tilt error, RMS (deg)'); grid on; ylim([0 max(eb) * 1.1]);
    title('Madgwick: chosen eta = 0.08');
    subplot(1, 2, 2);
    semilogx(kps, ek, 's-', 'Color', blue, 'MarkerFaceColor', blue, 'LineWidth', 1.6); hold on;
    semilogx([0.5 0.5], [0 max(ek) * 1.1], '--', 'Color', grey);
    xlabel('k_p'); grid on; ylim([0 max(ek) * 1.1]);
    title('Mahony (with bias learning): chosen k_p = 0.5');
    save_fig(fig, fullfile(out, 'gain_tuning.png'));
end

function d = rad2deg_(r)
    d = r * 180 / pi;
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
