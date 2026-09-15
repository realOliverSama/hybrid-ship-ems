% probe_runner.m - feed plan/ctx to the task-27 reprobe, dump full ME
addpath('scripts');
aj = fullfile('/Users/oliverlyu/Projects/hybrid-ship-ems', 'docs', ...
    'phase4', 'd_dataset_v3', 't432_activation_record.json');
plan = fleet_v3.plan_load(aj, 'stage_b_ood');
ctx = fleet_v3.ctx_build(plan);
try
    probe_t27(plan, ctx);
    fprintf('PROBE_SURVIVED\n');
catch ME
    fprintf('PROBE_FAIL id=[%s]\n', ME.identifier);
    fprintf('PROBE_FAIL msg=[%s]\n', ME.message);
    for k = 1:numel(ME.stack)
        fprintf('  frame %d: %s line %d (file %s)\n', k, ...
            ME.stack(k).name, ME.stack(k).line, ME.stack(k).file);
    end
    for k = 1:numel(ME.cause)
        fprintf('cause[%d]: id=[%s] msg=[%s]\n', k, ...
            ME.cause{k}.identifier, ME.cause{k}.message);
        for j = 1:min(4, numel(ME.cause{k}.stack))
            fprintf('    at %s:%d\n', ME.cause{k}.stack(j).name, ...
                ME.cause{k}.stack(j).line);
        end
    end
end
