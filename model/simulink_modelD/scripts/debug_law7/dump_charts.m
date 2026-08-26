%% ========================================================================
%  dump_charts.m — snapshot all EMChart scripts of one model file to JSON.
%  Inputs come from environment variables (set by the caller):
%    CHART_SNAP_SRC : full path to the .slx to inspect
%    CHART_SNAP_OUT : full path of the JSON output
%  Each invocation runs in its own MATLAB process, so Simulink's same-name
%  model cache can never mix two snapshots.
%% ========================================================================
src = getenv('CHART_SNAP_SRC');
out = getenv('CHART_SNAP_OUT');
assert(~isempty(src) && ~isempty(out), 'DUMP:ENV', 'CHART_SNAP_SRC/OUT not set');
MDL = 'HybridShip_ModelD_DCBus';
ROOT = 'E:/User/Tommy/Desktop/hybrid-ship-ems/model/simulink_modelD';

try, bdclose('all'); catch, end          % purge every loaded model / SF machine
load_system(src);
% Single-model process: enumerate ALL machines and merge their subtrees
% (avoids relying on Machine.Name matching the model name).
machs = sfroot().find('-isa', 'Stateflow.Machine');
assert(~isempty(machs), 'DUMP:MACHINE', 'no Stateflow machine found in %s', src);
chs = [];
for mi = 1:numel(machs)
    chs = [chs; machs(mi).find('-isa', 'Stateflow.EMChart')]; %#ok<AGROW>
end
paths = cell(1, numel(chs));
hashes = cell(1, numel(chs));
for i = 1:numel(chs)
    paths{i} = chs(i).Path;
    hashes{i} = sprintf('%02X', typecast(sum(double(uint16(chs(i).Script))) + numel(chs(i).Script), 'uint64'));
end
res = struct();
res.source = src;
res.n_charts = numel(chs);
res.paths = paths;
res.hashes = hashes;
fid = fopen(out, 'w', 'n', 'UTF-8');
fwrite(fid, jsonencode(res), 'char');
fclose(fid);
fprintf('[dump] %d charts from %s -> %s\n', numel(chs), src, out);
bdclose('all');
