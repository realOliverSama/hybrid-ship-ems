function out = write_ctrace_v1(ctrace, vdir, voyage_id, arm, run_id)
% ppe.write_ctrace_v1 — epoch-2 控制器决策迹线工件（FC-5A / C3-C、C3-D）。
%
% delta_lambda 与 forecast 溯源是 C* 控制器/runner 侧量，不是 Model-E
% logsout 信号：按控制器原生决策节拍（C* 更新网格，10 s）持久化：
%   time, delta_lambda, forecast_source_id, forecast_window_sha256。
% P0（连续 law5 基线）无 lambda 决策：applies=false 的最小记录，绑定方式
% 与 C* 臂相同（终端记录一律携带 ctrace 哈希）。
%
% 数据来源（恢复授权 C3-C：authoritative C* execution state）：
%   run_cstar meta.dlambda_1（决策序列）+ meta.window_sha256（窗口聚合哈希）
%   —— runner 字节冻结不变，截获于运行元数据。
%
% 原子写（tmp → movefile）；文件名：<vdir>/<arm>.ctrace.v1.json。
% 任何写入失败抛错（臂失败 → 原子重跑；不产生半成品）。

doc = struct();
doc.schema = 'ppe_controller_trace_v1';
doc.voyage_id = char(voyage_id);
doc.arm = char(arm);
doc.run_id = char(run_id);
if isfield(ctrace, 'applies') && ctrace.applies
    % 证据层自身校验（Q-OBS-13：RUN_METADATA 层缺失在正确的层失败，
    % 不伪装成 Model-E 信号缺失）
    if ~isfield(ctrace, 'delta_lambda') || isempty(ctrace.delta_lambda) ...
            || ~isfield(ctrace, 'time') || numel(ctrace.time) ~= numel(ctrace.delta_lambda)
        error('PPE:CTRACE:INVALID', 'applies=true 要求 time/delta_lambda 同长非空');
    end
    if ~isfield(ctrace, 'decision_period_s') || ~(ctrace.decision_period_s > 0)
        error('PPE:CTRACE:INVALID', 'decision_period_s 必须为正');
    end
    if ~isfield(ctrace, 'forecast_source_id') || isempty(ctrace.forecast_source_id)
        error('PPE:CTRACE:INVALID', 'forecast_source_id 缺失');
    end
    doc.applies = true;
    doc.decision_period_s = ctrace.decision_period_s;
    doc.n_decisions = numel(ctrace.delta_lambda);
    doc.time = ctrace.time(:);
    doc.delta_lambda = ctrace.delta_lambda(:);
    doc.forecast_source_id = ctrace.forecast_source_id;
    doc.forecast_window_sha256 = ctrace.forecast_window_sha256;
    doc.note = ['captured from the authoritative C* execution state ' ...
        '(run_cstar run metadata; runner bytes unchanged)'];
else
    doc.applies = false;
    doc.forecast_source_id = 'N/A';
    doc.note = 'P0 continuous law-5 baseline: no lambda decisions; forecast source N/A';
end
doc.utc = char(datetime('now', 'TimeZone', 'UTC', 'Format', ...
    'yyyy-MM-dd''T''HH:mm:ss''Z'));

if ~exist(vdir, 'dir'), mkdir(vdir); end
fp = fullfile(vdir, [char(arm) '.ctrace.v1.json']);
tmp = [fp '.writing'];
fid = fopen(tmp, 'w');
fwrite(fid, jsonencode(doc, 'PrettyPrint', true));
fclose(fid);
movefile(tmp, fp, 'f');

out = struct('path', fp, 'sha256', model.sha256_file(fp), ...
    'applies', doc.applies);
end
