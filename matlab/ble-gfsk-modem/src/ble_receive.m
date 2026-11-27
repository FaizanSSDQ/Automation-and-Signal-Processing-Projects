function rx = ble_receive(r, sps, n_pdu_bytes, channel, access_address)
%BLE_RECEIVE  Find, demodulate and check one LE 1M packet in a sampled signal.
%   rx = ble_receive(r, sps, n_pdu_bytes)
%   rx = ble_receive(r, sps, n_pdu_bytes, channel, access_address)
%
%   1. Discriminator soft values for every timing offset.
%   2. Frequency-offset removal: a carrier offset shifts every soft value by
%      the same amount; the mean over the alternating preamble estimates it.
%   3. Access-address search: correlate the +-1 pattern of the known
%      access address with the soft values at every timing offset and
%      position; the best match sets timing and packet start.
%   4. Slice the PDU and CRC bits, de-whiten, check the CRC.
%
%   rx fields: found, crc_ok, pdu (bytes), bits (de-whitened PDU bits),
%   offset (timing, samples), start (symbol index of the preamble),
%   cfo (cycles per symbol), score (correlation, 1 = perfect).
%
%   Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

    if nargin < 4
        channel = 37;
    end
    if nargin < 5
        access_address = hex2dec('8E89BED6');
    end
    aa = 2 * bitget(access_address, 1:32) - 1;
    soft = gfsk_demod(r, sps);
    nbody = 8 * n_pdu_bytes + 24;
    best = struct('score', -Inf, 'k', 1, 'pos', 1, 'cfo', 0);
    for k = 1:sps
        v = soft(k, :);
        for pos = 9:(numel(v) - 32 - nbody + 1)
            seg = v(pos:pos + 31);
            cfo = mean(v(pos - 8:pos - 1));                  % preamble averages to the offset
            seg = seg - cfo;
            score = (aa * seg.') / (32 * max(mean(abs(seg)), eps));
            if score > best.score
                best = struct('score', score, 'k', k, 'pos', pos, 'cfo', cfo);
            end
        end
    end
    rx = struct('found', best.score > 0.6, 'crc_ok', false, 'pdu', [], 'bits', [], ...
                'offset', best.k - 1, 'start', best.pos - 8, 'cfo', best.cfo / (2 * pi), 'score', best.score);
    if ~rx.found
        return;
    end
    v = soft(best.k, :) - best.cfo;
    body = double(v(best.pos + 32 : best.pos + 31 + nbody) > 0);
    body = ble_whiten(body, channel);
    pdu = body(1:end - 24);
    rx.bits = pdu;
    rx.crc_ok = isequal(ble_crc24([pdu, body(end - 23:end)]), zeros(1, 24));
    rx.pdu = bits_to_bytes(pdu);
end

function bytes = bits_to_bytes(b)
    n = numel(b) / 8;
    bytes = zeros(1, n);
    for k = 1:n
        bytes(k) = sum(b(8 * (k - 1) + (1:8)) .* 2.^(0:7));
    end
end
