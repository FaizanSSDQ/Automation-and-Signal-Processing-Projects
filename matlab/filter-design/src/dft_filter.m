function [y, G, Y] = dft_filter(g, H)
%DFT_FILTER  Filter one period of samples in the frequency domain.
%   [y, G, Y] = dft_filter(g, H)
%
%       G = DFT{g},   Y = G .* H,   y = IDFT{Y}
%
%   Multiplying DFTs is circular convolution: the result is the filtered
%   periodic signal, which is exactly right for a periodic input such as a
%   pulse train. The imaginary part of y is rounding noise when H is
%   symmetric, and is dropped.
%
%   Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

    G = fft(g(:).');
    Y = G .* H(:).';
    y = real(ifft(Y));
end
