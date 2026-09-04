function [nEvents, tEvents] = saturation_events(ds, T_start_hold_ticks)
% gate_ctrl.saturation_events — high-load episode 分割算法（design §6.3，DD-7）
%   重建 normal_start_timer 序列（0.01 s 网格，logsout 已对齐）；
%   eligible high-load episode = 该 timer 的饱和事件（从 <3000 到 ==3000 的穿越）计数。
%   stop 迁移要求 P_commit<350 持续 120 s ⇒ 两次正常 start 之间必有 timer 清零
%   ⇒ 每次正常 start 唯一对应一个饱和事件（episode 一对一防 chatter 的机器判据）。
    s = ds.start_hold_timer(:);
    sat = (s >= T_start_hold_ticks);
    ev = [false; diff(sat) == 1];          % 进入饱和态的沿
    tEvents = ds.t(ev);
    nEvents = numel(tEvents);
end