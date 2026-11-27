function make_figures()
%MAKE_FIGURES  Figures for the elementary-signals exercises. MATLAB and GNU Octave.
%   From the project folder:  addpath examples; make_figures
%
%   Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

    here = fileparts(mfilename('fullpath'));
    addpath(fullfile(here, '..', 'src'));
    out = fullfile(here, '..', 'docs');
    set(0, 'defaultaxesfontsize', 13, 'defaulttextfontsize', 13, 'defaultlinelinewidth', 1.6);
    blue = [0.17 0.42 0.69]; orange = [0.87 0.42 0.13]; green = [0.18 0.52 0.35]; grey = [0.45 0.47 0.5];

    % ---- 1. the building blocks ------------------------------------------------------------
    fig = figure('Visible', vis(), 'Position', [60 60 1300 760]);
    subplot(2, 3, 1);
    t = linspace(-10, 10, 2001);
    plot(t, unit_step(t), 'Color', blue); ylim([-0.3 1.3]); grid on; title('unit step u(t)'); xlabel('t');
    subplot(2, 3, 2);
    n = -10:10;
    stem(n, unit_impulse(n), 'filled', 'Color', blue); ylim([-0.3 1.3]); grid on; title('unit impulse \delta[n]'); xlabel('n');
    subplot(2, 3, 3);
    plot(t, ramp(t), 'Color', blue); grid on; title('ramp r(t) = t u(t)'); xlabel('t');
    subplot(2, 3, 4); hold on;
    t = linspace(0, 60, 3001);
    plot(t, unit_step(t - 20) - unit_step(t - 40), 'Color', blue);
    plot(t, 2 * (unit_step(t - 30) - unit_step(t - 35)), 'Color', orange);
    ylim([-0.3 2.4]); grid on; xlabel('t'); title('windows from steps');
    legend({'u(t-20) - u(t-40)', '2[u(t-30) - u(t-35)]'}, 'Location', 'northeast'); legend boxoff;
    subplot(2, 3, 5); hold on;
    t = linspace(-2, 2, 801);
    plot(t, exp(t), 'Color', blue); plot(t, exp(-t), 'Color', orange);
    plot(t, -exp(t), '--', 'Color', blue); plot(t, -exp(-t), '--', 'Color', orange);
    grid on; xlabel('t'); title('exponentials \pm e^{\pm t}');
    subplot(2, 3, 6);
    t = linspace(-2, 3, 5001);
    plot(t, exp(-t) .* sin(10 * pi * t) .* unit_step(t + 1), 'Color', green);
    grid on; xlabel('t'); title('e^{-t} sin(10\pit) u(t+1)');
    save_fig(fig, fullfile(out, 'elementary_signals.png'));

    % ---- 2. sinusoids and phase -------------------------------------------------------------
    t = linspace(0, 2, 2001);
    w = 2 * pi;
    fig = figure('Visible', vis(), 'Position', [60 60 1300 500]); hold on;
    plot(t, sin(w * t), 'Color', blue);
    plot(t, cos(w * t), 'Color', orange);
    plot(t, sin(w * t - pi / 4), '--', 'Color', blue);
    plot(t, cos(w * t + pi / 4), '--', 'Color', orange);
    legend({'sin(\omegat)', 'cos(\omegat) = sin(\omegat + 90°)', 'sin(\omegat - 45°)', 'cos(\omegat + 45°)'}, ...
           'Location', 'northoutside', 'Orientation', 'horizontal'); legend boxoff;
    xlabel('t (s), \omega = 2\pi rad/s'); grid on; ylim([-1.2 1.2]);
    title('A phase shift moves the wave in time: 45° is one eighth of a period');
    save_fig(fig, fullfile(out, 'sinusoids.png'));

    % ---- 3. energy and power ----------------------------------------------------------------
    dt = 0.002;
    t1 = 0:dt:6 - dt;
    x1 = exp(-abs(t1 / 2)) .* sin(2 * pi * t1) .* (unit_step(t1) - unit_step(t1 - 4));
    [E1, ~] = energy_power(x1, dt);
    [tp, xp] = periodic_extension(x1, dt, 6);
    [~, Pp] = energy_power(xp, dt);
    fig = figure('Visible', vis(), 'Position', [60 60 1300 500]);
    subplot(1, 2, 1);
    plot(t1, x1, 'Color', blue); grid on; xlabel('t (s)'); xlim([0 6]); ylim([-1.1 1.1]);
    title(sprintf('one pulse: energy E = %.4f, power -> 0', E1));
    subplot(1, 2, 2);
    plot(tp, xp, 'Color', orange); grid on; xlabel('t (s)'); xlim([-18 18]); ylim([-1.1 1.1]);
    title(sprintf('repeated every 6 s: power P = E / 6 = %.4f', Pp));
    save_fig(fig, fullfile(out, 'energy_power.png'));
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
