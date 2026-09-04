function reset_bench_nominal()
% gate_e.reset_bench_nominal — case 隔离不变量（DGN-15/FR-491，hygiene）
%   每个 case 运行前显式恢复全部 bench 注入通道 nominal：case 拥有完整输入初始化，
%   禁止依赖上一 case 的清理状态。assignin 机制与 run_scenario_sim 同通道。
%   ⚠ 实现约束（2026-09-05 40 号 E05 事故实证）：禁止在本函数内对 base 重跑
%   config 脚本——脚本型 Gate 与 base 共享工作区，ModelE_TestScenarios.m 的内部
%   循环变量（for k = 1:10）会覆写调用方的同名循环变量（六档浅水全部退化为
%   SCENARIOS(10)）。故本函数只做显式通道恢复，默认值与 config/ModelE_TestScenarios.m
%   冻结默认表逐字一致（GEN_CMD_STOP_TS L278 / VOYAGE_MANUAL_CMD_TS L282 /
%   EMS_HEALTH_TS L285 / ADAPTER_FORCE_* L302-304）；Parameter 通道注入（如 44 号
%   D4 的 hold_ticks）由该 case 所属链自有的配置重跑负责恢复。
%   幂等：重复调用无额外副作用。10b/40/42/43/44 各 case 入口接线（T-006）。

    assignin('base', 'ADAPTER_FORCE_MODE', 0);
    assignin('base', 'ADAPTER_FORCE_P1_KW', 0);
    assignin('base', 'ADAPTER_FORCE_P2_KW', 0);
    assignin('base', 'GEN_CMD_STOP_TS', [0 0 0; 1e6 0 0]);
    assignin('base', 'VOYAGE_MANUAL_CMD_TS', [0 0.30; 1e6 0.30]);
    assignin('base', 'EMS_HEALTH_TS', [0 1; 1e6 1]);
    assignin('base', 'VOYAGE_CTRL_MODE', 0);
    assignin('base', 'PMS_SOURCE_MODE', 0);
    assignin('base', 'EMS_CTRL_LAW', 0);
    assignin('base', 'EMS_MODE', 0);
end
