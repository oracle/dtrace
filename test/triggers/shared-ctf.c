/*
 * Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <errno.h>
#include <stdarg.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/btf.h>
#include <dt_module.h>
#include <dt_kernel_module.h>

static const char *watched_archive;
static int archive_opens, archive_io_error;

extern int __real_open64(const char *, int, ...);

int
__wrap_open64(const char *path, int flags, ...)
{
	mode_t mode = 0;

	if (flags & O_CREAT) {
		va_list ap;

		va_start(ap, flags);
		mode = va_arg(ap, int);
		va_end(ap);
	}
	if (watched_archive && strcmp(path, watched_archive) == 0) {
		archive_opens++;
		if (archive_io_error) {
			archive_io_error = 0;
			errno = EIO;
			return -1;
		}
	}
	return __real_open64(path, flags, mode);
}

#ifdef HAVE_LIBCTF
#define CHECK(expr) do { \
	if (!(expr)) { \
		fprintf(stderr, "Check failed at line %d: %s\n", __LINE__, #expr); \
		return 1; \
	} \
} while (0)

static int
write_elf(const char *path, void *contents, size_t size)
{
	Elf *elf;
	Elf64_Ehdr *ehdr;
	Elf_Scn *scn;
	Elf64_Shdr *shdr;
	Elf_Data *data;
	char strings[] = "\0.shstrtab\0.ctf";
	int fd;

	/* Kernel archives are ELF files containing a .ctf section. */
	CHECK(elf_version(EV_CURRENT) != EV_NONE);
	CHECK((fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600)) >= 0);
	CHECK((elf = elf_begin(fd, ELF_C_WRITE, NULL)) != NULL);
	CHECK((ehdr = elf64_newehdr(elf)) != NULL);
	ehdr->e_type = ET_REL;
	ehdr->e_shstrndx = 1;
	CHECK((scn = elf_newscn(elf)) != NULL);
	CHECK((shdr = elf64_getshdr(scn)) != NULL);
	shdr->sh_name = 1;
	shdr->sh_type = SHT_STRTAB;
	CHECK((data = elf_newdata(scn)) != NULL);
	data->d_buf = strings;
	data->d_size = sizeof(strings);
	data->d_align = 1;
	CHECK((scn = elf_newscn(elf)) != NULL);
	CHECK((shdr = elf64_getshdr(scn)) != NULL);
	shdr->sh_name = sizeof("\0.shstrtab");
	shdr->sh_type = SHT_PROGBITS;
	CHECK((data = elf_newdata(scn)) != NULL);
	data->d_buf = contents;
	data->d_size = size;
	data->d_align = 1;
	CHECK(elf_update(elf, ELF_C_WRITE) > 0);
	CHECK(elf_end(elf) == 0);
	CHECK(close(fd) == 0);
	return 0;
}

static int
write_archive(const char *path, const char *embedded)
{
	ctf_dict_t *dicts[2];
	const char *names[] = { "shared_ctf", "vmlinux" };
	ctf_encoding_t enc = { CTF_INT_SIGNED, 0, 32 };
	ctf_id_t integer, structure;
	FILE *raw;
	void *contents;
	long size;
	size_t embedded_size;
	void *embedded_data;
	int err;

	CHECK((dicts[0] = ctf_create(&err)) != NULL);
	CHECK((dicts[1] = ctf_create(&err)) != NULL);
	CHECK(ctf_setmodel(dicts[0], CTF_MODEL_LP64) == 0);
	CHECK(ctf_setmodel(dicts[1], CTF_MODEL_LP64) == 0);
	integer = ctf_add_integer(dicts[0], CTF_ADD_ROOT, "shared_test_int", &enc);
	CHECK(integer != CTF_ERR);
	structure = ctf_add_struct(dicts[0], CTF_ADD_ROOT, "shared_test");
	CHECK(structure != CTF_ERR);
	CHECK(ctf_add_member(dicts[0], structure, "member", integer) == 0);
	CHECK(ctf_update(dicts[0]) == 0);
	CHECK(ctf_import(dicts[1], dicts[0]) == 0);
	CHECK(ctf_parent_name_set(dicts[1], "shared_ctf") == 0);
	CHECK(ctf_add_integer(dicts[1], CTF_ADD_ROOT, "module_test_int", &enc) != CTF_ERR);
	CHECK(ctf_update(dicts[1]) == 0);
	CHECK((raw = tmpfile()) != NULL);
	CHECK(ctf_arc_write_fd(fileno(raw), dicts, 2, names, 0) == 0);
	CHECK(fseek(raw, 0, SEEK_END) == 0);
	CHECK((size = ftell(raw)) > 0);
	CHECK((contents = malloc(size)) != NULL);
	rewind(raw);
	CHECK(fread(contents, size, 1, raw) == 1);
	CHECK(fclose(raw) == 0);
	embedded_data = ctf_write_mem(dicts[0], &embedded_size, (size_t)-1);
	CHECK(embedded_data != NULL);
	CHECK(write_elf(embedded, embedded_data, embedded_size) == 0);
	free(embedded_data);
	ctf_close(dicts[1]);
	ctf_close(dicts[0]);

	CHECK(write_elf(path, contents, size) == 0);
	free(contents);
	return 0;
}

