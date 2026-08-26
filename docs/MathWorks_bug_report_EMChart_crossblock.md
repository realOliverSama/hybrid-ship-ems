# MathWorks Bug Report — Ready-to-Submit Package

> **提交方式**：登录你的 MathWorks 账户 → Support → Contact Support / Create a Service Request
> （或 https://www.mathworks.com/support.html → "Contact Support"），选择 Technical Support，
> 类别 Simulink / Stateflow，将下文 §A–§C 全文粘贴。附件建议附带本文件与
> `results/ecms_phase2/debug_law7/exp_matrix4_results.json`。

---

## §A Title

**Stateflow EMChart (MATLAB Function block) fails update compilation with
`Stateflow:cdr:ErrorsParsingEmlFcn` when a variable is assembled inside an if-block
and consumed in a subsequent if-block (R2026a)**

## §B Description

### Product & Version

- MATLAB R2026a Update 3 (26.1.0.3276743)
- Simulink, Stateflow
- OS: Windows 11 (x64)

### Summary

A MATLAB Function block (Stateflow EMChart) that assembles a vector **inside a first
`if` block** (from a `persistent` ring buffer via a `for`-loop reorder plus scalar
concatenation) and then **uses that variable inside a subsequent `if` block** fails
diagram update (`set_param(model,'SimulationCommand','update')`) with:

```
Stateflow:cdr:ErrorsParsingEmlFcn
"解析 ... 时出错。"   ("Error while parsing <chart path>")
```

The error has **no line number and no cause chain**, which makes diagnosis impossible.
The script-set stage (`chart.Script = code`) always succeeds — only the update/compile
stage fails.

### Minimal reproduction

Create any MATLAB Function block with the following content (no chart parameters,
no external data needed) and run diagram update:

```matlab
function P = ecms_nn_min(t, P_load, v_ship)
persistent lastT buf k vprev
if isempty(lastT)
    lastT = -1e9; k = 1;
    buf = zeros(300, 1); buf(:) = P_load;
    vprev = v_ship;
end
P = 0;
if t - lastT < 1.0 - 1e-9, return; end
lastT = t;
if isfinite(P_load), buf(k) = P_load; end
k = mod(k, 300) + 1;
accel = v_ship - vprev; vprev = v_ship;
ok = isfinite(P_load) && isfinite(v_ship) && isfinite(accel);
if ok
    xP = zeros(300, 1);
    for j = 1:300
        i = k - j;
        if i < 1, i = i + 300; end
        xP(j) = buf(i);
    end
    x = [xP; v_ship; accel];      % (A) assembled INSIDE the if-block
    ok = all(isfinite(x));
end
if ok
    P = sum(x);                    % (B) used in a SUBSEQUENT if-block — triggers failure
else
    P = 0;
end
end
```

Observed: `Stateflow:cdr:ErrorsParsingEmlFcn` at update stage.
Expected: successful compilation (this is plain, codegen-compatible MATLAB).

### Isolation evidence (22 single-factor experiments on a real model)

| Manipulation | Result |
|---|---|
| Replace usage with constant `y = 0;` | compiles OK |
| Keep usage but remove in-block reassignment of the condition variable | still FAILS |
| Move assembly outside the conditionals (unconditional), keep everything else | compiles OK |
| Explicit preallocation `x = zeros(302,1)` instead of concatenation | still FAILS |
| Scalar indexing only (`y = x(1);`) instead of vector math | still FAILS |
| Replace chart parameters by local constants | still FAILS |
| Zero-parameter minimal function above | still FAILS |

Conclusion: the trigger is structural — *first definition of a variable inside an
if-block whose value is consumed in a later if-block* — independent of operators,
sizes, parameter objects, or how the condition variable is updated.

### Workaround

Assemble the vector **unconditionally outside the conditional blocks** and merge the
condition afterwards:

```matlab
xP = zeros(300, 1);
for j = 1:300
    i = k - j; if i < 1, i = i + 300; end
    xP(j) = buf(i);
end
x = [xP; v_ship; accel];
ok = ok && all(isfinite(x));   % semantics preserved
if ok
    ...
end
```

This compiles and produces bitwise-identical simulation results to the intended
logic (verified on a closed-loop powertrain EMS model, S01 scenario, 92,785 samples).

### Impact

Silent-looking parse failure without diagnostics blocks legitimate modeling patterns
(buffered feature extraction feeding a gated consumer). We had to spend multiple
debugging sessions because the error message carries no line number or cause chain.

---

## §C Attachments checklist

- [x] Minimal reproduction source (§B)
- [x] Isolation matrix (§B table; machine-readable JSON available in repo)
- [x] Workaround source (§B)
- [ ] Model file (optional; minimal case above suffices)
