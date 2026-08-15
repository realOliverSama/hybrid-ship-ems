%% ========================================================================
%  05_export_dataset.m — 按完整航次划分 train/validation/test（阶段13）
%  划分规则（按完整航次，禁止滑动窗口后再随机划分）：
%    train:      S01, S02, S04, S07, S08（静水/顺水/阻力渐增/桥区机动/浅滩群）
%    validation: S03, S05（逆水/阻力渐减）
%    test:       S06（阻力阶跃）, S09（应急出动急加速）—— 训练中不可见
%  输出: datasets/train.csv, datasets/validation.csv, datasets/test.csv
%        datasets/README.md（划分说明）
%  附加列: P_load_future_max_60s（未来 60 s 负荷峰值，供负荷预测监督学习）
% ========================================================================
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
addpath(fullfile(ROOT, 'scripts'));
model.ensure_canonical(ROOT);
rawDir = fullfile(ROOT, 'datasets', 'raw');
if ~exist(rawDir, 'dir'), error('先运行 scripts/04_run_scenarios.m'); end

trainScen = [1 2 4 7 8];
valScen   = [3 5];
testScen  = [6 9];

trainFiles = model.pick_runs(rawDir, trainScen);
valFiles   = model.pick_runs(rawDir, valScen);
testFiles  = model.pick_runs(rawDir, testScen);
fprintf('[05] train: %d runs, validation: %d runs, test: %d runs\n', ...
    numel(trainFiles), numel(valFiles), numel(testFiles));

model.write_split(fullfile(ROOT, 'datasets', 'train.csv'), trainFiles);
model.write_split(fullfile(ROOT, 'datasets', 'validation.csv'), valFiles);
model.write_split(fullfile(ROOT, 'datasets', 'test.csv'), testFiles);

% 航次名称（用于 README 说明）
grp = {trainFiles, valFiles, testFiles};
grpName = cell(3, 1);
for g = 1:3
    nm = cell(numel(grp{g}), 1);
    for i = 1:numel(grp{g})
        S = load(grp{g}{i}, 'SCN');
        nm{i} = S.SCN.name;
    end
    grpName{g} = strjoin(nm, ', ');
end

fid = fopen(fullfile(ROOT, 'datasets', 'README.md'), 'w', 'n', 'UTF-8');
fprintf(fid, '# 数据集划分说明\n\n');
fprintf(fid, '- 按**完整航次**划分（禁止先做重叠滑动窗口再随机划分）。\n');
fprintf(fid, '- 每个航次 1 s 采样，字段见 CSV 列名；`time_s` 为航次内时间，`scenario_id` 为工况号，`ems_mode` 为 EMS 模式。\n');
fprintf(fid, '- `P_load_future_max_60s`：当前时刻后 60 s 内的总负荷峰值 [kW]（负荷预测监督目标，末端不足 60 s 填 NaN）。\n');
fprintf(fid, '- train: %s\n', grpName{1});
fprintf(fid, '- validation: %s\n', grpName{2});
fprintf(fid, '- test: %s\n', grpName{3});
fprintf(fid, '- 注意：当前 AI 预测源为 persistence，Baseline 与 AI 模式数据一致；接入真实预测后需重新生成。\n');
fclose(fid);
fprintf('[05] 数据集导出完成 -> %s\n', fullfile(ROOT, 'datasets'));
