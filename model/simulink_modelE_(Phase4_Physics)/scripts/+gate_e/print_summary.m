function print_summary(chk)
% gate_e.print_summary — Gate 结果摘要打印
    fprintf('[GATE] ---- summary ----\n');
    fns = fieldnames(chk);
    for k = 1:numel(fns)
        c = chk.(fns{k});
        if isfield(c, 'pass') && islogical(c.pass) && isscalar(c.pass)
            fprintf('[GATE] %-18s %s\n', fns{k}, string(c.pass));
        else
            fprintf('[GATE] %-18s (compat smoke)\n', fns{k});
        end
    end
end
