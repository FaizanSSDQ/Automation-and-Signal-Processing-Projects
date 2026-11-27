function run_tests()
%RUN_TESTS  Checks for the Bluetooth LE GFSK modem. Runs in MATLAB and GNU Octave.
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

    rand('state', 7); %#ok<RAND>

    % ---- CRC-24 ---------------------------------------------------------------------------
    msg = double(rand(1, 37 * 8) > 0.5);
    crc = ble_crc24(msg);
    check(numel(crc) == 24, 'CRC is 24 bits');
    check(isequal(ble_crc24([msg, crc]), zeros(1, 24)), 'message + CRC leaves a zero register');
    detected = 0;
    for i = 1:200
        bad = [msg, crc];
        bad(randi(numel(bad))) = ~bad(randi(numel(bad)));
        p = randi(numel(bad) - 23);
        bad(p:p + 23) = xor(bad(p:p + 23), [1, double(rand(1, 22) > 0.5), 1]);   % burst <= 24 bits
        detected = detected + any(ble_crc24(bad));
    end
    check(detected == 200, sprintf('all 200 corrupted packets detected (%d)', detected));
    check(~isequal(ble_crc24(msg), ble_crc24(msg, 0)), 'CRC depends on the initial value');

    % ---- whitening -------------------------------------------------------------------------
    w = ble_whiten(zeros(1, 254), 37);
    check(isequal(w(1:127), w(128:254)), 'whitening sequence repeats every 127 bits');
    check(sum(w(1:127)) == 64, 'maximum-length sequence: 64 ones in 127');
    check(isequal(ble_whiten(ble_whiten(msg, 38), 38), msg), 'whitening twice restores the data');
    check(~isequal(ble_whiten(msg, 37), ble_whiten(msg, 38)), 'each channel whitens differently');

    % ---- packet format -----------------------------------------------------------------------
    pdu = [hex2dec('02'), 6, 1, 2, 3, 4, 5, 6];
    [bits, parts] = ble_packet(pdu);
    check(numel(bits) == 8 + 32 + 8 * numel(pdu) + 24, 'packet length');
    aa = bitget(hex2dec('8E89BED6'), 1:32);
    check(isequal(bits(parts.aa), aa), 'access address sent LSB first');
    check(bits(1) ~= aa(1) && all(diff(bits(1:8)) ~= 0), 'preamble alternates and differs from the AA start');

    % ---- modulator ---------------------------------------------------------------------------
    sps = 8;
    [s, fr] = gfsk_mod([0 0 0 ones(1, 20) zeros(1, 20) 0 0 0], sps);
    check(max(abs(abs(s) - 1)) < 1e-12, 'constant envelope');
    check(abs(max(fr) - 0.25) < 1e-3 && abs(min(fr) + 0.25) < 1e-3, 'deviation +-0.25 cycles/symbol (250 kHz)');

    % ---- clean link: every packet decodes -----------------------------------------------------
    for ch = [37 38 39 12]
        [bits, ~] = ble_packet(pdu, ch);
        s = gfsk_mod([zeros(1, 20), bits, zeros(1, 20)], sps);
        rx = ble_receive(s, sps, numel(pdu), ch);
        check(rx.found && rx.crc_ok && isequal(rx.pdu, pdu), sprintf('clean packet on channel %d', ch));
    end

    % ---- impaired link: noise, timing and carrier offset ---------------------------------------
    [bits, ~] = ble_packet(pdu, 37);
    s = gfsk_mod([zeros(1, 33), bits, zeros(1, 20)], sps);
    s = [zeros(1, 3), s];                                     % arbitrary sample delay
    r = awgn_channel(s, 14, sps, 0.05, 3);                    % 14 dB, 50 kHz offset
    rx = ble_receive(r, sps, numel(pdu), 37);
    check(rx.found && rx.crc_ok && isequal(rx.pdu, pdu), 'packet at 14 dB with 50 kHz offset');
    check(abs(rx.cfo - 0.05) < 0.01, sprintf('offset estimated: %.3f cycles/symbol', rx.cfo));
    check(rx.start == 34, sprintf('packet start found at symbol %d', rx.start));
    rx = ble_receive(awgn_channel(s, 14, sps, 0, 4), sps, numel(pdu), 38);
    check(~rx.crc_ok, 'wrong channel (whitening) fails the CRC');

    % ---- bit error rate falls with Eb/N0 ----------------------------------------------------------
    ber = gfsk_ber([6 10 14], 20000);
    check(ber(1) > ber(2) && ber(2) > ber(3), sprintf('BER decreases: %.2g %.2g %.2g', ber));
    check(ber(3) < 1e-3, 'BER below 0.1 % at 14 dB (LE sensitivity criterion)');

    fprintf('%d checks, %d failures\n', checks, failures);
    if failures > 0
        error('run_tests:failed', '%d of %d checks failed', failures, checks);
    end
end
