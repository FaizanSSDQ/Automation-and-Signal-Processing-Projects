function out = ble_whiten(bits, channel)
%BLE_WHITEN  Bluetooth LE data whitening (and de-whitening: the same operation).
%   out = ble_whiten(bits, channel)    channel index 0..39 (37, 38, 39 = advertising)
%
%   A 7-stage linear feedback shift register with polynomial x^7 + x^4 + 1
%   (Core Specification Vol 6, Part B, 3.2). Position 0 starts at 1 and
%   positions 1..6 hold the channel index, most significant bit in
%   position 1. Each clock, the output of position 6 is XORed onto the data
%   bit, fed back into position 0 and XORed into position 4.
%
%   Whitening breaks up long runs of equal bits so the receiver's frequency
%   offset and clock recovery keep working; applying it twice restores the
%   data.
%
%   Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

    reg = [1, bitget(channel, 6:-1:1)];   % reg(k) is position k-1
    out = bits;
    for i = 1:numel(bits)
        fb = reg(7);
        out(i) = xor(bits(i), fb);
        reg = [fb, reg(1:6)];
        reg(5) = xor(reg(5), fb);
    end
end
