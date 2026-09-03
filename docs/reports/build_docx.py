#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""组装《甲醇混合动力船舶边缘 AI 自适应 ECMS 能量管理策略——从建模到 soft-HIL 真闭环验证的
研究总结报告》（导师版）Word 文档（docs/reports/*.docx）。

数据纪律：
  - 四臂×两场景 / 两两对比 / 仪器级①② / 层③ / FR-508 全部实时读取
    firmware/.../Host/results/closedloop/closedloop_metrics.json（T-615，唯一事实源；
    2026-09-03 增补 TCN 换型臂 b_tcn_ref_*，数据来自 TCN64 工程闭环会话）；
  - 历史阶段数字（Phase 0/1/2、G0~G3、v1 部署）取自仓库内报告/门禁 JSON，
    在本脚本中以字面量登记并注明来源，与 docs/research_roadmap.md 一致；
  - 图表来自 docs/reports/figures/（实验数据图由 generate_figures.py 生成；架构示意图由
    generate_model_schematic.py 生成；Simulink 模块截图由 export_model_figures.m 导出）。

幂等可重跑；相对路径（仓库根由脚本位置推导）。
用法：model/simulink/.venv_tf/bin/python docs/reports/build_docx.py
"""

import json
import os

from docx import Document
from docx.enum.table import WD_TABLE_ALIGNMENT
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.oxml.ns import qn
from docx.shared import Inches, Pt, RGBColor

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
FW = os.path.join(ROOT, "firmware", "stm32n6", "04_SourceCode", "SoftwarePackage",
                  "EMS_Projects", "EMS01_CHIL_ECMS")
METRICS = os.path.join(FW, "Host", "results", "closedloop", "closedloop_metrics.json")
FIGDIR = os.path.join(HERE, "figures")
OUT_DOCX = os.path.join(HERE, "EMS01_研究总结报告_soft-HIL真闭环_20260902.docx")

REPORT_DATE = "2026-09-02"


def load_metrics():
    with open(METRICS) as f:
        return json.load(f)


# ---------------------------------------------------------------- docx 基础设施

def set_run_font(run, size=10.5, east="宋体", ascii_f="Times New Roman", bold=None, color=None):
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
    for name, size in (("Heading 1", 16), ("Heading 2", 13), ("Heading 3", 11.5)):
        h = doc.styles[name]
        h.font.name = "Times New Roman"
        h.font.size = Pt(size)
        h.font.bold = True
        h.font.color.rgb = RGBColor(0, 0, 0)
        h.element.get_or_add_rPr()
        rf = h.element.rPr.get_or_add_rFonts()
        rf.set(qn("w:eastAsia"), "黑体")


def para(doc, text="", size=10.5, bold=None, align=None, east="宋体", space_after=6, indent=False):
    p = doc.add_paragraph()
    if align is not None:
        p.alignment = align
    p.paragraph_format.space_after = Pt(space_after)
    if indent:
        p.paragraph_format.first_line_indent = Pt(21)
    if text:
        set_run_font(p.add_run(text), size=size, bold=bold, east=east)
    return p


def heading(doc, text, level=1):
    h = doc.add_heading(text, level=level)
    for r in h.runs:
        set_run_font(r, size={1: 16, 2: 13, 3: 11.5}[level], east="黑体", bold=True)
    return h


TBL_NO = {"n": 0}


def add_table(doc, header, rows, widths=None, font_size=9, caption=None, note=None):
    if caption:
        TBL_NO["n"] += 1
        caption = "表 %d：%s" % (TBL_NO["n"], caption)
        c = para(doc, caption, size=9.5, bold=True, align=WD_ALIGN_PARAGRAPH.CENTER, space_after=2)
    t = doc.add_table(rows=1 + len(rows), cols=len(header))
    t.style = "Table Grid"
    t.alignment = WD_TABLE_ALIGNMENT.CENTER
    for j, h in enumerate(header):
        cell = t.rows[0].cells[j]
        cell.text = ""
        set_run_font(cell.paragraphs[0].add_run(str(h)), size=font_size, east="黑体", bold=True)
        cell.paragraphs[0].alignment = WD_ALIGN_PARAGRAPH.CENTER
    for i, row in enumerate(rows):
        for j, v in enumerate(row):
            cell = t.rows[i + 1].cells[j]
            cell.text = ""
            set_run_font(cell.paragraphs[0].add_run(str(v)), size=font_size)
    if widths:
        for j, w in enumerate(widths):
            for r in t.rows:
                r.cells[j].width = Inches(w)
    if note:
        para(doc, note, size=8.5, space_after=8)
    else:
        para(doc, "", size=6, space_after=4)
    return t


FIG_NO = {"n": 0}


def add_figure(doc, filename, caption, width=5.9):
    FIG_NO["n"] += 1
    path = os.path.join(FIGDIR, filename)
    p = doc.add_paragraph()
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    p.add_run().add_picture(path, width=Inches(width))
    para(doc, "图 %d：%s" % (FIG_NO["n"], caption), size=9, align=WD_ALIGN_PARAGRAPH.CENTER, space_after=10)


# ---------------------------------------------------------------- 数据格式化

def fmt(x, nd=6):
    return ("%." + str(nd) + "f") % x


ARM_ORDER = ("a_npu_onboard", "b_ref_onboard", "bp_native_law7", "c_native_law5")
ARM_NAME = {
    "a_npu_onboard": "(a) NPU 板载闭环",
    "b_ref_onboard": "(b) REF 板载闭环",
    "bp_native_law7": "(b′) 原生 law=7（EML）",
    "c_native_law5": "(c) 原生 law=5（基线）",
}
PAIR_ORDER = (
    "a_npu_onboard vs c_native_law5",
    "b_ref_onboard vs c_native_law5",
    "bp_native_law7 vs c_native_law5",
    "a_npu_onboard vs b_ref_onboard",
)


# ---------------------------------------------------------------- 报告正文

def build():
    d = load_metrics()
    doc = Document()
    init_styles(doc)
    doc.core_properties.title = "甲醇混合动力船舶边缘 AI 自适应 ECMS 能量管理策略——研究总结报告"
    doc.core_properties.author = "hybrid-ship-ems 项目组"

    # ---------- 封面 ----------
    for _ in range(4):
        para(doc, "")
    para(doc, "甲醇混合动力船舶边缘 AI 自适应 ECMS 能量管理策略",
         size=22, bold=True, align=WD_ALIGN_PARAGRAPH.CENTER, east="黑体", space_after=6)
    para(doc, "——从建模到 soft-HIL 真闭环验证的研究总结报告",
         size=16, bold=True, align=WD_ALIGN_PARAGRAPH.CENTER, east="黑体", space_after=24)
    para(doc, "（导师汇报版）", size=12, align=WD_ALIGN_PARAGRAPH.CENTER, space_after=30)
    for line in (
        "项目：hybrid-ship-ems（甲醇混合动力船舶能量管理系统）",
        "平台：Simulink Model D + STM32N647（Cortex-M55 + Neural-ART NPU）",
        "报告日期：%s" % REPORT_DATE,
        "数据基准：EMS01_CHIL_ECMS/Host/results/closedloop/closedloop_metrics.json（T-615，含 TCN 换型臂增补）等仓库产物",
        "状态：内部研究资料，数据均可按附录 A 复现",
    ):
        para(doc, line, size=11, align=WD_ALIGN_PARAGRAPH.CENTER, space_after=4)

    doc.add_page_break()

    # ---------- 目录（静态） ----------
    heading(doc, "目录", 1)
    for line in (
        "摘要",
        "1  研究背景与问题定义",
        "2  研究方法与技术路线",
        "     2.1  系统建模：Model D 船舶功率流与直流母线电气模型",
        "     2.2  EMS 控制律族",
        "     2.3  预测体系：MLP-Q 分位数预测 + 工况门控",
        "     2.4  边缘部署：STM32N647",
        "3  分阶段实验结果（Phase 0 → Phase 2.5）",
        "4  HIL 验证（v1：边缘部署 + 开环 HIL）",
        "5  真闭环 soft-HIL 实验（v3：四臂 × 两场景，本次核心）",
        "6  结论与贡献",
        "7  局限与展望",
        "附录 A  复现命令与指纹",
        "附录 B  主要数据文件与口径",
    ):
        para(doc, line, size=11, space_after=3)
    doc.add_page_break()

    # ---------- 摘要 ----------
    heading(doc, "摘要", 1)
    para(doc,
         "甲醇混合动力船舶的发电机组燃油消耗率（BSFC）随负荷强烈非线性，低负荷恶化显著；电池作为能量缓冲可"
         "将机组运行点搬移至高效区。本研究的核心问题是：在不依赖未来信息、可嵌入边缘硬件的约束下，能量管理策略"
         "（EMS）能捕获多少节油收益，并能否在真闭环回路中验证。研究沿“离线动态规划（DP）上限验证 → 实时 SOC 自适应 "
         "ECMS → 神经网络预测增强 → 数据集重做与预测体系换代 → STM32N6 边缘部署 → soft-HIL 真闭环”的路线递进，"
         "每一步均以机器可读门禁裁决。最终在 STM32N647 板载固件（NPU int8 推理）与主机侧被控对象推进器 "
         "（Simulink 子集模型 codegen，lock-step 同步）构成的四臂 × 两场景真闭环实验中：S20 复合航次四臂 SOC 终点"
         "全部落在 ±0.02 公平带内，NN 预测增强（MLP-Q 分位数预测 + 工况门控）较 SOC 自适应 ECMS 基线带内节油 "
         "-2.25 ~ -2.65 kg fuel_raw（≈0.68%~0.80%）；NPU 板载闭环相对参考后端的零混杂移植代价 +0.375 kg（≈+0.11%）；"
         "voyage002 场景电荷维持修正口径 fuel_corr 收益 -15.7 kg。仪器链 G-P1/G-P2 位级一致（max|Δ| ≤ 3.4e-8），"
         "仪器级①②门禁 4 组全 PASS，FR-508 故障注入回退语义在闭环中验证成立。v3 收官后按 FR-511 编译期换型"
         "流程换入更大候选模型 TCN-f64（165,851 参数，int8 219.7 KB）：经 REF 流式增量后端（依赖闭包精确重算，"
         "板上 5521 拍 P_gen exact 100%）完成双场景闭环——TCN 节油增益 -2.20 kg（-0.66%）落在 MLP-Q 增益带内，"
         "voyage002 与 MLP-Q 仅差 -0.069 kg，“更大模型入闭环”的平台能力得到完整验证；其 NPU 臂层②则暴露 "
         "Neural-ART 对 dilated 栈 int8 执行语义分歧（三路线 NO-GO，如实登记）。至此，从物理观察、上限验证、"
         "控制律设计、预测体系、边缘部署到真闭环验证的完整证据链闭合。",
         indent=True)
    para(doc, "关键词：甲醇混合动力；能量管理策略（EMS）；ECMS；分位数负荷预测；STM32N6；NPU；硬件在环（HIL）；soft-HIL 真闭环",
         size=10, bold=True)

    # ---------- 1 研究背景与问题定义 ----------
    heading(doc, "1  研究背景与问题定义", 1)
    heading(doc, "1.1  对象与物理问题", 2)
    para(doc,
         "研究对象为近海工作/示范用途的甲醇混合动力船：2×500 kW 甲醇发电机组经整流挂直流母排，"
         "1071.36 kWh 动力电池经双向 DC/DC 同母排，推进逆变器驱动推进电机——直流母排下机组转速与推进负荷解耦。"
         "机组 BSFC 随负荷强烈非线性：最优 427 g/kWh，25% 负荷时恶化 +30%~+70%（双真实锚点标定）。"
         "电池可在低负荷放电、高负荷充电，把机组运行点搬移到高效区——这就是节油收益的物理来源。",
         indent=True)
    para(doc, "研究目标：设计 EMS，在保证电量维持（电池不放干、SOC 降额阈值 0.25 不触及）的前提下，最小化全程甲醇消耗。",
         indent=True)
    heading(doc, "1.2  两轮 NO-GO 与第一纪律", 2)
    para(doc,
         "立项前两轮尝试（定时前馈、多步预测）均在控制器施工完成后因“收益上限在机理层面不存在”而 NO-GO。"
         "由此确立本研究第一纪律：先用离线最优（DP）验证节油收益上限存在，再投入任何控制器施工；"
         "每个阶段的 gate 判定均机器可读（JSON），不达标不进入下一阶段。", indent=True)
    heading(doc, "1.3  研究问题链", 2)
    para(doc,
         "Phase 0：收益上限存在吗（物理）？Phase 1：不依赖未来信息的实时 ECMS 能捕获多少？"
         "Phase 2/2.5：预测能再补多少、什么数据与预测体系可信？Phase 3：能否落地到边缘硬件并在真闭环中验证？"
         "本报告即对该问题链的最终回答。", indent=True)

    # ---------- 2 研究方法与技术路线 ----------
    heading(doc, "2  研究方法与技术路线", 1)
    heading(doc, "2.1  系统建模：Model D 船舶功率流与直流母线电气模型", 2)
    para(doc,
         "全部模型脚本驱动、可从零重建（禁止 GUI 手工状态）：设备参数表（2026-05-26，图纸确定值与 ASSUMED 估计值"
         "严格分离）→ 统一参数脚本（config/Model_Parameters.m）→ 首个整机模型 → Model A MVP（规则 EMS vs 预测增强 EMS "
         "最小可比平台）→ Model B（加回直流母线电气层）→ Model D（model/HybridShip_ModelD_DCBus.slx，"
         "Phase 1/2/2.5 正式平台，亦是第 5 章 v3 真闭环被控对象的来源模型）。本节按模块介绍 Model D 的"
         "顶层架构、各子系统物理语义与参数体系。", indent=True)
    heading(doc, "2.1.1  顶层架构与模块划分", 3)
    para(doc,
         "Model D 顶层由 14 个子系统构成（编号 07 不再使用：Model A 的电池均值模块在引入直流母线电气层后"
         "由 14_Battery_Converter 取代），按功能可分为四条链：", indent=True)
    for s in (
        "工况与航速链（01 → 02 → 03 → 04）：把“船怎么开”转化为母线侧总负荷 P_load_total；",
        "能量管理链（05）：根据负荷、电池状态与船况决策机组功率指令 P_gen_ref——本研究被优化的对象；",
        "功率流链（06 → 13 ⇄ 14）：甲醇机组经整流、电池经双向 DC/DC 共同挂接 750 V 直流母排，"
        "母排再经推进逆变驱动推进电机；",
        "监视与记录（08 / 09 / 11 / 12 / 15）：欠压降额、功率残差一致性监视、航程终止、数据记录与 CCS 电压带报警。",
    ):
        para(doc, "• " + s, indent=True)
    para(doc,
         "建模约定：双推进支路对称聚合等效（图纸单支路额定 ×2）；功率一律 kW，速度 m/s，距离 m，SOC ∈ [0,1]，"
         "P_bat > 0 为放电；求解器定步长 ode3 @ 0.01 s，EMS 控制拍 1 s，信号经 logsout 日志导出。",
         indent=True)
    add_figure(doc, "model/modeld_arch_schematic.png",
               "Model D 顶层架构示意：功率/能量流（实线）与控制/信号流（虚线）。框内为各子系统关键物理语义与参数。",
               width=6.3)
    add_figure(doc, "model/modeld_top.png",
               "Model D 顶层 Simulink 实现全景（14 个子系统 + 信号日志；由 scripts/10_build_modelD.m 从零脚本构建，"
               "可完全复现）。", width=6.3)
    add_table(
        doc,
        ["编号", "子系统", "职责", "关键输入 → 输出"],
        [
            ["01", "Scenario_Manager", "目标航速/水流/阻力倍率剖面、日用负荷、场景号",
             "x_ship → v_cmd / v_current / res_mult / P_hotel / scen_id"],
            ["02", "Ship_Speed_Controller", "PI 航速环（限幅抗饱和）", "v_cmd, v_ship → P_prop_demand"],
            ["03", "Propulsion_Plant", "推进电机-齿轮箱-轴系-螺旋桨-船体动力学",
             "demand/limit/v_current/res_mult → P_prop_actual, v, x, a, R, F, n"],
            ["04", "Load_Measurement", "母线侧负荷聚合", "P_prop_actual, P_hotel → P_load_total"],
            ["05", "EMS_Controller", "Variant 变体（law=0~7），功率分配决策", "P_load/SOC/v/x/a/t → P_gen_ref"],
            ["06", "Genset_Average_Plant", "机组均值模型：限幅-爬坡-惯性", "P_gen_ref → P_gen_actual"],
            ["08", "Undervolt_Derate", "母线欠压 → 推进功率降额", "V_dc, demand → P_prop_limit / P_unserved"],
            ["09", "Bus_Residual_Monitor", "母线功率残差一致性监视", "P_gen / P_bat / P_load → P_residual"],
            ["10", "Fuel_Consumption", "BSFC 查表 + 燃油流量积分", "P_gen_actual → fuel_total"],
            ["11", "Voyage_Stop_Logic", "到达航程终止仿真", "x_ship → stop"],
            ["12", "Data_Logging", "信号日志配置说明（logsout）", "—"],
            ["13", "DC_Bus", "母线电容电压动态", "P_gen / P_bat / P_load → V_dc"],
            ["14", "Battery_Converter", "双向 DC/DC 下垂并网 + SOC 积分与降额",
             "V_dc → P_bat_bus / SOC / dis·chg 可用功率"],
            ["15", "Bus_Voltage_Monitor", "CCS 稳态/瞬态电压带报警（不干预）", "V_dc → alarm ×2"],
        ],
        font_size=8.5,
        caption="Model D 顶层子系统清单（14 个；构建脚本 +model/build_model_c.m 为唯一事实源）")
    heading(doc, "2.1.2  工况与航速控制链（01 / 02 / 04 / 11）", 3)
    para(doc,
         "01_Scenario_Manager 以 MATLAB Function 生成三类剖面：目标航速 v_cmd（支持距离节点 mode=0 与时间节点 "
         "mode=1 两种模式，后者使 S20 复合航次可经 VOYAGE_* 工作区变量在仿真编译期注入，批量航次无需重建模型；"
         "过渡由下游 Rate Limiter 按加/减速率平滑）、水流速度 v_current（S20 第 9 段逆水 -1 m/s，其余为 0）、"
         "阻力倍率（4 类剖面 + 最多 4 段阻力带，距离触发、平滑过渡）。02_Ship_Speed_Controller 为 PI 航速环"
         "（比例 + 限幅积分抗饱和，输出限幅至电机额定 700 kW）。04_Load_Measurement 输出 "
         "P_load_total = max(P_prop_actual, 0) + P_hotel，是 EMS 与母线共同的负荷接口。"
         "11_Voyage_Stop_Logic 在 x_ship ≥ 航程时终止仿真。", indent=True)
    heading(doc, "2.1.3  推进与船体（03）", 3)
    para(doc,
         "03_Propulsion_Plant 是模型的动力学核心，信号链为：可用功率限幅 min(demand, limit) → 驱动滞后一阶环节"
         "（τ = 0.5 s）→ 功率限幅 → 电-机综合效率 0.92 → 转矩换算（角速度除零保护）→ 电机转矩限幅 ±T_max → "
         "齿轮箱（减速比 6.08，效率 0.96）→ 轴系动力学 J·dω/dt = T_drive - Q_prop - B·ω（聚合惯量 J = 1600 kg·m²）"
         "→ 螺旋桨水动力 → 船体纵向动力学 → 积分得 v_ship 与 x_ship。", indent=True)
    para(doc,
         "螺旋桨采用简化水动力模型：进速 V_A = (1-w)·(v_ship - v_current)（伴流分数 w = 0.15），"
         "推力 F = K_Fw·ω|ω| - K_Fv·V_A|V_A|，转矩 Q = K_Qu·ω|ω| - K_Qv·V_A|V_A|，"
         "四个 K 系数由系泊点与设计点双锚定。船体阻力 R = m(x)·K_R·V_rel|V_rel|（K_R ≈ 1780 N/(m/s)²，"
         "按服务航速 11 kn、Fn∇≈0.57 文献区间交叉校核；m(x) 为 01 给出的阻力倍率），"
         "纵向方程 dv/dt = (F - R + F_env)/M，等效质量 M = 1.0×10⁶ kg（约 1000 t 排水量）。",
         indent=True)
    add_figure(doc, "model/modeld_03_propulsion.png",
               "03_Propulsion_Plant 内部结构：功率限幅 → 驱动滞后 → 转矩-齿轮箱-轴系 → 螺旋桨/船体 MATLAB Function "
               "→ v/x 积分（模型截图，脚本构建）。", width=5.9)
    heading(doc, "2.1.4  发电与燃油（06 / 10）", 3)
    para(doc,
         "06_Genset_Average_Plant 为机组常开均值模型：指令限幅 [0, 1000] kW → 爬坡率 ±40 kW/s（双机聚合）→ "
         "一阶惯性 τ = 5 s → 输出限幅；最低稳定功率取 0（可怠速空载，属 ASSUMED 假设，三档敏感性已量化登记）。"
         "10_Fuel_Consumption 以 1-D 查表给出 BSFC(P_gen)，燃油流量 ṁ = P·BSFC/3.6×10⁶ 积分得累计油耗。"
         "BSFC 曲线以两个真实锚点标定（最优 427 g/kWh；25% 负荷恶化 +30%~+70%，见 §1.1），默认 7 断点表为 "
         "[0 100 200 400 600 800 1000] kW → [1200 700 550 480 450 430 453] g/kWh；Phase 0 起经 "
         "config/ecms_bsfc_override.mat 注入统一口径（保证 Baseline 与 DP 严格同口径）。", indent=True)
    heading(doc, "2.1.5  直流母线电气层（13 / 14 / 08 / 09 / 15）", 3)
    para(doc,
         "13_DC_Bus 将母排建模为聚合电容：C·V·dV/dt = P_gen·η_rect + P_bat_bus - P_load_total"
         "（C = 40 F，η_rect = 0.97，dV/dt = P_res·1000/(C·max(V, 50))，电压限幅保护）。"
         "14_Battery_Converter 以电压下垂方式并网（grid-forming）：P_bat = clamp(K_droop·(V_nom - V_dc), "
         "-chg_avail, dis_avail)，K_droop = 10 kW/V（经灵敏度扫描固化）；SOC 由电池端功率积分并限幅 "
         "[0.10, 0.95]，充放电效率 0.95、DC/DC 效率 0.97 折算；可用功率按 SOC 降额（放电自 SOC = 0.25 起线性降至 "
         "0.10 截止，充电自 0.90 起降至 0.95 截止）——该降额即 §1.1 所述 SOC 0.25 阈值的模型化。",
         indent=True)
    para(doc,
         "08_Undervolt_Derate 在母线欠压时按 f = clamp((V - 600)/(675 - 600), 0, 1) 线性降额推进可用功率"
         "（CCS 稳态下限 675 V 至瞬态下限 600 V），并输出未满足功率 P_unserved 供监视；09_Bus_Residual_Monitor "
         "输出母线功率残差（应等于 C·V·dV/dt），供后处理校验电气层一致性；15_Bus_Voltage_Monitor 按 CCS 2026 "
         "稳态带 [675, 795] V / 瞬态带 [600, 802.5] V 越限报警（只报警、不干预）。", indent=True)
    add_figure(doc, "model/modeld_14_battery.png",
               "14_Battery_Converter 内部结构：电压下垂 → 可用功率限幅 → 母线侧功率；SOC 积分与充放电降额"
               "（模型截图）。", width=5.5)
    add_figure(doc, "model/modeld_13_dcbus.png",
               "13_DC_Bus 内部结构：功率平衡 ΣP → dV/dt → 母线电压积分（模型截图）。", width=4.6)
    heading(doc, "2.1.6  EMS 控制器变体结构（05）", 3)
    para(doc,
         "05_EMS_Controller 为 Variant Subsystem，由工作区变量 EMS_CTRL_LAW 选择七个变体之一；全部变体共享统一"
         "接口（7 入 1 出：P_load_total、SOC、v_cmd、v_ship、acceleration、x_ship、t → P_gen_ref），"
         "使控制律消融可在同一被控对象上严格对照。变体清单见下表，控制律语义与数学形式见 §2.2。",
         indent=True)
    add_table(
        doc,
        ["law", "变体", "语义", "研究角色"],
        [
            ["0", "EMS_Baseline", "负荷跟随 + SOC 修正（规则型）", "基线对照"],
            ["1", "EMS_AI_Enhanced", "即时前馈（早期 AI 形态）", "历史对照"],
            ["2", "EMS_ScheduledFF", "定时前馈（T-004）", "NO-GO 研究线（§1.2）"],
            ["4", "EMS_Fixed_ECMS", "固定 λ 单步网格穷举 ECMS（T-103）", "C1（出带，参考）"],
            ["5", "EMS_SOC_Adaptive_ECMS", "λ(t) = λ₀ + K_soc·(SOC_ref - SOC)（T-105）", "C2 基线主控"],
            ["6", "EMS_Oracle_ECMS", "law=5 + 全知未来前瞻 ê（T-106）", "C3 上界（非实时可实现）"],
            ["7", "EMS_NN_AECMS", "law=6 同构，ê 来自 NN 预测（T-204；G2 门控）", "C4 / 最终 Winner"],
        ],
        font_size=8.5,
        caption="05_EMS_Controller 变体清单（Variant 由 EMS_CTRL_LAW 编译期选择，无 law=3）")
    add_figure(doc, "model/modeld_05_ems_law7.png",
               "law=7（EMS_NN_AECMS）变体内部：ecms_step MATLAB Function（网格穷举 + SOC 硬约束 + λ 钳位）"
               "+ NN 权重/归一化常数显式接线（绕开参数固化缺陷）→ 输出限幅（模型截图）。", width=3.0)
    heading(doc, "2.1.7  参数体系与假设登记", 3)
    para(doc,
         "参数全部集中于 config/Model_Parameters.m，图纸确定值与 ASSUMED 估计值严格分离（构建时同步输出 "
         "reports/parameter_audit.csv 供审计）。关键参数见下表；BSFC 低负荷段形状、最低稳定负荷等假设参数"
         "的三档敏感性已在 Phase 0 量化（§7.1），不及预期的指标不在假设参数上硬调。", indent=True)
    add_table(
        doc,
        ["类别", "参数", "取值", "来源"],
        [
            ["电池", "容量 2×535.68 = 1071.36 kWh；充放效率 0.95", "图纸 / ASSUMED"],
            ["机组", "额定 2×500 = 1000 kW；爬坡 ±40 kW/s；惯性 τ=5 s", "图纸 / ASSUMED"],
            ["推进电机", "额定 2×350 = 700 kW；1500 rpm；综合电效率 0.92", "图纸 / ASSUMED"],
            ["支路", "双推进支路对称聚合（×2）", "图纸（用户确认）"],
            ["母线", "额定 750 V；聚合电容 40 F；η_rect=η_dcdc=0.97", "设备参数 / ASSUMED"],
            ["船体", "等效质量 1.0×10⁶ kg；K_R≈1780 N/(m/s)²；w=0.15", "ASSUMED（文献校核）"],
            ["传动", "齿轮箱 6.08 / 效率 0.96；轴系 J=1600 kg·m²", "ASSUMED"],
            ["电池并网", "下垂系数 10 kW/V；SOC 窗 [0.10,0.95]，降额起始 0.25/0.90", "ASSUMED（扫描固化）"],
            ["BSFC", "7 断点查表，最优 427 g/kWh 锚点；低负荷 +30%~+70%", "真实锚点标定"],
            ["求解", "ode3 定步长 0.01 s；EMS 控制拍 1 s", "模型设定"],
        ],
        font_size=8.5,
        caption="Model D 关键参数（图纸确定值与 ASSUMED 估计值分离登记；完整 69 项见 parameter_audit.csv）")
    heading(doc, "2.1.8  模型可信度与在闭环验证中的复用", 3)
    para(doc,
         "第 5 章 v3 真闭环实验的被控对象并非另建模型，而是由本模型经原位子集变换得到"
         "（HybridShip_ModelD_PlantStep.slx：删除 05/12，根级注入 P_gen_ref，主模型 sha256 零改动断言）；"
         "其对冻结参考解释执行位级一致（G-P1，max|Δ|=0）、codegen C 与解释执行等价（G-P2，最差 3.4e-8）"
         "——即本节所介绍的模型就是第 5 章闭环实验中“板上看到的那条船”，仪器链证据见 §5.2。", indent=True)
    heading(doc, "2.2  EMS 控制律族", 2)
    para(doc,
         "ECMS 核心思想：每控制步在 P_gen 可行网格上求 argmin [ṁ_f(P_g) + λ·P_bat_eq]，把电池充放电折算为"
         "等效燃油，λ 为电价因子。控制律族递进：law=5（SOC-自适应 ECMS，基线）、law=6（oracle 全知未来前瞻，上界）、"
         "law=7（NN 预测 A-ECMS：未来 300 s 能量需求预测 ê 经 SOC_ref 前瞻通道驱动，K_S=0.01 保守增益）。"
         "全部控制器为嵌入式可行设计（单步网格穷举 ≤200 点，无迭代优化器）。", indent=True)
    heading(doc, "2.3  预测体系：MLP-Q 分位数预测 + 工况门控", 2)
    para(doc,
         "预测目标为未来 300 s 能量需求 Ê（与 BSFC 非线性的“运行点优化”机理对齐，规避前两轮 NO-GO 的时序优化陷阱）。"
         "数据集 v2 由 400 条连续随机化航次构成（155,087 窗口，航次级 split 280/60/60，泄漏审计全过）。"
         "最终形态（G2 门控，用户裁决 Winner）：MLP-Q 分位数预测器（q10/q50/q90，21,571 参数，int8 26.2 KB）+ "
         "工况识别器 workclf（Conv1D，7 类，7,871 参数，int8 21.9 KB）+ 逆水工况门控（upstream 段预测关断、"
         "回退 law=5）。分位数区间宽度用于“AI 什么时候可以被相信”的安全门控。", indent=True)
    heading(doc, "2.4  边缘部署：STM32N647", 2)
    para(doc,
         "部署平台 AlienTek ATK-CNN647B（STM32N647：Cortex-M55 @800 MHz + Neural-ART NPU，外挂 256 Mbit NOR）。"
         "工具链 ST Edge AI v2.2.0 + Neural-ART 1.1.1：FP32 → int8 tflite → NPU 权重。"
         "各阶段 S20 基准油耗演进见图 7（口径注记见图内说明：Phase 2 与 Phase 2.5/3 的数字因数据集重做与"
         "workclf 归一化口径 v1.1 修订，不可直接互比）。", indent=True)
    add_figure(doc, "phase_evolution.png",
               "各阶段 S20 基准油耗演进（Phase 0 DP 上界 → Phase 3 板载闭环）。注意 C4（旧口径）、MLP-Q G2（v2 口径）与板载闭环（闭环口径）口径不同，不可直接互比。")

    # ---------- 3 分阶段实验结果 ----------
    heading(doc, "3  分阶段实验结果（Phase 0 → Phase 2.5）", 1)
    heading(doc, "3.1  Phase 0：离线收益上限（DP）", 2)
    para(doc,
         "8 个典型场景离线 DP 求解（SOC×P_gen 网格后向归纳，终端电量维持对称带 |SOC(T)-SOC(0)| ≤ 0.005）："
         "连续运行主判定口径节油上限 0.50%~4.22%，无场景 ≥5%（同可行域 P_min=0 口径 3.64%~4.61%；电池容量为强杠杆）。"
         "机器判定 DECISION_REQUIRED，用户裁决 GO_TO_PHASE1——收益机理真实存在（运行点优化）。", indent=True)
    heading(doc, "3.2  Phase 1：实时 ECMS 控制器链", 2)
    add_table(
        doc,
        ["控制器", "fuel_corr [kg]", "saving vs C0", "SOC_end", "捕获 DP 上限"],
        [
            ["C0 Baseline", "349.00", "—", "0.5784", "—"],
            ["C1 Fixed ECMS", "324.92", "+6.90%", "0.4394（出带）", "72.6%*"],
            ["C2 SOC-Adaptive", "330.66", "+5.26%", "0.6002 ✓", "55.3%"],
            ["C3 Oracle Predictive", "324.96", "+6.89%", "0.6039 ✓", "72.5%"],
            ["DP（上界）", "315.83", "+9.51%", "0.5963 ✓", "100%"],
        ],
        caption="Phase 1 S20 复合航次结果（tier2 BSFC；*C1 出带，其数字依赖大偏移线性修正，仅供参考）",
        note="机器判定 PROCEED_TO_PHASE2_NN：oracle 增量 +1.63 pp ≥ 0.5 pp 阈值，预测价值值得用 NN 逼近。"
             "（旧口径：v1 数据集，与 §3.3 口径差异见 3.4 节标注。）")
    heading(doc, "3.3  Phase 2：NN 预测通道（诚实边界）", 2)
    add_table(
        doc,
        ["控制器", "fuel_corr [kg]", "saving vs C0", "备注"],
        [
            ["C0 Baseline", "349.002", "—", "Phase 1 复用"],
            ["C2 A-ECMS（规则）", "330.662", "5.26%", "Phase 1 复用"],
            ["C4 NN-A-ECMS", "328.549", "5.86%", "本阶段新跑（旧口径）"],
            ["C3 oracle-A-ECMS", "324.957", "6.89%", "上界参照"],
            ["DP", "315.83", "9.51%", "物理上界"],
        ],
        caption="Phase 2 S20 结果（旧口径 = v1 数据集 + 旧归一化，见 3.4 节标注）",
        note="nn_oracle_capture = 0.3704 → PARTIAL（未达 ≥50%）；留出泛化 median -0.232%（4/10 航次为正）；"
             "部署预算完全满足（47k 参数 / 184 KB / <1 ms）。失败模式诚实登记：S07 桥区机动恶化 1.2 kg、低估偏向、"
             "R²≈0.73 天花板（约 27% 方差不可由历史 300 s 解释）。用户裁决：带 C4 进 Phase 3（C2 主控、C4 对照）。")
    heading(doc, "3.4  Phase 2.5：数据重做与预测体系换代（收官）", 2)
    para(doc,
         "针对 Phase 2 复盘发现的三个深层问题（数据模板化、单点预测无不确定性、选模缺闭环维度），"
         "Phase 2.5 完成：v2 数据集（400 航次连续随机化 + K-shape 覆盖度审计）、统一训练平台（MLP/TCN/CNN-MLP/"
         "N-BEATS × 分位数）、三级选模协议（val 资格筛选 → 闭环 capture → NPU 成本 Pareto）、G0~G3 门控消融。"
         "G0~G3 闭环消融结果（gate_result_v2.json，2026-09-02 重跑口径）见表 6。", indent=True)
    add_table(
        doc,
        ["门控", "分位数门控", "工况门控", "S20 fuel_corr [kg]", "capture", "FR-318 留出门禁", "判定"],
        [
            ["G0 裸预测", "—", "—", "328.0757", "0.4533", "FAIL（min -1.51%）", "不通过"],
            ["G1", "✓", "—", "328.3501", "0.4052", "PASS（median +3.49%）", "通过（最简备选）"],
            ["G2", "—", "✓", "328.125", "0.4447", "PASS（median +3.19%，min -0.85%）", "通过"],
            ["G3", "✓", "✓", "328.3996", "0.3966", "PASS（median +3.50%）", "通过"],
        ],
        caption="Phase 2.5 G0~G3 门控消融（gate_result_v2.json，2026-09-02 重跑口径）",
        note="用户裁决（2026-08-30）：最终 Winner = G2（MLP-Q 26 KB int8 + 工况识别器 ~22 KB int8 + 逆水门控）。"
             "决策基线为 v1_frozen 口径（gate_result_v2_v1_frozen.json，2026-08-30：G2 capture 0.5893、"
             "FR-318 median +3.78%）；重跑口径（v2 数据集 + workclf 归一化 v1.1）下 G2 capture 0.4447、FR-318 仍通过。"
             "机器推荐 G1 为最简安全方案，用户在 G1/G2 间裁决取 G2（闭环最优 + NPU 容量充裕）。")
    para(doc,
         "口径差异标注（重要）：§3.3 表 5 的 C4 = 328.549 kg 与 Phase 2.5 G2 = 328.095 kg（原生臂回放 fuel_raw）"
         "口径不同——前者为 v1 数据集 + law=7 无门控 + 旧归一化；后者为 v2 数据集重训 MLP-Q + workclf 归一化口径 v1.1 "
         "+ G2 门控。数据集 v2 重做与口径修订后两数不可直接互比；表 6 中 fuel_corr 口径差异同理。", indent=True)

    # ---------- 4 HIL 验证（v1） ----------
    heading(doc, "4  HIL 验证（v1：边缘部署 + 开环 HIL）", 1)
    heading(doc, "4.1  N6 板上 int8 验证（T-311，M1 报告）", 2)
    add_table(
        doc,
        ["验收项", "MLP-Q (mlpq)", "工况识别器 (workclf)", "结论"],
        [
            ["int8 模型大小", "tflite 26.2 KB（weights 21,537 B）", "tflite 21.9 KB（weights 8,129 B）", "数据完备"],
            ["activation RAM", "1,518 B", "808 B", "数据完备"],
            ["HW coverage", "6 epochs：4 NPU + 2 SW fallback", "22 epochs：16 NPU + 6 SW", "数据完备"],
            ["实测时延", "129 μs", "42.5 ms", "合计 ≤60 ms（1 Hz 拍占用 4.2%）PASS"],
            ["量化精度差", "val RMSE 9.9292→9.9659 kWh（+0.37%）", "val acc -0.02 pp", "记录性，量级极小"],
            ["板上 vs Python 一致性（FR-504）", "×200：p50=1 / max=6 LSB，无偏置", "argmax 一致 200/200 = 100%", "PASS（全项有余量）"],
        ],
        caption="T-311 八项验收摘要（完整数据见 m1_deploy_report.md；权重 NOR 双 1 MB mpool：0x70200000 / 0x70400000）")
    heading(doc, "4.2  N6 SRAM 加载完整性（v3 板会话，2026-09-02）", 2)
    para(doc, "双镜像经确定性复位 SRAM 加载（probe-rs，分块写入 + BSS 清零 + 逐块回读）："
              "加载计划与回读的 SHA-256 完全一致（Plan = Readback），"
              "VTOR/MSP/xPSR/PC 由向量表编程，PRIMASK/BASEPRI/FAULTMASK 归零后回读核验，"
              "证明板上运行的镜像与仓库构建产物逐字节一致，令牌机制单次核销。", size=10.5)
    add_table(
        doc,
        ["镜像", "ELF SHA-256（前 16）", "Plan/Readback SHA-256（前 16）", "加载模式"],
        [
            ["REF（软件参考后端）", "f31714eac47529f7", "2fd6cd8f6747acb3 = 2fd6cd8f6747acb3（MATCH）", "deterministic_reset + start"],
            ["NPU（int8 推理后端）", "e7eb99c69c745dd3", "6d38e108a99b2cd9 = 6d38e108a99b2cd9（MATCH）", "deterministic_reset + start"],
        ],
        caption="SRAM 加载完整性证据（加载会话记录；SRAM 白名单 [0x34000000, 0x34200000)，向量表 0x34000400）")
    heading(doc, "4.3  开环 HIL 三层指标与 v1.1 口径修订（M2 报告）", 2)
    add_table(
        doc,
        ["层", "内容", "S20", "voyage002", "判定"],
        [
            ["① 移植保真", "板上 REF vs Simulink 参考同拍 P_gen", "exact 100%（max ΔP=0）", "exact 100%", "PASS"],
            ["② NN 一致", "板上 NPU vs host-int8 网格一致率（门禁 ≥99%）",
             "98.877%（62/5521 拍 >1 网格）", "99.003%", "S20 边际 FAIL → (a) 接受登记"],
            ["③ 报告性", "板上 NPU vs Simulink 参考", "98.46%", "98.39%", "报告性（自 v1 的 45.5%/95.25% 大幅收敛）"],
        ],
        caption="v1 开环 HIL 三层指标（v1.1 workclf 喂入口径修订后板上复验，2026-09-08）",
        note="层② 裁决（用户，2026-09-08）：(a) 接受并登记为口径修订已知代价，层② 在 v3 中降为参考指标，"
             "不放宽任何既有门禁。机理：v1.1 后 ê 生效占比 11%→~90%，NPU↔解释器 LSB 级差异经 λ 链传播至网格边界"
             "（v1 被 ê=0 掩盖）；NN 端口误差本身极小（cls_match 99.96%/100%，max|Δê| 0.346/0.297）。"
             "FR-508 板上验证：四场景（NaN/停发/垃圾帧/复位恢复）全部符合 design 状态机。")

    # ---------- 5 真闭环 soft-HIL ----------
    heading(doc, "5  真闭环 soft-HIL 实验（v3：四臂 × 两场景）", 1)
    heading(doc, "5.1  方法学：PlantStep 被控对象 + codegen + lock-step", 2)
    para(doc,
         "v1 的层①②③均为开环注入形态（参考序列喂入，板子不反馈）。v3 构造 soft-HIL 真闭环：主机侧被控对象推进器"
         "取自 Model D 子集（HybridShip_ModelD_PlantStep.slx，12 个子系统，主模型 sha256 零改动断言），"
         "经 Simulink Coder 生成 C（grt.tlc，ode3 @0.01 s × 100 子步/拍，P_gen 零阶保持）编译为动态库；"
         "STM32N647 板载 EMS 与其按 lock-step 逐拍互馈（CMD_TICK 7×f32 状态入帧 → 板上 MLP-Q/workclf 推理 + "
         "G2 门控 + ECMS 网格穷举 → RSP_TICK P_gen_ref/ê/K/flags），SOC 首次被板子输出影响。固件零改动。", indent=True)
    para(doc,
         "ERT 时序语义：wrapper step() 后的 Outport 写出发生于子步末级阶段时刻 t=(k+1)-0.0025（ode3 第 3 级，"
         "占 1 s 控制拍 0.25%）；拍边界位级真值经 plantstep_get_states() 直读 9 个连续状态（design §4.4a）。"
         "拍耗时（NFR-602）：板载 RTT REF ≈23.4 ms vs NPU ≈10.7 ms——NPU int8 推理使单拍控制延迟降低约 54%；"
         "被控对象 wrapper ≈28 μs/拍；两者均远小于 1 s 控制拍。", indent=True)
    heading(doc, "5.2  仪器链：G-P1 / G-P2 位级", 2)
    para(doc,
         "G-P1（被控对象子集解释执行 vs ref_G2 冻结参考回放）：S20 5521 拍 / voyage002 4212 拍，SOC / v_ship / "
         "x_ship / P_load 逐拍 max|Δ| = 0（位级一致），fuel 终值逐位相等（328.094641944 kg）。"
         "G-P2（codegen C vs 解释执行）：拍边界 X 直读 9 状态逐拍对照，全场最差 3.4e-8（P_gen inertia 项），"
         "远小于 1e-6 阈值——codegen ≡ 解释执行等价成立。G-P1 残差分布见图 8（全部为零，直方图单杆即位级一致证明）。",
         indent=True)
    add_figure(doc, "gp1_residual_hist.png",
               "G-P1 逐拍残差分布（S20）：子集模型解释执行回放 vs ref_G2 冻结参考，SOC/v_ship/x_ship/P_load 全部 5521 拍 |Δ|=0（位级一致）。")
    heading(doc, "5.3  四臂 × 两场景闭环结果", 2)
    TCN_ARM = {"S20": "b_tcn_ref_cpu", "voyage002": "b_tcn_ref_voyage"}
    arms_rows = []
    for scn, scn_label in (("S20", "S20"), ("voyage002", "voyage002")):
        for arm in ARM_ORDER + (TCN_ARM[scn],):
            a = d["arms"][scn][arm]
            v_rmse = a.get("v_track_rmse_mps")
            if v_rmse is None:
                v_rmse = a.get("v_rmse")
            v_cell = ("—" if v_rmse is None else
                      ("%.4f" % v_rmse if arm.startswith("b_tcn") else "%.6f" % v_rmse))
            e_ratio = a.get("e_hat_active_ratio")
            arms_rows.append([
                scn_label, a.get("arm_label", arm),
                "%.6f" % a["fuel_raw_kg"], "%.6f" % a["fuel_corr_kg"], "%.5f" % a["SOC_end"],
                v_cell, "—" if e_ratio is None else "%.4f" % e_ratio,
            ])
    add_table(
        doc,
        ["场景", "臂", "fuel_raw [kg]", "fuel_corr [kg]", "SOC_end", "v_track RMSE [m/s]", "ê 生效占比"],
        arms_rows,
        caption="四臂 + TCN 换型臂 × 两场景核心指标（fuel_corr = fuel_raw + (0.60−SOC_end)×1071.36 kWh×0.4774 kg/kWh；"
                "数据：closedloop_metrics.json。全矩阵 SOC_min ∈ [0.586, 0.5968]，SOC<0.25 事件全零）",
        note="TCN-REF 流式臂（(b) TCN-REF 板载闭环）为 v3 收官后的编译期换型实验（独立板会话，§5.7）："
             "v_track RMSE 按其遥测精度（4 位小数）显示；该臂无 ê 门控遥测（—），其余口径与四臂一致。")
    add_figure(doc, "soc_s20_four_arms.png",
               "S20 四臂 + TCN 换型臂 SOC 轨迹（真闭环 (a)(b) + 原生臂 (b′)(c) + TCN-REF 流式臂，按 tick 对齐）。"
               "板载三形态轨迹不可分辨地重合；(c) 基线维持能量中性；TCN-REF 换型臂与 MLP-Q 臂几乎重合（SOC_end 差 8e-5）。")
    add_figure(doc, "soc_voyage002_four_arms.png",
               "voyage002 四臂 + TCN 换型臂 SOC 轨迹。NN 臂持续充电至 ~0.678（SOC 终点 +0.0794），为 SOC-UNFAIR 判定来源；"
               "fuel 结论须以 fuel_corr 口径解读；TCN-REF 换型臂与 MLP-Q 同样充电至 ~0.678。")
    add_figure(doc, "fuel_bars_two_scenarios.png",
               "四臂 + TCN 换型臂 × 两场景油耗对比（fuel_raw 主口径 + fuel_corr 电荷维持修正口径；纵轴截断以显差异；"
               "TCN-REF 流式臂口径见 §5.7）。")
    pw_rows = []
    for scn in ("S20", "voyage002"):
        for pc in d["pairwise_comparisons"][scn]:
            if pc["pair"] in PAIR_ORDER:
                pw_rows.append([
                    scn, pc["purpose"], "%.6f" % pc["fuel_raw_diff_kg"],
                    "%.6f" % pc["fuel_corr_diff_kg"], "%.8f" % pc["SOC_end_diff"], pc["fairness"],
                ])
    add_table(
        doc,
        ["场景", "对比（用途）", "Δfuel_raw [kg]", "Δfuel_corr [kg]", "ΔSOC_end", "判定"],
        pw_rows,
        caption="SOC 公平性判定（±0.02 带，design §8.2。FAIR：fuel_raw 差即主结论；SOC-UNFAIR：fuel_raw 差不作主结论，"
                "fuel_corr 为次级结论）",
        note="S20：NN 闭环增益 -2.25 ~ -2.65 kg fuel_raw（≈0.68%~0.80%，带内）；零混杂移植代价（(a) NPU vs (b) REF）"
             "+0.375 kg（≈+0.11%）。voyage002：SOC-UNFAIR，fuel_corr 次级结论 -15.7 kg（三形态一致）——预测增强收益体现为"
             "“少烧油多充电”的能量重分配。")
    heading(doc, "5.4  闭环正确性：仪器级①②", 2)
    ig_rows = []
    for gname, gkey in (("① 闭环(b) vs 开环参考(gp1)", "instrument_grade_1_closedloop_b_vs_gp1"),
                        ("② 闭环(b) vs 原生(b′)", "instrument_grade_2_closedloop_b_vs_native_bprime")):
        for scn in ("S20", "voyage002"):
            g = d[gkey][scn]
            items = {i["metric"]: i["value"] for i in g["items"]}
            ig_rows.append([
                gname, scn, "%.2e" % items["soc_end_abs_delta"], "%.2e" % items["fuel_raw_rel_delta"],
                "%.2e" % items["v_ship_rmse_mps"], "%.4f" % items["p_gen_grid_match_rate"],
                "PASS" if g["overall"] else "FAIL",
            ])
    add_table(
        doc,
        ["仪器级", "场景", "SOC 终点差 (≤0.005)", "fuel 相对偏差 (≤0.01)", "v_ship RMSE (≤0.01)",
         "P_gen 网格一致率 (≥0.95)", "overall"],
        ig_rows,
        caption="仪器级①②门禁（4 组全 PASS；差异源为 f32 序列化经 λ 网格边界的偶发翻转，无结构性偏差）")
    add_figure(doc, "pgen_board_vs_ref_s20.png",
               "板载 REF / TCN-REF 换型臂 P_gen vs 解释执行参考（S20，窗口 3000–3600 s）：全程 exact 率 99.18%、"
               "|ΔP|≤1 网格步 99.60%（仪器级① PASS）；TCN-REF 为换型臂行为对照，其与参考的 P_gen 位级差异属换型预期，"
               "不作门禁，见 §5.7。")
    add_figure(doc, "vship_tracking_s20.png",
               "航速跟踪全程（(b) REF 板载，S20）：v_ship 对 v_cmd 的全任务 RMSE = 0.351676 m/s（1 s 轴口径），四臂差异 ≤3e-6 m/s。")
    heading(doc, "5.5  临界工况与回退语义（FR-508 闭环注入）", 2)
    fr = d["critical_section_fr508"]
    fr_rows = []
    for key in ("F1_nan_ticks", "F2_stall_ticks", "F3_garbage_frames", "F4_reset_recovery"):
        s = fr["scenarios"][key]
        seg = s["fallback_segments"][0] if s["fallback_segments"] else None
        fr_rows.append([
            key, s["focus"].split("：")[1] if "：" in s["focus"] else s["focus"],
            "%d / %d" % (s["counts"]["n_fallback_segments"], seg["n_ticks"] if seg else 0),
            ("%.6f" % seg["d_fuel_kg"]) if seg else "0.000000",
            ("%.2e" % seg["d_soc"]) if seg else "0",
            "True",
        ])
    add_table(
        doc,
        ["注入场景", "焦点", "FALLBACK 段/拍", "段内 Δfuel [kg]", "段内 ΔSOC", "会话存活"],
        fr_rows,
        caption="FR-508 闭环故障注入（图像=NPU，stub advancer + pty mock，S20 60 拍。段内油耗为 mock 口径，"
                "验证对象为协议/flags/回退语义，不代表 plant 保真油耗，不进入研究级结论）",
        note="FALLBACK 汇总：2 段 / 3 拍 / Δfuel +0.040 kg / ΔSOC +7.78e-5。回退=安全降级继续控制："
             "P_gen 由回退律给出、被控对象正常推进、回路不解体（图 14 特写：FALLBACK 进入/恢复全程 P 无跳变、SOC/fuel 连续）。")
    add_figure(doc, "fr508_fallback_zoom.png",
               "FR-508 闭环注入回退特写（F1 NaN 注入 / F4 复位恢复）：FALLBACK（flags bit1）置位期间 ê=0、P_gen 无跳变，恢复后正常。")
    heading(doc, "5.6  层③报告性输出", 2)
    l3 = d["layer3_report_only_a_vs_refG2_p_gen"]
    para(doc,
         "闭环 (a) NPU vs ref_G2 解释执行同拍 P_gen 分布（报告性，不作门禁）：S20 exact 率 %.2f%% / ±1 网格率 %.2f%%，"
         "voyage002 %.2f%% / %.2f%%；max|ΔP| = 980 kW 集中于 tick 1 起动瞬态（闭环 (a) 自 v_ship=0 起动，"
         "开环参考无此瞬态），其余为零散 λ 网格边界翻转。与 v1 板上层③（45.5%%/95.25%%）及开环层③（98.46%%/98.39%%）口径链可追溯。"
         % (l3["S20"]["exact_rate"] * 100, l3["S20"]["plus1_grid_rate"] * 100,
            l3["voyage002"]["exact_rate"] * 100, l3["voyage002"]["plus1_grid_rate"] * 100), indent=True)

    heading(doc, "5.7  TCN-f64 编译期换型闭环验证（v3 收官后换型实验，2026-09-03）", 2)
    para(doc,
         "FR-511 编译期换型流程（换 NOR 权重 hex 即换模型）首次真实兑现：换入 phase25 同签名候选 TCN-f64"
         "（165,851 参数，int8 219.7 KB，双输入 [1,300,1]+[1,2]），验证“更大模型入闭环”的平台能力。"
         "按用户工程隔离规则，模型适配性修改在新工程 EMS01_CHIL_ECMS_TCN64 实施，P1 保持 v3 纯净态。"
         "因 batch TCN C 前向 1.35 s/拍（52M MAC double）超出注入 2 s STALE 契约，REF 后端改为"
         "依赖闭包精确增量前向：每拍仅重算末输出依赖的固定节点集（889 节点，MAC ÷7.1）；"
         "无跨拍状态设计使 bit-exact 构造性成立——L0 实测 streaming ≡ batch 逐位一致（9965/9965，max=0.00e+00），"
         "vs-keras ≤4.5e-5 kWh（门 0.266214 kWh 裕量约 6000 倍）。", indent=True)
    add_table(
        doc,
        ["项", "结果（REF 流式臂）"],
        [
            ["层① S20 全轨迹（INJECT 注入态）", "5521 拍 P_gen exact 100.0000%、max_dP=0、零错误（门禁 100% PASS）"],
            ["闭环 (b) S20", "328.544 kg，SOC_end 0.60002 带内；NN 增益 vs (c) law=5 = −2.20 kg（−0.66%）"],
            ["闭环 (b) voyage002", "264.156 kg，SOC_end 0.67826；与 MLP-Q (b) Δfuel 仅 −0.069 kg（基本无差）"],
            ["SOC_min", "0.587 / 0.586（> 0.25 降额阈值，全零事件）"],
        ],
        caption="TCN-f64 换型 REF 流式臂板级结果（数据：EMS01_CHIL_ECMS_TCN64/Host/results/closedloop/）")
    para(doc,
         "结论：TCN 经 REF 后端的闭环节油增益（−0.66%）落在 MLP-Q 的增益带内，voyage002 与 MLP-Q 基本无差——"
         "更大模型能带来相当（非更优）的闭环增益，且换型部署链（keras→deploy JSON→NOR 权重→固件编译期选择）"
         "完整可行。", indent=True)
    para(doc,
         "NPU 臂层② 未达标（如实登记）：板 NPU 对同一输入输出与解释器差 88 LSB_TCN（45.39 vs 21.96 kWh），"
         "而输入装配逐位正确（板上 dump 实证）——根因为 Neural-ART v1.1.1 对 64 通道 dilated 栈（14 层分解形态 + "
         "SW fallback 数据搬运 epoch 交接）的 int8 执行语义分歧。三种图形态均未达 ≥99% 门：2.2 分解图 34.0%、"
         "2.2 原生 dilated 重导出 47.7%、4.0.1 原生 stai API 44.6%（三路线 NO-GO；简单图 workclf 同 NPU 正常 "
         "99.96%，复杂图才暴露）。NPU 600MHz 提频实验亦 NO-GO：IC2 与 XSPI2 kernel clock 同域耦合，"
         "NOR@300MHz 超 MX25UM25645G 规格读失配致 LL_ATON_RT_Main 挂死，已回退原频率域留记录。"
         "层② (a) 臂门禁 FAIL，闭环 (a) 未跑；层② 出路已裁决：用户定最终路线为 4.0.1 全 CPU 编译形态"
         "（tcn_q_v2_cpu，无 --st-neural-art，68 节点 Lite 内核 CM55 执行，权重 611KB 编入 ELF，TCN 不再"
         "依赖 NOR 权重 hex；构建 + fw_gate 7/7 为交付），NPU 形态产物保留为回退资产，板上验证（层② 重验 + "
         "闭环 (a)）待接手执行，诊断证据链见 "
         "EMS01_CHIL_ECMS_TCN64/Host/results/tcn_swap/layer2_diagnosis_20260903.json。", indent=True)

    # ---------- 6 结论与贡献 ----------
    heading(doc, "6  结论与贡献", 1)
    conclusions = (
        ("建模可信（仿真即真值）",
         "全模型脚本驱动可从零重建；被控对象子集对冻结参考位级一致（G-P1 max|Δ|=0），codegen C ≡ 解释执行"
         "（G-P2 最差 3.4e-8）。仪器链证明“板上看到的船就是仿真里的船”，使后续一切闭环数字具备因果意义。"),
        ("控制律族与消融方法学",
         "law=5/6/7 递进消融（C2 捕获 DP 上限 55.3%、oracle 增量 +1.63 pp）确立“先上限、再实时、再预测”的"
         "立项纪律；四臂对比设计（NPU/REF/EML/law=5 基线）使 NN 增益与移植代价在同一被控对象上解耦度量。"),
        ("预测体系：数据—模型—门控三位一体",
         "数据集 v2（400 航次连续随机化 + 覆盖度/泄漏审计）证明数据质量是预测体系上限；MLP-Q 分位数输出"
         "（q10/q50/q90）+ 工况识别 G2 门控构成“AI 什么时候可以被相信”的安全机制；三级选模协议（val 筛选 → "
         "闭环 capture → NPU 成本 Pareto）以闭环结果定 Winner。"),
        ("边缘部署：千分位代价的 int8 NPU 化",
         "MLP-Q 26.2 KB int8 + workclf 21.9 KB 上板，NPU 推理 129 μs，板上-宿主一致性 max 6 LSB（FR-504）；"
         "真闭环中零混杂移植代价 +0.375 kg（≈+0.11%），NPU 使单拍控制延迟降低约 54%——边缘 AI 的实时性与"
         "经济性代价同时可忽略。"),
        ("真闭环验证：完整证据链闭合",
         "四臂 × 两场景 soft-HIL 真闭环：S20 带内节油 −2.25 ~ −2.65 kg（≈0.68%~0.80%，SOC 公平带内），"
         "voyage002 fuel_corr 口径 −15.7 kg；仪器级①② 4 组全 PASS；FR-508 回退语义闭环成立。"
         "从物理观察、上限验证、控制律设计、预测体系、边缘部署到真闭环的完整研究链闭合。"),
        ("编译期换型：更大模型入闭环的平台能力验证",
         "FR-511 换型流程首次真实兑现：TCN-f64（165,851 参数，int8 219.7 KB）经 REF 流式增量后端"
         "（依赖闭包重算 889 节点，MAC ÷7.1，bit-exact 9965/9965）完成层① 全轨迹 100% exact 与双场景闭环——"
         "节油增益 −2.20 kg（−0.66%）落在 MLP-Q 增益带内，voyage002 与 MLP-Q 基本无差；"
         "同时层② NPU 臂暴露 Neural-ART 对 dilated 栈 int8 执行语义分歧（三路线 NO-GO），"
         "已按用户裁决切全 CPU 编译形态落地——换型既验证了平台容量，也划定了工具链边界并给出了工程出路。"),
    )
    for i, (t, body) in enumerate(conclusions, 1):
        para(doc, "（%d）%s：%s" % (i, t, body), indent=True)

    # ---------- 7 局限与展望 ----------
    heading(doc, "7  局限与展望", 1)
    heading(doc, "7.1  局限（如实登记）", 2)
    for s in (
        "voyage002 场景 SOC-UNFAIR：NN 臂 SOC 终点 +0.0794 超出 ±0.02 带，fuel_raw 差不作主结论，"
        "收益解读依赖电荷维持修正口径 fuel_corr（-15.7 kg）——跨场景结论须按公平性判定分别陈述。",
        "层②（board-NPU vs host-int8）S20 98.877% 边际 FAIL 已按用户裁决接受登记，NPU↔解释器 LSB 级差异"
        "经 λ 链的收敛属遗留研究项（门禁数值未放宽）。",
        "v3 为 lock-step（无墙钟约束）；1 Hz 实时约束与真 HIL（经纬恒润平台）对接未验证，"
        "依赖 HIL 设备型号、I/O 周期与实时步长等外部资料。",
        "预测精度天花板 R²≈0.73：约 27% 的未来能量方差不可由历史 300 s 解释（浪、流随机分量），"
        "属纯历史外推的理论上限。",
        "FR-508 闭环注入的段内油耗为 mock 口径（stub advancer + pty mock 板），验证对象是回退语义而非保真油耗。",
        "TCN-f64 换型的 NPU 臂层② 未达标（board-NPU vs host-int8 exact 34.0%~47.7%，门 ≥99%）："
        "Neural-ART v1.1.1 对 64 通道 dilated 栈 int8 执行语义与解释器分歧（同输入差 88 LSB_TCN，三图形态均复现）；"
        "已按用户裁决切 4.0.1 全 CPU 编译形态绕开（NPU 形态保留为回退资产），板上验证与闭环 (a) 待执行（§5.7）。",
        "BSFC 低负荷曲线形状、最低稳定负荷等仍为假设参数（三档敏感性已量化）；不及预期的指标不在假设参数上硬调，"
        "列入优化 backlog 待真实参数到位后重启。",
    ):
        para(doc, "• " + s, indent=True)
    heading(doc, "7.2  展望（另立项）", 2)
    for s in (
        "TCN 全 CPU 形态板上验证：tcnq 槽位已切 4.0.1 全 CPU 编译形态（无 --st-neural-art，构建 + "
        "fw_gate 7/7 交付，NPU 形态留作回退资产），板上层② 重验与闭环 (a) 待执行（§5.7）。",
        "1 Hz 实时与真 HIL：lock-step → 实时化，经纬恒润 HIL 平台对接。",
        "层② 收敛：NPU↔解释器差异经 λ 链收敛至 ≥99%（研究项）。",
        "模型热切换 v2：NOR 双 mpool 布局已留位，运行期换模型机制。",
        "生成器 v3 与 trajectory-aware EMS：从生成器层面根治工况可辨识性，从单点预测演进到轨迹级优化。",
        "Phase 4（Model E）：面向 HIL 的模型重构与实时化（参数升级 + sample time 决策）。",
    ):
        para(doc, "• " + s, indent=True)

    # ---------- 附录 A ----------
    doc.add_page_break()
    heading(doc, "附录 A  复现命令与指纹", 1)
    para(doc, "A.1  闭环复现命令序列（依赖序 20→24→23→build→22→G-P2→21→closedloop→fault_inject→metrics）", size=10.5, bold=True)
    for cmd in (
        "# A. ModelD 仓库（MATLAB R2026a）",
        "eval(fileread('scripts/20_build_plantstep_model.m'))    # 被控对象子集模型 + 零改动断言",
        "eval(fileread('scripts/24_export_gp2_simref.m'))        # G-P2 sim 参考导出",
        "eval(fileread('scripts/23_codegen_plantstep.m'))        # codegen C（grt.tlc，ode3 @0.01s）",
        "# B. Host 编译",
        "./Host/tools/build_plantstep.sh S20 && ./Host/tools/build_plantstep.sh voyage002",
        "# C. 无板门禁（全绿才允许板会话）",
        "eval(fileread('scripts/22_run_gp1_gate.m'))             # G-P1 位级",
        "python3 Host/tools/run_gp2_gate.py --scenario S20       # G-P2 位级（voyage002 同）",
        "# D. 原生臂（(b′)/(c) × 两场景）",
        "eval(fileread('scripts/21_run_v3_native_arms.m'))",
        "# E. 板会话（用户授权 + SRAM 令牌；先 REF 后 NPU）",
        "python3 Host/ems_closedloop.py --scenario S20 --port <DEV> --image ref",
        "python3 Host/ems_closedloop.py --scenario S20 --port <DEV> --image npu   # voyage002 同",
        "# F. 门禁 + 统一指标（仓库根）",
        "python3 firmware/gate/fw_gate.py firmware/stm32n6/04_SourceCode/SoftwarePackage/EMS_Projects/EMS01_CHIL_ECMS",
        "python3 EMS01_CHIL_ECMS/Host/tools/closedloop_metrics.py",
        "# G. 本报告产物",
        "model/simulink/.venv_tf/bin/python docs/reports/generate_figures.py            # 实验数据图",
        "model/simulink/.venv_tf/bin/python docs/reports/generate_model_schematic.py    # 架构示意图",
        "matlab -batch \"run('docs/reports/export_model_figures.m')\"   # Simulink 模块截图（模型只读零改动）",
        "model/simulink/.venv_tf/bin/python docs/reports/build_docx.py",
    ):
        para(doc, cmd, size=9, east="Menlo", space_after=1)
    para(doc, "")
    para(doc, "A.2  关键指纹（复核锚点，完整值见 v3_closedloop_report.md §7.2 与各 build_manifest.json）", size=10.5, bold=True)
    add_table(
        doc,
        ["资产", "sha256（前 16 位）", "说明"],
        [
            ["NPU 固件镜像（build/Debug ELF）", "e7eb99c69c745dd3", "fw_gate PASS 7/7（t612_image_fingerprints.json）"],
            ["REF 固件镜像（build-ref2 ELF）", "f31714eac47529f7", "同上"],
            ["libplantstep_S20.dylib", "9476428b840b3534", "各 closedloop meta"],
            ["libplantstep_voyage002.dylib", "7b154ebb1fd4151f", "同上"],
            ["HybridShip_ModelD_PlantStep.slx", "f438ff9637e01302", "build_manifest.json（两场景同值）"],
            ["主模型（零改动断言 before=after）", "50359e65d584fd6c", "plantstep_build_audit.json"],
        ])

    # ---------- 附录 B ----------
    heading(doc, "附录 B  主要数据文件与口径", 1)
    add_table(
        doc,
        ["文件", "内容", "口径要点"],
        [
            ["EMS01_.../Host/results/closedloop/closedloop_metrics.json", "四臂×两场景统一指标（T-615，含 TCN 换型臂增补）",
             "fuel_corr = fuel_raw + (0.60−SOC_end)×1071.36×0.4774；SOC 公平带 ±0.02"],
            ["EMS01_CHIL_ECMS_TCN64/Host/results/closedloop/closedloop_metrics.json", "TCN 换型臂统一指标（T-814）",
             "与 P1 metrics 同构（四臂 + b_tcn_ref_* 换型臂）；换型证据链见同工程 Host/results/tcn_swap/"],
            ["Host/results/closedloop/{S20,voyage002}/closedloop_*_{ref,npu}.csv", "板载闭环轨迹（19 列）",
             "P_gen 列名 P_gen_kW，t 列名 t_s；flags 位镜像 ems_proto.h"],
            ["model/.../results/ecms_phase25/closedloop/native_arms/V3_{B,C}_*.csv", "原生臂轨迹（22 列）",
             "P_gen 列名 P_gen_ref_kW，t 列名 t；无 flags 列"],
            ["Host/reference/v3_gp1_{S20,voyage002}_replay.csv", "开环解释执行参考（13 列）",
             "仪器级①参考；G-P1 T-602 gate fuel_rel=0"],
            ["Host/fault_out/closedloop_npu/closedloop/CL_*.json + *_closedloop_S20_npu.csv", "FR-508 闭环注入",
             "mock 口径（stub advancer + pty+_MockFW），验证回退语义"],
            ["model/.../results/ecms_phase25/gating/gate_result_v2.json（+ *_v1_frozen.json）", "G0~G3 消融门禁",
             "2026-09-02 重跑口径（决策基线 2026-08-30 v1_frozen）"],
            ["model/.../results/ecms_phase25/deploy/m1_deploy_report.md、m2_port_report.md", "v1 部署与移植报告",
             "T-311 八项验收 / 层①②③ / v1.1 口径修订"],
        ])

    doc.save(OUT_DOCX)
    print("written:", os.path.relpath(OUT_DOCX, ROOT))
    return OUT_DOCX


if __name__ == "__main__":
    build()