function make_figures()
%MAKE_FIGURES  Regenerate the README figures and the example C header. MATLAB and GNU Octave.
%   From the project folder:  addpath examples; make_figures
%
%   Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

    here = fileparts(mfilename('fullpath'));
    addpath(fullfile(here, '..', 'src'));
    out = fullfile(here, '..', 'docs');
    set(0, 'defaultaxesfontsize', 13, 'defaulttextfontsize', 13, 'defaultlinelinewidth', 1.3);
    blue = [0.17 0.42 0.69]; orange = [0.87 0.42 0.13]; grey = [0.45 0.47 0.5]; red = [0.75 0.15 0.15];
    fs = 8000;
    f = linspace(1, fs / 2 - 1, 4000);
    db = @(H) 20 * log10(abs(H) + 1e-15);

    % ---- 1. FIR vs IIR for the same job ----------------------------------------------------
    [h, info] = fir_kaiser('low', [1000 1500], fs, 60);
    sos = iir_butter(8, 1150, fs);
    Hf = freq_resp(h, f, fs);
    Hi = freq_resp(sos, f, fs);
    gd = @(H) -diff(unwrap(angle(H))) ./ diff(2 * pi * f / fs);   % group delay, samples
    fig = figure('Visible', vis(), 'Position', [60 60 1300 559]);
    subplot(1, 2, 1); hold on;
    plot(f, db(Hf), 'Color', blue, 'LineWidth', 1.6);
    plot(f, db(Hi), 'Color', orange, 'LineWidth', 1.6);
    plot([1500 4000], [-60 -60], '--', 'Color', grey);
    legend({sprintf('FIR, Kaiser, %d taps', numel(h)), 'IIR, Butterworth order 8 (4 biquads)', '60 dB spec'}, ...
           'Location', 'southwest'); legend boxoff;
    xlabel('frequency (Hz)'); ylabel('dB'); title('Magnitude, fs = 8 kHz'); grid on; ylim([-120 5]);
    subplot(1, 2, 2); hold on;
    fm = f(1:end-1);
    plot(fm, gd(Hf), 'Color', blue, 'LineWidth', 1.6);
    plot(fm, gd(Hi), 'Color', orange, 'LineWidth', 1.6);
    xlabel('frequency (Hz)'); ylabel('samples'); grid on; xlim([0 1500]); ylim([0 40]);
    title(sprintf('Group delay: FIR constant (%d samples), IIR is not', info.delay));
    save_fig(fig, fullfile(out, 'fir_vs_iir.png'));

    % ---- 2. coefficient sensitivity --------------------------------------------------------
    sos = iir_butter(8, 150, fs);
    sq = quantize_q14(sos) / 2^14;
    [b, a] = sos_to_tf(sos);
    s = 2^15 / max(abs(a));
    aq = round(a * s) / s;
    bq = round(b * 2^15 / max(abs(b))) * max(abs(b)) / 2^15;
    fl = linspace(1, 600, 3000);
    fig = figure('Visible', vis(), 'Position', [60 60 1300 559]);
    subplot(1, 2, 1); hold on;
    plot(fl, db(freq_resp(sos, fl, fs)), 'Color', grey, 'LineWidth', 4);
    plot(fl, db(freq_resp(sq, fl, fs)), 'Color', blue, 'LineWidth', 1.6);
    plot(fl, db(freq_resp({bq, aq}, fl, fs)), 'Color', red, 'LineWidth', 1.3);
    legend({'exact', '16-bit, 4 biquads', '16-bit, one 8th-order polynomial'}, 'Location', 'southwest'); legend boxoff;
    xlabel('frequency (Hz)'); ylabel('dB'); title('8th-order low-pass at 150 Hz, fs = 8 kHz'); grid on; ylim([-80 30]);
    subplot(1, 2, 2); hold on;
    th = linspace(0, 2 * pi, 400);
    plot(cos(th), sin(th), 'Color', grey);
    pe = []; for k = 1:4, pe = [pe; roots(sos(k, 4:6))]; end %#ok<AGROW>
    pc = []; for k = 1:4, pc = [pc; roots(sq(k, 4:6))]; end %#ok<AGROW>
    pd = roots(aq);
    plot(real(pe), imag(pe), 'o', 'Color', grey, 'MarkerSize', 9, 'LineWidth', 1.5);
    plot(real(pc), imag(pc), 'x', 'Color', blue, 'MarkerSize', 9, 'LineWidth', 1.5);
    plot(real(pd), imag(pd), '+', 'Color', red, 'MarkerSize', 9, 'LineWidth', 1.5);
    axis equal; xlim([0.85 1.03]); ylim([-0.12 0.12]); grid on;
    legend({'unit circle', 'exact poles', 'biquads, 16-bit', 'direct form, 16-bit'}, ...
           'Location', 'northoutside', 'Orientation', 'horizontal'); legend boxoff;
    text(0.86, -0.105, sprintf('direct form: one pole at |p| = %.2f, outside the circle (off this view): unstable', ...
         max(abs(pd))), 'Color', red, 'FontSize', 11);
    title('Pole positions (zoom near z = 1)');
    save_fig(fig, fullfile(out, 'coeff_sensitivity.png'));

    % ---- 3. Q15 fixed-point filtering, bit-exact MCU model -------------------------------------
    sos = iir_butter(6, 700, fs);
    n = 0:399;
    x = round(10000 * sin(2 * pi * 300 * n / fs) + 5000 * sin(2 * pi * 2500 * n / fs));
    yq = sos_filter_q15(sos, x);
    yd = sos_filter(quantize_q14(sos) / 2^14, x);
    fig = figure('Visible', vis(), 'Position', [60 60 1300 559]);
    subplot(2, 1, 1); hold on;
    plot(n / fs * 1000, x, 'Color', grey);
    plot(n / fs * 1000, yq, 'Color', blue, 'LineWidth', 1.6);
    legend({'input: 300 Hz + 2.5 kHz', 'Q15 output (2.5 kHz removed)'}, 'Location', 'northoutside', 'Orientation', 'horizontal'); legend boxoff;
    ylabel('Q15 counts'); title('Order-6 Butterworth low-pass at 700 Hz, integer arithmetic as on the MCU'); grid on;
    subplot(2, 1, 2);
    plot(n / fs * 1000, yq - yd, 'Color', orange);
    xlabel('time (ms)'); ylabel('counts'); grid on;
    snr = 10 * log10(sum(yd.^2) / sum((yq - yd).^2));
    title(sprintf('Q15 minus double precision: a few counts, SNR %.0f dB', snr));
    save_fig(fig, fullfile(out, 'q15_filtering.png'));

    % ---- example header for firmware ---------------------------------------------------------
    export_c_header(iir_butter(4, 1000, fs), 'lowpass_1k', fs, fullfile(here, 'lowpass_1k.h'));
    fprintf('wrote %s\n', fullfile(here, 'lowpass_1k.h'));
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
