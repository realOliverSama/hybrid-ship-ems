#!/bin/bash
# E3 双卡编排（T-015 确认 ∥ T-016 消融；R4/R5 语义；由 Main Agent 验收 E1+T-014 后放行）
# 卡0: 五候选确认（mlpq tcnq transformer）      卡1: tft reatt 确认 → 消融 6 新训
# 21 个唯一工件 = 15 确认 + 6 消融新训；H1+K+F 臂复用 reatt 确认运行
set -u
cd /root/autodl-tmp/hybrid-ship-ems/model/modele_link
export EMS_PINNED_GIT_COMMIT=待E1验收后填
V=/root/autodl-tmp/ems_venv/bin/python

echo "[e3] card0: confirm mlpq tcnq transformer $(date -Is)"
( $V -m predictor_v3.confirm --key mlpq --gpu 0 && \
  $V -m predictor_v3.confirm --key tcnq --gpu 0 && \
  $V -m predictor_v3.confirm --key transformer --gpu 0 ) > /root/autodl-tmp/e3_confirm_c0.log 2>&1 &
P0=$!
echo "[e3] card1: confirm tft reatt + ablation $(date -Is)"
( $V -m predictor_v3.confirm --key tft --gpu 1 && \
  $V -m predictor_v3.confirm --key reatt --gpu 1 && \
  $V -m predictor_v3.confirm --key reatt_abl_h1 --gpu 1 && \
  $V -m predictor_v3.confirm --key reatt_abl_h1k --gpu 1 ) > /root/autodl-tmp/e3_confirm_c1.log 2>&1 &
P1=$!
wait $P0 $P1
echo "[e3] all confirm/ablation done $(date -Is)"
# 汇总核验（15/15 硬门 + 9 arm-seed 条目）由 Main Agent 拉回后机器核对
echo "[e3] complete $(date -Is); shutdown in 30min"
sleep 1800
/usr/bin/shutdown
