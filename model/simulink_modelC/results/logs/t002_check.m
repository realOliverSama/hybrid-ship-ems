% 语法与生成代码自检
p = '/Users/oliverlyu/Projects/hybrid-ship-ems/model/simulink_modelC/scripts/+model/build_model_c.m';
msgs = checkcode(p, '-struct');
errs = msgs(arrayfun(@(m) isfield(m,'message') && ~isempty(regexp(m.message,'parse|unbalanced|end','once')), msgs));
fprintf('checkcode: %d messages, parse errors: %d\n', numel(msgs), numel(errs));
% 评估 CODE_AI_PREDICT_C 生成的字符串并检查占位符
c = model.build_model_c;
code = c.CODE_AI_PREDICT_C(22);
fprintf('generated code length=%d\n', numel(code));
% 检查三个 %d 都被填充（无残留）
fprintf('residual %%d: %d\n', numel(regexp(code, '%d')));
% 用生成的代码定义函数并跑一个冒烟单元测试（Source=1 N×3 插值）
fid = fopen('/tmp/ai_predict_unit.m','w'); fprintf(fid,'%s',code); fclose(fid);
addpath('/tmp');
[tpk, t_peak, valid] = ai_predict(0, 500, 5, 4, 0, 100);
fprintf('t=0: Ppred=%g t_peak=%g valid=%g\n', tpk, t_peak, valid);
[tpk2, t_peak2, valid2] = ai_predict(60, 500, 5, 4, 0, 100);
fprintf('t=60: Ppred=%g t_peak=%g valid=%g\n', tpk2, t_peak2, valid2);
