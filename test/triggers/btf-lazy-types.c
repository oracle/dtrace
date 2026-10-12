/*
 * Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include "libdtrace/dt_btf.c"

#ifdef HAVE_LIBCTF
#define CHECK(expr) do { \
	if (!(expr)) { \
		fprintf(stderr, "line %d: %s (CTF: %s)\n", __LINE__, #expr, \
			ctf_errmsg(dtp.dt_ctferr)); \
		return 1; \
	} \
} while (0)

/* Build raw BTF, so the test also exercises decoding and the name index. */
static dt_btf_t *
fixture(dtrace_hdl_t *dtp, const uint32_t *types, size_t type_len,
	const char *strings, size_t str_len, const dt_btf_t *parent)
{
	dt_btf_t *btf = dt_zalloc(dtp, sizeof(*btf));
	btf_header_t hdr = {
		.magic = BTF_MAGIC, .version = BTF_VERSION,
		.hdr_len = sizeof(hdr), .type_len = type_len,
		.str_off = type_len, .str_len = str_len
	};

	if (btf == NULL)
		return NULL;
	btf->size = sizeof(hdr) + type_len + str_len;
	btf->data = dt_alloc(dtp, btf->size);
	if (btf->data == NULL)
		goto fail;
	memcpy(btf->data, &hdr, sizeof(hdr));
	memcpy((char *)btf->data + sizeof(hdr), types, type_len);
	memcpy((char *)btf->data + sizeof(hdr) + type_len, strings, str_len);
	btf->parent = parent;
	if (dt_btf_decode(dtp, btf) != 0)
		goto fail;
	return btf;

fail:
	dt_btf_destroy(dtp, btf);
	return NULL;
}

/* String offsets are computed, rather than coupled to literal lengths. */
static const struct {
	char empty[1], integer[4], leaf[5], value[6], node[5], next[5];
	char node_t[7], unused[7], bad[4], cycle[6], callback_t[11], cb[3];
	char bits[5], local[6], parent_leaf[12], parent_only[12], bad_slice[10];
	char target[7], target_ptr[11], local_bad[10];
} names = {
	"", "int", "leaf", "value", "node", "next", "node_t", "unused",
	"bad", "cycle", "callback_t", "cb", "bits", "local", "parent_leaf",
	"parent_only", "bad_slice", "target", "target_ptr", "local_bad"
};
#define NAME(n) offsetof(__typeof__(names), n)
#define TYPE(n, k, v, t) NAME(n), ((k) << 24) | (v), (t)

