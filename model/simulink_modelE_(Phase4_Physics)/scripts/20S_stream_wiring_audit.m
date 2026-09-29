%% ========================================================================
%  20S_stream_wiring_audit.m — S4-P3 B2 步骤①（规格 S4_P3_STREAM_PLANT_SPEC.md §1 ⚠️）
%  用途: HybridShip_ModelE_PlantStepE.slx 只读接线实测（load_system，不 save），
%        为流驱动变体（20S build）注入点决策提供机器可读事实：
%          a) 根级全边表（depth-1 全部 In/Out 端口连线，端点定位到块+口型+序号）
%          b) 04_Load_Measurement 内部结构（P_load_total 合成点：Inport 清单 + Sum 输入数）
%          c) 03_Propulsion_Plant 输出的全部根级消费者
%          d) 02/03 反馈环判定（02→03 与 03→02 边存在性）
%          e) 08_Undervolt_Derate 的降额输出的全部根级消费者（回授判定）
%          f) 能量域（06/07/09/10/13/14/15/16）的根级信号源清单（推进侧状态消费判定）
%  红线: 本脚本零写模型（无 save_system）；主模型不装载不改动。
%  产物: results/s4/plantstepE/stream_wiring_audit.json
%  运行: cd <模型工作区根>; eval(fileread('scripts/20S_stream_wiring_audit.m'))
% ========================================================================
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end

MDL_PS = 'HybridShip_ModelE_PlantStepE';
psPath = fullfile(ROOT, 'model', [MDL_PS '.slx']);
if ~exist(psPath, 'file'), error('GATE:P3AUDIT:NO_PS — %s 不存在', psPath); end
auditDir = fullfile(ROOT, 'results', 's4', 'plantstepE');
if ~exist(auditDir, 'dir'), mkdir(auditDir); end

