"""pytest 共享配置：确保 MODELE_ROOT（dataset_v3 包父目录）在 sys.path。

主运行方式为 `python -m pytest`（CWD=MODELE_ROOT，CWD 自动入 sys.path）；
本 conftest 兜底覆盖直接调用 pytest 的情形（禁绝对路径：由 __file__ 推导）。
"""

import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
MODELE_ROOT = os.path.dirname(HERE)
if MODELE_ROOT not in sys.path:
    sys.path.insert(0, MODELE_ROOT)
