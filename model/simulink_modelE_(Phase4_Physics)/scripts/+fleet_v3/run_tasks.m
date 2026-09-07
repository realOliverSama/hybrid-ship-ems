function outs = run_tasks(fn, args_cell, nworkers_cap, tag)
% fleet_v3.run_tasks - bounded-parallel task dispatcher for the 73
% pipeline (T-018 Phase 4-D).
%
%   outs = fleet_v3.run_tasks(fn, args_cell, nworkers_cap, tag)
%     fn          task function; called as fn(args_cell{i}{:})
%     args_cell   1 x N cell of arg cells (one per independent task)
%     nworkers_cap  max pool workers (memory-bound on small machines)
%     tag         progress label
%
% Dispatch semantics:
%   - Parallel (PCT available and enabled): parfeval one future per
%     task; collection by State polling with a bounded pause - the
%     dispatcher never blocks unboundedly (anti-deadlock face).  A
%     failed future is recorded as a task failure and does not stop the
%     remaining tasks.  A wall-clock guard (default 24 h) fails the
%     dispatcher fail-closed instead of hanging forever.
%   - Serial fallback: PCT unavailable, FLEET_PARALLEL='off', or a
%     single task - identical task functions called in order.
%
% outs is a 1 x N cell of task outputs in TASK ORDER (canonical),
% regardless of completion order.  Task failures raise
% GATE:FLEETV3:PAR:TASK after all futures have settled (audit
% completeness), listing the failed task indices.
%
% Error ids: GATE:FLEETV3:PAR:<REASON>.
%
% Discipline: ASCII-only source; no absolute paths.

    n = numel(args_cell);
    outs = cell(1, n);
    failed = {};

    use_par = local_parallel_enabled(n, nworkers_cap);

    if ~use_par
        for i = 1:n
            outs{i} = fn(args_cell{i}{:});
            fprintf('[fleet_v3] %s task %d/%d done (serial)\n', ...
                tag, i, n);
        end
        return;
    end

    w = gcp('nocreate');
    if isempty(w)
        w = parpool(nworkers_cap);
    end
    nw = min(double(nworkers_cap), double(w.NumWorkers));
    fprintf('[fleet_v3] %s: parallel dispatch %d tasks on <=%d workers\n', ...
        tag, n, nw);

    t_all = tic;
    futs = cell(1, n);
    for i = 1:n
        futs{i} = parfeval(w, fn, 1, args_cell{i}{:});
    end

    collected = false(1, n);
    while nnz(collected) < n
        for i = 1:n
            if collected(i) || ~strcmp(char(futs{i}.State), 'finished')
                continue;
            end
            try
                outs{i} = fetchOutputs(futs{i});
                fprintf('[fleet_v3] %s task %d/%d done\n', tag, i, n);
            catch ta_err
                failed{end+1} = sprintf( ...
                    'task %d: %s', i, local_one_line(ta_err)); %#ok<AGROW>
                fprintf('[fleet_v3] %s task %d/%d FAILED\n', tag, i, n);
            end
            collected(i) = true;
        end
        if nnz(collected) < n && toc(t_all) > PAR_CONST_WALLTIME_S()
            for i = 1:n
                if ~collected(i)
                    failed{end+1} = sprintf( ...
                        'task %d: WALLTIME timeout', i); %#ok<AGROW>
                end
            end
            break;
        end
        pause(5);
    end

    if ~isempty(failed)
        error('GATE:FLEETV3:PAR:TASK', ...
            '%d of %d %s task(s) failed: %s', numel(failed), n, tag, ...
            strjoin(failed, ' | '));
    end
end

function tf = local_parallel_enabled(n, nworkers_cap)
% Parallel face requires: PCT present+licensed, more than one task, a
% positive worker cap, and FLEET_PARALLEL not 'off' (default 'on').
    tf = false;
    if n < 2 || double(nworkers_cap) < 1
        return;
    end
    ev = getenv('FLEET_PARALLEL');
    if ~isempty(ev) && strcmpi(char(ev), 'off')
        return;
    end
    try
        if isempty(ver('parallel')) ...
                || ~license('test', 'Distrib_Computing_Toolbox')
            fprintf(['[fleet_v3] Parallel Computing Toolbox ', ...
                'unavailable - serial fallback\n']);
            return;
        end
        tf = true;
    catch
        fprintf('[fleet_v3] PCT probe failed - serial fallback\n');
        tf = false;
    end
end

function l = local_one_line(ME)
    msg = char(ME.message);
    nl = strfind(msg, newline);
    if ~isempty(nl)
        msg = msg(1:nl(1)-1);
    end
    if numel(msg) > 160
        msg = [msg(1:160) '...'];
    end
    id = 'UNKNOWN';
    if isfield(ME, 'identifier') && ~isempty(ME.identifier)
        id = char(ME.identifier);
    end
    l = [id ': ' msg];
end

function v = PAR_CONST_WALLTIME_S()
% Dispatcher wall-clock guard: fail-closed instead of hanging forever
% (anti-deadlock face; 24 h covers the largest Phase 2 batches many
% times over).
    v = 24 * 3600;
end
