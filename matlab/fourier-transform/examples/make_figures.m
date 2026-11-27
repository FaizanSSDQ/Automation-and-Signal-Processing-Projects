function make_figures()
%MAKE_FIGURES  Figures for the Fourier transform exercises. MATLAB and GNU Octave.
%   From the project folder:  addpath examples; make_figures
%
%   Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

    here = fileparts(mfilename('fullpath'));
    addpath(fullfile(here, '..', 'src'));
    out = fullfile(here, '..', 'docs');
    set(0, 'defaultaxesfontsize', 13, 'defaulttextfontsize', 13, 'defaultlinelinewidth', 1.6);
    blue = [0.17 0.42 0.69]; orange = [0.87 0.42 0.13]; green = [0.18 0.52 0.35]; grey = [0.45 0.47 0.5];

    % ---- 1. rectangular pulses: shorter in time, wider in frequency --------------------------
    f = linspace(-5, 5, 2001);
    fig = figure('Visible', vis(), 'Position', [60 60 1300 520]);
    subplot(1, 2, 1); hold on;
    plot([-2 -1 -1 1 1 2], [0 0 1 1 0 0], 'Color', blue);
    plot([-2 -0.25 -0.25 0.25 0.25 2], [0 0 1 1 0 0], 'Color', orange);
    xlabel('t (s)'); ylim([-0.2 1.2]); grid on; title('pulses of width 2 s and 0.5 s');
    subplot(1, 2, 2); hold on;
    plot(f, 2 * sincn(2 * f), 'Color', blue);
    plot(f, 0.5 * sincn(0.5 * f), 'Color', orange);
    fk = -5:0.25:5;
    plot(fk, real(ctft(@(t) ones(size(t)), fk, -1, 1)), 'o', 'Color', blue, 'MarkerSize', 5);
    legend({'2 sinc(2f)', '0.5 sinc(0.5f)', 'numerical integral (2 s pulse)'}, 'Location', 'northeast'); legend boxoff;
    xlabel('f (Hz)'); ylabel('X(f)'); grid on;
    title('X(f) = \tau sinc(f\tau): zeros every 1/\tau');
    save_fig(fig, fullfile(out, 'rect_transforms.png'));

    % ---- 2. e^{-at} u(t): magnitude and phase -------------------------------------------------
    f = linspace(-1, 1, 801);
    fig = figure('Visible', vis(), 'Position', [60 60 1300 520]);
    subplot(1, 2, 1); hold on;
    for a = [0.5 1 2]
        plot(f, 1 ./ sqrt(a^2 + (2 * pi * f).^2));
    end
    legend({'a = 0.5', 'a = 1', 'a = 2'}); legend boxoff;
    xlabel('f (Hz)'); ylabel('|X(f)|'); grid on; title('|X(f)| = 1 / \surd(a^2 + (2\pif)^2)');
    subplot(1, 2, 2); hold on;
    for a = [0.5 1 2]
        plot(f, -atan(2 * pi * f / a) * 180 / pi);
    end
    xlabel('f (Hz)'); ylabel('degrees'); grid on; ylim([-95 95]);
    title('\angleX(f) = -atan(2\pif / a)');
    save_fig(fig, fullfile(out, 'exponential_transform.png'));

    % ---- 3. time shift: same magnitude, linear phase -------------------------------------------
    f = linspace(-3, 3, 240);                             % avoids the zeros of X0 at whole hertz
    X0 = ctft(@(t) ones(size(t)), f, -0.5, 0.5);
    X1 = ctft(@(t) ones(size(t)), f, 0.5, 1.5);
    fig = figure('Visible', vis(), 'Position', [60 60 1300 480]);
    subplot(1, 2, 1); hold on;
    plot(f, abs(X0), 'Color', grey, 'LineWidth', 4); plot(f, abs(X1), '--', 'Color', orange);
    legend({'centred pulse', 'delayed by 1 s'}); legend boxoff; xlabel('f (Hz)'); grid on; title('|X(f)|: identical');
    subplot(1, 2, 2); hold on;
    ph = unwrap(angle(X1 ./ X0));
    [~, m] = min(abs(f));
    ph = ph - 2 * pi * round((ph(m) + 2 * pi * f(m)) / (2 * pi));   % unwrap is defined up to 2*pi*k
    plot(f, ph, 'Color', orange, 'LineWidth', 5);
    plot(f, -2 * pi * f, ':', 'Color', [0.1 0.1 0.1], 'LineWidth', 2);
    legend({'phase of X_{delayed} / X_{centred}', '-2\pi f t_0, t_0 = 1 s'}, 'Location', 'northeast'); legend boxoff;
    xlabel('f (Hz)'); ylabel('rad'); grid on; title('a delay adds a phase that grows with frequency');
    save_fig(fig, fullfile(out, 'time_shift.png'));
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
