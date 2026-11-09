/*
 * Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
#include <fcntl.h>
#include "libdtrace/dt_btf.c"

static int testcase, format;
#define CHECK(x) do { if (!(x)) { \
	fprintf(stderr, "case %d format %d line %d: %s\n", testcase, format, __LINE__, #x); \
	exit(1); \
} } while (0)

static void
write_object(const char *path, void *bytes, size_t size)
{
	int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
	CHECK(fd >= 0);
	if (format == 0) {
		CHECK(write(fd, bytes, size) == size);
	} else {
		Elf *elf = elf_begin(fd, ELF_C_WRITE, NULL);
		GElf_Ehdr ehdr;
		GElf_Shdr shdr = { 0 };
		Elf_Scn *section;
		Elf_Data *data;
		char names[] = "\0.shstrtab\0.BTF\0";

		CHECK(elf != NULL);
		CHECK(gelf_newehdr(elf, format == 1 ? ELFCLASS32 : ELFCLASS64) != 0);
		CHECK(gelf_getehdr(elf, &ehdr) != NULL);
		ehdr.e_type = ET_REL;
		ehdr.e_shstrndx = 1;
		CHECK(gelf_update_ehdr(elf, &ehdr) != 0);
		CHECK((section = elf_newscn(elf)) != NULL);
		CHECK((data = elf_newdata(section)) != NULL);
		data->d_buf = names;
		data->d_size = sizeof(names);
		data->d_type = ELF_T_BYTE;
		data->d_version = EV_CURRENT;
		shdr.sh_name = 1;
		shdr.sh_type = SHT_STRTAB;
		shdr.sh_addralign = 1;
		CHECK(gelf_update_shdr(section, &shdr) != 0);
		CHECK((section = elf_newscn(elf)) != NULL);
		CHECK((data = elf_newdata(section)) != NULL);
		data->d_buf = bytes;
		data->d_size = size;
		data->d_type = ELF_T_BYTE;
		data->d_version = EV_CURRENT;
		shdr.sh_name = 11;
		shdr.sh_type = SHT_PROGBITS;
		CHECK(gelf_update_shdr(section, &shdr) != 0);
		CHECK(elf_update(elf, ELF_C_WRITE) != -1);
		CHECK(elf_end(elf) == 0);
	}
	CHECK(close(fd) == 0);
}

int
main(int argc, char **argv)
{
	struct fixture {
		btf_header_t hdr;
		uint32_t types[10];
		char names[17];
	} good = {
		.hdr = { .magic = BTF_MAGIC, .version = BTF_VERSION,
			.hdr_len = sizeof(btf_header_t), .type_len = 40,
			.str_off = 40, .str_len = 17 },
		.types = { 1, BTF_KIND_INT << 24, 4, BTF_INT_SIGNED << 24 | 32,
			5, BTF_KIND_STRUCT << 24 | 1, 4, 10, 1, 0 },
		.names = "\0int\0node\0value\0"
	}, bad;
	size_t length = offsetof(struct fixture, names) + sizeof(good.names);
	dtrace_hdl_t dt = { 0 };
	dt_btf_t *parent, *loaded;
	int i;

	CHECK(argc == 2 && elf_version(EV_CURRENT) != EV_NONE);
	write_object(argv[1], &good, length);
	parent = dt_btf_load_file(&dt, argv[1], NULL);
	CHECK(parent && parent->type_cnt == 3);
	for (format = 0; format < 3; format++) {
		/* Every truncated header length, plus malformed sections and types. */
		for (testcase = 0; testcase < (int)sizeof(btf_header_t) + 15; testcase++) {
			dt_btf_t decoded = { 0 };
			size_t size = length;
			bad = good;
			if (testcase < sizeof(btf_header_t)) {
				size = testcase;
			} else switch (testcase - sizeof(btf_header_t)) {
			case 0: bad.hdr.magic = 0; break;
			case 1: bad.hdr.hdr_len = sizeof(btf_header_t) - 1; break;
			case 2: bad.hdr.hdr_len = UINT32_MAX; break;
			case 3: bad.hdr.type_off = 1; break;
			case 4: bad.hdr.type_off = UINT32_MAX - 3; break;
			case 5: bad.hdr.str_off = bad.hdr.type_off; break;
			case 6: bad.hdr.str_off = UINT32_MAX; break;
			case 7: bad.hdr.type_len = bad.hdr.str_off + 1; break;
			case 8: bad.hdr.str_len = UINT32_MAX; break;
			case 9: bad.names[sizeof(bad.names) - 1] = 'x'; break;
			case 10: bad.types[1] = 31U << 24; break;
			case 11: bad.types[5] = BTF_KIND_STRUCT << 24 | 65535; break;
			case 12: bad.types[0] = UINT32_MAX; break;
			case 13: bad.hdr.type_len--; break;
			case 14: bad.hdr.str_off = length - sizeof(btf_header_t); break;
			}
			/* Test decoding separately from the file loader's fallback policy. */
			CHECK((decoded.data = malloc(length)) != NULL);
			memcpy(decoded.data, &bad, length);
			decoded.size = size;
			dt.dt_btferr = 0;
			CHECK(dt_btf_decode(&dt, &decoded) == -1 && dt.dt_btferr == EINVAL);
			free(decoded.data);
			free(decoded.types);
			free(decoded.names);
			free(decoded.name_next);
			write_object(argv[1], &bad, size);
			dt.dt_btferr = 0;
			loaded = dt_btf_load_file(&dt, argv[1], parent);
			CHECK(dt.dt_btferr == EINVAL);
			if (loaded) {
				CHECK(loaded->parent == parent && loaded->type_cnt == 0);
				CHECK(loaded->data == NULL && loaded->types == NULL && loaded->names == NULL);
				dt_btf_destroy(&dt, loaded);
			}
			CHECK(parent->type_cnt == 3 && dt_btf_get_string(parent, 1) != NULL);
			/* Reuse the same pathname and handle after every rejected input. */
			write_object(argv[1], &good, length);
			loaded = dt_btf_load_file(&dt, argv[1], NULL);
			CHECK(loaded && loaded->type_cnt == 3);
			CHECK(strcmp(dt_btf_get_string(loaded, 5), "node") == 0);
			dt_btf_destroy(&dt, loaded);
		}
	}
	/* Truncated per-kind payloads, not just incomplete btf_type headers. */
	for (i = BTF_KIND_INT; i <= BTF_KIND_ENUM64; i++) {
		dt_btf_t decoded = { .data = &bad, .size = length };
		ssize_t size;
		bad = good;
		bad.types[1] = (uint32_t)i << 24 | 1;
		size = dt_btf_type_size((const char *)bad.types);
		if (size <= sizeof(btf_type_t))
			continue;
		bad.hdr.type_len = size - 1;
		CHECK(dt_btf_decode(&dt, &decoded) == -1);
	}
	dt_btf_destroy(&dt, parent);
	return 0;
}