static int
write_btf(const char *path)
{
	const struct {
		struct btf_header hdr;
		struct btf_type integer;
		uint32_t encoding;
		struct btf_type structure;
		struct btf_member member;
		char strings[sizeof("\0shared_test_int\0shared_test\0member")];
	} data = {
		.hdr = {
			.magic = BTF_MAGIC, .version = BTF_VERSION,
			.hdr_len = sizeof(struct btf_header),
			.type_len = 2 * sizeof(struct btf_type) + sizeof(uint32_t) +
				    sizeof(struct btf_member),
			.str_off = 2 * sizeof(struct btf_type) + sizeof(uint32_t) +
				   sizeof(struct btf_member),
			.str_len = sizeof(data.strings),
		},
		.integer = { .name_off = 1, .info = BTF_KIND_INT << 24, .size = 4 },
		.encoding = (BTF_INT_SIGNED << 24) | 32,
		.structure = { .name_off = sizeof("\0shared_test_int"),
			       .info = (BTF_KIND_STRUCT << 24) | 1, .size = 4 },
		.member = { .name_off = sizeof("\0shared_test_int\0shared_test"),
			    .type = 1 },
		.strings = "\0shared_test_int\0shared_test\0member",
	};
	int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);

	CHECK(fd >= 0);
	CHECK(write(fd, &data, sizeof(data)) == sizeof(data));
	CHECK(close(fd) == 0);
	return 0;
}

