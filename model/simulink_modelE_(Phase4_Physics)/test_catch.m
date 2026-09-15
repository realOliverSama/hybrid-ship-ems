addpath('scripts');
aj = fullfile('/Users/oliverlyu/Projects/hybrid-ship-ems', 'docs', ...
    'phase4', 'd_dataset_v3', 't432_activation_record.json');
plan = fleet_v3.plan_load(aj, 'stage_b_iid');
seed = char(plan.iid.seed);
fprintf('seed=[%s]\n', seed);
try
    s = mission_v3.env_realize('sample', seed, 99999, ...
        'ROUTINE_PATROL', 'NORMAL', 'STRAT_ROUTINE_PATROL__NORMAL', ...
        'MaxAttempts', 1);
    fprintf('NO_ERROR accepted=%d\n', s.accepted);
catch ME
    fprintf('CAUGHT id=[%s]\n', ME.identifier);
    fprintf('msg=%s\n', ME.message);
    fprintf('strcmp_match=%d\n', strcmp(ME.identifier, 'GATE:MISSION_V3:ENV:MAX_RETRY'));
end
fprintf('TEST_DONE\n');
