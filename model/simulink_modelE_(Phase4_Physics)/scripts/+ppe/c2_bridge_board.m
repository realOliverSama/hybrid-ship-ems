function bridge = c2_bridge_board(token, port, use_mock)
% ppe.c2_bridge_board — C2 推理**板上后端**文件桥（phase4-g，spec
% .specs/phase4-g-reatt-board-deployment D7）。与 ppe.c2_bridge 同协议同
% 传输（req/resp .mat 目录轮询）；唯一差异 = Python 服务进程为
% ppe.c2_server_board（推理在 EMS01_CHIL_REATT8 固件板，int8 全 CPU 形态），
% 非本进程 keras fp32。
%
% 本文件为 phase4-g 新增副本；ppe.c2_bridge.m（冻结件）零改动。
% 板串口 port 缺省取环境变量 EMS_REATT_PORT，再缺省报错（fail-closed）。
% use_mock = true 时服务以 --mock 启动（host int8 模拟板，联调用；产物必须
% 标 MOCK，不作板级证据）。
%
% bridge = struct with function handles（与 c2_bridge 同形）：
%   Pbar = bridge.request(voyage_id, t0, t_raw, cols)  % cols: struct of 11 H1
%   bridge.stop()
%
% Determinism: response provenance carries keras_sha256（源权重身份，FR836
% 冻结值——板 int8 由该权重量化派生）+ backend 字段（board_int8 /
% mock_host_int8 + tflite sha256）；MATLAB 侧 keras 身份校验同 c2_bridge 不变。
ROOT = ppe.ppe_root();
base = fullfile(ROOT, 'artifacts', 'ppe_softhil', token);
reqdir = fullfile(base, 'req');
respdir = fullfile(base, 'resp');
stopf = fullfile(base, 'STOP');
if ~exist(reqdir, 'dir'), mkdir(reqdir); end
if ~exist(respdir, 'dir'), mkdir(respdir); end
if exist(stopf, 'file'), delete(stopf); end

mockarg = '';
if nargin >= 3 && use_mock
    mockarg = ' --mock';
    port = 'mock';   % --mock 不需要串口
elseif nargin < 2 || isempty(port)
    port = getenv('EMS_REATT_PORT');
end
if isempty(port)
    error('PPE:BRIDGE:PORT', ...
        '板串口缺失：port 形参或环境变量 EMS_REATT_PORT');
end

py = 'python3';
[st, ~] = system([py ' -c "import serial,numpy,scipy;print(1)"']);
if st ~= 0
    % 回退仓库根 .venv_tf（含 pyserial/scipy/TF；冻结 c2_bridge.m 同款推导）
    cand = fullfile(fileparts(ROOT), '..', '.venv_tf', 'bin', 'python');
    if exist(cand, 'file') ~= 2 ...
            || system(['"' cand '" -c "import serial,numpy,scipy;print(1)"']) ~= 0
        error('PPE:BRIDGE:PY', '找不到含 numpy/scipy/pyserial 的 python');
    end
    py = cand;
end

pydir = fullfile(ROOT, 'python');
env = sprintf('PYTHONPATH="%s:%s" ', pydir, ROOT);
logf = fullfile(base, 'server.log');
cmd = sprintf('%s"%s" -m ppe.c2_server_board --reqdir "%s" --respdir "%s" --stop-file "%s" --port "%s"%s > "%s" 2>&1 &', ...
    env, py, reqdir, respdir, stopf, port, mockarg, logf);
system(cmd);

mock_on = ~isempty(mockarg);
bridge = struct();
bridge.request = @(vid, t0, t_raw, cols) local_request(reqdir, respdir, vid, t0, t_raw, cols, mock_on);
bridge.stop = @() local_stop(stopf);
bridge.reqdir = reqdir;
bridge.respdir = respdir;
bridge.mock = mock_on;
end

function Pbar = local_request(reqdir, respdir, vid, t0, t_raw, cols, mock_on)
req = struct('voyage_id', char(vid), 't0', double(t0), 't', t_raw(:));
fn = fieldnames(cols);
for i = 1:numel(fn)
    req.(['H1_' fn{i}]) = cols.(fn{i})(:);
end
key = sprintf('%s_%.0f', char(vid), t0);
reqf = fullfile(reqdir, ['req_' key '.mat']);
respf = fullfile(respdir, ['resp_' key '.mat']);
tmpf = fullfile(reqdir, ['tmp_' key '.mat']);
save(tmpf, '-struct', 'req', '-v7');
movefile(tmpf, reqf, 'f');   % 原子改名：server 永不读到半写请求（传输层同 c2_bridge）
t_deadline = tic;
while true
    if exist(respf, 'file')
        pause(0.2);   % writer flush guard
        S = load(respf);
        break;
    end
    if toc(t_deadline) > 300
        error('PPE:BRIDGE:TIMEOUT', 'no response for %s', key);
    end
    pause(0.2);
end
if isfield(S, 'error')
    error('PPE:BRIDGE:REMOTE', 'server fail-closed for %s: %s', key, S.error);
end
Pbar = S.Pbar_q50(:);
prov = S.provenance;
if isstruct(prov)
    expected = 'd8da6af4667875c45de7de90383d2b6b750420f5af62c7dcdcb1e5997897f894';
    if isfield(prov, 'keras_sha256') && ~strcmp(prov.keras_sha256, expected)
        error('PPE:BRIDGE:IDENTITY', 'response keras SHA mismatch');
    end
    backend_ok = strcmp(prov.backend, 'board_int8') ...
        || (mock_on && strcmp(prov.backend, 'mock_host_int8'));
    if ~isfield(prov, 'backend') || ~backend_ok
        error('PPE:BRIDGE:BACKEND', 'response 非板后端（backend 字段缺失/不符）');
    end
end
if numel(Pbar) ~= 30 || any(~isfinite(Pbar))
    error('PPE:BRIDGE:SHAPE', 'Pbar contract broken for %s', key);
end
end

function local_stop(stopf)
fid = fopen(stopf, 'w');
if fid > 0, fwrite(fid, 'stop'); fclose(fid); end
end
