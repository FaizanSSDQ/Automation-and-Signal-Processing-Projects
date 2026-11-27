function w = window_fn(kind, L)
%WINDOW_FN  Spectral analysis windows of length L (symmetric), as a column.
%   w = window_fn('rect' | 'hann' | 'hamming' | 'blackman', L)
%
%   Written out so the project needs no toolbox. Each window trades main-lobe
%   width for side-lobe height; window_sidelobe() measures the trade.
%
%   Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

    k = (0:L-1).';
    x = 2 * pi * k / (L - 1);
    switch lower(kind)
        case 'rect'
            w = ones(L, 1);
        case 'hann'
            w = 0.5 - 0.5 * cos(x);
        case 'hamming'
            w = 0.54 - 0.46 * cos(x);
        case 'blackman'
            w = 0.42 - 0.5 * cos(x) + 0.08 * cos(2 * x);
        otherwise
            error('window_fn:kind', 'unknown window "%s"', kind);
    end
end
