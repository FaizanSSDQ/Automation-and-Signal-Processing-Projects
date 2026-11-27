function run_tests()
%RUN_TESTS  Checks for the digital filter designer. Runs in MATLAB and GNU Octave.
%   From the project folder:  addpath tests; run_tests
%   Throws an error (non-zero exit in batch mode) if any check fails.
%
%   Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

    here = fileparts(mfilename('fullpath'));
    addpath(fullfile(here, '..', 'src'));
    checks = 0;
    failures = 0;

    function check(cond, name)
        checks = checks + 1;
        if ~cond
            failures = failures + 1;
            fprintf('  FAIL  %s\n', name);
        end
    end

    fs = 8000;
    f = linspace(0, fs / 2 - 1, 2000);
    db = @(H) 20 * log10(abs(H));

    % ---- Butterworth: exact bilinear-transform magnitude --------------------------------
    for order = [2 4 5 8]
        sos = iir_butter(order, 1000, fs);
        H = freq_resp(sos, f, fs);
        ideal = 1 ./ sqrt(1 + (tan(pi * f / fs) / tan(pi * 1000 / fs)).^(2 * order));
        check(max(abs(abs(H) - ideal)) < 1e-9, sprintf('low-pass order %d matches the bilinear Butterworth', order));
        check(abs(db(freq_resp(sos, 1000, fs)) + 3.0103) < 1e-3, sprintf('order %d is -3.01 dB at fc', order));
        check(size(sos, 1) == ceil(order / 2), sprintf('order %d gives %d sections', order, ceil(order / 2)));
        r = [];
        for k = 1:size(sos, 1)
            r = [r; abs(roots(sos(k, 4:6)))]; %#ok<AGROW>
        end
        check(all(r < 1), sprintf('order %d is stable (max pole radius %.4f)', order, max(r)));
    end
    sos = iir_butter(3, 500, fs, 'high');
    fh = f(2:end);
    ideal = 1 ./ sqrt(1 + (tan(pi * 500 / fs) ./ tan(pi * fh / fs)).^6);
    check(max(abs(abs(freq_resp(sos, fh, fs)) - ideal)) < 1e-9, 'high-pass order 3 matches the bilinear Butterworth');
    check(abs(freq_resp(sos, 0, fs)) < 1e-12, 'high-pass blocks DC');

    % ---- Kaiser FIR: meets the specification, linear phase -------------------------------
    [h, info] = fir_kaiser('low', [1000 1500], fs, 60);
    H = freq_resp(h, f, fs);
    check(max(abs(h - fliplr(h))) < 1e-15, 'FIR is symmetric (linear phase)');
    check(max(db(H(f >= 1500))) <= -59, sprintf('low-pass stop band <= -59 dB (got %.1f)', max(db(H(f >= 1500)))));
    check(max(abs(db(H(f <= 1000)))) < 0.02, 'low-pass pass-band ripple < 0.02 dB');
    check(info.N == numel(h) - 1 && mod(info.N, 2) == 0 && info.delay == info.N / 2, 'FIR order is even, delay N/2');
    [h, ~] = fir_kaiser('high', [1500 2000], fs, 50);
    H = freq_resp(h, f, fs);
    check(max(db(H(f <= 1500))) <= -49 && max(abs(db(H(f >= 2000)))) < 0.05, 'high-pass meets 50 dB');
    [h, ~] = fir_kaiser('band', [500 800 1500 1800], fs, 55);
    H = freq_resp(h, f, fs);
    stop = f <= 500 | f >= 1800;
    pass = f >= 800 & f <= 1500;
    check(max(db(H(stop))) <= -54 && max(abs(db(H(pass)))) < 0.05, 'band-pass meets 55 dB');

    % ---- filtering: cascade equals the transfer function -----------------------------------
    sos = iir_butter(6, 700, fs);
    x = sin(2 * pi * 300 * (0:999) / fs) + 0.5 * sin(2 * pi * 2500 * (0:999) / fs);
    [b, a] = sos_to_tf(sos);
    check(max(abs(sos_filter(sos, x) - filter(b, a, x))) < 1e-10, 'sos_filter equals filter(b, a, x)');

    % ---- fixed point: Q15 cascade tracks the double-precision filter ------------------------
    xq = round(16000 * x / 1.5);
    [yq, sat] = sos_filter_q15(sos, xq);
    yd = sos_filter(quantize_q14(sos) / 2^14, xq);
    err = yq - yd;
    snr = 10 * log10(sum(yd.^2) / sum(err.^2));
    check(sat == 0, 'no saturation at -6 dBFS');
    check(snr > 60, sprintf('Q15 output within %.0f dB SNR of double precision (> 60 dB)', snr));
    q = quantize_q14(sos);
    check(all(abs(q(:)) <= 32767) && all(q(:, 4) == 16384), 'Q14 coefficients fit int16, a0 = 1.0');

    % ---- coefficient sensitivity: cascade survives 16-bit rounding, direct form does not ----
    sos = iir_butter(8, 150, fs);
    rs = [];
    for k = 1:size(sos, 1)
        rs = [rs; abs(roots(quantize_q14(sos(k, 4:6)) / 2^14))]; %#ok<AGROW>
    end
    check(all(rs < 1), 'Q14 cascade stays stable');
    [b, a] = sos_to_tf(sos);
    aq = round(a * 2^15 / max(abs(a))) * max(abs(a)) / 2^15;   % 16-bit direct-form denominator
    check(max(abs(roots(aq))) > max(rs), 'direct form moves the poles further than the cascade');

    % ---- C header export ------------------------------------------------------------------
    sos = iir_butter(4, 1000, fs);
    txt = export_c_header(sos, 'lp1k', fs);
    check(~isempty(strfind(txt, '#define LP1K_SECTIONS   2')), 'header: section count');
    check(~isempty(strfind(txt, 'static const float lp1k_f32[10]')), 'header: f32 array size');
    check(~isempty(strfind(txt, 'static const int16_t lp1k_q15[12]')), 'header: q15 array size');

    fprintf('%d checks, %d failures\n', checks, failures);
    if failures > 0
        error('run_tests:failed', '%d of %d checks failed', failures, checks);
    end
end
