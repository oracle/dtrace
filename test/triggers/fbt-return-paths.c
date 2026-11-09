/*
 * Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
#include "libdtrace/dt_btf.c"
#include "libdtrace/dt_prov_fbt.c"

#define CHECK(x) do { if (!(x)) { \
	fprintf(stderr, "line %d: %s\n", __LINE__, #x); exit(1); \
} } while (0)

/* Isolate argument handling from the common trampoline and clause call. */
void __wrap_dt_cg_tramp_prologue(dt_pcb_t *pcb) { }
void __wrap_dt_cg_tramp_epilogue(dt_pcb_t *pcb) { }

/* Execute the generated argument-copy instructions, with checked memory
 * access and helper semantics.  Unexpected opcodes fail the test. */
static void
execute(dt_irlist_t *ir, int argc, uint64_t value, int helper, int is_void)
{
	uint64_t regs[11] = { 0 }, stack[256] = { 0 }, context[10];
	dt_mstate_t state;
	dt_irnode_t *node;
	int calls = 0, i;

	memset(&state, 0xa5, sizeof(state));
	for (i = 0; i < 10; i++)
		context[i] = 0x12340000 + i;
	context[argc] = value;
	regs[BPF_REG_FP] = (uintptr_t)(stack + 256);
	regs[BPF_REG_7] = (uintptr_t)&state;
	regs[BPF_REG_8] = (uintptr_t)context;
	for (node = ir->dl_list; node; node = node->di_next) {
		struct bpf_insn insn = node->di_instr;
		uint64_t src = regs[insn.src_reg];
		uintptr_t addr;

		switch (insn.code) {
		case BPF_LD | BPF_DW | BPF_IMM:
			CHECK(node->di_next != NULL);
			regs[insn.dst_reg] = (uint32_t)insn.imm |
			    (uint64_t)(uint32_t)node->di_next->di_instr.imm << 32;
			node = node->di_next;
			break;
		case BPF_ALU64 | BPF_MOV | BPF_X: regs[insn.dst_reg] = src; break;
		case BPF_ALU64 | BPF_MOV | BPF_K: regs[insn.dst_reg] = (int64_t)insn.imm; break;
		case BPF_ALU64 | BPF_ADD | BPF_K: regs[insn.dst_reg] += (int64_t)insn.imm; break;
		case BPF_JMP | BPF_CALL:
			calls++;
			CHECK(insn.imm == (helper ? BPF_FUNC_get_func_ret : BPF_FUNC_probe_read_kernel));
			addr = helper ? regs[BPF_REG_2] : regs[BPF_REG_1];
			CHECK(addr >= (uintptr_t)stack && addr + 8 <= (uintptr_t)(stack + 256));
			if (helper) {
				CHECK(regs[BPF_REG_1] == (uintptr_t)context);
				memcpy((void *)addr, &context[argc], 8);
			} else {
				CHECK(regs[BPF_REG_2] == 8);
				CHECK(regs[BPF_REG_3] == (uintptr_t)&context[argc]);
				memcpy((void *)addr, (void *)(uintptr_t)regs[BPF_REG_3], 8);
			}
			regs[BPF_REG_0] = 0;
			break;
		case BPF_LDX | BPF_DW | BPF_MEM:
			addr = src + insn.off;
			CHECK(addr >= (uintptr_t)stack && addr + 8 <= (uintptr_t)(stack + 256));
			memcpy(&regs[insn.dst_reg], (void *)addr, 8);
			break;
		case BPF_STX | BPF_DW | BPF_MEM:
			addr = regs[insn.dst_reg] + insn.off;
			CHECK(addr == (uintptr_t)&state.argv[0] || addr == (uintptr_t)&state.argv[1]);
			memcpy((void *)addr, &src, 8);
			break;
		default: CHECK(0);
		}
	}
	CHECK(state.argv[0] == UINT64_MAX);
	CHECK(calls == !is_void);
	CHECK(state.argv[1] == (is_void ? UINT64_C(0xa5a5a5a5a5a5a5a5) : value));
}

int
main(void)
{
	dtrace_hdl_t dt = { 0 };
	dt_module_t *module = dt_module_create(&dt, "fixture");
	btf_type_t func = { .info = BTF_KIND_FUNC << 24, .type = 2 };
	btf_type_t proto = { .info = BTF_KIND_FUNC_PROTO << 24 };
	btf_type_t *types[] = { NULL, &func, &proto };
	dt_btf_t btf = { .type_cnt = 3, .types = types };
	dtrace_probedesc_t desc = { .mod = "fixture", .prb = "return" };
	dt_probe_t probe = { .desc = &desc, .argc = 2 };
	dt_pcb_t pcb = { .pcb_hdl = &dt, .pcb_probe = &probe };
	uint64_t values[] = { 0, 42, (uint64_t)-37, UINT64_MAX, UINT64_C(0x123456789abcdef0) };
	int counts[] = { 0, 1, 5, 8 }, helper, is_void;
	size_t a, v;

	CHECK(module != NULL);
	module->dm_btf = &btf;
	CHECK((probe.prv_data = dt_tp_alloc(&dt)) != NULL);
	dt_tp_probe_set_id(&probe, 1);
	dt.dt_bpfhelper[BPF_FUNC_probe_read_kernel] = BPF_FUNC_probe_read_kernel;
	for (helper = 0; helper < 2; helper++) {
		dt.dt_bpfhelper[BPF_FUNC_get_func_ret] = helper ? BPF_FUNC_get_func_ret : BPF_FUNC_unspec;
		for (is_void = 0; is_void < 2; is_void++) {
			probe.argc = is_void ? 1 : 2;
			for (a = 0; a < ARRAY_SIZE(counts); a++) {
				proto.info = BTF_KIND_FUNC_PROTO << 24 | counts[a];
				for (v = 0; v < ARRAY_SIZE(values); v++) {
					dt_irlist_create(&pcb.pcb_ir);
					CHECK(fprobe_trampoline(&pcb, 0) == 0);
					execute(&pcb.pcb_ir, counts[a], values[v], helper, is_void);
					dt_irlist_destroy(&pcb.pcb_ir);
				}
			}
		}
	}
	/* A cyclic BTF prototype cannot supply a fallback slot index. */
	func.type = 1;
	probe.argc = 2;
	dt.dt_bpfhelper[BPF_FUNC_get_func_ret] = BPF_FUNC_unspec;
	dt_irlist_create(&pcb.pcb_ir);
	CHECK(fprobe_trampoline(&pcb, 0) == -1);
	dt_irlist_destroy(&pcb.pcb_ir);
	dt_tp_destroy(&dt, probe.prv_data);
	module->dm_btf = NULL;
	dt_htab_destroy(dt.dt_mods);
	return 0;
}
