function make_figures()
%MAKE_FIGURES  Figures for the Fourier series assignment. MATLAB and GNU Octave.
%   From the project folder:  addpath examples; make_figures
%
%   Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

    here = fileparts(mfilename('fullpath'));
    addpath(fullfile(here, '..', 'src'));
    out = fullfile(here, '..', 'docs');
    set(0, 'defaultaxesfontsize', 13, 'defaulttextfontsize', 13, 'defaultlinelinewidth', 1.6);
    blue = [0.17 0.42 0.69]; orange = [0.87 0.42 0.13]; green = [0.18 0.52 0.35]; grey = [0.45 0.47 0.5];
    f = @(t) tri_pulse(t / 2);
    T = 4;

    % ---- 1. the spectrum of triangle(t/2) ------------------------------------------------------
    [D, n] = fs_coefficients(f, -2, 2, 11);
    fig = figure('Visible', vis(), 'Position', [60 60 1300 560]);
    subplot(2, 1, 1);
    stem(n, abs(D), 'filled', 'Color', blue, 'LineWidth', 2); grid on;
    ylabel('|D_n|'); title('Amplitude spectrum of the periodic triangle(t/2), T = 4 s');
    subplot(2, 1, 2);
    stem(n, angle(D) * 180 / pi, 'filled', 'Color', orange, 'LineWidth', 2); grid on;
    ylabel('\angle D_n (deg)'); xlabel('harmonic n'); ylim([-10 10]);
    title('Phase: zero for every harmonic, because the pulse is even');
    save_fig(fig, fullfile(out, 'triangle_spectrum.png'));

    % ---- 2. reconstruction with 7, 11 and 17 harmonics ---------------------------------------
    t = linspace(-6, 6, 3001);
    fp = tri_pulse(mod(t + 2, 4) / 2 - 1);                 % periodic version, peaks at 0, +-4
    Ns = [7 11 17]; cols = {orange, green, blue};
    fig = figure('Visible', vis(), 'Position', [60 60 1300 620]);
    subplot(2, 1, 1); hold on;
    plot(t, fp, 'Color', grey, 'LineWidth', 4);
    leg = {'periodic triangle(t/2)'};
    for k = 1:3
        [Dk, ~] = fs_coefficients(f, -2, 2, Ns(k));
        plot(t, fs_reconstruct(Dk, T, t), 'Color', cols{k});
        leg{end + 1} = sprintf('N = %d', Ns(k)); %#ok<AGROW>
    end
    legend(leg, 'Location', 'northoutside', 'Orientation', 'horizontal'); legend boxoff;
    grid on; ylim([-0.1 1.1]);
    subplot(2, 1, 2); hold on;
    for k = 1:3
        [Dk, ~] = fs_coefficients(f, -2, 2, Ns(k));
        e = fs_reconstruct(Dk, T, t) - fp;
        plot(t, e, 'Color', cols{k});
        leg{k} = sprintf('N = %d, max error %.4f', Ns(k), max(abs(e)));
    end
    legend(leg(1:3), 'Location', 'southeast'); legend boxoff;
    grid on; xlabel('t (s)'); ylabel('error');
    title('The error is largest at the corners and shrinks as harmonics are added');
    save_fig(fig, fullfile(out, 'triangle_reconstruction.png'));

    % ---- 3. a rectangular pulse train: sinc envelope and Gibbs ripple ------------------------------
    [Dr, n] = fs_coefficients(@(t) rect_pulse(t), -2, 2, 24);
    fig = figure('Visible', vis(), 'Position', [60 60 1300 520]);
    subplot(1, 2, 1); hold on;
    nf = linspace(-24, 24, 2000);
    env = 0.25 * ones(size(nf)); k = nf ~= 0;
    env(k) = sin(pi * nf(k) / 4) ./ (pi * nf(k));
    plot(nf, env, '--', 'Color', grey);
    stem(n, real(Dr), 'filled', 'Color', blue);
    grid on; xlabel('harmonic n'); ylabel('D_n');
    title('Pulse width 1, period 4: D_n on a sinc, zero every 4th');
    subplot(1, 2, 2); hold on;
    t = linspace(-3, 3, 3001);
    plot(t, rect_pulse(mod(t + 2, 4) - 2), 'Color', grey, 'LineWidth', 4);
    plot(t, fs_reconstruct(Dr, T, t), 'Color', blue);
    grid on; xlabel('t (s)'); ylim([-0.2 1.2]);
    title('24 harmonics: the jumps ring (Gibbs)');
    save_fig(fig, fullfile(out, 'pulse_train.png'));
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
