function [bits, parts] = ble_packet(payload_bytes, channel, access_address)
%BLE_PACKET  Build an LE 1M uncoded packet as a bit stream in air order.
%   [bits, parts] = ble_packet(pdu_bytes)                         advertising, channel 37
%   [bits, parts] = ble_packet(pdu_bytes, channel, access_address)
%
%   Air order, every field least significant bit first:
%       preamble (8)  access address (32)  PDU (8 x n)  CRC (24)
%   The preamble alternates and starts with the opposite of the access
%   address's first bit (0xAA or 0x55). PDU and CRC are whitened.
%   parts gives the bit index ranges of each field (for the figures).
%
%   Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

    if nargin < 2
        channel = 37;
    end
    if nargin < 3
        access_address = hex2dec('8E89BED6');   % the advertising access address
    end
    aa = bitget(access_address, 1:32);
    preamble = mod((0:7) + 1 + aa(1), 2);      % first bit differs from aa(1)
    pdu = bytes_lsb_first(payload_bytes);
    crc = ble_crc24(pdu);
    body = ble_whiten([pdu, crc], channel);
    bits = [preamble, aa, body];
    parts = struct('preamble', 1:8, 'aa', 9:40, 'pdu', 40 + (1:numel(pdu)), ...
                   'crc', 40 + numel(pdu) + (1:24));
end

function b = bytes_lsb_first(bytes)
    b = zeros(1, 8 * numel(bytes));
    for k = 1:numel(bytes)
        b(8 * (k - 1) + (1:8)) = bitget(bytes(k), 1:8);
    end
end
