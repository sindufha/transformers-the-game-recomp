"""Project-owned x87 environment extension; never patches the pinned checkout."""
from __future__ import annotations

import re
from capstone.x86_const import (X86_OP_MEM, X86_OP_REG, X86_REG_ST0, X86_REG_BP,
    X86_REG_EBP, X86_REG_ESP, X86_REG_CS, X86_REG_DS, X86_REG_SS, X86_REG_ES,
    X86_REG_FS, X86_REG_GS)

NO_WAIT = {"fnstenv", "fnstcw", "fnstsw", "fnclex", "fninit"}


def make_x87_lifter(base_class):
    """Return a mixin-compatible subclass of pinned tools.lift.lift32.Lifter.

    Callers include runtime/transformers_x87.h after recomp_types.h. Override
    _lift_instruction, not lift_instruction, so upstream flag capture survives.
    """
    class X87Lifter(base_class):
        def _lift_instruction(self, insn):
            m, ops = insn.mnemonic, insn.operands
            if not m.startswith("f") and m not in {"wait", "emms"}:
                return super()._lift_instruction(insn)
            va = f"0x{insn.address:08X}u"
            wait = [] if m in NO_WAIT else [f"tf_x87_wait({va});"]
            mem = next((op for op in ops if op.type == X86_OP_MEM), None)
            addr = self._fmt_mem_addr(mem.mem) if mem is not None else "0"
            if m in {"fnstenv", "fstenv", "fldenv"}:
                # Capstone 5 reports operand-size-override fldenv as size 28
                # too: the actual 66 prefix, not op.size, selects 14 bytes.
                raw = bytes(insn.bytes)
                prefix_end = next((i for i, byte in enumerate(raw)
                                   if byte not in {0xf0, 0xf2, 0xf3, 0x2e, 0x36, 0x3e, 0x26, 0x64, 0x65, 0x66, 0x67}), len(raw))
                if mem is None or 0x66 in raw[:prefix_end] or mem.size != 28:
                    return [f'RECOMP_TODO({va}, "unsupported x87 environment format: {m}");']
                op = "load" if m == "fldenv" else "store"
                const = "const " if op == "load" else ""
                lines = wait + [f"tf_x87_{op}_env(({const}unsigned char *)ADDR({addr}));"]
                if op == "load": lines.append("_fpu_cmp = (int)(0x10000u | (tf_x87_status() & 0x4700u));")
                return lines
            if m in {"fninit", "finit"}:
                return wait + ["tf_x87_init(); _fpu_cmp = 0x10000;"]
            if m in {"fnclex", "fclex"}:
                return wait + ["g_tf_x87.sw &= (uint16_t)~0x80ffu;"]
            if m in {"wait", "fwait"}:
                return wait or [";"]
            if m == "fldcw":
                return wait + [f"_fpu_cw = tf_x87_cw(MEM16({addr})); tf_x87_summary();"]
            if m in {"fnstcw", "fstcw"}:
                return wait + [f"MEM16({addr}) = _fpu_cw;"]
            if m in {"fnstsw", "fstsw"}:
                dest = "SET_LO16(eax, tf_x87_status());" if ops and ops[0].type == X86_OP_REG else f"MEM16({addr}) = tf_x87_status();"
                return wait + [dest]
            if m == "emms":
                return ["g_tf_x87.tags = 0xffffu; tf_x87_depth();"]
            raw = bytes(insn.bytes)
            position = next((i for i, byte in enumerate(raw) if 0xd8 <= byte <= 0xdf), None)
            if position is None or position + 1 >= len(raw):
                return super()._lift_instruction(insn)
            opcode = ((raw[position] & 7) << 8) | raw[position + 1]
            selector = "g_seg_ds"
            pointer = addr
            if mem is not None:
                segments = {X86_REG_CS: "cs", X86_REG_DS: "ds", X86_REG_SS: "ss",
                            X86_REG_ES: "es", X86_REG_FS: "fs", X86_REG_GS: "gs"}
                segment = getattr(mem.mem, "segment", 0)
                default = "ss" if mem.mem.base in {X86_REG_BP, X86_REG_EBP, X86_REG_ESP} else "ds"
                selector = "g_seg_" + segments.get(segment, default)
                # The saved FDP is selector-relative, whereas normal loads use
                # the runtime's linear TIB/TEB base for FS/GS.
                for prefix in ("FS_BASE + (", "GS_BASE + (", "ES_BASE + ("):
                    if pointer.startswith(prefix) and pointer.endswith(")"):
                        pointer = pointer[len(prefix):-1]
                        break
            note = [f"tf_x87_note({va}, 0x{opcode:03X}u, (uint32_t)({pointer}), {int(mem is not None)}, {selector});"]
            if m == "fnop":
                return wait + note
            if m in {"fincstp", "fdecstp"}:
                return wait + note + [f"tf_x87_rotate({int(m == 'fincstp')}); g_tf_x87.sw &= (uint16_t)~0x0200u;"]
            if m in {"ffree", "ffreep"}:
                index = ops[-1].reg - X86_REG_ST0 if ops else 0
                suffix = " tf_x87_rotate(1);" if m == "ffreep" else ""
                return wait + note + [f"tf_x87_free({index});{suffix}"]
            if m == "fxam":
                return wait + note + ["tf_x87_examine(); _fpu_cmp = (int)(0x10000u | (tf_x87_status() & 0x4700u));"]
            lines = super()._lift_instruction(insn)
            # Fail explicitly on empty operands. Upstream has no masked stack
            # fault model; accepting stale empty-register values is unsafe.
            required = set()
            if m not in {"fld", "fild", "fldz", "fld1", "fldpi", "fldl2e", "fldl2t", "fldlg2", "fldln2"}:
                required.add(0)
            for op in ops:
                if op.type == X86_OP_REG and X86_REG_ST0 <= op.reg <= X86_REG_ST0 + 7:
                    required.add(op.reg - X86_REG_ST0)
            # fst st(i) writes its destination, which may legitimately be empty.
            if m in {"fst", "fstp"} and mem is None:
                required = {0}
                index = ops[-1].reg - X86_REG_ST0 if ops else 0
                lines = [line.replace("fp_pop()", f"tf_x87_mark({index}); fp_pop()") for line in lines]
                if m == "fst": lines.append(f"tf_x87_mark({index});")
            if m in {"fcompp", "fucompp", "fpatan", "fyl2x", "fyl2xp1", "fprem", "fprem1", "fscale"}:
                required.add(1)
            checks = [f"tf_x87_require({i}, {va});" for i in sorted(required)]
            if any(re.search(r"\b_fpu_cmp\s*=", line) for line in lines):
                lines.append("tf_x87_condition(_fpu_cmp);")
            return wait + note + checks + lines

    return X87Lifter