[~, h0] = system(['shasum -a 256 ''' psPath '''']);
tk0 = regexp(h0, '([0-9a-f]{64})', 'tokens', 'once');
if isempty(tk0), error('GATE:P3AUDIT:HASH — 哈希失败'); end

if bdIsLoaded(MDL_PS), close_system(MDL_PS, 0); end
load_system(psPath);   % 只读装载（结束时 close 不保存）

% ---------- depth-1 全块盘点 + 端口注册 ----------
b1 = find_system(MDL_PS, 'SearchDepth', 1);
b1 = b1(2:end);  % 去掉模型自身
nB = numel(b1);
blkName = cell(nB, 1); blkType = cell(nB, 1);
portMap = containers.Map('KeyType', 'double', 'ValueType', 'any');
for i = 1:nB
    blkName{i} = strrep(b1{i}, [MDL_PS '/'], '');
    blkType{i} = get_param(b1{i}, 'BlockType');
    ph = get_param(b1{i}, 'PortHandles');
    fn = fieldnames(ph);
    for kf = 1:numel(fn)
        arr = ph.(fn{kf});
        for p = 1:numel(arr)
            portMap(arr(p)) = struct('block', blkName{i}, ...
                'port', fn{kf}, 'no', p);
        end
    end
end

% ---------- 根级全边表（每条线记 src→dst 全部端点对） ----------
edges = cell(0, 6);
unmapped = cell(0, 1);
for i = 1:nB
    ph = get_param(b1{i}, 'PortHandles');
    if ~isfield(ph, 'Outport'), continue; end
    for p = 1:numel(ph.Outport)
        lh = get_param(ph.Outport(p), 'Line');
        if lh == -1, continue; end
        src = get_param(lh, 'SrcPortHandle');
        dsts = get_param(lh, 'DstPortHandle');
        for j = 1:numel(dsts)
            if ~isKey(portMap, src) || ~isKey(portMap, dsts(j)) ...
                    || src <= 0 || dsts(j) <= 0
                unmapped(end+1, 1) = {sprintf('%s/out%d -> line %d (src=%g dst=%g)', ...
                    blkName{i}, p, lh, src, dsts(j))}; %#ok<AGROW>
                continue;
            end
            s = portMap(src); d = portMap(dsts(j));
            edges(end+1, :) = {s.block, s.port, s.no, ...
                d.block, d.port, d.no}; %#ok<AGROW>
        end
    end
end
% 去重
nE = size(edges, 1);
edgeKey = cell(nE, 1);
for i = 1:nE
    edgeKey{i} = sprintf('%s|%s%d>%s|%s%d', edges{i,1}, edges{i,2}, ...
        edges{i,3}, edges{i,4}, edges{i,5}, edges{i,6});
end
[~, ia] = unique(edgeKey);
edgesU = edges(sort(ia), :);

% ---------- 重点子系统内部结构（depth 1：块名+类型） ----------
subNames = {'02_Propulsion_Controller', '03_Propulsion_Plant', ...
    '04_Load_Measurement', '08_Undervolt_Derate', '11_Voyage_Stop_Logic', ...
    '06_Genset_System', '13_DC_Bus', '14_Battery_Converter'};
subDump = cell(numel(subNames), 1);
for s = 1:numel(subNames)
    p = fullfile(MDL_PS, subNames{s});
    ib = find_system(p, 'SearchDepth', 1);
    ib = ib(2:end);
    rows = cell(numel(ib), 2);
    for i = 1:numel(ib)
        rows{i, 1} = strrep(ib{i}, [p '/'], '');
        rows{i, 2} = get_param(ib{i}, 'BlockType');
    end
    subDump{s} = rows;
end
% 04 Inport 名清单与 Sum 输入数（合成点判定）
p4 = fullfile(MDL_PS, '04_Load_Measurement');
in04 = find_system(p4, 'SearchDepth', 1, 'BlockType', 'Inport');
in04n = cell(numel(in04), 1);
in04ord = zeros(numel(in04), 1);
for i = 1:numel(in04)
    in04n{i} = strrep(in04{i}, [p4 '/'], '');
    in04ord(i) = str2double(get_param(in04{i}, 'Port'));
end
sum04 = find_system(p4, 'SearchDepth', 1, 'BlockType', 'Sum');
sum04info = cell(numel(sum04), 2);
for i = 1:numel(sum04)
    sum04info{i, 1} = strrep(sum04{i}, [p4 '/'], '');
    sum04info{i, 2} = get_param(sum04{i}, 'Inputs');
end

% ---------- 专项判定（根级边表机械推导） ----------
fullName = @(s) s;   % edgesU 内块名不带模型前缀（strrep 已去）
isU03 = strcmp(edgesU(:,1), '03_Propulsion_Plant');
consumers03 = unique(edgesU(isU03, 4));
feed02to03 = any(strcmp(edgesU(:,1), '02_Propulsion_Controller') & ...
                 strcmp(edgesU(:,4), '03_Propulsion_Plant'));
feed03to02 = any(strcmp(edgesU(:,1), '03_Propulsion_Plant') & ...
                 strcmp(edgesU(:,4), '02_Propulsion_Controller'));
isU08 = strcmp(edgesU(:,1), '08_Undervolt_Derate');
consumers08 = unique(edgesU(isU08, 4));
energySubs = {'06_Genset_System', '07_PMS_v2', '09_Bus_Residual_Monitor', ...
    '10_Fuel_Consumption', '13_DC_Bus', '14_Battery_Converter', ...
    '15_Bus_Voltage_Monitor', '16_Safety_Interlock'};
energySrc = cell(numel(energySubs), 1);
for i = 1:numel(energySubs)
    m = edgesU(strcmp(edgesU(:,4), energySubs{i}), 1);
    energySrc{i} = unique(m);
end
pgenCons = unique(edgesU(strcmp(edgesU(:,1), 'P_gen_ref_kW'), 4));

% ---------- JSON 安全编码（jsonencode 会把 cell 矩阵按线性序压平，
%             故一律用平行 1-D 数组 / struct 数组） ----------
nE = size(edgesU, 1);
eSrc = cell(nE, 1); eDst = cell(nE, 1);
eSP = zeros(nE, 1); eDP = zeros(nE, 1);   % 编码：+n=Outport n，-n=Inport n
for i = 1:nE
    eSrc{i} = edgesU{i, 1};
    eDst{i} = edgesU{i, 4};
    eSP(i) = edgesU{i, 3} * (strcmp(edgesU{i, 2}, 'Outport') * 2 - 1);
    eDP(i) = edgesU{i, 6} * (strcmp(edgesU{i, 5}, 'Outport') * 2 - 1);
end
nO4 = numel(in04n);
o4names = cell(nO4, 1); o4port = zeros(nO4, 1);
for i = 1:nO4, o4names{i} = in04n{i}; o4port(i) = in04ord(i); end
sumN = numel(sum04);
sumNames = cell(sumN, 1); sumInputs = cell(sumN, 1);
for i = 1:sumN
    sumNames{i} = sum04info{i, 1};
    sumInputs{i} = char(sum04info{i, 2});
end
subStruct = struct('sub', '', 'blocks', struct('name', {}, 'type', {}));
subStruct(1) = [];   % 置空但保留字段骨架
for s = 1:numel(subNames)
    rows = subDump{s};
    br = struct('name', {}, 'type', {});
    for i = 1:size(rows, 1)
        br(end+1) = struct('name', rows{i, 1}, 'type', rows{i, 2}); %#ok<AGROW>
    end
    if s == 1
        subStruct = struct('sub', subNames{s}, 'blocks', br);
    else
        subStruct(end+1) = struct('sub', subNames{s}, 'blocks', br); %#ok<AGROW>
    end
end

audit = struct();
audit.schema = 's4_p3_stream_wiring_audit_v1';
audit.created_utc = char(datetime('now', 'TimeZone', 'UTC', ...
    'Format', 'yyyy-MM-dd''T''HH:mm:ss''Z'''));
audit.model = MDL_PS;
audit.model_sha256 = tk0{1};
audit.readonly = true;
audit.depth1_block_names = blkName;
audit.depth1_block_types = blkType;
audit.edge_src_block = eSrc;
audit.edge_src_port_coded = eSP;     % +n=Outport n / -n=Inport n
audit.edge_dst_block = eDst;
audit.edge_dst_port_coded = eDP;
audit.sub04_inport_names = o4names;
audit.sub04_inport_port = o4port;
audit.sub04_sums = struct('name', sumNames, 'inputs', sumInputs);
audit.subs_internals = subStruct;
audit.consumers_of_03 = consumers03;
audit.feed_02_to_03 = feed02to03;
audit.feed_03_to_02 = feed03to02;
audit.consumers_of_08 = consumers08;
audit.consumers_of_root_inport_pgen = pgenCons;
audit.energy_domain_sub = {energySubs};
audit.energy_domain_sources = {energySrc};

outPath = fullfile(auditDir, 'stream_wiring_audit.json');
fid = fopen(outPath, 'w');
fwrite(fid, jsonencode(audit, 'PrettyPrint', true));
fclose(fid);
fprintf(['[20Sa] 审计 -> %s\n[20Sa] 边 %d 条 | 03 消费者: %s\n' ...
    '[20Sa] 08 消费者: %s | 环 02→03=%d 03→02=%d\n'], ...
    outPath, nE, strjoin(consumers03, ', '), ...
    strjoin(consumers08, ', '), feed02to03, feed03to02);
close_system(MDL_PS, 0);   % 不保存
