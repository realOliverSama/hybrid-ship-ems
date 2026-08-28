% 55_repro_check.m — Phase 2 复现验证（FR-212）
% 以"干净进程 + 缓存续跑"方式重跑 53/54，验证 runs_phase2.csv 与 gate_result.json 一致。
% 说明：53 带逐航次断点缓存（runs/run_*.mat），重跑 0 重仿真（FR-204 test 航次绝不重跑），
% 因此复现语义 = 相同 commit 下后处理链（读取缓存→聚合→判定）确定。
% 用法（model/simulink_modelD 下）：eval(fileread('scripts/55_repro_check.m'))
% 注意：eval(fileread()) 共享工作区且不支持局部函数；本文件所有变量加 rc_ 前缀防 53/54 泄漏污染。
rcRoot = pwd;  % 约定 cwd = model/simulink_modelD
cd(rcRoot);

rcCsv = fullfile(rcRoot, 'results', 'ecms_phase2', 'runs_phase2.csv');
rcJson = fullfile(rcRoot, 'results', 'ecms_phase2', 'gate_result.json');
rcBakCsv = [rcCsv '.bak_repro']; rcBakJson = [rcJson '.bak_repro'];
copyfile(rcCsv, rcBakCsv); copyfile(rcJson, rcBakJson);

eval(fileread(fullfile('scripts', '53_run_phase2_matrix.m')));   % 应 0 重仿真（全部命中缓存）
eval(fileread(fullfile('scripts', '54_phase2_gate_judge.m')));

rcF = fopen(rcCsv, 'rb');     rcD1 = fread(rcF, '*uint8'); fclose(rcF);
rcF = fopen(rcBakCsv, 'rb');  rcD2 = fread(rcF, '*uint8'); fclose(rcF);
rcSameCsv = isequal(rcD1, rcD2);
% JSON：54 的文本键序非确定（容器迭代序），对决策相关字段逐项容差比较
rcJ1 = jsondecode(fileread(rcJson)); rcJ2 = jsondecode(fileread(rcBakJson));
rcSameJson = strcmp(rcJ1.recommendation, rcJ2.recommendation) ...
    && abs(rcJ1.nn_oracle_capture - rcJ2.nn_oracle_capture) < 1e-9 ...
    && abs(rcJ1.fuel_c4_S20_kg - rcJ2.fuel_c4_S20_kg) < 1e-9 ...
    && abs(rcJ1.generalization.saving_diff_pct_median - rcJ2.generalization.saving_diff_pct_median) < 1e-9 ...
    && abs(rcJ1.generalization.saving_diff_pct_min - rcJ2.generalization.saving_diff_pct_min) < 1e-9 ...
    && rcJ1.generalization.n_test_voyages == rcJ2.generalization.n_test_voyages;
rcPass = rcSameCsv && rcSameJson;
fprintf('[55] runs_phase2.csv 逐位一致: %d | gate_result.json 决策字段一致: %d\n', rcSameCsv, rcSameJson);
if rcPass
    fprintf('[55] REPRO PASS ✓（同 commit 下结果一致）\n');
else
    fprintf('[55] REPRO FAIL ✗\n');
end
delete(rcBakCsv); delete(rcBakJson);
assert(rcPass, 'GATE:repro_failed');
