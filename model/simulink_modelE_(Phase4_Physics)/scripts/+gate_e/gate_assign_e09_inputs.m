function gate_assign_e09_inputs()
% gate_e.assign_e09_inputs — E09：DG1 带载 + t=80 s 双机全停 → 电池兜底 + 欠压保护链
    SCN = evalin('base', 'SCENARIOS(1)');
    SCN.voyage_distance_m = 2000;
    gate_e.gate_assign_bench_inputs(1, 400, 0, [0 0 0; 80 1 1; 1e5 1 1], SCN);
end
