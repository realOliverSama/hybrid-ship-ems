# pytest 路径引导：保证 predictor_v3 包可从 Model E 工作区根 import（NFR-442 相对推导）。
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
MODELE_ROOT = os.path.dirname(HERE)
if MODELE_ROOT not in sys.path:
    sys.path.insert(0, MODELE_ROOT)
