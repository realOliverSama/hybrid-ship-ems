function ROOT = ppe_root()
% ppe.ppe_root — Model-E workspace root (mfilename-derived; pwd fallback).
p = fileparts(mfilename('fullpath'));
if isempty(p)
    p = pwd;
end
ROOT = fileparts(fileparts(p));   % scripts/+ppe -> scripts -> ModelE
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir')
    ROOT = pwd;
end
end