int
main(int argc, char **argv)
{
	dtrace_hdl_t dtp = { 0 };
	dt_module_t kernel = { .dm_name = "vmlinux" };
	dt_module_t *shared;
	dt_module_t module = { .dm_name = "module" };
	dt_btf_t *btf, *child;
	ctf_file_t *ctf, *mctf, *ordinary;
	ctf_id_t id, leaf, cb;
	ctf_membinfo_t member;
	ctf_funcinfo_t func;
	ctf_id_t args[2];
	uint32_t i;
	const uint32_t types[] = {
		/* 1: int; 2: leaf with one int member. */
		TYPE(integer, BTF_KIND_INT, 0, 4), BTF_INT_SIGNED << 24 | 32,
		TYPE(leaf, BTF_KIND_STRUCT, 1, 4), NAME(value), 1, 0,
		/* 3: node; 4: node *; 5: typedef node_t. */
		TYPE(node, BTF_KIND_STRUCT, 1, 8), NAME(next), 4, 0,
		TYPE(empty, BTF_KIND_PTR, 0, 3),
		TYPE(node_t, BTF_KIND_TYPEDEF, 0, 4),
		/* 6: unused; 7: bad with an invalid member type. */
		TYPE(unused, BTF_KIND_STRUCT, 0, 16),
		TYPE(bad, BTF_KIND_STRUCT, 1, 8), NAME(value), 9999, 0,
		/* 8: a cyclic typedef that should fail only when requested. */
		TYPE(cycle, BTF_KIND_TYPEDEF, 0, 8),
		/* 9: callback typedef; 10: pointer; 11: prototype. */
		TYPE(callback_t, BTF_KIND_TYPEDEF, 0, 10),
		TYPE(empty, BTF_KIND_PTR, 0, 11),
		TYPE(empty, BTF_KIND_FUNC_PROTO, 2, 1),
		NAME(empty), 12, NAME(empty), 1,
		/* 12: callback argument with a member of the same typedef. */
		TYPE(cb, BTF_KIND_STRUCT, 1, 8), NAME(value), 9, 0,
		/* 13: a bitfield whose packed BTF offset must be preserved. */
		TYPE(bits, BTF_KIND_STRUCT, 0x80000001U, 4),
		NAME(value), 1, 3U << 24,
		/* 14: a parent type first requested through split BTF. */
		TYPE(parent_only, BTF_KIND_STRUCT, 1, 4), NAME(value), 1, 0,
		/* 15: invalid bitfield base, detected while filling members. */
		TYPE(bad_slice, BTF_KIND_STRUCT, 0x80000001U, 8),
		NAME(value), 4, 3U << 24,
		/* 16-19: a forward reference with an available definition. */
		TYPE(target, BTF_KIND_FWD, 0, 0),
		TYPE(empty, BTF_KIND_PTR, 0, 16),
		TYPE(target_ptr, BTF_KIND_TYPEDEF, 0, 17),
		TYPE(target, BTF_KIND_STRUCT, 1, 4), NAME(value), 1, 0
	};
	const uint32_t split[] = {
		/* Parent string offsets precede split BTF string offsets. */
		TYPE(local, BTF_KIND_STRUCT, 1, 4), NAME(parent_leaf), 14, 0,
		/* Fail after creating a previously unconverted parent shell. */
		TYPE(local_bad, BTF_KIND_STRUCT, 2, 8),
		NAME(value), 19, 0, NAME(next), 9999, 32
	};

	dtp.dt_exec = &kernel;
	dtp.dt_conf.dtc_ctfmodel = CTF_MODEL_LP64;
	shared = dt_module_create(&dtp, "shared_ctf");
	CHECK(shared != NULL);
	btf = fixture(&dtp, types, sizeof(types), (const char *)&names,
		      sizeof(names), NULL);
	CHECK(btf != NULL);
	kernel.dm_btf = btf;

	/* Archive-backed shared CTF has no BTF-to-CTF parent id mapping. */
	ctf = shared->dm_ctfp = ctf_create(&dtp.dt_ctferr);
	CHECK(ctf != NULL);
	id = ctf_add_struct_sized(ctf, CTF_ADD_ROOT, "archive_only", 16);
	CHECK(id != CTF_ERR && ctf_update(ctf) == 0);
	ctf_setspecific(ctf, shared);
	kernel.dm_ctfp = dt_btf_module_ctf(&dtp, &kernel);
	CHECK(kernel.dm_ctfp != NULL && kernel.dm_ctfp != ctf);
	child = fixture(&dtp, split, sizeof(split), "", 1, btf);
	CHECK(child != NULL);
	module.dm_btf = child;
	CHECK(dt_btf_module_ctf(&dtp, &module) == NULL);
	CHECK(child->ctfids == NULL && btf->ctfids == NULL);
	CHECK(dt_ctf_lookup_by_name(&dtp, ctf, "struct archive_only") == id);
	dt_btf_destroy(&dtp, child);

	/* Missing module BTF still inherits its unconverted BTF parent. */
	child = dt_zalloc(&dtp, sizeof(*child));
	CHECK(child != NULL);
	child->parent = btf;
	module.dm_btf = child;
	CHECK(dt_btf_module_ctf(&dtp, &module) == NULL);
	CHECK(child->ctfids == NULL && btf->ctfids == NULL);
	CHECK(dt_ctf_lookup_by_name(&dtp, ctf, "struct archive_only") == id);
	dt_btf_destroy(&dtp, child);
	ctf_close(kernel.dm_ctfp);
	ctf_close(ctf);
	kernel.dm_ctfp = shared->dm_ctfp = NULL;

	ctf = kernel.dm_ctfp = dt_btf_module_ctf(&dtp, &kernel);
	CHECK(ctf != NULL && ctf != shared->dm_ctfp);
	CHECK(ctf_parent_dict(ctf) == shared->dm_ctfp);
	CHECK(dt_module_lookup_by_ctf(&dtp, shared->dm_ctfp) == shared);
	for (i = 1; i < btf->type_cnt; i++)
		CHECK(btf->ctfids[i] == CTF_ERR);

	leaf = dt_ctf_lookup_by_name(&dtp, ctf, "struct leaf");
	CHECK(leaf != CTF_ERR && ctf_type_size(ctf, leaf) == 4);
	CHECK(ctf_member_info(ctf, leaf, "value", &member) == 0);
	CHECK(member.ctm_type == btf->ctfids[1]);
	CHECK(btf->ctfids[2] == leaf && btf->ctfids[6] == CTF_ERR);
	CHECK(dt_ctf_lookup_by_name(&dtp, ctf, "struct leaf") == leaf);

	id = dt_ctf_lookup_by_name(&dtp, ctf, "node_t");
	CHECK(id != CTF_ERR);
	CHECK(ctf_type_kind(ctf, ctf_type_resolve(ctf, id)) == CTF_K_POINTER);
	CHECK(ctf_member_info(ctf, btf->ctfids[3], "next", &member) == 0);
	CHECK(member.ctm_type == btf->ctfids[4]);

	id = dt_ctf_lookup_by_name(&dtp, ctf, "callback_t");
	CHECK(id != CTF_ERR);
	cb = ctf_type_reference(ctf, ctf_type_resolve(ctf, id));
	CHECK(ctf_func_type_info(ctf, cb, &func) == 0 && func.ctc_argc == 2);
	CHECK(ctf_func_type_args(ctf, cb, 2, args) == 0);
	CHECK(args[0] == btf->ctfids[12] && args[1] == btf->ctfids[1]);

	id = dt_ctf_lookup_by_name(&dtp, ctf, "struct bits");
	CHECK(id != CTF_ERR);
	CHECK(ctf_member_info(ctf, id, "value", &member) == 0);
	CHECK(member.ctm_offset == 0);
	CHECK(((btf_member_t *)(btf->types[13] + 1))->offset ==
	      (3U << 24));

	CHECK(dt_ctf_lookup_by_name(&dtp, ctf, "struct bad") == CTF_ERR);
	CHECK(btf->ctfids[7] == CTF_ERR);
	CHECK(dt_ctf_lookup_by_name(&dtp, ctf, "cycle") == CTF_ERR);
	CHECK(btf->ctfids[8] == CTF_ERR);
	CHECK(dt_ctf_lookup_by_name(&dtp, ctf, "struct bad_slice") == CTF_ERR);
	CHECK(btf->ctfids[15] == CTF_ERR);
	CHECK(dt_ctf_lookup_by_name(&dtp, ctf, "struct unused") != CTF_ERR);
	CHECK(dt_ctf_lookup_by_name(&dtp, ctf, "struct leaf") == leaf);

	id = dt_ctf_lookup_by_name(&dtp, ctf, "const struct leaf **");
	CHECK(id != CTF_ERR);
	CHECK(ctf_type_reference(ctf, ctf_type_reference(ctf, id)) == leaf);
	CHECK(dt_ctf_lookup_by_name(&dtp, ctf, "const struct leaf **") == id);

	child = fixture(&dtp, split, sizeof(split), "", 1, btf);
	CHECK(child != NULL);
	module.dm_btf = child;
	mctf = module.dm_ctfp = dt_btf_module_ctf(&dtp, &module);
	CHECK(mctf != NULL && child->ctfids[1] == CTF_ERR);
	CHECK(dt_ctf_lookup_by_name(&dtp, mctf, "struct local_bad") == CTF_ERR);
	CHECK(child->ctfids[2] == CTF_ERR && btf->ctfids[19] == CTF_ERR);
	CHECK(btf->ctfids[14] == CTF_ERR);
	id = dt_ctf_lookup_by_name(&dtp, mctf, "struct local");
	CHECK(id != CTF_ERR && ctf_type_size(mctf, id) == 4);
	CHECK(ctf_member_info(mctf, id, "parent_leaf", &member) == 0);
	CHECK(member.ctm_type == btf->ctfids[14]);
	CHECK(btf->ctfids[14] != CTF_ERR);
	CHECK(dt_ctf_lookup_by_name(&dtp, mctf, "struct leaf") == leaf);

	/* A parent-only module can request a previously unconverted type. */
	ctf_close(mctf);
	dt_btf_destroy(&dtp, child);
	child = dt_zalloc(&dtp, sizeof(*child));
	CHECK(child != NULL);
	child->parent = btf;
	module.dm_btf = child;
	module.dm_ctfp = NULL;
	mctf = module.dm_ctfp = dt_btf_module_ctf(&dtp, &module);
	CHECK(mctf != NULL);
	CHECK(dt_ctf_lookup_by_name(&dtp, mctf, "struct leaf") == leaf);

	id = dt_ctf_lookup_by_name(&dtp, mctf, "target_ptr");
	CHECK(id != CTF_ERR && btf->ctfids[16] == btf->ctfids[19]);
	CHECK(ctf_type_kind(ctf, btf->ctfids[16]) == CTF_K_STRUCT);
	CHECK(ctf_member_info(ctf, btf->ctfids[16], "value", &member) == 0);

	/* An ordinary CTF dictionary remains independent of kernel BTF. */
	ordinary = ctf_create(&dtp.dt_ctferr);
	CHECK(ordinary != NULL);
	id = ctf_add_struct_sized(ordinary, CTF_ADD_ROOT, "ordinary", 16);
	CHECK(id != CTF_ERR);
	CHECK(dt_ctf_lookup_by_name(&dtp, ordinary, "struct ordinary") == id);
	CHECK(dt_ctf_lookup_by_name(&dtp, ordinary, "struct leaf") == CTF_ERR);
	ctf_close(ordinary);

	ctf_close(mctf);
	dt_btf_destroy(&dtp, child);
	ctf_close(ctf);
	ctf_close(shared->dm_ctfp);
	shared->dm_ctfp = NULL;
	dt_btf_destroy(&dtp, btf);

	/* Optional real-kernel coverage and conversion counts. */
	if (argc > 1) {
		btf = dt_btf_load_file(&dtp, argv[1], NULL);
		CHECK(btf != NULL);
		kernel.dm_btf = btf;
		kernel.dm_ctfp = shared->dm_ctfp = NULL;
		ctf = kernel.dm_ctfp = dt_btf_module_ctf(&dtp, &kernel);
		CHECK(ctf != NULL);
		CHECK(dt_ctf_lookup_by_name(&dtp, ctf,
					    "struct task_struct") != CTF_ERR);
		for (i = 1, cb = 0; i < btf->type_cnt; i++)
			cb += btf->ctfids[i] != CTF_ERR;
		printf("Converted %lu of %u BTF types\n", (unsigned long)cb,
		       btf->type_cnt - 1);
		ctf_close(ctf);
		ctf_close(shared->dm_ctfp);
		shared->dm_ctfp = NULL;
		dt_btf_destroy(&dtp, btf);
	}
	dt_htab_destroy(dtp.dt_mods);
	return 0;
}
#else
int main(void) { return 0; }
#endif
