% 提取 CODE_AI_PREDICT_C 生成代码并做独立单元验证
txt = fileread('/Users/oliverlyu/Projects/hybrid-ship-ems/model/simulink_modelC/scripts/+model/build_model_c.m');
start = strfind(txt, 'function c = CODE_AI_PREDICT_C(win)');
assert(~isempty(start), 'CODE_AI_PREDICT_C not found');
% 提取到下一个 'function ' 顶层定义（缩进为 0 的 function 行）
rest = txt(start:end);
pat  = '(?m)^function c = CODE_AI_PREDICT_C\(win\)\n.*?\nend\n(?=\nfunction |\Z)';
tok  = regexp(rest, pat, 'match', 'once');
assert(~isempty(tok), 'regex extraction failed');
% 求值得到生成代码字符串
evalc('eval(tok)');
code = CODE_AI_PREDICT_C(22);
assert(ischar(code) && numel(code) > 1000, 'generated code too short');
fprintf('residual %%d: %d\n', numel(regexp(code, '%d')));
fid = fopen('/tmp/ai_predict.m','w'); fprintf(fid,'%s',code); fclose(fid);
addpath('/tmp');
% 设置图级参数
AI_Pred_Source = 1; AI_TPeak_Source = 0;
AI_Pred_Data = [0 100 30; 50 200 30; 100 300 30];  % t, P_peak, t_peak_rel 常数30
STM32_Pred_Data = [0 0]; ASSUMED_AI_DataValid = true; ASSUMED_Pred_Bound_kW = 2000;
MLP_WeightsLoaded = 0; MLP_FeatMean=zeros(1,5); MLP_FeatStd=ones(1,5); MLP_YMean=0; MLP_YStd=1;
MLP_W1=zeros(1,1); MLP_b1=0; MLP_W2=zeros(1,1); MLP_b2=0; MLP_W3=0; MLP_b3=0;
% t=0 与 t=60 连续调用（验证 persistent 窗口不崩）
[P1, tp1, v1] = ai_predict(0, 500, 5, 4, 0, 100);
[P2, tp2, v2] = ai_predict(60, 500, 5, 4, 0, 100);
fprintf('t=0:  Ppred=%g (expect ~100), t_peak=%g (expect 30), valid=%g\n', P1, tp1, v1);
fprintf('t=60: Ppred=%g (expect ~300), t_peak=%g (expect 30), valid=%g\n', P2, tp2, v2);
% AI_TPeak_Source=1 覆盖（Source=3 用 MLP 幅值，但 t_peak 从 oracle 表）
AI_TPeak_Source = 1; AI_Pred_Source = 3;
MLP_WeightsLoaded = 1; MLP_W1 = [1 2 3]; MLP_b1 = [0 0 0]; MLP_W2 = zeros(3,1); MLP_b2 = 0; MLP_W3 = 1; MLP_b3 = 0; MLP_YMean = 0; MLP_YStd = 1; MLP_FeatMean = zeros(1,5); MLP_FeatStd = ones(1,5);
% 重新定义函数体（persistent 隔离）——直接再写一份
fprintf('override test pending (needs fresh function state)\n');
