/**
 * hybrid-ship-safety — 项目安全强制层
 *
 * 与 AGENTS.md 配套的技术强制（行为约定的安全网）：
 *   🔴 hard-block  无任何例外，直接拦截（对应 AGENTS.md §2 红区）
 *   🟡 confirm     需用户现场确认；非交互模式（print/RPC 无 UI）默认拦截
 *
 * 覆盖面：
 *   - bash 工具：Git 红线、rm -rf、main 分支直接提交/推送、tag/merge/release
 *   - write/edit 工具：受治理文件（AGENTS.md、.gitignore、.pi/ 等，对应 AGENTS.md §2 黄区）
 *   - embedded-debugger MCP：外部 NOR Flash 擦写（对应 AGENTS.md §7 固件红线）
 *
 * 已知限制：
 *   - bash 匹配前会剥离引号内容以避免 commit message 误伤，
 *     因此不能防范刻意用引号拼接绕过（那是 AGENTS.md 行为层的职责）
 *   - bash 命令内间接写受保护路径（如 sed -i .gitignore）不在检测范围
 */

import type { ExtensionAPI } from "@earendil-works/pi-coding-agent";
import { appendFileSync, mkdirSync } from "node:fs";
import { join } from "node:path";

/** 审计日志：记录每次拦截/放行，供任务汇报的 "Destructive operations" 行取证 */
function audit(ctx: { cwd: string }, kind: "BLOCK" | "CONFIRM_ALLOW" | "CONFIRM_DENY", detail: string): void {
	try {
		const dir = join(ctx.cwd, ".pi");
		mkdirSync(dir, { recursive: true });
		appendFileSync(
			join(dir, "safety-audit.log"),
			`${new Date().toISOString()} ${kind} ${detail.replace(/\s+/g, " ").slice(0, 300)}\n`,
		);
	} catch {
		// 审计失败不影响拦截主流程
	}
}

// ---------------------------------------------------------------------------
// 规则定义
// ---------------------------------------------------------------------------

interface Rule {
	pattern: RegExp;
	reason: string;
}

