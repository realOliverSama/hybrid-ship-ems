function [cand, chk] = make_candidate_v1_1(ROOT)
% gate_ctrl.make_candidate_v1_1 — DD-9 Step B：生成 candidate 基线 v1.1 + 机器一致性检查
%   输入：config/gate_baseline_modele.json（v1.0 永久保留不回写）+ results/gate_result.json
%   （T-412 against candidate 重跑结果）。机器检查（任一失败即 candidate 生成失败）：
%     M1 SHA(ModelE_Physics_Parameters) 与 v1.0 逐位一致（物理事实源零改动）
%     M2 Model D 参照哈希与 v1.0 逐位一致（历史资产零改动）
%     M3 G1~G6 判据结构逐字段一致（bench 观测量在 DD-8 force 兼容适用范围内逐位一致；
%        B 包新增控制/logging 信号不参与该等值比较——T-012 口径）
%     M4 E10 兼容 smoke ok 一致（smoke 无数值阈值，仅 ok 位）
%   允许变化（DD-9）：version / frozen_from 记录 / config_sha256（TestScenarios 因 T-006
%   MANUAL 通道扩展而变 + Control 新增——control/config manifest 新增部分）。
    v10 = jsondecode(fileread(fullfile(ROOT, 'config', 'gate_baseline_modele.json')));
    fresh = jsondecode(fileread(fullfile(ROOT, 'results', 'gate_result.json')));
    chk = struct();
    ok = true;

    % M1 物理参数 sha 一致
    shaNow = model.sha256_file(fullfile(ROOT, 'config', 'ModelE_Physics_Parameters.m'));
    shaV10 = v10.config_sha256.ModelE_Physics_Parameters_m;
    m1 = strcmp(shaNow, shaV10);
    chk.M1_physics_sha_equal = m1;
    ok = ok && m1;

    % M2 Model D 参照哈希一致（按 A2 write_config_manifest 同清单逐文件核对）
    m2 = true; m2detail = {};
    legacy = {'config/Model_Parameters.m', 'config/Model_Scenarios.m', ...
        'scripts/+model/build_model_c.m', 'model/HybridShip_ModelD_DCBus.slx'};
    legacyKey = {'config_Model_Parameters_m', 'config_Model_Scenarios_m', ...
        'scripts__model_build_model_c_m', 'model_HybridShip_ModelD_DCBus_slx'};
    for k = 1:numel(legacy)
        p = fullfile(ROOT, '..', 'simulink_modelD_(EMS_NN_AECMS)', legacy{k});
        nowSha = model.sha256_file(p);
        same = strcmp(nowSha, v10.model_D_reference_hashes.(legacyKey{k}));
        m2 = m2 && same;
        m2detail{end+1} = sprintf('%s=%s', legacy{k}, string(same)); %#ok<SAGROW>
    end
    chk.M2_modelD_hashes_equal = m2;
    chk.M2_detail = m2detail;
    ok = ok && m2;

    % M3 判据一致性（DD-9“G2~G6 阈值逐字段一致”）：
    %   G3/G4 —— force 模式 bench（DD-8 兼容适用范围）要求**测量值逐位一致**；
    %   G1/G2/G5/G6 —— AUTO 跑经新控制链（执行序变化致 ~1e-6 级浮点漂移），
    %   比对**判据结构**（item 序列 + pass + gate_class）逐字段一致，测量值不比对
    %   （其正确性由本次重跑自身 PASS 背书；A2 历史物理观测量等值口径见 T-012）。
    m3 = true; m3diff = {};
    gBitwise = {'G3', 'G4'};
    gStruct = {'G1', 'G2', 'G5'};
    for k = 1:numel(gBitwise)
        a = v10.checks.(gBitwise{k});
        b = fresh.checks.(gBitwise{k});
        if ~strcmp(jsonencode(strip_ts(a)), jsonencode(strip_ts(b)))
            m3 = false;
            m3diff{end+1} = sprintf('%s(bitwise)', gBitwise{k}); %#ok<SAGROW>
        end
    end
    for k = 1:numel(gStruct)
        a = v10.checks.(gStruct{k});
        b = fresh.checks.(gStruct{k});
        if ~strcmp(jsonencode(struct_of(a)), jsonencode(struct_of(b)))
            m3 = false;
            m3diff{end+1} = sprintf('%s(structure)', gStruct{k}); %#ok<SAGROW>
        end
    end
    % G6 专项（D-B19/D-B20：checker fail-closed 化后条目集合法演进）——比较：
    %   ① 确定性 item 的 pass 位一致；② fresh 的 comparability_pass 必须 true 且
    %   compared==required、skipped==0（fail-closed 语义生效的证据本身即 candidate 内容）。
    g6a = v10.checks.G6; g6b = fresh.checks.G6;
    pa = g6a.pass; pb = g6b.pass;
    if isfield(g6b, 'comparability_pass')
        g6ok = pb && g6b.comparability_pass && g6b.compared_fields == g6b.required_fields ...
            && g6b.skipped_fields == 0;
    else
        g6ok = false;   % candidate 必须携带 fail-closed G6（否则判据未升级）
    end
    % 确定性 item pass 位一致
    detA = v10.checks.G6.checks{1}.pass;
    detB = false;
    for c = 1:numel(g6b.checks)
        if contains(g6b.checks{c}.item, 'determinism'), detB = g6b.checks{c}.pass; end
    end
    g6ok = g6ok && (detA == detB);
    if ~(pb == pa && g6ok)
        m3 = false;
        m3diff{end+1} = 'G6(special: fail-closed determinism)'; %#ok<SAGROW>
    end
    chk.M3_threshold_structure_equal = m3;
    chk.M3_g6_fail_closed_verified = g6ok;
    chk.M3_diff = m3diff;
    ok = ok && m3;

    % M4 E10 ok 一致（smoke 无数值阈值）
    m4 = (v10.checks.E10_compat_smoke.ok == fresh.checks.E10_compat_smoke.ok);
    chk.M4_e10_ok_equal = m4;
    ok = ok && m4;

    if ~ok
        error('GATE:candidate_v1_1_machine_check_failed — DD-9 Step B 机器检查失败（禁止生成 candidate）');
    end

    % candidate 组装（显式 _candidate 命名；v1.0 永久保留）
    cand = v10;
    cand.version = '1.1-candidate';
    cand.spec = 'phase4-b-pms-safety / T-012（candidate，promotion 需用户书面批准）';
    cand.frozen_from = sprintf('results/gate_result.json (T-412 against candidate, overall=%s, %s)', ...
        string(fresh.overall), char(datetime('now', 'Format', 'yyyy-MM-dd')));
    cand.checks = fresh.checks;
    % config sha：物理参数不变（M1 已证）；TestScenarios 变化与 Control 新增为 DD-9
    % 允许的 manifest 新增部分，如实登记
    cand.config_sha256 = struct();
    cand.config_sha256.ModelE_Physics_Parameters_m = shaNow;
    cand.config_sha256.ModelE_Control_Parameters_m = model.sha256_file(fullfile(ROOT, 'config', 'ModelE_Control_Parameters.m'));
    cand.config_sha256.ModelE_TestScenarios_m = model.sha256_file(fullfile(ROOT, 'config', 'ModelE_TestScenarios.m'));
    cand.config_sha256.ModelE_Sensitivity_Profile_m = model.sha256_file(fullfile(ROOT, 'config', 'ModelE_Sensitivity_Profile.m'));
    cand.dd9_machine_check = chk;
    outPath = fullfile(ROOT, 'config', 'gate_baseline_modele_candidate_v1_1.json');
    fid = fopen(outPath, 'w', 'n', 'UTF-8');
    fwrite(fid, jsonencode(cand, 'PrettyPrint', true));
    fclose(fid);
    fprintf('[candidate] DD-9 机器检查全过 → %s\n', outPath);
end

function s = struct_of(x)
% 判据结构提取：item 名 + pass + gate_class（测量值/gate_class 详情字段除外）
    s = struct('name', x.name, 'items', {{}});
    for k = 1:numel(x.checks)
        it = x.checks{k};
        e = struct('item', it.item, 'pass', it.pass);
        if isfield(it, 'gate_class'), e.gate_class = it.gate_class; end
        s.items{end+1} = e; %#ok<SAGROW>
    end
    s.pass = x.pass;
end

function s = strip_ts(x)
% 剥离生成时间戳类字段（非物理判据）
    if isstruct(x)
        f = fieldnames(x);
        for k = 1:numel(f)
            if strcmpi(f{k}, 'generated_at') || strcmpi(f{k}, 'wall_time_s')
                x = rmfield(x, f{k});
            else
                x.(f{k}) = strip_ts(x.(f{k}));
            end
        end
        s = x;
    else
        s = x;
    end
end