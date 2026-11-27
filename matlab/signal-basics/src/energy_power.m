function [E, P] = energy_power(x, dt)
%ENERGY_POWER  Energy and average power of a sampled signal.
%   [E, P] = energy_power(x, dt)
%
%   E = integral |x(t)|^2 dt   ~  sum |x|^2 * dt
%   P = E / (record length)       average power over the record
%
%   An energy signal (a pulse) has finite E and P -> 0 as the record grows.
%   A power signal (anything periodic) has infinite E, but finite P: the
%   energy of one period divided by the period, whatever the record length.
%
%   Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

    E = sum(abs(x(:)).^2) * dt;
    P = E / (numel(x) * dt);
end
