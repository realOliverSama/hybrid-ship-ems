function dirpath = formal_outdir(ROOT, formal_epoch)
% ppe.formal_outdir — 正式输出命名空间解析（epoch 绑定，恢复授权 §2）。
%
%   epoch 1 → results/ppe_practical/formal          （历史命名空间，不可变）
%   epoch 2 → results/ppe_practical/formal_epoch2   （恢复执行命名空间）
%
% 两个 epoch 的终端记录/轨迹/审计工件不得共享命名空间。

switch double(formal_epoch)
    case 1
        dirpath = fullfile(ROOT, 'results', 'ppe_practical', 'formal');
    case 2
        dirpath = fullfile(ROOT, 'results', 'ppe_practical', 'formal_epoch2');
    otherwise
        error('PPE:NS:EPOCH', 'formal_epoch 必须为 1 或 2');
end
end
