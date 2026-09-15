function test_t445_t002()
% test_t445_t002 — T-002 入口（窗装配 + 真 ZOH 机器测试）
HERE = fileparts(mfilename('fullpath'));
ROOT = fileparts(fileparts(HERE));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));
t445.run_t002_tests();
end
