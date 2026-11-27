function mae = hr_error(hr_bpm, t_hr, sim)
%HR_ERROR  Mean absolute heart-rate error (bpm) against the simulation's true rate.
%   mae = hr_error(hr_bpm, t_hr, sim)
%
%   Author: Faizan Saleem Siddiqui (https://github.com/FaizanSSDQ)

    if isempty(hr_bpm)
        mae = Inf;
        return;
    end
    truth = interp1(sim.t, sim.hr_true, t_hr, 'linear', 'extrap');
    mae = mean(abs(hr_bpm - truth));
end
