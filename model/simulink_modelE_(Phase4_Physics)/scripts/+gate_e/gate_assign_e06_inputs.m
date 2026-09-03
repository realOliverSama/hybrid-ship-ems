function gate_assign_e06_inputs()
% gate_e.assign_e06_inputs — E06：force DG2 启动 + t=50 s 停机注入（aborted-start 由 E06 时序覆盖）
    SCN = evalin('base', 'SCENARIOS(1)');
    gate_e.gate_assign_bench_inputs(1, 0, 300, [0 0 0; 50 0 1; 1e5 0 1], SCN);
end