/** 🔴 硬拦截：任何情况下都不允许执行 */
const HARD_BLOCK_RULES: Rule[] = [
	{ pattern: /\bgit\s+push\b[^|;&]*(--force\b|--force-with-lease\b|\s-f\s|\s-f$)/, reason: "禁止 force push（AGENTS.md 🔴）" },
	{ pattern: /\bgit\s+reset\s+--hard\b/, reason: "禁止 git reset --hard；已 push 的用 revert，未 push 的需用户批准（AGENTS.md 🔴）" },
	{ pattern: /\bgit\s+clean\s+-\w*f/, reason: "禁止 git clean -f*（AGENTS.md 🔴）" },
	{ pattern: /\bgit\s+checkout\s+(\.|--\s+\.)/, reason: "禁止 git checkout .（AGENTS.md 🔴）" },
	{ pattern: /\bgit\s+restore\s+\./, reason: "禁止 git restore .（AGENTS.md 🔴）" },
	{ pattern: /\bgit\s+add\b[^|;&]*(\s|^)(\.{1,2}\/?|-[a-zA-Z]*A[a-zA-Z]*|--all)(\s|$)/, reason: "禁止 git add . / -A / --all 等宽泛暂存；必须逐个显式 add（AGENTS.md 🔴）" },
	{ pattern: /\bgit\s+filter-repo\b/, reason: "禁止 git filter-repo 改写历史（AGENTS.md 🔴）" },
	{ pattern: /\bgit\s+commit\b[^|;&]*--no-verify\b/, reason: "禁止 git commit --no-verify（AGENTS.md 🔴）" },
	{ pattern: /\brm\s+-\w*[rf]\w*\b[^|;&]*(\s|^)(\.{1,2}|\*|~\/?|\/)($|\s)/, reason: "禁止对根目录/家目录/通配符/当前目录执行 rm -rf" },
	{ pattern: /\bsudo\s+rm\b/, reason: "禁止 sudo rm" },
	{ pattern: /\bmkfs\b|\bdd\s+.*of=\/dev\//, reason: "禁止磁盘级破坏命令" },
];

/** 🟡 确认门：匹配后需用户现场确认 */
const CONFIRM_RULES: Rule[] = [
	{ pattern: /\brm\s+-\w*r/, reason: "rm -r 递归删除需要确认（AGENTS.md：删除应使用精确路径）" },
	{ pattern: /\bgit\s+reset\b/, reason: "git reset 仅限「未 push + disposable 分支 + 用户批准」（AGENTS.md 🟡）" },
	{ pattern: /\bgit\s+tag\s+(-d\b|-a\b|\w)/, reason: "tag 仅用于里程碑，需用户批准（AGENTS.md 🟡）" },
	{ pattern: /\bgit\s+push\b[^|;&]*(--delete\b|\s:)/, reason: "删除远程分支/引用需确认" },
	{ pattern: /\bgh\s+pr\s+merge\b/, reason: "合并 PR 到 main 是用户保留权利（AGENTS.md 🟡）" },
	{ pattern: /\bgh\s+release\s+(create|delete)\b/, reason: "GitHub Release 需用户批准（AGENTS.md 🟡）" },
];

/** 🟡 需要确认的分支上下文规则（结合当前 git 分支判断） */
const MAIN_BRANCH = "main";

/** 🟡 write/edit 受治理路径（修改需确认；匹配相对/绝对路径） */
const GOVERNED_PATHS: Array<{ pattern: RegExp; label: string }> = [
	{ pattern: /(^|\/)AGENTS\.md$/, label: "AGENTS.md（治理文件）" },
	{ pattern: /(^|\/)\.gitignore$/, label: ".gitignore" },
	{ pattern: /(^|\/)\.gitattributes$/, label: ".gitattributes" },
	{ pattern: /(^|\/)\.env($|\.)/, label: ".env*（密钥文件）" },
	{ pattern: /(^|\/)\.pi\//, label: ".pi/（Agent 配置与治理扩展）" },
	{ pattern: /(^|\/)\.github\//, label: ".github/（CI 配置）" },
];

// ---------------------------------------------------------------------------
// 工具函数
// ---------------------------------------------------------------------------

/** 剥离单/双引号内容，避免 commit message 等文本触发误伤 */
function stripQuotes(command: string): string {
	return command.replace(/"[^"]*"|'[^']*'/g, '""');
}

async function confirmOrBlock(
	ctx: { hasUI: boolean; ui: { select: (t: string, c: string[]) => Promise<string | undefined>; notify: (m: string, l: "warning" | "info" | "error") => void } },
	title: string,
	detail: string,
): Promise<{ block: true; reason: string } | undefined> {
	if (!ctx.hasUI) {
		audit(ctx, "BLOCK", `${title}（非交互模式默认拦截）:: ${detail}`);
		return { block: true, reason: `${title}（非交互模式默认拦截）：${detail}` };
	}
	const choice = await ctx.ui.select(`⚠️ ${title}\n\n  ${detail}\n\n是否允许？`, ["允许一次", "拒绝"]);
	if (choice !== "允许一次") {
		audit(ctx, "CONFIRM_DENY", `${title} :: ${detail}`);
		return { block: true, reason: `用户拒绝：${title}` };
	}
	ctx.ui.notify(`已允许：${title}`, "warning");
	audit(ctx, "CONFIRM_ALLOW", `${title} :: ${detail}`);
	return undefined;
}

// ---------------------------------------------------------------------------
// 扩展主体
// ---------------------------------------------------------------------------

export default function (pi: ExtensionAPI) {
	pi.on("session_start", async (_event, ctx) => {
		ctx.ui.setStatus("safety", "🛡 safety");
	});

	pi.on("tool_call", async (event, ctx) => {
		// ------------------------------------------------------------------
		// bash：Git / 文件系统红线
		// ------------------------------------------------------------------
		if (event.toolName === "bash") {
			const raw = String(event.input.command ?? "");
			const cmd = stripQuotes(raw);

			for (const rule of HARD_BLOCK_RULES) {
				if (rule.pattern.test(cmd)) {
					ctx.ui.notify(`🚫 已拦截：${rule.reason}`, "error");
					audit(ctx, "BLOCK", `${rule.reason} :: ${raw}`);
					return { block: true, reason: `[hybrid-ship-safety] ${rule.reason}` };
				}
			}

			for (const rule of CONFIRM_RULES) {
				if (rule.pattern.test(cmd)) {
					const r = await confirmOrBlock(ctx, rule.reason, raw.slice(0, 300));
					if (r) return r;
				}
			}

			// 分支上下文规则：main 上的直接 commit / push / merge
			if (/\bgit\s+(commit|push|merge|rebase)\b/.test(cmd)) {
				const { stdout, code } = await pi.exec("git", ["branch", "--show-current"], { cwd: ctx.cwd });
				const branch = code === 0 ? stdout.trim() : "";
				if (branch === MAIN_BRANCH) {
					if (/\bgit\s+(commit|merge|rebase)\b/.test(cmd)) {
						const r = await confirmOrBlock(ctx, `当前在 ${MAIN_BRANCH} 分支上`, `直接在 main 上提交/合并需要确认：${raw.slice(0, 200)}`);
						if (r) return r;
					}
				}
				if (/\bgit\s+push\b/.test(cmd) && (branch === MAIN_BRANCH || new RegExp(`\\b${MAIN_BRANCH}\\b`).test(cmd))) {
					const r = await confirmOrBlock(ctx, `推送到 ${MAIN_BRANCH}`, `push main 需要确认：${raw.slice(0, 200)}`);
					if (r) return r;
				}
			}

			return undefined;
		}

		// ------------------------------------------------------------------
		// write / edit：受治理文件
		// ------------------------------------------------------------------
		if (event.toolName === "write" || event.toolName === "edit") {
			const path = String(event.input.path ?? "");
			for (const g of GOVERNED_PATHS) {
				if (g.pattern.test(path)) {
					const r = await confirmOrBlock(ctx, `修改受治理文件：${g.label}`, path);
					if (r) return r;
				}
			}
			return undefined;
		}

		// ------------------------------------------------------------------
		// embedded-debugger MCP：外部 NOR Flash 红线（AGENTS.md §7）
		// ------------------------------------------------------------------
		if (event.toolName === "embedded-debugger_flash_erase") {
			const eraseType = String(event.input.erase_type ?? "all");
			if (eraseType === "all") {
				ctx.ui.notify("🚫 已拦截：禁止 mass erase 外部 NOR（release 工作流红线）", "error");
				audit(ctx, "BLOCK", `flash mass erase :: ${JSON.stringify(event.input)}`);
				return { block: true, reason: "[hybrid-ship-safety] 禁止 flash mass erase；请走 release-stm32n647-appli skill" };
			}
			const r = await confirmOrBlock(ctx, "Flash sector erase", JSON.stringify(event.input).slice(0, 200));
			if (r) return r;
			return undefined;
		}

		if (event.toolName === "embedded-debugger_flash_program" || event.toolName === "embedded-debugger_run_firmware") {
			const r = await confirmOrBlock(
				ctx,
				`MCP 烧录操作：${event.toolName}`,
				"AGENTS.md §7：烧录 Release 应走 release-stm32n647-appli skill（STM32CubeProgrammer + 外部 loader）",
			);
			if (r) return r;
			return undefined;
		}

		return undefined;
	});
}
