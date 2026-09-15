function ROOT = modele_root()
% t445.modele_root — Model E 工作区根（mfilename + pwd 兜底，禁绝对路径）
HERE = fileparts(mfilename('fullpath'));
ROOT = fileparts(fileparts(HERE));  % +t445 -> scripts -> ModelE
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir')
    ROOT = pwd;
end
end
