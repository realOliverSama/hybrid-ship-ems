#!/usr/bin/env bash
# 55_repro_check.sh — Phase 2 复现验证入口（FR-212）
# 用法：bash scripts/55_repro_check.sh  （在 model/simulink_modelD 下）
set -euo pipefail
cd "$(dirname "$0")/.."
/Applications/MATLAB_R2026a.app/bin/matlab -batch "eval(fileread('scripts/55_repro_check.m'))"
