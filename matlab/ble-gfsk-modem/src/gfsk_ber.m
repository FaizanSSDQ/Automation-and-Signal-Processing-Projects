function [ber, nerr] = gfsk_ber(ebn0_db, nbits, sps, seed)
%GFSK_BER  Monte-Carlo bit error rate of the GFSK discriminator receiver.
%   [ber, nerr] = gfsk_ber(ebn0_db, nbits)
%   [ber, nerr] = gfsk_ber(ebn0_db, nbits, sps, seed)
%
%   Random bits are modulated (BT 0.5, h 0.5), sent through AWGN and
%   demodulated. Symbol timing is ideal (the offset that is best for a
%   clean signal), so the result isolates the modem from synchronisation.
%   One value per Eb/N0 entry.
%
%   Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

    if nargin < 3
        sps = 8;
    end
    if nargin < 4
        seed = 1;
    end
    rand('state', seed); %#ok<RAND>
    bits = double(rand(1, nbits) > 0.5);
    s = gfsk_mod([0 0 0 bits 0 0 0], sps);          % guard symbols for the filter tails
    clean = gfsk_demod(s, sps);
    k = best_offset(clean, bits);
    ber = zeros(size(ebn0_db));
    nerr = zeros(size(ebn0_db));
    for i = 1:numel(ebn0_db)
        r = awgn_channel(s, ebn0_db(i), sps, 0, seed + i);
        soft = gfsk_demod(r, sps);
        hat = double(soft(k, 4:3 + nbits) > 0);
        nerr(i) = sum(hat ~= bits);
        ber(i) = nerr(i) / nbits;
    end
end

function k = best_offset(soft, bits)
    n = numel(bits);
    err = zeros(1, size(soft, 1));
    for k = 1:size(soft, 1)
        err(k) = sum(double(soft(k, 4:3 + n) > 0) ~= bits) - 1e-6 * min(abs(soft(k, 4:3 + n)));
    end
    [~, k] = min(err);
end
