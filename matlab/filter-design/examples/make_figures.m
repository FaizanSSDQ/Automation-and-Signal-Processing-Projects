function make_figures()
%MAKE_FIGURES  Figures for DFT-based ideal low-pass filtering. MATLAB and GNU Octave.
%   From the project folder:  addpath examples; make_figures
%
%   Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

    here = fileparts(mfilename('fullpath'));
    addpath(fullfile(here, '..', 'src'));
    out = fullfile(here, '..', 'docs');
    set(0, 'defaultaxesfontsize', 13, 'defaulttextfontsize', 13, 'defaultlinelinewidth', 1.6);
    blue = [0.17 0.42 0.69]; orange = [0.87 0.42 0.13]; green = [0.18 0.52 0.35]; grey = [0.45 0.47 0.5];
    T0 = 4; Ts = 1 / 8; N0 = 32; f0 = 1 / T0;

    % ---- 1. the three steps: samples, spectrum and mask, result -------------------------------
    [g, k] = pulse_samples(1, T0, Ts);
    [H, fq] = lowpass_mask(N0, f0, 2);
    [y, G] = dft_filter(g, H);
    [fs_sorted, order] = sort(fq);
    fig = figure('Visible', vis(), 'Position', [60 60 1300 760]);
    subplot(2, 2, 1);
    stem(k, g, 'filled', 'Color', blue); grid on; xlabel('sample k'); ylim([-0.2 1.2]);
    title('g[k]: 1-s pulse, T_0 = 4 s, T_s = 1/8 s (N_0 = 32)');
    subplot(2, 2, 2); hold on;
    stem(fs_sorted, real(G(order)) * Ts, 'filled', 'Color', blue);
    plot(fs_sorted, H(order), 's--', 'Color', orange, 'MarkerFaceColor', orange);
    grid on; xlabel('frequency (Hz)');
    legend({'T_s G[q]  (the pulse spectrum)', 'H[q]  (ideal low-pass, 2 Hz)'}, 'Location', 'southoutside'); legend boxoff;
    title('spectrum and filter on the DFT grid, f_0 = 0.25 Hz');
    subplot(2, 1, 2); hold on;
    t = linspace(-2, 2, 2001);
    plot(t, double(abs(t) < 0.5), 'Color', grey, 'LineWidth', 3);
    plot(t, lowpass_pulse_train(t, 1, T0, 2), 'Color', green);
    kk = k; kk(kk >= N0 / 2) = kk(kk >= N0 / 2) - N0;
    [kt, o] = sort(kk * Ts);
    plot(kt, y(o), 'o', 'Color', orange, 'MarkerFaceColor', orange, 'MarkerSize', 7);
    legend({'input pulse', 'exact filtered output', 'IDFT\{G H\} (32 samples)'}, 'Location', 'northeast'); legend boxoff;
    grid on; xlabel('t (s)'); ylim([-0.25 1.25]);
    title('output: the edges are smoothed and ring, overshooting by 12 %');
    save_fig(fig, fullfile(out, 'dft_filtering.png'));

    % ---- 2. cut-off frequency ----------------------------------------------------------------------
    fig = figure('Visible', vis(), 'Position', [60 60 1300 500]); hold on;
    plot(t, double(abs(t) < 0.5), 'Color', grey, 'LineWidth', 3);
    cols = {orange, green, blue};
    fcs = [1 2 3];
    for i = 1:3
        plot(t, lowpass_pulse_train(t, 1, T0, fcs(i)), 'Color', cols{i});
    end
    legend({'input', 'cut-off 1 Hz', 'cut-off 2 Hz', 'cut-off 3 Hz'}, 'Location', 'northeast'); legend boxoff;
    grid on; xlabel('t (s)'); ylim([-0.25 1.25]);
    title('A wider filter passes more harmonics: steeper edges, faster ringing');
    save_fig(fig, fullfile(out, 'cutoff.png'));

    % ---- 3. aliasing error against the sampling interval ------------------------------------------
    Tss = 1 ./ [8 16 32 64 128];
    e = zeros(size(Tss));
    for i = 1:numel(Tss)
        [gi, ki] = pulse_samples(1, T0, Tss(i));
        yi = dft_filter(gi, lowpass_mask(numel(gi), f0, 2));
        e(i) = max(abs(yi - lowpass_pulse_train(ki * Tss(i), 1, T0, 2)));
    end
    fig = figure('Visible', vis(), 'Position', [60 60 1300 480]);
    loglog(1 ./ Tss, e, 'o-', 'Color', blue, 'MarkerFaceColor', blue); hold on;
    loglog(1 ./ Tss, e(1) * (Tss / Tss(1)).^2, '--', 'Color', grey);
    legend({'max error of the DFT result', '\propto T_s^2'}, 'Location', 'northeast'); legend boxoff;
    grid on; xlabel('sampling rate 1/T_s (Hz)'); ylabel('max error');
    title('The pulse is not band-limited, so sampling aliases: 2x the rate, 4x less error');
    save_fig(fig, fullfile(out, 'aliasing_error.png'));
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
