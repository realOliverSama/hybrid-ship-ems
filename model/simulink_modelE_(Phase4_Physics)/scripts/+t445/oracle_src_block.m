function p = oracle_src_block(mdl)
% t445.oracle_src_block — law=6 变体内 oracle From Workspace 块路径
if nargin < 1 || isempty(mdl)
    mdl = 'HybridShip_ModelE';
end
p = [mdl '/05_EMS_Controller/EMS_Oracle_ECMS/oracle_src'];
end
