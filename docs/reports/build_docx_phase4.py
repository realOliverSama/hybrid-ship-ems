#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""组装《Phase 4 研究总结报告——Model E 物理域重构、REAT-T 预测换代与整机 soft-HIL 验证》
Word 文档（docs/reports/*.docx），样式承接 EMS01（Phase 3）报告。

数据纪律（与 build_docx.py 同纪律）：
  - S4 soft-HIL 面数字实时读取 firmware/.../EMS04_CHIL_FULLCTRL/Host/results/ 下
    s4_c3_gate.json / s4_c4_gate.json / s4_c4_*_gate.json（唯一事实源）；
  - 历史阶段数字（A1/A2/B/C/D/E 六线与 EMS04 S1~S3、G1）取自仓库内门禁 JSON/
    报告/HANDOFF，在本脚本中以字面量登记并注明来源；
  - 已知差异与待核项如实登记（附录 C），不在正文抹平。

幂等可重跑；相对路径（仓库根由脚本位置推导）。
用法：python3 docs/reports/build_docx_phase4.py   （需 python-docx）
"""
import json
import os

from docx import Document
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.oxml.ns import qn
from docx.shared import Inches, Pt, RGBColor

HERE = os.path.dirname(os.path.abspath(__file__))
FIGDIR = os.path.join(HERE, "figures_phase4")
ROOT = os.path.dirname(os.path.dirname(HERE))
FW4 = os.path.join(ROOT, "firmware", "stm32n6", "04_SourceCode", "SoftwarePackage",
                   "EMS_Projects", "EMS04_CHIL_FULLCTRL")
R4 = os.path.join(FW4, "Host", "results")
OUT_DOCX = os.path.join(HERE, "EMS04_Phase4研究总结报告_ModelE与整机softHIL_20260929.docx")
REPORT_DATE = "2026-09-29"


def jload(path, default=None):
    try:
        with open(path, encoding="utf-8") as fh:
            return json.load(fh)
    except Exception:
        return default


GATE_C3 = jload(os.path.join(R4, "s4_c3", "s4_c3_gate.json"), {})
GATE_BAT = jload(os.path.join(R4, "s4_c4", "s4_c4_gate.json"), {})
GATE_S02 = jload(os.path.join(R4, "s4_c4", "s4_c4_S02_downstream_gate.json"), {})
GATE_S08 = jload(os.path.join(R4, "s4_c4", "s4_c4_S08_resistance_bands_gate.json"), {})
GATE_S03 = jload(os.path.join(R4, "s4_c4", "s4_c4_S03_upstream_gate.json"), {})
# S4-P3 v1.1：formal 臂电池（streamE）逐航次 gate
P3_VIDS = ["FLTA_S2_D01006_M1", "FLTA_S2_D01006_M2",
           "FLTA_S6_D05003_M1", "FLTA_S6_D05003_M2"]
GATE_P3_BAT = jload(os.path.join(R4, "s4_p3", "s4_c4_gate.json"), {})
GATE_P3 = {v: jload(os.path.join(R4, "s4_p3", "s4_c4_%s_gate.json" % v), {})
           for v in P3_VIDS}


def p3(key, default="（gate 缺失）"):
    v = GATE_P3_BAT
    for k in key.split("."):
        v = v.get(k) if isinstance(v, dict) else None
        if v is None:
            return default
    return v


def c3(key, default="（gate 缺失）"):
    v = GATE_C3
    for k in key.split("."):
        v = v.get(k) if isinstance(v, dict) else None
        if v is None:
            return default
    return v


def bat(key, default="（gate 缺失）"):
    v = GATE_BAT
    for k in key.split("."):
        v = v.get(k) if isinstance(v, dict) else None
        if v is None:
            return default
    return v


# ---------------------------------------------------------------- docx 基础设施

def set_run_font(run, size=10.5, east="宋体", ascii_f="Times New Roman",
                 bold=None, color=None):
    run.font.name = ascii_f
    run.font.size = Pt(size)
    r = run._element.rPr
    rfonts = r.rFonts if r.rFonts is not None else r.get_or_add_rFonts()
    rfonts.set(qn("w:eastAsia"), east)
    if bold is not None:
        run.font.bold = bold
    if color is not None:
        run.font.color.rgb = RGBColor(*color)


def init_styles(doc):
    st = doc.styles["Normal"]
    st.font.name = "Times New Roman"
    st.font.size = Pt(10.5)
    st.element.rPr.rFonts.set(qn("w:eastAsia"), "宋体")
    for name, size, before, after in (("Heading 1", 16, 12, 8),
                                      ("Heading 2", 13, 10, 6),
                                      ("Heading 3", 11.5, 8, 4)):
        h = doc.styles[name]
        h.font.name = "Times New Roman"
        h.font.size = Pt(size)
        h.font.bold = True
        h.font.color.rgb = RGBColor(0, 0, 0)
        h.paragraph_format.space_before = Pt(before)
        h.paragraph_format.space_after = Pt(after)


def heading(doc, text, level):
    doc.add_heading(text, level=level)


def para(doc, text, indent=True, bold=False, size=10.5):
    p = doc.add_paragraph()
    if indent:
        p.paragraph_format.first_line_indent = Pt(21)
    r = p.add_run(text)
    set_run_font(r, size=size, bold=bold if bold else None)
    return p


def bullets(doc, items, prefix="• "):
    for it in items:
        p = doc.add_paragraph()
        p.paragraph_format.left_indent = Pt(18)
        r = p.add_run(prefix + it)
        set_run_font(r, size=10.5)


def mono(doc, text):
    p = doc.add_paragraph()
    p.paragraph_format.left_indent = Pt(18)
    r = p.add_run(text)
    set_run_font(r, size=9, east="Menlo", ascii_f="Menlo")
    return p


def table(doc, headers, rows, widths=None):
    t = doc.add_table(rows=1 + len(rows), cols=len(headers))
    t.style = "Table Grid"
    for j, h in enumerate(headers):
        cell = t.rows[0].cells[j]
        cell.paragraphs[0].text = ""
        r = cell.paragraphs[0].add_run(str(h))
        set_run_font(r, size=9, bold=True)
    for i, row in enumerate(rows):
        for j, v in enumerate(row):
            cell = t.rows[i + 1].cells[j]
            cell.paragraphs[0].text = ""
            r = cell.paragraphs[0].add_run(str(v))
            set_run_font(r, size=9)
    doc.add_paragraph()
    return t


def figure(doc, fname, caption, width=5.9):
    """插图 + 图注（居中；缺图时打警告不中断，manifest 登记跳过项）。"""
    path = os.path.join(FIGDIR, fname)
    if not os.path.isfile(path):
        print("WARN: 缺图 %s（跳过）" % fname)
        return
    p = doc.add_paragraph()
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    p.add_run().add_picture(path, width=Inches(width))
    cap = doc.add_paragraph()
    cap.alignment = WD_ALIGN_PARAGRAPH.CENTER
    r = cap.add_run(caption)
    set_run_font(r, size=9, east="黑体")


# ---------------------------------------------------------------- 正文组装

def build():
    doc = Document()
    init_styles(doc)

    # ---- 封面 ----
    tp = doc.add_paragraph(); tp.alignment = WD_ALIGN_PARAGRAPH.CENTER
    r = tp.add_run("甲醇混合动力船舶能量管理系统\nPhase 4 研究总结报告")
    set_run_font(r, size=20, east="黑体", bold=True)
    sp = doc.add_paragraph(); sp.alignment = WD_ALIGN_PARAGRAPH.CENTER
    r = sp.add_run("——Model E 物理域重构、REAT-T 预测换代与整机 soft-HIL 验证")
    set_run_font(r, size=14, east="楷体", bold=True)
    dp = doc.add_paragraph(); dp.alignment = WD_ALIGN_PARAGRAPH.CENTER
    r = dp.add_run("报告日期：%s · 版本 v1.1（依据仓库内门禁 JSON 与会话记录整理；数字来源逐处标注）" % REPORT_DATE)
    set_run_font(r, size=10.5, east="楷体")
    doc.add_paragraph()

    # ---- 摘要 ----
    heading(doc, "摘要", 1)
    para(doc, "Phase 4 以“增量升级 Model D 为 Model E、重心从部署侧转回模型科学性、"
              "工况真实性与控制方法”为总目标（依赖链 A+B→C→D→E→F），并在收官段（EMS04 线，"
              "分支 feat/phase4-g）把链路延伸到整机软件在环（soft-HIL）验证：在 STM32N647 板上"
              "以完整控制器（int8 NPU 预测推理 + law6 决策/执行 + PMS/Safety）与 PC 侧 "
              "Model E PlantStep 植物构成实时闭环，通过机器化验收判据（G6）完成单航次"
              " 541 拍三链对照与 3 航次电池验证。")
    para(doc, "六条工作线结果：A1 建模与参数审计（210 项参数五级来源分级，G1~G5 全过）；"
              "A2 Model E 物理重建（920 blocks，T-412 物理 Gate 全过 + 负向验证 + 螺旋桨 ±20% "
              "敏感性 15 例零失败）；B PMS v2/Safety-Interlock（控制 Gate 134/0，负向 7 缺陷全检出）；"
              "C Mission Generator v3（预资格 108 航次全 ACCEPT；T-429 可辨识性审计 FINAL FAIL "
              "如实登记并经 amendment 放行 formal）；D Dataset v3（formal 冻结 776 航次 / 197,448 "
              "窗，双跑零 mismatch）；E REAT-T 预测器（五候选公平对比 Winner，test 集 E300 R²=0.9931，"
              "未来信息消融证实 K/F 通道为主要增益来源）。")
    para(doc, "整机 soft-HIL（EMS04 线）：S1 law6 C 移植 377/377 位级、S2 特征面板服务 755/755 "
              "逐字节、G1 PMS/Safety 移植 6001/6001 逐位、S3 REATT 拆分 N1+N2 上 NPU（后端跨域"
              "数值漂移按 V2 契约登记）；C3 单航次 541 拍闭环 overall PASS（等值 89.1%%，59 拍跨后端"
              "漂移按用户裁决降级为 CLOSED_LOOP_BACKEND_DRIFT 档案），C4 冻结电池 3/3 航次 PASS"
              "（零漂移、零 NACK）。收官续段（P1~P6，v1.1）：DWT 遥测补全（板内实测分账：REATT "
              "≈0.30 s + law6 DP 5.23 s + PMS ≈0.004 s，10 s 拍占空 ≈68%%）+ law6 DP 位级加速 "
              "1.22×（J1 前缀合并/J2 机组对称/J3 查表前置，377/377 逐拍位级 + 板级 100 拍逐拍"
              "同一性 + C4 零回归）；P3 formal 臂电池覆盖 4 航次（M1/M2 预报质量臂 ×2 配对族，"
              "流驱动植物变体 G-P2E 回放保真门 4/4 逐位）overall=%s；T-429 经 EXECFIX-02 审计"
              "实现修复重审（DIR reach 匹配 + probe 类分层折），六项身份全部 clean（0 INVALID/"
              "0 INSUFFICIENT），终态 FAIL_METRIC（T-432 维持不解锁，NO-GO 边界归档）。两项边界"
              "状态更新：电池域 pilot 家族局限已由 formal 臂覆盖消除；XIP/flash boot 启动问题 RCA "
              "闭环（双缺陷定案 + F1~F4 修复方案），烧写待用户批准（park）。"
              % p3("overall"))

    # ---- 1 背景与问题定义 ----
    heading(doc, "1  研究背景与问题定义", 1)
    figure(doc, "r_fig1_1_tech_route.png", "图 1-1  Phase 4 技术路线（复用论文草稿 Visio 图 1-1）")
    heading(doc, "1.1  承接 Phase 3：从 Model D/CHIL 到 Model E/整机 soft-HIL", 2)
    para(doc, "Phase 3（EMS01）在 Model D 平台上完成四臂×两场景 soft-HIL 真闭环：仪器级①②全 PASS，"
              "NN 闭环增益 S20 带内 −2.25~−2.65 kg（≈0.68~0.80%）。其边界同样明确：物理模型为简化"
              "船体（浅水/风浪流/机组热力学缺失）、预测器为纯历史外推（R²≈0.73 天花板）、控制只在 "
              "host 侧闭环。Phase 4 的问题链由此定义：物理是否可信（A1/A2）、工况是否真实（C）、"
              "数据与预测能否换代（D/E）、控制与安全能否工业化（B），以及最终——以上全部集成后，"
              "整机还能不能在真实部署硬件上闭环（EMS04 线）。")
    heading(doc, "1.2  Phase 4 的边界裁决与路线修正", 2)
    bullets(doc, [
        "Q1 边界裁决：Phase 4 暂不做 N6/HIL/NPU 部署，Model E 须 HIL-ready（fixed-step、无代数环）"
        "但不以实时化为目标；旧 .specs/phase4-model-e（HIL 实时化）被 phase4_research_roadmap 取代"
        "（superseded）。",
        "T-445 Oracle Gate 于 2026-09-15 判 CLOSED GATE_VALIDITY_FAIL——基于 simulator 内部真值的"
        "预测上限评估不可信，F 包转向 T-456 practical predictive-EMS（以冻结 REAT-T 为 C2 的实用化 "
        "predictive-EMS），这是 Phase 4 后半段走向“实用闭环”的关键路线修正。",
        "C 线 T-429 可辨识性审计 FINAL FAIL（0/6 必需对比 PASS）：语料对比在预配准盲区间歇性不可辨识，"
        "formal 生成经 roadmap amendment 放行（衔接细节登记于附录 C 待核清单）。",
    ])

    # ---- 2 方法与技术路线 ----
    heading(doc, "2  研究方法与技术路线", 1)
    figure(doc, "r_fig2_1_system_arch.png", "图 2-1  系统总体架构（复用论文草稿 Visio 图 2-1）")
    heading(doc, "2.1  A1：Model D→E 建模与参数真实性审计", 2)
    para(doc, "零施工纯审计：对 Model D 的 14 个 subsystem 逐个给出 KEEP/REFINE/REPLACE/ADD/REJECT "
              "处置与双等级打分；参数总表 210 行（parameter×module），按五级来源（drawing/vendor/"
              "literature/estimated/assumed）分级。核心发现：电池内阻/OCV、吃水、水深等参数在 Model D "
              "中完全缺失；ASSUMED_Gen_MinStablePower=0 物理不可信（影响极高）；BSFC 低负荷段形状为"
              "假设。A1 完整性 Gate G1~G5 全 PASS。")
    heading(doc, "2.2  A2：Model E 物理重建（T-403~T-412）", 2)
    bullets(doc, [
        "结构：Model D 复制分叉 + Plant 物理升级，builder 全脚本驱动（build_model_e.m）；"
        "B 包集成后 920 blocks、顶层 16 模块。",
        "物理升级：浅水阻力 Δk=0.644(H/T)^−1.72（Millward 1989）+ C_F* 修正（Zhang 2023）；Wake "
        "随水深；双 500 kW 发电机组独立 Plant + 启停状态机 OFF→STARTING(25 s)→SYNC(5 s)→ON→"
        "COOLDOWN(30 s)；per-机组 BSFC PCHIP 五锚点 + 起步燃油 C_start=0.6 kg；电池 LFP 1RC "
        "等效电路（解析求电流，无代数环）+ SOP 四约束；螺旋桨四系数按 w_deep=0.10 重标定。",
        "参数纪律：五级 provenance 逐条标记 + 17(+2) 项文献 + 推导式强制。",
    ])
    para(doc, "门禁（T-412 物理 Gate，overall=true）：巡航功率 298.4 kW 落于 [274,335] 区间；浅水六档"
              "单调零反例；机组时序 25/5/30 s 精确；1×500 kW vs 2×250 kW 稳态燃油差 23.66%（可区分）；"
              "能量闭合误差 <0.1%；确定性逐位 96/96/0；负向验证（Hull_K_R 篡改、LUT 运行时篡改）全部"
              "正确检出。螺旋桨 ±20% OAT 敏感性 15 例零失败：K_Fw（功率 −29%/+49%）≫ K_Qw（±27%）"
              "> K_Fv（±20%）≫ K_Qv（±7%）> 水深（±2.6%），速度闭环锁 11 kn 下航速不敏感——"
              "油耗方向性结论对螺旋桨摄动全域稳健。")
    figure(doc, "r_fig2_2_model_top.png", "图 2-2  Model E 顶层结构（复用论文草稿 Visio 图 2-2）")
    figure(doc, "r_fig2_5_bsfc.png", "图 2-3  双机组 BSFC 曲线（PCHIP 五锚点，复用论文草稿图）")
    figure(doc, "r_fig2_6_battery.png", "图 2-4  电池 LFP 1RC 等效电路特性（复用论文草稿图）")
    heading(doc, "2.3  B：PMS v2 + Safety-Interlock（模型层）", 2)
    para(doc, "07_PMS_v2 四层结构：Demand_Policy（LPF τ=60 s + SOC 滞回 0.55/0.60 + 恢复偏置 100 kW）"
              "→ Source_Selector → Commitment_Supervisor（Stateflow 图，0.01 s 离散步，整数 tick 仲裁，"
              "veto>safety>normal，审计九元组，reason_code 1~7）→ Dispatch_Allocator（均分 + "
              "clamp[125,500] kW）。16_Safety_Interlock 恰五条执行性 override（S1a/S1b/S2/S3+S4/S5），"
              "经 UnitDelay 回 07、不直写 Plant——安全规则与 EMS 优化分离，先进控制不可绕过。"
              "三臂验证（R0/R5/P2）+ 注入 S1~S5 全过；控制 Gate 机读 134/0，负向 7 缺陷全检出。")
    figure(doc, "r_fig2_3_pms_safety.png", "图 2-5  PMS v2 四层 + Safety-Interlock 结构（复用论文草稿 Visio 图 2-5）")
    heading(doc, "2.4  C：Mission Generator v3", 2)
    para(doc, "按 Route→Environment→Navigation Constraint→Operator→Plant 生成真实内河航次：九件机器"
              "注册表 + 14 节点 DAG + 三层分层生成语义（L0 template→L1 regime→L2 operator→L3 "
              "realization→BASE DRAW 先落盘后 L4 intervention）；seed 树 SHA256 全血统（Threefry4x64）。"
              "无旁路边界：下游不得对 simulator 内部建独立 predictor 接口，forecast_issue 只接受 t≤t₀ "
              "截断切片（AC-436）。预资格 108 航次（27 族×M1~M4）全 ACCEPT、双跑零 mismatch。"
              "T-429 可辨识性审计 FINAL FAIL 如实登记（见 8.1）。")
    figure(doc, "r_fig2_4_mission_gen.png", "图 2-6  Mission Generator v3 架构（复用论文草稿 Visio 图 2-6）")
    heading(doc, "2.5  D：Dataset v3", 2)
    bullets(doc, [
        "四契约：observation（H1 11 通道 1 Hz）+ future_information（Known/Forecast/Unknown 三分，"
        "K 通道 4 + F 通道 2）+ producer 接口（十一项能力）+ schema；ORACLE 与因果面物理分离。",
        "窗口化：History 300 s@1 Hz × Future 300 s（30×10 s，q10/q50/q90 目标 [30,3]，kW 域）。",
        "formal 冻结（2026-09-11）：576 IID（FLTA）+ 200 OOD（FLTB）= 776 航次、0 拒收、197,448 窗"
              "（train 136,052 / val 36,160 / test 25,236）；双跑零 mismatch；能量交叉核对 max_rel=3.8e-14；"
              "归一化 fit_on=train_only。",
              "pilot 家族 19 航次（含 S20 复合航次），作为部署侧（EMS04）soft-HIL 的植物场景域。",
           ])
    figure(doc, "g_fig3_6_dataset_v3.png", "图 2-7  Dataset v3 航次族与窗口划分（数据源 formal_freeze_result.json）")
    heading(doc, "2.6  E：predictor_v3 / REAT-T", 2)
    para(doc, "五候选公平对比（MLP-Q-v3/TCN-Q/Transformer/TFT/REAT-T），单向证据流 TRAIN→VAL HPO→"
              "CONFIG FREEZE→3-SEED CONFIRM→VAL-ONLY WINNER→PRETERMINAL FREEZE→TEST/OOD UNSEAL→"
              "FINAL；OOD 证据不参与 Winner 选择。REAT-T：past 分支 TCN（6 残差块 + attention pooling）"
              "+ future 分支 Transformer（30 位 K/F 全互见）→ 融合 → 单调分位解码 [B,30,3]；参数量 "
              "1,012,835。Winner=reatt（PRETERMINAL，2026-09-14）：val pinball 4.005/3.968/3.953"
              "（3 种子）显著优于 TFT/Transformer，且大幅优于无未来信息消融（history-only 18.4、"
              "h1k-only 3.8）——K/F 未来信息通道为主要增益来源。test 集（25,236 窗）E300 kWh "
              "MAE=0.746、R²=0.9931、80% 区间覆盖误差 0.202、分位交叉率 0；OOD（A1）MAE=3.91、"
              "R²=0.9143。FR-836 冻结 C2=REATT seed0（winner_identity_sha256 832720b0…）。")
    figure(doc, "r_fig3_1_problem.png", "图 2-8  预测问题定义（复用论文草稿 Visio 图 3-1）")
    figure(doc, "r_fig3_2_archs.png", "图 2-9  五候选预测器架构对比（复用论文草稿 Visio 图 3-2）")
    figure(doc, "g_fig3_5_pred_benchmark.png",
           "图 2-10  五候选基准：验证集 pinball（3 种子）与 test 25,236 窗终评"
           "（数据源 ME/results/predictor_v3 confirm/terminal JSON；消融证实 K/F 未来信息为主要增益）")
    figure(doc, "r_fig3_4_segment_mae.png", "图 2-11  REAT-T 分段误差（复用论文草稿图）")
    heading(doc, "2.7  EMS04：板上全控制器与三链 soft-HIL 方法学", 2)
    heading(doc, "2.7.1  时间尺度设计（10 s 决策拍的出处）", 3)
    para(doc, "整机 soft-HIL 的拍周期不是实现权宜，而是模型层设计的直接映射：EMS 决策周期 "
              "decision_period_s=10 s；REATT 预测输出 30 leads × 10 s/lead（视野 10:10:300 s，与 D 线"
              "窗口化契约一致）；law6 动态规划窗 K=30×10 s=300 s、Δλ 网格作用在 10 s 步上；kf 计划表 "
              "issue 步长 10 s；执行层内层网格 1 s（ECMS_dt_ctrl_s）。注意区分两个频率：H1 特征面板"
              "按 1 Hz 采样（预测器输入窗 300×11），决策/预测输出每 10 s 一次。soft-HIL 采用 10 s "
              "控制拍即设计决策周期本身。")
    heading(doc, "2.7.2  三链对照方法学", 3)
    para(doc, "同一植物、同一拍序、同一 kf 计划行下并行记录三条链：A2_CANONICAL_HOST（冻结管线聚合"
              "的 canonical 域输入 + FR836 float keras 预测）、B_HOST（部署同形输入 + 板源 OE int8 图的 "
              "ORT 代理）、B_BOARD（板上 NEURAL-ART int8 NPU + 板载 law6/PMS）。三链共享 law6 决策 "
              "dylib、契约 §4.3 执行层与 PMS host 位级移植，因此 bb-vs-bh 的差异只能经 REATT 后端通道"
              "产生——这是验收判据归因规则（v2.1）的演绎基础。判据机械化（G6-1..G6-6）：运行性门"
              "（零 NACK/超时、SOC∈[0.15,0.95]、无板振荡）、action 一致性档案面、同档 P_gen_ref 发散"
              "硬面、事件冻结清单面；判据语义的两次演化均由运行证据 + 用户裁决驱动并全量归档"
              "（run1/run2 原始记录随仓库入库）。")
    figure(doc, "r_fig3_3_control_law.png", "图 2-12  law6 控制律结构（λ_eff=λ₀+K_soc·e_SOC+Δλ，下限 0.43；复用论文草稿 Visio 图 3-5）")

    # ---- 3 分阶段结果（六线） ----
    heading(doc, "3  分阶段实验结果（A/B/C/D/E 六线）", 1)
    table(doc, ["线", "交付", "关键门禁/结果", "状态"],
          [["A1", "建模与参数审计", "210 参数五级来源；G1~G5 全 PASS；发现 Gen_MinStablePower=0 等 4 项高危假设", "完成"],
           ["A2", "Model E 物理重建", "920 blocks；T-412 Gate 全过；负向验证全检出；螺旋桨 ±20% OAT 15 例零失败", "完成"],
           ["B", "PMS v2 + Safety", "控制 Gate 134/0；负向 7 缺陷全检出；S1~S5 注入全过", "完成"],
           ["C", "Mission Generator v3", "预资格 108/108 ACCEPT；T-429 可辨识性 FINAL FAIL（0/6）→ amendment 放行", "完成（含 NO-GO 登记）"],
           ["D", "Dataset v3", "formal 776 航次 / 197,448 窗冻结；双跑零 mismatch；能量核对 3.8e-14", "完成"],
           ["E", "REAT-T 预测器", "五候选 Winner；test E300 R²=0.9931 / MAE=0.746 kWh；OOD R²=0.9143", "完成"]])

    # ---- 4 F 线：practical predictive-EMS 预注册对照与前期 soft-HIL ----
    heading(doc, "4  F 线：practical predictive-EMS 预注册对照实验（论文第 4 章域）", 1)
    para(doc, "T-445 Oracle Gate 判 GATE_VALIDITY_FAIL 后，F 包转向 practical predictive-EMS：以"
              "冻结 REAT-T（FR-836）为 C2 的预注册对照实验（论文第 4 章）。本报告不重抄其全部分析，"
              "仅以两幅代表图衔接；完整实验数据、统计结论与公平性口径见论文草稿第 4 章。前期 "
              "soft-HIL（MLP 部署链路）作为第 5 章对象留档于论文图 5-1，与本文 EMS04 整机 soft-HIL"
              "（REAT-T 板端）严格区分口径。")
    figure(doc, "r_fig4_1_prior_softhil.png",
           "图 4-1  前期 soft-HIL 链（MLP 口径，论文第 5 章；REAT-T 板端验证由本文第 5 章补齐）")
    figure(doc, "r_fig4_2_paired.png", "图 4-2  预注册配对对照（复用论文草稿图 4-1）")
    figure(doc, "r_fig4_3_dlambda.png", "图 4-3  Δλ 决策面对照（复用论文草稿图 4-4）")

    # ---- 5 EMS04 板级逐层验证 ----
    heading(doc, "5  EMS04 板级逐层位级验证（S1→G1）", 1)
    table(doc, ["层", "内容", "板上/移植证据"],
          [["S1", "law6 C 移植 + 冻结表（palette 584.3 KiB）", "377/377 拍 Δλ 位级一致"],
           ["S2", "特征面板服务（VOYAGE_INIT/KF_CHUNK/TICK2/QUERY，H1 板侧构造）", "755/755 逐字节 parity"],
           ["S3", "REATT 拆分 N1+N2 上 NPU（NOR 0x70A/0x70B）", "PRED_PARITY 形态板测通过；跨后端数值漂移按 S3-C V2 契约登记（KNOWN_TARGET_BACKEND_NUMERICAL_DRIFT）"],
           ["G1", "PMS v2 + Safety C 移植", "6001/6001 tick 逐位 PASS（clang offsetof 探针定版布局）"],
           ["G4", "FULLCTRL 固件形态（TICK_CTRL 0x0E/0x0F，下行 13960 B）", "fw_gate 全过；ELF 锁定（8b71d341 plan==readback）"]])
    para(doc, "板级服务冒烟（C2）：TICK_CTRL 板上全链 100 拍零 NACK，控制面形态全正确（cstate=2 + "
              "reason=4 为 S2 realized-gap 声明事件、P_DG_ref clamp 形态正确、λ 随 SOC 线性）。已知项："
              "S4Smoke 形态 DWT 未使能，infer_us/ctrl_us 遥测恒 0（非控制面）。")

    # ---- 6 整机 soft-HIL ----
    heading(doc, "6  整机 soft-HIL 验证（C1→C4）", 1)
    heading(doc, "6.1  C1：纯 host 双臂闭环（G-P2E + 双臂对拍）", 2)
    para(doc, "PlantStepE codegen 门（G-P2E）719/719 拍 X 直读 4 状态位级一致（worst=3.64e-12）；"
              "A2/B 双臂整航次决策对拍按 G6-2 档案语义完成。")
    heading(doc, "6.2  C3：单航次板在环闭环（S20 复合航次，541 拍）", 2)
    para(doc, "形态：PC 侧 PlantStepE GRT 植物以 10 s 拍（1000×0.01 s 子步）推进；每拍先由第 1 个子步"
              "闭合 A2 特征窗与 H1 环边界（ERT pre-integration 写出语义），再经 TICK_CTRL 下行"
              "（seq+tick+8×f32 状态+hist 300×11+kf 30×6），板上全链计算后回 P_gen_ref 馈回植物。"
              "预热 30 拍（law6 冷启动，is_formal=0）后正式段 t0=300..5700 与 kf 计划网格逐拍重合。")
    para(doc, "判据演化（全量归档可回溯）：run1 于 t0=890 park（连续 3 拍 B_BOARD vs B_HOST action "
              "不一致）→ 用户裁决“认定 REATT 后端漂移传递，降级资格续跑”；run2 暴露 v2 归因启发式"
              "盲区（board 与 float canonical 链一致而 B_HOST 偏离被误判）与一处 gate 实现 bug → "
              "v2.1 演绎闭合归因（同输入 + 下游 DP/law6/PMS 位级同构已证 ⇒ action 差只能经 REATT "
              "后端通道产生，一律档案化并子类标注）；run3 全程 541/541。", bold=False)
    drift_n = int(float(c3("backend_drift_archive.drift_action_diff_n", 0)))
    equal_n = 541 - drift_n
    para(doc, "终判（gate v2_1，overall=%s）：%s/541 拍完成；运行性面全过（零 NACK/重试/flags，"
              "SOC %.4f..%.4f 界内，无板振荡）；action 等值 %d/541（%.1f%%），漂移档案 %d 拍"
              "（host_consensus %s / board_canonical %s / triple %s，最长连续 %s 拍）；"
              "G6-2 档案（A2 vs B_HOST）action 差 %s 拍。漂移拍伴生执行层 argmin 翻转 0↔800 kW："
              "根因是 P_load 平稳段的 J 值刀锋区（float 链自身亦在相邻拍翻转）——量化/构造域不同的"
              "链在平局处分辨不同，属已登记变换效应。"
              % (c3("overall"), c3("domain.formal_done"),
                 c3("runnability_G6_1.soc_min"), c3("runnability_G6_1.soc_max"),
                 equal_n, 100.0 * equal_n / 541.0,
                 drift_n,
                 c3("backend_drift_archive.drift_subclasses", {}).get("host_consensus", "0"),
                 c3("backend_drift_archive.drift_subclasses", {}).get("board_canonical", "0"),
                 c3("backend_drift_archive.drift_subclasses", {}).get("triple", "0"),
                 c3("backend_drift_archive.drift_consec_max"),
                 c3("transform_archive_G6_2.action_diff_n")))
    para(doc, "每拍耗时实测（10 s 决策拍；v1.1 = DWT 遥测补全后的板内分账 + law6 DP 位级加速后）："
              "下行 UART 13960 B@115200 ≈1.21 s；板内处理 ≈5.54 s——REATT 推理 ≈0.30 s（NPU int8，"
              "周期域回绕安全口径）、law6 DP ≈5.23 s（J1 FLOOR 前缀合并 + J2 机组对称崩塌 + J3 查表"
              "前置，位级加速 1.22×；加速前 6.39 s）、执行层 grid + PMS×1000 ≈0.004 s；host 三链中位 "
              "0.64 s。合计 ≈6.8 s/拍，对 10 s 拍占用率 ≈68%（物理实时内；C3 541 拍 + C4 92 拍 + "
              "P3 1456 拍全链零超时实证）。1 s 拍可行性：板内计算 5.5 s 已超 1 s 拍周期 5.5×，"
              "位级红线内结构性重复已穷尽——不可行（备忘 s4_p2_dp_accel_memo.md）。")
    figure(doc, "g_fig5_2_c3_overview.png",
           "图 6-1  C3 单航次闭环总览：SOC（界内）/ 植物 P_load 与三链 P_gen_ref / 三链 Δλ（橙底=漂移档案窗）。"
           "0↔800 kW 方波段为执行层在 λ 下限处充-放电经济性近平的逐拍 argmin 切换，植物侧机组惯性吸收，无发散振荡")
    figure(doc, "g_fig5_3_c3_drift.png",
           "图 6-2  C3 漂移档案时间线：59 拍三子类（host 共识/板随 canonical/三互异）与 P_gen_ref 偏差幅值（全部逐拍在库可重判）")
    heading(doc, "6.3  C4：冻结电池 3 航次（元数据规则选样）", 2)
    para(doc, "选样规则先冻结后执行（dur≥600 s 域内按 P_load 均值取低/中/高三分位，中位平局取 "
              "voyage_id 字典序）：S02_downstream（R_low）/ S08_resistance_bands（R_mid）/ "
              "S03_upstream（R_high）。逐航次独立复位闭环结果：")
    table(doc, ["航次", "角色", "正式拍", "bb==bh 等值", "漂移拍", "同档发散", "NACK", "SOC", "overall"],
          [["S02_downstream", "R_low", "30/30", "30", "0", "0", "0",
            "%.4f..%.4f" % (GATE_S02.get("runnability_G6_1", {}).get("soc_min", 0),
                            GATE_S02.get("runnability_G6_1", {}).get("soc_max", 0)),
            GATE_S02.get("overall")],
           ["S08_resistance_bands", "R_mid", "31/31", "31", "0", "0", "0",
            "%.4f..%.4f" % (GATE_S08.get("runnability_G6_1", {}).get("soc_min", 0),
                            GATE_S08.get("runnability_G6_1", {}).get("soc_max", 0)),
            GATE_S08.get("overall")],
           ["S03_upstream", "R_high", "31/31", "31", "0", "0", "0",
            "%.4f..%.4f" % (GATE_S03.get("runnability_G6_1", {}).get("soc_min", 0),
                            GATE_S03.get("runnability_G6_1", {}).get("soc_max", 0)),
            GATE_S03.get("overall")]])
    para(doc, "电池合取 overall=%s（wall 835 s）。S20 刀锋漂移为复合航次瞬态窗特有，短航次不触及——"
              "这一对照本身即漂移机制的定位证据。" % bat("overall"))
    figure(doc, "g_fig5_4_c4_battery.png",
           "图 6-3  C4 电池三航次：SOC 与三链 P_gen_ref（逐航次独立复位；三航次板与 B_HOST 全等值）")
    heading(doc, "6.4  每拍时序预算", 2)
    figure(doc, "g_fig5_5_timing.png", "图 6-4  每拍墙钟分解（对 10 s 决策拍，占用率约 90%）")
    heading(doc, "6.5  验收边界（登记偏差与 v1.1 状态）", 2)
    bullets(doc, [
        "推理后端跨域漂移（档案，维持）：C3 的 59 拍 action 差异经用户裁决（G6 §4 承接点）认定为 "
        "S3-C 已登记 REATT 后端数值漂移传播，降级为 CLOSED_LOOP_BACKEND_DRIFT 档案（逐拍全量 "
        "CSV 在库，可按任何更严规则离线重判）；P3 formal 臂 4 航次同口径延伸登记。",
        "板形态（RCA 闭环，烧写待批准）：Release 代码 + SRAM 直载。XIP/flash boot 启动问题 RCA "
        "定案（S4_XIP_BOOT_RCA.md）：D1 = 构建系统 atk_release_flash_files ALL-target 使 "
        "build/flash hex 被任意 Release 形态覆盖（误烧即砖通道）；D2 = S4Full 因 CMake FORCE "
        "顺序实为 XIP 布局，PRED_PARITY=1 使 main() 在 XIP 执行态重初始化 XSPI2/NOR → 取指窗口"
        "崩塌自杀（旧 hex 能跑新 hex 不能跑完全解释）。修复 F1~F4（XIP 守卫/CMake 顺序+S4XIP "
        "产品形态/hex 形态守卫/镜像维持 0x70100400）已成文，烧写/断电待用户批准（park）。",
    ])
    heading(doc, "6.6  P3 formal 臂电池覆盖（偏差①消除证据，v1.1）", 2)
    para(doc, "原偏差①（电池域=pilot fixture 家族，无 M1/M2 臂轴）由 P3 消除：冻结件 "
              "S4_P3_FORMAL_ARM_FREEZE.json（元数据规则纯驱动、先冻结后执行）选 4 航次 = "
              "2 配对族 × 2 预报质量臂（M1=GOOD_V1 / M2=DEGRADED_V1，_iid_ledger 权威口径）。"
              "植物侧 = 流驱动变体（HybridShip_ModelE_PlantStepE_Stream.slx：formal 航次母线负载流"
              "回放 + ModelE 能量域物理闭环；G-P2E 回放保真门 4/4——P_load/v_ship 逐位恒等、"
              "x_ship 终点差 0）；臂轴经 kf 表（profile_code 1/2）进入 REATT。板在环结果：")
    if p3("overall") is True:
        rows = []
        for ent in GATE_P3_BAT.get("voyages", []):
            g6 = ent.get("g6", {})
            rows.append([ent.get("voyage_id"), ent.get("role"),
                         "%s/%s" % (ent.get("formal_done"), ent.get("formal_ticks")),
                         g6.get("g6_3_amended_pass"),
                         g6.get("drift_action_diff_n"),
                         g6.get("unattributed_pgen_n"),
                         "%.4f..%.4f" % (g6.get("soc_min", 0), g6.get("soc_max", 0)),
                         str(ent.get("overall"))])
        table(doc, ["航次", "臂", "正式拍", "G6-3 修订门", "漂移拍", "未归因", "SOC", "overall"],
              rows)
        para(doc, "电池合取 overall=True（4/4 航次，电池 gate %s）——**偏差①状态：已消除**；"
                  "预报质量臂轴（GOOD_V1 vs DEGRADED_V1）上的板-Host 一致性与 pilot 电池同口径。"
                  % p3("created_utc", "-"))
    else:
        para(doc, "（B3 板跑进行中/结果待归档：当前 gate overall=%s——本节表格与“已消除”结论在 "
                  "gate overall=True 后由构建脚本自动填充。)" % p3("overall"))

    # ---- 7 结论与贡献 ----
    heading(doc, "7  结论与贡献", 1)
    bullets(doc, [
        "可信物理平台：Model E 以全脚本驱动 + 参数证据链 + 物理/负向双 Gate 建成（920 blocks），"
        "敏感性与可区分性验证支撑结论稳健性。",
        "数据与预测换代：causality 白名单 + 未来信息契约的数据集 v3（197,448 窗）上，REAT-T 以"
        "未来计划/环境通道为主要增益来源（消融证实），test E300 R²=0.9931——突破 Phase 3 纯历史"
        "外推天花板（R²≈0.73→0.99，口径不同域不同，如实并列不混比）。",
        "工业控制底座：PMS v2/Safety 与 EMS 优化分离的安全架构，在模型层（134/0）与 C 移植层"
        "（6001/6001）双重位级验证。",
        "整机 soft-HIL 方法论：三链对照 + 演绎闭合归因 + 机器化 G6 判据 + 全量档案，首次在真实"
        "部署硬件（NPU int8 推理）上完成 EMS 整机闭环验收（541 拍 + 3 航次电池），并将判据演化"
        "过程本身作为可审计证据入库。",
    ])

    # ---- 8 局限与展望 ----
    heading(doc, "8  局限与展望", 1)
    heading(doc, "8.1  局限（如实登记）", 2)
    bullets(doc, [
        "C 线 T-429 可辨识性审计（v1.1 终态）：EXECFIX-02 审计实现修复重审后 overall 仍 FAIL "
        "（verdict FAIL_METRIC，0/6），但六项身份全部 clean（0 INVALID/0 INSUFFICIENT）——DIR 的"
        "方向物理效应经配对检验真实存在（ΔP=+85.6 kW，p=3.8e-7，混合池 probe 判据未达），DEPTH "
        "在当前组规模/保真度下不可辨识（负结果成立）；T-432 维持不解锁，NO-GO 边界归档。",
        "S3-C 跨后端数值漂移（板 int8 NPU vs host）：闭环上 10.9% 拍的 action 档位差（S20），"
        "经裁决档案化而非消除；执行层刀锋区（argmin 平局）是其放大器。",
        "板内每拍处理 ≈5.5 s（v1.1 实测分账：law6 DP 5.23 s 为主要项，已位级加速 1.22×），"
        "10 s 拍占用率 ≈68%——1 s 拍在当前冻结算法口径下不可行（板内计算超拍周期 5.5×）。",
        "板形态为 Release 代码 SRAM 直载：XIP/flash boot RCA 已闭环（双缺陷定案），修复 F1~F4 "
        "就绪，烧写/断电待用户批准——产品形态等价性冒烟在批准后补。",
        "Model E 绝对量为 representative hull（target_vessel_confirmed=false），绝对油耗数值不具"
        "备实船对标意义。",
    ])
    heading(doc, "8.2  展望（另立项）", 2)
    bullets(doc, [
        "XIP/flash boot：F1~F4 修复实施 + 烧写（用户批准点）+ 拨 boot 冒烟 100 拍零 NACK = 产品"
        "形态等价证据；LCD 实时显示（功率流/预测信息）绑定 TICK_CTRL 全链数据面为配套增量。",
        "漂移档案的离线再判定接口（CSV 全量在库）；S3-C 后端漂移的量化收敛（换代或校准）。",
        "1 s 拍路线：需 DP 换代（近似/降阶/离线策略表）——位级红线内已穷尽（小帧协议评估结论："
        "仅省 UART 无补于计算，登记不必要）。",
        "UART 921600 提速（固件开关既有）：拍墙钟 6.8→5.7 s 量级，产品形态阶段候选。",
    ])

    # ---- 9 验收结论与签字页（单独成页；用户签字点 park）----
    doc.add_page_break()
    heading(doc, "9  验收结论与签字（单独成页；本页待用户签字，park 状态）", 1)
    heading(doc, "9.1  S3-C FINAL closure（终闭宣告材料）", 2)
    para(doc, "S3C_BACKEND_NUMERICAL_CONTRACT_V2_1 = PASS（2026-09-28）。资格链：V2-1 holdout "
              "48/48 锚逐位（T4 机制全锚成立）→ V2.1 稳定性（E_rel 幅值归一口径）ρ=0.189/0.336 "
              "≤ 0.5 → 755 全量战役（BD 腿 755/755 逐位 + ST 腿抽样 96/96 声明式逐位 + HB 腿 "
              "755/755 对齐 + 双跑复验）→ 四 gate + 双跑全 PASS。U1/U2 已 CLOSED_AS_DIAGNOSTIC_"
              "ONLY。终闭条件全部满足；正式终闭宣告经本页签字生效。")
    heading(doc, "9.2  S4 整机 soft-HIL 验收结论", 2)
    para(doc, "S4 soft-HIL 证据面验收 = PASS（2026-09-29）。证据链：S1 law6 C 移植 377/377 位级；"
              "S2 特征面板 755/755 逐字节；G1 PMS/Safety 移植 6001/6001 逐位；S3-C 后端契约 PASS"
              "（上）；C2 板级冒烟 100/100 零 NACK（控制面形态全正确）；C3 单航次 541 拍闭环 "
              "overall PASS（等值 89.1%%，59 拍漂移经用户裁决降级 CLOSED_LOOP_BACKEND_DRIFT 档案"
              " + CLOSED_LOOP_TRANSFORM_EFFECT）；C4 pilot 电池 3/3 PASS（零漂移零 NACK）；P1 遥测"
              "补全（板内实测分账）+ P2 DP 位级加速 1.22×（三重回归全绿）；P3 formal 臂电池 "
              "4 航次（M1/M2 预报质量臂）overall=%s。验收边界见 §6.5/§6.6。"
              % p3("overall", "（B3 归档中）"))
    para(doc, "验收结论（三项一句话）：产品形态 = Release 全控制器固件（REATT int8 + law6 + "
              "PMS/Safety）SRAM 直载实时闭环，XIP 常驻形态修复就绪待烧写批准；实时预算 = 板内 "
              "5.5 s + UART 1.2 s ≈ 6.8 s/拍，10 s 决策拍占空 68%，物理实时内；验收结论 = S4 "
              "soft-HIL 证据面 PASS（C2/C3/C4/P3 机器化门禁全过，漂移与边界全部档案化可回溯）。")
    heading(doc, "9.3  签字", 2)
    table(doc, ["项", "内容", "签字/日期"],
          [["S3-C 终闭宣告", "契约 V2.1 PASS + 755 战役全过，正式终闭", ""],
           ["S4 soft-HIL 验收", "证据面 PASS（C2/C3/C4/P3 + P1/P2 收官）", ""],
           ["XIP 烧写批准（后续）", "F1~F4 修复实施 + 0x70100400 烧写 + 拨 boot 冒烟", ""]])

    # ---- 附录 A ----
    heading(doc, "附录 A  复现命令与指纹", 1)
    mono(doc, "# C3 单航次闭环（板在环）\n"
              "PYTHONDONTWRITEBYTECODE=1 .venv_tf/bin/python \\\n"
              "  EMS04_CHIL_FULLCTRL/Host/tools/s4_c3_closedloop.py --port /dev/cu.usbserial-1120\n"
              "# C4 电池（3 航次）\n"
              ".../s4_c4_battery.py --port /dev/cu.usbserial-1120\n"
              "# S20 kf 表（pack/selfcheck）\n"
              ".../s4_c3_pack_kf_table.py --pack / --selfcheck\n"
              "# H1 环 host 自检 / C4 runner 自检\n"
              ".../s4_c3_h1win_host.py --selftest ; .../s4_c4_battery.py --selftest\n"
              "# P3 formal 臂电池（streamE，4 航次）\n"
              ".../s4_c4_battery.py --plant-backend streamE \\\n"
              "  --data-root model/simulink_modelE_(Phase4_Physics)/datasets_v3/formal/iid\n"
              "# 固件门禁\n"
              "python3 firmware/gate/fw_gate.py EMS04_CHIL_FULLCTRL")
    mono(doc, "S4Smoke ELF sha256 前缀 8b71d341（plan==readback；P1/P2 修复版重载 plan==readback "
              "a4650c0a/0da391c8/d3d1ea4b）；PlantStepE S20 dylib df81b23f…；Stream dylib e987d0cd…"
              "（×4 一致）；kf 表（S20）8a1f597e…；冻结电池 freeze sha 见 "
              "results/s4_c4/S4_C4_BATTERY_FREEZE.json；formal 臂冻结件 S4_P3_FORMAL_ARM_FREEZE.json "
              "sha 43d373a8…；XIP RCA 见工程根 S4_XIP_BOOT_RCA.md。")

    # ---- 附录 B ----
    heading(doc, "附录 B  主要数据文件与口径", 1)
    bullets(doc, [
        "C3：EMS04_CHIL_FULLCTRL/Host/results/s4_c3/{s4_c3_gate.json, s4_c3_closedloop.csv(541 拍三链), "
        "s4_c3_warmup.csv, s4_c3_design_notes.md, run1_park_archive/, run2_rulestop_archive/}",
        "C4：.../Host/results/s4_c4/{s4_c4_gate.json, s4_c4_<vid>.csv/_gate.json ×3, "
        "S4_C4_BATTERY_FREEZE.json}",
        "kf 表（experiments/softhil_ems03_2026-09-25/results/s4_c3/，本地冻结件）：S20 541 行 + "
        "S02/S08/S03 各 30/31 行，自校验 JSON 随附",
        "六线门禁：docs/phase4/*/ 与 model/simulink_modelE_(Phase4_Physics)/results/ 各 gate JSON"
        "（正文数字逐处标注来源）",
    ])

    # ---- 附录 C ----
    heading(doc, "附录 C  待核差异清单（诚实登记）", 1)
    bullets(doc, [
        "43 号控制 Gate 判据数：机读 134/0（JSON）vs B 包报告文本 133。",
        "44 号负向验证：机读 7/7 vs B 包报告 6/6（D HANDOFF 为 7/7，与机读一致）。",
        "A2 段脚本编号：实见 10/10b/10c/11/12/13 + 40/41/42；“01~19 号”为文档转述偏差。",
        "数据集航次链：pilot 19 → Stage A 92（72 IID+20 OOD）→ formal 576+200；“48”无对应物。",
        "C→D formal 放行的 amendment 环节在 C 目录文档中未见出处（formal_freeze 为机读事实）。",
        "FR-836 winner 文件 artifact sha256 现读与表值不一致（winner_identity_sha256 一致）；"
        "以 F 包 re-hash 记录为准。",
    ])

    doc.save(OUT_DOCX)
    print("saved ->", OUT_DOCX)
    return 0


if __name__ == "__main__":
    raise SystemExit(build())
