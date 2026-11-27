function make_figures()
%MAKE_FIGURES  Figures for the waveform generator. MATLAB and GNU Octave.
%   From the project folder:  addpath examples; make_figures
%
%   Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

    here = fileparts(mfilename('fullpath'));
    addpath(fullfile(here, '..', 'src'));
    out = fullfile(here, '..', 'docs');
    set(0, 'defaultaxesfontsize', 12, 'defaulttextfontsize', 12, 'defaultlinelinewidth', 1.6);
    blue = [0.17 0.42 0.69]; orange = [0.87 0.42 0.13]; green = [0.18 0.52 0.35]; grey = [0.45 0.47 0.5];
    fs = 1e5; f0 = 100; T = 1 / f0;
    t = (0:9999) / fs;                                         % 10 periods

    % ---- 1. waveforms and their harmonics -----------------------------------------------------
    kinds = {'sine', 'square', 'triangle', 'sawtooth'};
    fig = figure('Visible', vis(), 'Position', [60 60 1300 860]);
    for i = 1:4
        y = periodic_wave(kinds{i}, t, T);
        subplot(4, 2, 2 * i - 1);
        plot(t(1:2000) * 1000, y(1:2000), 'Color', blue); grid on; ylim([-1.3 1.3]);
        title(kinds{i}); if i == 4, xlabel('t (ms)'); end
        subplot(4, 2, 2 * i);
        [A, f] = harmonics(y, fs, f0, 15);
        stem(f, A, 'filled', 'Color', orange); grid on; ylim([0 1.4]); xlim([0 1600]);
        if i == 4, xlabel('frequency (Hz)'); end
        title(sprintf('harmonics of the %s', kinds{i}));
    end
    save_fig(fig, fullfile(out, 'waveforms_harmonics.png'));

    % ---- 2. duty cycle of a pulse wave ------------------------------------------------------------
    duties = [0.5 0.25 0.1];
    cols = {blue, green, orange};
    fig = figure('Visible', vis(), 'Position', [60 60 1300 560]);
    for i = 1:3
        d = duties(i);
        y = periodic_wave('pulse', t, T, d);
        subplot(3, 2, 2 * i - 1);
        plot(t(1:3000) * 1000, y(1:3000), 'Color', cols{i}); grid on; ylim([-0.2 1.2]);
        title(sprintf('pulse, duty %d %%', round(100 * d)));
        subplot(3, 2, 2 * i); hold on;
        [A, f] = harmonics(y, fs, f0, 30);
        fe = linspace(f0 / 10, 3000, 1000);
        plot(fe, 2 * d * abs(sin(pi * fe / f0 * d) ./ (pi * fe / f0 * d)), '--', 'Color', grey);
        stem(f, A, 'filled', 'Color', cols{i}); grid on; xlim([0 3000]); ylim([0 1.1]);
        title(sprintf('zeros every %g Hz = f_0 / duty', f0 / d));
    end
    save_fig(fig, fullfile(out, 'duty_cycle.png'));

    % ---- 3. pulse shapes ------------------------------------------------------------------------------
    tt = linspace(-4, 4, 4001);
    fig = figure('Visible', vis(), 'Position', [60 60 1300 440]); hold on;
    plot([-4 -2 -2 2 2 4], [0 0 1 1 0 0], 'Color', blue);
    plot(tt, pulse_shapes('tri', tt / 2), 'Color', orange);
    plot(tt, pulse_shapes('gauss', tt), 'Color', green);
    legend({'rect(t/4): 4 s wide', 'tri(t/2): 2 s half-width', 'exp(-\pit^2)'}, 'Location', 'northeast'); legend boxoff;
    grid on; xlabel('t (s)'); ylim([-0.1 1.2]);
    title('Pulse building blocks: scaling t by 1/a stretches a pulse a times');
    save_fig(fig, fullfile(out, 'pulse_shapes.png'));
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
