function profile = ModelE_Sensitivity_Profile(profile_name)
% ModelE_Sensitivity_Profile — Model E 敏感性档位选择器（brainstorm Q1 冻结登记）
%
%  【Q1 裁决】A2 只扫掠螺旋桨参数族 ±20% OAT（41 号脚本，FR-417）。
%  本文件登记的其余档位属**冻结备案**：不在 A2 扫掠（扫掠依赖 B 包 PMS v2 才有
%  完整意义），仅供未来 B/F 包按 profile 机制调用。
%
%  用法：
%    profile = ModelE_Sensitivity_Profile();          % nominal（全零改动）
%    profile = ModelE_Sensitivity_Profile('nominal'); % 同上（显式）
%    profile.overrides  % 空 struct = 不覆盖任何参数（nominal 档语义）
%
%  每个非 nominal profile 的 overrides 为 struct 数组：
%    .name（参数名，须存在于 ModelE_Physics_Parameters.m）
%    .base_value / .effective_value / .source_class / .reference
%  build_model_e / 40 号 Gate 会将 profile 写入 results/config_manifest.json
%  （parameter/base_value/effective_value/source_class/reference/profile_name）。
%
%  禁止：在本文件中登记未经 spec 修订批准的新档位（Gate Threshold 修改属 🟡 操作）。

if nargin < 1
    profile_name = 'nominal';
else
    profile_name = profile;
end

switch lower(profile_name)
    case 'nominal'
        profile = struct( ...
            'profile_name', 'nominal', ...
            'description', 'Model E nominal（全部参数取 ModelE_Physics_Parameters.m 原值，零改动）', ...
            'overrides', struct('name', {}, 'base_value', {}, 'effective_value', {}, ...
                                'source_class', {}, 'reference', {}));
    otherwise
        error(['ModelE_Sensitivity_Profile:UnknownProfile', ...
            ' 未登记的敏感性档位 ''%s''。A2 仅登记冻结档位（P_min/ramp/tau/C_start），', ...
            '登记值见下 FROZEN_TIERS；新档位须走 spec 修订。'], profile_name);
end
end