static int
check_source(const char *ctfpath, const char *btfpath, int archived,
    int kernel_first)
{
	dtrace_hdl_t *dtp;
	dtrace_typeinfo_t ti;
	dt_module_t *shared, *kernel, *child;
	ctf_file_t *parent = NULL;
	ctf_file_t *kernel_ctf = NULL;
	struct dt_btf *kernel_btf = NULL;
	ctf_membinfo_t member;
	int err, i;

	CHECK((dtp = dtrace_open(DTRACE_VERSION, DTRACE_O_LP64, &err)) != NULL);
	CHECK(dtrace_setopt(dtp, "ctfpath", ctfpath) == 0);
	CHECK(dtrace_setopt(dtp, "btfpath", btfpath) == 0);

	/* Updates preserve dictionaries and BTF after parents and children load. */
	for (i = 0; i < 3; i++) {
		CHECK(dtrace_update(dtp) == 0);
		CHECK(dt_module_shared_ctf(dtp) == parent);
		kernel = dtp->dt_exec;
		CHECK(kernel != NULL && kernel->dm_ctfp == kernel_ctf);
		CHECK(kernel->dm_btf == kernel_btf);
		CHECK((shared = dt_module_create(dtp, "shared_ctf")) != NULL);

		/* Both the synthetic module and vmlinux can initialize the source. */
		if (kernel_first) {
			CHECK(dtrace_lookup_by_type(dtp, "vmlinux",
			    "struct shared_test", &ti) == 0);
			CHECK(!!(kernel->dm_flags & DT_DM_CTF_ARCHIVED) == archived);
		}
		CHECK(dtrace_lookup_by_type(dtp, "shared_ctf", "struct shared_test", &ti) == 0);
		parent = dt_module_shared_ctf(dtp);
		CHECK(parent != NULL && parent == shared->dm_ctfp);
		CHECK(dt_module_lookup_by_ctf(dtp, parent) == shared);
		CHECK(ctf_member_info(parent, ti.dtt_type, "member", &member) == 0);
		CHECK(member.ctm_offset == 0);
		CHECK(ctf_type_size(parent, ti.dtt_type) == 4);
		CHECK(dtrace_lookup_by_type(dtp, "vmlinux", "struct shared_test", &ti) == 0);
		CHECK(ti.dtt_ctfp == kernel->dm_ctfp && ti.dtt_ctfp != parent);
		kernel_ctf = kernel->dm_ctfp;
		kernel_btf = kernel->dm_btf;
		CHECK(dt_module_lookup_by_ctf(dtp, ti.dtt_ctfp) == kernel);
		CHECK(ctf_parent_dict(ti.dtt_ctfp) == parent);
		if (!archived) {
			CHECK(dtrace_lookup_by_type(dtp, "vmlinux",
			    "const struct shared_test **", &ti) == 0);
			CHECK(ctf_type_kind(ti.dtt_ctfp, ti.dtt_type) == CTF_K_POINTER);
		}

		if (archived) {
			CHECK(dtrace_lookup_by_type(dtp, "vmlinux", "module_test_int", &ti) == 0);
			CHECK(ctf_lookup_by_name(parent, "module_test_int") == CTF_ERR);
		} else {
			GElf_Sym sym;

			/* Inherited BTF must not make an absent ELF module loadable. */
			CHECK(dtrace_lookup_by_name(dtp, "shared_ctf_test_absent",
			    "missing_symbol", &sym, NULL) == -1);

			/* A module without local BTF must import the same parent. */
			CHECK((child = dt_module_create(dtp, "shared_ctf_test_child")) != NULL);
			child->dm_flags |= DT_DM_KERNEL;
			CHECK(dtrace_lookup_by_type(dtp, child->dm_name, "struct shared_test", &ti) == 0);
			CHECK(ctf_parent_dict(ti.dtt_ctfp) == parent);
			CHECK(dt_module_lookup_by_ctf(dtp, ti.dtt_ctfp) == child);
		}
	}
	dtrace_close(dtp);
	return 0;
}
static int
check_embedded(const char *missing, const char *embedded)
{
	dtrace_hdl_t *dtp;
	dtrace_typeinfo_t ti;
	dt_module_t *shared;
	int err, i;

	CHECK((dtp = dtrace_open(DTRACE_VERSION, DTRACE_O_LP64, &err)) != NULL);
	CHECK(dtrace_setopt(dtp, "ctfpath", missing) == 0);
	CHECK(dtrace_setopt(dtp, "btfpath", "none") == 0);
	CHECK(dtrace_update(dtp) == 0);
	/* Use a private path map so host ctf.ko files cannot satisfy this lookup. */
	dt_htab_destroy(dtp->dt_kernpaths);
	dtp->dt_kernpaths = NULL;
	CHECK(dt_kern_path_create(dtp, strdup("shared_ctf_test"), strdup(missing)) != NULL);
	CHECK((shared = dt_module_create(dtp, "shared_ctf")) != NULL);
	/* Transient errors retry; confirmed absence is cached until a path changes. */
	watched_archive = missing;
	archive_opens = 0;
	archive_io_error = 1;
	CHECK(dt_module_getctf(dtp, shared) == NULL);
	CHECK(archive_opens == 1);
	CHECK(dt_module_getctf(dtp, shared) == NULL);
	CHECK(archive_opens == 2);
	CHECK(dt_module_getctf(dtp, shared) == NULL);
	CHECK(archive_opens == 2);
	CHECK(dtrace_setopt(dtp, "ctfpath", missing) == 0);
	CHECK(dt_module_getctf(dtp, shared) == NULL);
	CHECK(archive_opens == 3);
	CHECK(dtrace_setopt(dtp, "btfpath", "none") == 0);
	CHECK(dt_module_getctf(dtp, shared) == NULL);
	CHECK(archive_opens == 4);
	CHECK(dtrace_setopt(dtp, "modpath", dtp->dt_module_path) == 0);
	CHECK(dt_module_getctf(dtp, shared) == NULL);
	CHECK(archive_opens == 5);
	watched_archive = NULL;
	CHECK(dtrace_errno(dtp) == EDT_NOCTF);
	CHECK(dt_module_shared_ctf(dtp) == NULL);

	/* The old ctf.ko dictionary becomes the synthetic module's dictionary. */
	CHECK(dt_kern_path_create(dtp, strdup("ctf"), strdup(embedded)) != NULL);
	for (i = 0; i < 3; i++) {
		CHECK(dtrace_lookup_by_type(dtp, "shared_ctf", "struct shared_test", &ti) == 0);
		CHECK(ti.dtt_ctfp == dt_module_shared_ctf(dtp));
		CHECK(dt_module_lookup_by_ctf(dtp, ti.dtt_ctfp) == shared);
		CHECK(ctf_type_size(ti.dtt_ctfp, ti.dtt_type) == 4);
		CHECK(dtrace_update(dtp) == 0);
		CHECK(dt_module_shared_ctf(dtp) == ti.dtt_ctfp);
	}
	dtrace_close(dtp);
	return 0;
}

#endif

int
main(int argc, char **argv)
{
#ifdef HAVE_LIBCTF
	char archive[PATH_MAX], btf[PATH_MAX], missing[PATH_MAX], embedded[PATH_MAX];

	CHECK(argc == 2);
	snprintf(archive, sizeof(archive), "%s/test.ctfa", argv[1]);
	snprintf(btf, sizeof(btf), "%s/vmlinux", argv[1]);
	snprintf(missing, sizeof(missing), "%s/missing.ctfa", argv[1]);
	snprintf(embedded, sizeof(embedded), "%s/ctf.ko", argv[1]);
	CHECK(write_archive(archive, embedded) == 0);
	CHECK(write_btf(btf) == 0);
	CHECK(check_source(archive, "none", 1, 0) == 0);
	CHECK(check_source(archive, "none", 1, 1) == 0);
	CHECK(check_source(missing, argv[1], 0, 0) == 0);
	CHECK(check_source(missing, argv[1], 0, 1) == 0);
	CHECK(check_embedded(missing, embedded) == 0);
#endif
	return 0;
}
