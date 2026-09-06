%% ========================================================================
%  81_run_mission_v3_t429.m - Phase 4-C T-022 thin entry: T429_EVALUATE
%  identifiability Gate execution (FR-547~551; AC-437; design DGN-12
%  R8/R9).  Orchestration body lives in
%  +mission_v3/+audits/identifiability.m (numbered files cannot be called
%  as functions - project convention, same as the 70/71/72/75/80 thin
%  entries; eval(fileread) context).
%
%  The run is machine-verified against the blind freeze at start
%  (results/mission_v3/t429_freeze.json self_hash recompute + referenced
%  objects zero-change: contrast registry hash, heldout token registry
%  hash, prequal corpus manifest SHA-256, qualified generation stack
%  composite re-collected Strict).  Any mismatch raises
%  GATE:MISSION_V3:T429:FREEZE_VIOLATION and the Gate result is void.
%
%  Products (written BEFORE any controlled GATE error):
%    results/mission_v3/identifiability_gate_result.json
%    results/mission_v3/identifiability_report.md
%  Naming discipline (FR-547 / Tasks Gate R8): identifiability_* only;
%  the D-side leakage_sbd_* naming is never reused.
%
%  Usage:
%    cd model/simulink_modelE_(Phase4_Physics)
%    eval(fileread('scripts/81_run_mission_v3_t429.m'))
%
%  Optional base-variable switch (set BEFORE eval(fileread)):
%    T429_MODE = 'evaluate' | 'verify'   (default 'evaluate')
%      evaluate : full Gate execution (freeze verification -> corpus
%                 replay -> SEGMENT_PURE_DIAGNOSTIC_SLICE construction ->
%                 six REQUIRED contrasts + two SECONDARY diagnostics ->
%                 overall verdict -> artifacts; overall=false raises
%                 GATE:MISSION_V3:T429:FAIL after the artifacts are
%                 written - controlled FAIL by design, FR-551 escalation)
%      verify   : machine freeze verification only
%
%  Exit semantics: controlled GATE errors terminate with a nonzero exit
%  status; Gate products are always written first.
%% ========================================================================
ROOT = fileparts(fileparts(mfilename('fullpath')));
if isempty(ROOT) || ~exist(fullfile(ROOT, 'config'), 'dir'), ROOT = pwd; end
addpath(fullfile(ROOT, 'config'), fullfile(ROOT, 'scripts'));

mode_ = 'evaluate';
if evalin('base', 'exist(''T429_MODE'', ''var'')'), mode_ = char(T429_MODE); end

switch mode_
    case 'evaluate'
        res = mission_v3.audits.identifiability('evaluate');
        fprintf('[81] T429 evaluate: overall=%d verdict=%s\n', ...
            res.overall, res.verdict);
    case 'verify'
        res = mission_v3.audits.identifiability('verify_freeze');
        fprintf('[81] T429 verify_freeze: overall=%d\n', res.overall);
    otherwise
        error('GATE:MISSION_V3:T429:ARG', ...
            'T429_MODE must be evaluate or verify (got %s)', mode_);
end
