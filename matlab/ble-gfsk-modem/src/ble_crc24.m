function crc = ble_crc24(bits, init)
%BLE_CRC24  Bluetooth LE CRC-24 over a bit stream (bits in transmission order).
%   crc = ble_crc24(bits)          init = 0x555555 (advertising channels)
%   crc = ble_crc24(bits, init)    init from the connection set-up
%
%   Polynomial x^24 + x^10 + x^9 + x^6 + x^4 + x^3 + x + 1, computed with a
%   24-bit linear feedback shift register, one bit at a time, as in the Core
%   Specification (Vol 6, Part B, 3.1.1). The PDU is sent least significant
%   bit first; the 24 CRC bits returned here are in transmission order.
%
%   Property used by the receiver: running the register over PDU + CRC
%   leaves it at zero when no bit was corrupted.
%
%   Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

    if nargin < 2
        init = hex2dec('555555');
    end
    % reg(1) is position 0 of the specification's register, reg(24) position 23.
    reg = bitget(init, 1:24);
    taps = false(1, 24);
    taps([1 3 4 6 9 10] + 1) = true;   % x, x^3, x^4, x^6, x^9, x^10 feed back (x^0 handled below)
    for i = 1:numel(bits)
        fb = xor(reg(24), bits(i));
        reg = [0, reg(1:23)];
        reg(1) = fb;
        reg(taps) = xor(reg(taps), fb);
    end
    crc = fliplr(reg);   % position 23 is transmitted first
end
