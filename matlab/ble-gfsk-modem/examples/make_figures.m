function make_figures()
%MAKE_FIGURES  Regenerate the README figures. Runs in MATLAB and GNU Octave.
%   From the project folder:  addpath examples; make_figures
%
%   Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

    here = fileparts(mfilename('fullpath'));
    addpath(fullfile(here, '..', 'src'));
    out = fullfile(here, '..', 'docs');
    set(0, 'defaultaxesfontsize', 13, 'defaulttextfontsize', 13, 'defaultlinelinewidth', 1.3);
    blue = [0.17 0.42 0.69]; orange = [0.87 0.42 0.13]; grey = [0.45 0.47 0.5]; green = [0.18 0.52 0.35];
    sps = 8;

    % ---- 1. a packet on the air ------------------------------------------------------------
    pdu = [2, 6, hex2dec('A1'), hex2dec('B2'), hex2dec('C3'), hex2dec('D4'), hex2dec('E5'), hex2dec('F6')];
    [bits, parts] = ble_packet(pdu);
    [~, fr] = gfsk_mod([zeros(1, 3), bits, zeros(1, 3)], sps);
    nshow = 56;
    t = (0:nshow * sps - 1) / sps;
    fig = figure('Visible', vis(), 'Position', [60 60 1300 494]); hold on;
    nrz = kron(2 * bits(1:nshow) - 1, ones(1, sps)) * 0.25;
    plot(t, nrz, 'Color', grey, 'LineWidth', 1);
    plot(t, fr(3 * sps + (1:nshow * sps)), 'Color', blue, 'LineWidth', 2);
    for x = [8 40]
        plot([x x], [-0.33 0.33], ':', 'Color', [0.3 0.3 0.3]);
    end
    text(1.5, 0.31, 'preamble'); text(18, 0.31, 'access address 0x8E89BED6'); text(42, 0.31, 'PDU (whitened)');
    xlabel('time (\mus at 1 Msym/s)'); ylabel('frequency (MHz from carrier)');
    title('LE 1M GFSK: bits (grey) and the Gaussian-smoothed frequency (blue), \pm250 kHz'); grid on;
    ylim([-0.36 0.36]); xlim([0 nshow]);
    save_fig(fig, fullfile(out, 'packet_frequency.png'));

    % ---- 2. spectrum: the Gaussian filter narrows the signal -----------------------------------
    rand('state', 3); %#ok<RAND>
    b = double(rand(1, 20000) > 0.5);
    [Pg, f] = psd_avg(gfsk_mod(b, sps), sps);
    [Pf, ~] = psd_avg(gfsk_mod(b, sps, 50), sps);            % BT = 50: practically no filter
    fig = figure('Visible', vis(), 'Position', [60 60 1300 494]); hold on;
    plot(f, 10 * log10(Pf / max(Pf)), 'Color', grey, 'LineWidth', 1.4);
    plot(f, 10 * log10(Pg / max(Pg)), 'Color', orange, 'LineWidth', 1.8);
    legend({'FSK, rectangular pulses', 'GFSK, BT = 0.5 (Bluetooth LE)'}, 'Location', 'northeast'); legend boxoff;
    xlabel('frequency (MHz from carrier)'); ylabel('dB'); grid on; xlim([-3 3]); ylim([-70 3]);
    title('Power spectrum: the Gaussian filter cuts the side lobes by tens of dB');
    save_fig(fig, fullfile(out, 'spectrum.png'));

    % ---- 3. bit error rate ------------------------------------------------------------------------
    eb = 0:1:15;
    ber = gfsk_ber(eb, 200000);
    ebl = 10.^(eb / 10);
    fig = figure('Visible', vis(), 'Position', [60 60 1300 559]);
    semilogy(eb, max(ber, 1e-7), 'o-', 'Color', blue, 'MarkerFaceColor', blue, 'LineWidth', 1.6); hold on;
    semilogy(eb, 0.5 * exp(-ebl / 2), '--', 'Color', grey, 'LineWidth', 1.3);
    semilogy(eb, 0.5 * erfc(sqrt(ebl)), ':', 'Color', green, 'LineWidth', 1.6);
    semilogy(eb([1 end]), [1e-3 1e-3], '-', 'Color', orange);
    text(0.3, 1.5e-3, '0.1 % BER: LE receiver sensitivity criterion', 'Color', orange);
    legend({'GFSK discriminator (simulated, 200 000 bits)', 'non-coherent FSK, theory', ...
            'coherent BPSK / MSK, theory'}, 'Location', 'southwest'); legend boxoff;
    xlabel('E_b/N_0 (dB)'); ylabel('bit error rate'); grid on; ylim([1e-6 0.6]); xlim([0 15]);
    title('Bit error rate of the simple frequency-discriminator receiver');
    save_fig(fig, fullfile(out, 'ber.png'));

    % ---- 4. eye diagram of the discriminator output ------------------------------------------------
    rand('state', 5); %#ok<RAND>
    b = double(rand(1, 400) > 0.5);
    s = gfsk_mod([0 0 0 b 0 0 0], sps);
    r = channel_filter(awgn_channel(s, 14, sps, 0, 9), 0.6 / sps, 4 * sps);   % as in gfsk_demod
    dphi = [0, angle(r(2:end) .* conj(r(1:end-1)))];
    acc = filter(ones(1, sps), 1, dphi) / (2 * pi);
    fig = figure('Visible', vis(), 'Position', [60 60 1300 494]); hold on;
    for m = 10:380
        seg = acc((m - 1) * sps + (1:2 * sps + 1));
        plot((0:2 * sps) / sps, seg, 'Color', [0.62 0.75 0.88]);
    end
    xlabel('time (symbols)'); ylabel('cycles per symbol'); grid on;
    ylim([-0.45 0.45]);
    title('Eye diagram after the channel filter, E_b/N_0 = 14 dB: decide where the eye is widest');
    save_fig(fig, fullfile(out, 'eye.png'));
end

function [P, f] = psd_avg(x, sps)
    % Averaged periodogram (Hann window, 50 % overlap), f in MHz for 1 Msym/s.
    L = 512;
    w = 0.5 - 0.5 * cos(2 * pi * (0:L-1) / (L - 1));
    P = zeros(1, L);
    n = 0;
    for st = 1:L/2:(numel(x) - L)
        X = fft(x(st:st + L - 1) .* w);
        P = P + abs(X).^2;
        n = n + 1;
    end
    P = fftshift(P / n);
    f = (-L/2:L/2-1) * sps / L;
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
