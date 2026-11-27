function make_figures()
%MAKE_FIGURES  Regenerate the README figures in docs/. Runs in MATLAB and GNU Octave.
%   From the project folder:  addpath examples; make_figures
%
%   Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

    here = fileparts(mfilename('fullpath'));
    addpath(fullfile(here, '..', 'src'));
    out = fullfile(here, '..', 'docs');
    set(0, 'defaultaxesfontsize', 13, 'defaulttextfontsize', 13, 'defaultlinelinewidth', 1.3);
    blue = [0.17 0.42 0.69]; orange = [0.87 0.42 0.13]; grey = [0.45 0.47 0.5]; green = [0.18 0.52 0.35];

    % ---- 1. partial sums of a square wave and the Gibbs overshoot ----------------------
    T = 1;
    t = linspace(-0.25, 1.25, 3000);
    fig = figure('Visible', vis(), 'Position', [60 60 1300 728]);
    subplot(1, 2, 1); hold on;
    plot(t, waveform('square', t, T), 'Color', grey, 'LineWidth', 1);
    Ns = [1 5 25]; cols = {orange, green, blue};
    for k = 1:3
        plot(t, fs_synth(fs_analytic('square', -Ns(k):Ns(k)), T, t), 'Color', cols{k}, 'LineWidth', 1.6);
    end
    legend({'square wave', 'N = 1', 'N = 5', 'N = 25'}, 'Location', 'northoutside', 'Orientation', 'horizontal');
    legend boxoff;
    xlabel('t / T'); ylabel('amplitude'); title('Partial Fourier sums'); grid on; ylim([-1.4 1.4]);
    subplot(1, 2, 2); hold on;
    tz = linspace(-0.02, 0.06, 3000);
    for N = [25 101 401]
        plot(tz, fs_synth(fs_analytic('square', -N:N), T, tz), 'LineWidth', 1.4);
    end
    plot(tz([1 end]), [1.179 1.179], '--', 'Color', grey);
    text(0.03, 1.21, '+8.95 % of the jump', 'Color', grey);
    legend({'N = 25', 'N = 101', 'N = 401'}, 'Location', 'southeast'); legend boxoff;
    xlabel('t / T (zoom at the jump)'); title('Gibbs: narrower, never lower'); grid on; ylim([-0.4 1.35]);
    save_fig(fig, fullfile(out, 'gibbs.png'));

    % ---- 2. line spectra: jumps decay as 1/n, corners as 1/n^2 --------------------------
    n = 1:40;
    fig = figure('Visible', vis(), 'Position', [60 60 1300 559]);
    loglog(n, abs(fs_analytic('square', n)), 'o', 'Color', blue, 'MarkerFaceColor', blue); hold on;
    loglog(n, abs(fs_analytic('sawtooth', n)), 's', 'Color', orange);
    loglog(n, abs(fs_analytic('triangle', n)), 'd', 'Color', green, 'MarkerFaceColor', green);
    loglog(n, 0.64 ./ n, '--', 'Color', grey); loglog(n, 0.41 ./ n.^2, ':', 'Color', grey);
    legend({'square |c_n|', 'sawtooth |c_n|', 'triangle |c_n|', '\propto 1/n (jumps)', '\propto 1/n^2 (corners)'}, ...
           'Location', 'southwest'); legend boxoff;
    xlabel('harmonic n'); ylabel('|c_n|'); title('How fast a spectrum decays depends on the smoothest flaw'); grid on;
    save_fig(fig, fullfile(out, 'line_spectra.png'));

    % ---- 3. CTFT of a rectangular pulse ---------------------------------------------------
    fs = 1000; tau = 0.5;
    tt = (0:20 * fs - 1) / fs;
    [X, f] = ctft_approx(double(tt < tau), fs);
    band = abs(f) <= 10;
    fig = figure('Visible', vis(), 'Position', [60 60 1300 520]);
    subplot(1, 2, 1);
    plot([-0.5 0 0 tau tau 1.5], [0 0 1 1 0 0], 'Color', blue, 'LineWidth', 2);
    xlabel('t (s)'); title('Pulse, width \tau = 0.5 s'); grid on; ylim([-0.2 1.2]);
    subplot(1, 2, 2); hold on;
    fa = linspace(-10, 10, 2000);
    plot(fa, tau * abs(sincn(fa * tau)), 'Color', grey, 'LineWidth', 3);
    plot(f(band), abs(X(band)), '.', 'Color', orange, 'MarkerSize', 7);
    legend({'|\tau sinc(f\tau)| (analytic)', 'FFT estimate'}); legend boxoff;
    xlabel('f (Hz)'); title('|X(f)|: zeros every 1/\tau = 2 Hz'); grid on;
    save_fig(fig, fullfile(out, 'rect_transform.png'));

    % ---- 4. windows and spectral leakage --------------------------------------------------
    L = 64; fs = 64; f0 = 10.37;   % between two bins: maximum leakage
    k = (0:L-1).';
    x = cos(2 * pi * f0 * k / fs) + 0.003 * cos(2 * pi * 20.5 * k / fs);   % weak tone 50 dB lower
    names = {'rect', 'hann', 'blackman'}; cols = {grey, blue, orange};
    fig = figure('Visible', vis(), 'Position', [60 60 1300 559]);
    subplot(1, 2, 1); hold on;
    for i = 1:3
        plot(k, window_fn(names{i}, L), 'Color', cols{i}, 'LineWidth', 1.6);
    end
    legend(names, 'Location', 'south'); legend boxoff; xlabel('sample'); title('Windows'); grid on; xlim([0 L-1]);
    subplot(1, 2, 2); hold on;
    P = 4096;
    fax = (0:P/2-1) * fs / P;
    for i = 1:3
        w = window_fn(names{i}, L);
        S = abs(fft(x .* w, P)); S = S(1:P/2) / max(S);
        [psl, ~] = window_sidelobe(w);
        plot(fax, 20 * log10(S + 1e-12), 'Color', cols{i}, 'LineWidth', 1.3);
        labels{i} = sprintf('%s (side lobes %.0f dB)', names{i}, psl); %#ok<AGROW>
    end
    plot([20.5 20.5], [-110 0], ':', 'Color', [0.6 0.1 0.1]);
    text(21, -15, 'weak tone, -50 dB', 'Color', [0.6 0.1 0.1]);
    legend(labels, 'Location', 'southwest'); legend boxoff;
    xlabel('frequency (Hz)'); ylabel('dB'); title('Leakage hides the weak tone without a good window'); grid on;
    ylim([-110 5]); xlim([0 32]);
    save_fig(fig, fullfile(out, 'leakage.png'));
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
