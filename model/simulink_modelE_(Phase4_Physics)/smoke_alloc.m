% smoke_alloc.m - OOD A1 single-condition allocation smoke (Phase S)
addpath('scripts');
aj = fullfile('/Users/oliverlyu/Projects/hybrid-ship-ems', 'docs', ...
    'phase4', 'd_dataset_v3', 't432_activation_record.json');
plan = fleet_v3.plan_load(aj, 'stage_b_ood');
ctx = fleet_v3.ctx_build(plan);
out = fleet_v3.allocate_draws(plan, ctx, 'ood', 'Conditions', {'A1'});
sa = out.ood.conditions(1);
fprintf('SMOKE condition=%s verdict=%s slots=%d attempts=%d\n', ...
    sa.condition_id, sa.verdict, sa.accepted_slots, sa.attempts);
for k = 1:numel(sa.slots)
    fprintf('  slot %d bdi=%d token=%s\n', k, ...
        sa.slots{k}.base_draw_index, sa.slots{k}.token_id);
end
rej = 0; acc = 0;
for k = 1:numel(sa.consumed)
    if strcmp(sa.consumed{k}.disposition, 'ACCEPT'); acc = acc + 1;
    else; rej = rej + 1; end
end
fprintf('consumed: accepted=%d rejected=%d reject_rate=%.1f%%\n', ...
    acc, rej, 100*rej/max(1,acc+rej));
fprintf('self_hash=%s\n', out.self_hash(1:16));
fprintf('SMOKE_ALLOC_PASS\n');
