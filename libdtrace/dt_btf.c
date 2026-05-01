/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <assert.h>
#include <byteswap.h>
#include <errno.h>
#include <stdio.h>
#include <sys/mman.h>
#include <linux/btf.h>

#include <dt_impl.h>
#include <dt_bpf.h>
#include <dt_module.h>

typedef struct btf_header	btf_header_t;
typedef struct btf_type		btf_type_t;
typedef struct btf_enum		btf_enum_t;
typedef struct btf_array	btf_array_t;
typedef struct btf_member	btf_member_t;
typedef struct btf_param	btf_param_t;
typedef struct btf_var		btf_var_t;
typedef struct btf_var_secinfo	btf_var_secinfo_t;
typedef struct btf_decl_tag	btf_decl_tag_t;
typedef struct btf_enum64	btf_enum64_t;

struct dt_btf {
	void		*data;			/* raw BTF data */
	size_t		size;			/* raw BTF data size */
	btf_header_t	*hdr;			/* BTF header */
	char		*sdata;			/* string data */
	const dt_btf_t	*parent;		/* parent BTF */
	uint32_t	type_cnt;		/* number of types */
	btf_type_t	**types;		/* type table */
	ctf_id_t	*ctfids;		/* matching CTF type ids */
};

static btf_type_t	btf_void;

static const char *
dt_btf_errmsg(int err)
{
	return strerror(err);
}

static int
dt_btf_set_errno(dtrace_hdl_t *dtp, int err)
{
	dtp->dt_btferr = err;
	return -1;
}

static void *
dt_btf_set_load_errno(dtrace_hdl_t *dtp, int err)
{
	dtp->dt_btferr = err;
	return NULL;
}

static void
dt_btf_error(dtrace_hdl_t *dtp, int eid, const char *format, ...)
{
	va_list	ap;

	va_start(ap, format);
	dt_set_errmsg(dtp, dt_errtag(eid), NULL, NULL, 0, format, ap);
	dt_set_errno(dtp, EDT_BPF);
	va_end(ap);
}

static btf_type_t *
dt_btf_type_by_id(const dt_btf_t *btf, int32_t id)
{
	const dt_btf_t	*shared_btf;
	uint32_t	shared_cnt;

	if (id == 0)
		return &btf_void;

	shared_btf = btf->parent;
	if (shared_btf != NULL) {
		shared_cnt = shared_btf->type_cnt;
		if (id < shared_cnt)
			return shared_btf->types[id];

		if (shared_cnt > 0)
			id -= shared_cnt - 1;
	}

	if (id < btf->type_cnt && id > 0)
		return btf->types[id];

	return NULL;
}

static btf_type_t *
dt_btf_real_type_by_id(const dt_btf_t *btf, int32_t id)
{
	btf_type_t	*type = dt_btf_type_by_id(btf, id);

	while (type != NULL) {
		switch (BTF_INFO_KIND(type->info)) {
		case BTF_KIND_CONST:
		case BTF_KIND_FUNC:
		case BTF_KIND_RESTRICT:
		case BTF_KIND_TYPE_TAG:
		case BTF_KIND_TYPEDEF:
		case BTF_KIND_VOLATILE:
			type = dt_btf_type_by_id(btf, type->type);
			continue;
		default:
			return type;
		}
	}

	return NULL;
}

static const char *
dt_btf_get_string(const dt_btf_t *btf, uint32_t off)
{
	const dt_btf_t	*shared_btf;
	int32_t		shared_len;

	shared_btf = btf->parent;
	if (shared_btf != NULL && shared_btf->hdr != NULL) {
		shared_len = shared_btf->hdr->str_len;
		if (off < shared_len)
			return shared_btf->sdata + off;

		off -= shared_len;
	}

	if (off < btf->hdr->str_len)
		return btf->sdata + off;

	return NULL;
}

static int
dt_btf_validate_header(dtrace_hdl_t *dtp, dt_btf_t *btf)
{
	btf_header_t	*hdr = (btf_header_t *)btf->data;

	if (btf->size < sizeof(btf_header_t)) {
		dt_dprintf("BTF truncated header (%ld)\n", btf->size);
		return dt_btf_set_errno(dtp, EINVAL);
	}

	hdr = (btf_header_t *)btf->data;

	/* Validate magic number. */
	if (hdr->magic != BTF_MAGIC) {
		dt_dprintf("BTF magic mismatch (%04x, expected %04x)\n",
			   hdr->magic, BTF_MAGIC);
		return dt_btf_set_errno(dtp, EINVAL);
	}

	/* Validate header length. */
	if (hdr->hdr_len < sizeof(btf_header_t) ||
	    hdr->hdr_len >= btf->size) {
		dt_dprintf("BTF bad header length (%d)\n", hdr->hdr_len);
		return dt_btf_set_errno(dtp, EINVAL);
	}

	/* Validate data offsets and lengths. */
	if (hdr->type_off % 4) {
		dt_dprintf("BTF type offset misaligned (%d)\n", hdr->type_off);
		return dt_btf_set_errno(dtp, EINVAL);
	}
	if (hdr->type_off >= btf->size - hdr->hdr_len) {
		dt_dprintf("BTF bad type offset (%d)\n", hdr->type_off);
		return dt_btf_set_errno(dtp, EINVAL);
	}
	if (hdr->str_off <= hdr->type_off ||
	    hdr->str_off >= btf->size - hdr->hdr_len) {
		dt_dprintf("BTF bad string offset (%d)\n", hdr->str_off);
		return dt_btf_set_errno(dtp, EINVAL);
	}
	if (hdr->type_len > hdr->str_off - hdr->type_off) {
		dt_dprintf("BTF bad type section length (%d)\n", hdr->type_len);
		return dt_btf_set_errno(dtp, EINVAL);
	}
	if (hdr->str_len > btf->size - hdr->hdr_len - hdr->str_off) {
		dt_dprintf("BTF bad string section length (%d)\n",
			   hdr->str_len);
		return dt_btf_set_errno(dtp, EINVAL);
	}
	if (((char *)btf->data)[hdr->hdr_len + hdr->str_off + hdr->str_len - 1] != 0) {
		dt_dprintf("BTF string section is not NUL-terminated\n");
		return dt_btf_set_errno(dtp, EINVAL);
	}

	btf->hdr = hdr;

	return 0;
}

static ssize_t
dt_btf_type_size(const char *ptr)
{
	btf_type_t	*type = (btf_type_t *)ptr;
	ssize_t		size = sizeof(btf_type_t);
	int		vlen = BTF_INFO_VLEN(type->info);

	switch (BTF_INFO_KIND(type->info)) {
	case BTF_KIND_INT:
		return size + sizeof(uint32_t);
	case BTF_KIND_ARRAY:
		return size + sizeof(btf_array_t);
	case BTF_KIND_STRUCT:
	case BTF_KIND_UNION:
		return size + vlen * sizeof(btf_member_t);
	case BTF_KIND_ENUM:
		return size + vlen * sizeof(btf_enum_t);
	case BTF_KIND_FUNC_PROTO:
		return size + vlen * sizeof(btf_param_t);
	case BTF_KIND_VAR:
		return size + sizeof(btf_var_t);
	case BTF_KIND_DATASEC:
		return size + vlen * sizeof(btf_var_secinfo_t);
	case BTF_KIND_DECL_TAG:
		return size + sizeof(btf_decl_tag_t);
	case BTF_KIND_TYPE_TAG:
		return size;
	case BTF_KIND_ENUM64:
		return size + vlen * sizeof(btf_enum64_t);
	case BTF_KIND_PTR:
	case BTF_KIND_FWD:
	case BTF_KIND_TYPEDEF:
	case BTF_KIND_VOLATILE:
	case BTF_KIND_CONST:
	case BTF_KIND_RESTRICT:
	case BTF_KIND_FUNC:
	case BTF_KIND_FLOAT:
		return size;
	default:
		return -EINVAL;
	}

	return -EINVAL;
}

static int
dt_btf_decode(dtrace_hdl_t *dtp, dt_btf_t *btf)
{
	btf_header_t	*hdr;
	char		*ptr, *end;
	int32_t		idx;
	char		*tdata;

	if (dt_btf_validate_header(dtp, btf) == -1)
		return dt_btf_set_errno(dtp, EINVAL);

	hdr = btf->hdr;
	btf->sdata = (char *)btf->data + hdr->hdr_len + hdr->str_off;
	tdata = (char *)btf->data + hdr->hdr_len + hdr->type_off;

	/* First count how many types there really are. */
	ptr = tdata;
	end = tdata + hdr->type_len;
	idx = 1;
	while (ptr + sizeof(btf_type_t) <= end) {
		ssize_t		size = dt_btf_type_size(ptr);
		btf_type_t	*type = (btf_type_t *)ptr;

		if (size < 0 || ptr + size > end ||
		    dt_btf_get_string(btf, type->name_off) == NULL)
			return dt_btf_set_errno(dtp, EINVAL);

		idx++;
		ptr += size;
	}
	if (ptr != end)
		return dt_btf_set_errno(dtp, EINVAL);

	/* Next, populate the type offsets table. */
	btf->type_cnt = idx;
	btf->types = dt_calloc(dtp, btf->type_cnt, sizeof(btf_type_t *));
	if (btf->types == NULL)
		return dt_btf_set_errno(dtp, ENOMEM);

	ptr = tdata;
	idx = 1;
	while (ptr + sizeof(btf_type_t) <= end) {
		ssize_t	size = dt_btf_type_size(ptr);

		if (size < 0)
			return dt_btf_set_errno(dtp, EINVAL);

		btf->types[idx++] = (btf_type_t *)ptr;
		ptr += size;
	}
	if (ptr != end)
		return dt_btf_set_errno(dtp, EINVAL);

	return 0;
}

/*
 * Try to load BTF data from the given file.  We use FILE-based IO primitives
 * instead of fd-based IO primitives because the BTF data is usually provided
 * by sysfs pseudo-files and the fd-based IO primitives do not seem to work
 * with that.
 */
static dt_btf_t *
dt_btf_load(dtrace_hdl_t *dtp, const char *fn)
{
	FILE		*fp = NULL;
	dt_btf_t	*btf = NULL;
	btf_header_t	hdr;
	Elf		*elf = NULL;
	size_t		shstrs;
	Elf_Scn		*sp = NULL;
	int		err = 0;

	btf = dt_zalloc(dtp, sizeof(dt_btf_t));
	if (btf == NULL)
		return dt_btf_set_load_errno(dtp, ENOMEM);

	fp = fopen(fn, "rb");
	if (fp == NULL) {
		dtp->dt_btferr = errno;
		goto fail;
	}

	/* First see whether this might be a file with raw BTF data. */
	if (fread(&hdr, 1, sizeof(hdr), fp) < sizeof(hdr)) {
		dtp->dt_btferr = ferror(fp) ? EIO : EINVAL;
		goto fail;
	}

	rewind(fp);
	if (hdr.magic == BTF_MAGIC) {
		struct stat	st;

		if (fstat(fileno(fp), &st) == -1) {
			dtp->dt_btferr = errno;
			goto fail;
		}

		btf->data = dt_alloc(dtp, st.st_size);
		if (btf->data == NULL) {
			dtp->dt_btferr = ENOMEM;
			goto fail;
		}

		if (fread(btf->data, 1, st.st_size, fp) < st.st_size) {
			dtp->dt_btferr = ferror(fp) ? EIO : EINVAL;
			goto fail;
		}

		btf->size = st.st_size;
	} else {
		/* Next see whether this might be an ELF file with BTF data. */
		elf = elf_begin(fileno(fp), ELF_C_READ_MMAP, NULL);
		if (elf == NULL) {
			dtp->dt_btferr = EINVAL;
			goto elf_fail;
		}
		if (elf_getshdrstrndx(elf, &shstrs) == -1)
			goto elf_fail;

		while ((sp = elf_nextscn(elf, sp)) != NULL) {
			GElf_Shdr	sh;
			Elf_Data	*edata;
			const char	*name;

			/* Skip malformed sections. */
			if (gelf_getshdr(sp, &sh) == NULL ||
			    sh.sh_type == SHT_NULL)
				continue;

			name = elf_strptr(elf, shstrs, sh.sh_name);
			if (name == NULL || strcmp(name, ".BTF") != 0)
				continue;

			/* Found the section we need. */
			edata = elf_getdata(sp, 0);
			if (edata == NULL)
				goto elf_fail;

			btf->data = dt_alloc(dtp, edata->d_size);
			if (btf->data == NULL) {
				dtp->dt_btferr = ENOMEM;
				goto fail;
			}

			memcpy(btf->data, edata->d_buf, edata->d_size);

			btf->size = edata->d_size;
			break;
		}
		if (btf->data == NULL) {
			dtp->dt_btferr = ENOENT;
			goto fail;
		}
	}

	if (elf != NULL)
		elf_end(elf);
	fclose(fp);
	return btf;

elf_fail:
	dtp->dt_btferr = EINVAL;
	err = elf_errno();
	dt_btf_error(dtp, 0, "ELF: %s", elf_errmsg(err));

fail:
	if (elf != NULL)
		elf_end(elf);
	if (fp != NULL)
		fclose(fp);
	dt_dprintf("Cannot open BTF file %s: %s\n", fn,
			   dt_btf_errmsg(dtp->dt_btferr));
	dt_btf_destroy(dtp, btf);
	return NULL;
}

dt_btf_t *
dt_btf_load_file(dtrace_hdl_t *dtp, const char *fn, const dt_btf_t *parent)
{
	dt_btf_t	*btf;

	/*
	 * Missing BTF or allocation failure returns NULL; invalid data leaves
	 * an empty BTF.
	 */
	btf = dt_btf_load(dtp, fn);
	if (btf == NULL)
		return NULL;

	btf->parent = parent;

	/* Decode the loaded BTF data. */
	if (btf->data && dt_btf_decode(dtp, btf) == -1) {
		if (dtp->dt_btferr == ENOMEM) {
			dt_btf_destroy(dtp, btf);
			return NULL;
		}

		/*
		 * Failure to decode the data leaves us with just one option:
		 * pretend the module has no type data beyond its parent's
		 * data.
		 */
		dt_dprintf("Cannot decode BTF data %s: %s\n", fn,
			   dt_btf_errmsg(dtp->dt_btferr));

		dt_free(dtp, btf->data);
		dt_free(dtp, btf->types);
		memset(btf, 0, sizeof(*btf));
		btf->parent = parent;
	}

	dt_dprintf("BTF file %s: %d types\n", fn, btf->type_cnt);

	return btf;
}

dt_btf_t *
dt_btf_load_module(dtrace_hdl_t *dtp, dt_module_t *dmp)
{
	char		*fn = NULL;
	int		rc = 0;
	const dt_btf_t	*parent = NULL;

	assert(dmp != NULL);

	/* If already tried this for the module, return what we have. */
	if (dmp->dm_flags & DT_DM_BTF_LOADED)
		return dmp->dm_btf;

	/*
	 * If the module is not vmlinux, we will use the vmlinux BTF data as the
	 * parent.
	 */
	if (dmp != dtp->dt_exec) {
		parent = dtp->dt_exec->dm_btf;
		/* If we do not have it yet, try to load it. */
		if (parent == NULL)
			parent = dt_btf_load_module(dtp, dtp->dt_exec);
		/* If loading failed, the module cannot have BTF data either. */
		if (parent == NULL)
			return NULL;
	}

	/*
	 * Default: /sys/kernel/btf/<module>
	 * If "none", disable BTF.
	 * Otherwise: <BTF path>/<module>
	 */
	if (dtp->dt_btf_path == NULL)
		rc = asprintf(&fn, "/sys/kernel/btf/%s", dmp->dm_name);
	else if (strcmp(dtp->dt_btf_path, "none") == 0)
		return NULL;
	else
		rc = asprintf(&fn, "%s/%s", dtp->dt_btf_path, dmp->dm_name);

	if (rc == -1)
		return dt_btf_set_load_errno(dtp, ENOMEM);

	dmp->dm_btf = dt_btf_load_file(dtp, fn, parent);
	free(fn);

	if (dmp->dm_btf == NULL) {
		if (dtp->dt_btferr != ENOENT)
			return NULL;

		/* A missing module BTF file still inherits its parent's types. */
		if (parent != NULL) {
			dmp->dm_btf = dt_zalloc(dtp, sizeof(dt_btf_t));
			if (dmp->dm_btf == NULL)
				return dt_btf_set_load_errno(dtp, ENOMEM);

			dmp->dm_btf->parent = parent;
		}
	}

	dmp->dm_flags |= DT_DM_BTF_LOADED;

	return dmp->dm_btf;
}

void
dt_btf_destroy(dtrace_hdl_t *dtp, dt_btf_t *btf)
{
	if (!btf)
		return;

	dt_free(dtp, btf->data);
	dt_free(dtp, btf->types);
	dt_free(dtp, btf->ctfids);
	dt_free(dtp, btf);
}

#ifdef HAVE_LIBCTF
static ctf_id_t
dt_ctf_set_errno(dtrace_hdl_t *dtp, int err)
{
	dtp->dt_ctferr = err;
	dt_set_errno(dtp, EDT_CTF);
	return CTF_ERR;
}

static ctf_id_t
dt_ctf_error(dtrace_hdl_t *dtp, ctf_dict_t *ctf)
{
	return dt_ctf_set_errno(dtp, ctf_errno(ctf));
}

static ctf_id_t
dt_btf_add_to_ctf(dtrace_hdl_t *dtp, dt_btf_t *btf, ctf_dict_t *ctf,
		  uint32_t type_id)
{
	const dt_btf_t	*shared_btf = btf->parent;
	btf_type_t	*type;
	int		kind, vlen;
	const char	*name;
	ctf_id_t	ctfid;
	ctf_encoding_t	enc;

	/*
	 * If we already have shared_ctf, we may be looking for a type in that
	 * shared_ctf.
	 */
	if (dt_module_shared_ctf(dtp) && shared_btf != NULL) {
		if (type_id < shared_btf->type_cnt)
			return shared_btf->ctfids[type_id];

		if (shared_btf->type_cnt > 0)
			type_id -= shared_btf->type_cnt - 1;
	}

	assert(type_id < btf->type_cnt);

	if (btf->ctfids[type_id] != CTF_ERR)
		return btf->ctfids[type_id];

	assert(type_id > 0);

	type = btf->types[type_id];
	kind = BTF_INFO_KIND(type->info);
	vlen = BTF_INFO_VLEN(type->info);
	name = dt_btf_get_string(btf, type->name_off);
	if (name == NULL)
		return dt_ctf_set_errno(dtp, ECTF_NAMELEN);

	if (name[0]) {
		char	n[DT_TYPE_NAMELEN];

		/* Do we already have this type? */
		switch (kind) {
		case BTF_KIND_ENUM:
		case BTF_KIND_ENUM64:
			if (snprintf(n, sizeof(n), "enum %s", name) < 0)
				return dt_ctf_set_errno(dtp, ECTF_NAMELEN);

			ctfid = ctf_lookup_by_name(ctf, n);
			break;
		case BTF_KIND_UNION:
			if (snprintf(n, sizeof(n), "union %s", name) < 0)
				return dt_ctf_set_errno(dtp, ECTF_NAMELEN);
	
			ctfid = ctf_lookup_by_name(ctf, n);
			break;
		case BTF_KIND_STRUCT:
			if (snprintf(n, sizeof(n), "struct %s", name) < 0)
				return dt_ctf_set_errno(dtp, ECTF_NAMELEN);
	
			ctfid = ctf_lookup_by_name(ctf, n);
			break;
		default:
			ctfid = ctf_lookup_by_name(ctf, name);
		}

		if (ctfid != CTF_ERR) {
			btf->ctfids[type_id] = ctfid;
			return ctfid;
		}
	}

	switch (kind) {
	case BTF_KIND_INT: {
		uint32_t	data = *(uint32_t *)(type + 1);
		uint32_t	benc = BTF_INT_ENCODING(data);

		enc.cte_format = 0;
		enc.cte_offset = BTF_INT_OFFSET(data);
		enc.cte_bits = BTF_INT_BITS(data);

		if (benc & BTF_INT_SIGNED)
			enc.cte_format |= CTF_INT_SIGNED;
		if (benc & BTF_INT_CHAR)
			enc.cte_format |= CTF_INT_CHAR;
		if (benc & BTF_INT_BOOL)
			enc.cte_format |= CTF_INT_BOOL;

		/* Some special case handling. */
		if (enc.cte_bits == 8) {
			if (strcmp(name, "_Bool") == 0)
				enc.cte_format = CTF_INT_BOOL;
			else
				enc.cte_format = CTF_INT_CHAR;
		}

		ctfid = ctf_add_integer(ctf, CTF_ADD_ROOT, name, &enc);
		btf->ctfids[type_id] = ctfid;

		return ctfid == CTF_ERR ? dt_ctf_error(dtp, ctf) : ctfid;
	}
	case BTF_KIND_FLOAT: {
		switch (type->size) {
		case 4:
			enc.cte_format = CTF_FP_SINGLE;
			break;
		case 8:
			enc.cte_format = CTF_FP_DOUBLE;
			break;
		case 16:
			enc.cte_format = CTF_FP_LDOUBLE;
			break;
		}

		enc.cte_offset = 0;
		enc.cte_bits = type->size * 8;

		ctfid = ctf_add_float(ctf, CTF_ADD_ROOT, name, &enc);
		btf->ctfids[type_id] = ctfid;

		return ctfid == CTF_ERR ? dt_ctf_error(dtp, ctf) : ctfid;
	}
	case BTF_KIND_ENUM: {
		int		i;
		btf_enum_t	*item = (btf_enum_t *)(type + 1);

		ctfid = ctf_add_enum(ctf, CTF_ADD_ROOT, name);
		if (ctfid == CTF_ERR)
			return dt_ctf_error(dtp, ctf);

		for (i = 0; i < vlen; i++, item++) {
			int	err;
			const char	*iname;

			iname = dt_btf_get_string(btf, item->name_off);
			if (iname == NULL)
				return dt_ctf_set_errno(dtp, ECTF_NAMELEN);
			err = ctf_add_enumerator(ctf, ctfid, iname, item->val);
			if (err == CTF_ERR)
				return dt_ctf_error(dtp, ctf);
		}

		btf->ctfids[type_id] = ctfid;

		return ctfid == CTF_ERR ? dt_ctf_error(dtp, ctf) : ctfid;
	}
	case BTF_KIND_PTR: {
		ctfid = dt_btf_add_to_ctf(dtp, btf, ctf, type->type);
		if (ctfid == CTF_ERR)
			return CTF_ERR;		/* errno already set */

		ctfid = ctf_add_pointer(ctf, CTF_ADD_ROOT, ctfid);
		btf->ctfids[type_id] = ctfid;

		return ctfid == CTF_ERR ? dt_ctf_error(dtp, ctf) : ctfid;
	}
	case BTF_KIND_CONST: {
		ctfid = dt_btf_add_to_ctf(dtp, btf, ctf, type->type);
		if (ctfid == CTF_ERR)
			return CTF_ERR;		/* errno already set */

		ctfid = ctf_add_const(ctf, CTF_ADD_ROOT, ctfid);
		btf->ctfids[type_id] = ctfid;

		return ctfid == CTF_ERR ? dt_ctf_error(dtp, ctf) : ctfid;
	}
	case BTF_KIND_VOLATILE: {
		ctfid = dt_btf_add_to_ctf(dtp, btf, ctf, type->type);
		if (ctfid == CTF_ERR)
			return CTF_ERR;		/* errno already set */

		ctfid = ctf_add_volatile(ctf, CTF_ADD_ROOT, ctfid);
		btf->ctfids[type_id] = ctfid;

		return ctfid == CTF_ERR ? dt_ctf_error(dtp, ctf) : ctfid;
	}
	case BTF_KIND_RESTRICT: {
		ctfid = dt_btf_add_to_ctf(dtp, btf, ctf, type->type);
		if (ctfid == CTF_ERR)
			return CTF_ERR;		/* errno already set */

		ctfid = ctf_add_restrict(ctf, CTF_ADD_ROOT, ctfid);
		btf->ctfids[type_id] = ctfid;

		return ctfid == CTF_ERR ? dt_ctf_error(dtp, ctf) : ctfid;
	}
	case BTF_KIND_ARRAY: {
		btf_array_t	*arr = (btf_array_t *)(type + 1);
		ctf_arinfo_t	arp;

		arp.ctr_contents = dt_btf_add_to_ctf(dtp, btf, ctf, arr->type);
		if (arp.ctr_contents == CTF_ERR)
			return CTF_ERR;
		arp.ctr_index = dt_btf_add_to_ctf(dtp, btf, ctf, arr->index_type);
		if (arp.ctr_contents == CTF_ERR)
			return CTF_ERR;
		arp.ctr_nelems = arr->nelems;

		ctfid = ctf_add_array(ctf, CTF_ADD_ROOT, &arp);
		btf->ctfids[type_id] = ctfid;

		return ctfid == CTF_ERR ? dt_ctf_error(dtp, ctf) : ctfid;
	}
	case BTF_KIND_TYPEDEF: {
		ctfid = dt_btf_add_to_ctf(dtp, btf, ctf, type->type);
		if (ctfid == CTF_ERR)
			return CTF_ERR;		/* errno already set */

		ctfid = ctf_add_typedef(ctf, CTF_ADD_ROOT, name, ctfid);
		btf->ctfids[type_id] = ctfid;

		return ctfid == CTF_ERR ? dt_ctf_error(dtp, ctf) : ctfid;
	}
	case BTF_KIND_STRUCT:
	case BTF_KIND_UNION: {
		int		i;
		btf_member_t	*item = (btf_member_t *)(type + 1);

		if (kind == BTF_KIND_STRUCT)
			ctfid = ctf_add_struct_sized(ctf, CTF_ADD_ROOT, name,
						     type->size);
		else
			ctfid = ctf_add_union_sized(ctf, CTF_ADD_ROOT, name,
						    type->size);

		btf->ctfids[type_id] = ctfid;
		if (ctfid == CTF_ERR)
			return dt_ctf_error(dtp, ctf);

		for (i = 0; i < vlen; i++, item++) {
			int		err;
			ctf_id_t	mbid;
			const char	*mname;

			mbid = dt_btf_add_to_ctf(dtp, btf, ctf, item->type);
			if (mbid == CTF_ERR)
				return CTF_ERR;
			mname = dt_btf_get_string(btf, item->name_off);
			if (mname == NULL)
				return dt_ctf_set_errno(dtp, ECTF_NAMELEN);

			if (BTF_INFO_KFLAG(type->info)) {
				enc.cte_format = 0;
				enc.cte_offset = 0;
				enc.cte_bits = BTF_MEMBER_BITFIELD_SIZE(item->offset);
				item->offset = BTF_MEMBER_BIT_OFFSET(item->offset);

				if (enc.cte_bits > 0) {
					int	kind;

					/* Resolve member type for CTF. */
					mbid = ctf_type_resolve(ctf, mbid);
					kind = ctf_type_kind(ctf, mbid);

					if (kind != CTF_K_INTEGER &&
					    kind != CTF_K_FLOAT &&
					    kind != CTF_K_ENUM)
						return dt_ctf_set_errno(
							dtp, ECTF_NOTINTFP);

					mbid = ctf_add_slice(ctf,
							     CTF_ADD_NONROOT,
							     mbid, &enc);
					if (mbid == CTF_ERR)
						return dt_ctf_error(dtp, ctf);

					/* Fall-through */
				}

				/* Fall-through - add as regular member. */
			}

			err = ctf_add_member_offset(ctf, ctfid, mname, mbid,
						    item->offset);
			if (err == CTF_ERR)
				return dt_ctf_error(dtp, ctf);
		}

		return ctfid;
	}
	case BTF_KIND_FUNC_PROTO: {
		int		i;
		ctf_id_t	argv[vlen];
		btf_param_t	*item = (btf_param_t *)(type + 1);
		ctf_funcinfo_t	ctc;

		ctc.ctc_flags = 0;
		ctc.ctc_argc = vlen;
		ctc.ctc_return = dt_btf_add_to_ctf(dtp, btf, ctf, type->type);
		if (ctc.ctc_return == CTF_ERR)
			return CTF_ERR;

		for (i = 0; i < vlen; i++) {
			argv[i] = dt_btf_add_to_ctf(dtp, btf, ctf, item->type);
			if (argv[i] == CTF_ERR)
				return CTF_ERR;
		}

		ctfid = ctf_add_function(ctf, CTF_ADD_ROOT, &ctc, argv);
		btf->ctfids[type_id] = ctfid;

		return ctfid == CTF_ERR ? dt_ctf_error(dtp, ctf) : ctfid;
	}
	case BTF_KIND_FWD: {
		uint32_t	kind;

		kind = BTF_INFO_KFLAG(type->info) ? CTF_K_UNION : CTF_K_STRUCT;
		ctfid = ctf_add_forward(ctf, CTF_ADD_ROOT, name, kind);
		btf->ctfids[type_id] = ctfid;

		return ctfid == CTF_ERR ? dt_ctf_error(dtp, ctf) : ctfid;
	}
	case BTF_KIND_TYPE_TAG: {
		/*
		 * A type tag (e.g. __rcu, __user, __percpu) is a transparent
		 * annotation on the type it wraps.  CTF has no equivalent, so
		 * we resolve straight through to the underlying type, just as
		 * we do for const/volatile/restrict.  Treating it as ignored
		 * (mapping it to void) would leave every tagged member -- such
		 * as task_struct.real_parent (__rcu) -- resolving to "void",
		 * which breaks translator compilation on kernels whose BTF
		 * carries type tags.
		 */
		ctfid = dt_btf_add_to_ctf(dtp, btf, ctf, type->type);
		if (ctfid == CTF_ERR)
			return CTF_ERR;		/* errno already set */

		btf->ctfids[type_id] = ctfid;

		return ctfid;
	}
	case BTF_KIND_VAR:
	case BTF_KIND_DATASEC:
	case BTF_KIND_DECL_TAG:
	case BTF_KIND_ENUM64:
	case BTF_KIND_FUNC:
		return btf->ctfids[0];		/* Ignored for CTF */
	default:
		return dt_ctf_error(dtp, ctf);
	}

	assert(0);
}

static ctf_dict_t *
dt_btf_to_ctf(dtrace_hdl_t *dtp, dt_module_t *dmp, dt_btf_t *btf)
{
	int		i, base = 0;
	ctf_dict_t	*ctf;
	ctf_encoding_t	enc = { CTF_INT_SIGNED, 0, 0 };

	ctf = ctf_create(&dtp->dt_ctferr);
	if (ctf == NULL)
		return NULL;

	ctf_setmodel(ctf, dtp->dt_conf.dtc_ctfmodel);

	/*
	 * All modules import 'shared_ctf'.  The 'vmlinux' module is handled as
	 * a special case because it simply imports 'shared_ctf'.
	 */
	if (dmp) {
		int	err;

		err = ctf_import(ctf, dt_module_shared_ctf(dtp));
		if (err == CTF_ERR ||
		    ctf_parent_name_set(ctf, "shared_ctf") == CTF_ERR) {
			dt_ctf_error(dtp, ctf);
			ctf_close(ctf);
			return NULL;
		}

		ctf_setspecific(ctf, dmp);

		if (strcmp(dmp->dm_name, "vmlinux") == 0)
			goto out;
	}

	if (!btf)
		goto out;

	/*
	 * If we were not able to read the BTF data, and we are constructing
	 * CTF for "shared_ctf", we need space for the default 'void' type.
	 */
	if (!dmp && !btf->type_cnt)
		btf->type_cnt = 1;

	btf->ctfids = dt_calloc(dtp, btf->type_cnt, sizeof(ctf_id_t));
	for (i = 1; i < btf->type_cnt; i++)
		btf->ctfids[i] = CTF_ERR;

	/*
	 * If we are constructing 'shared_ctf' (no module), we need to add an
	 * entry for the implicit 'void' BTF type (id 0).
	 */
	if (!dmp) {
		btf->ctfids[0] = ctf_add_integer(ctf, CTF_ADD_ROOT, "void",
						 &enc);
		if (btf->ctfids[0] == CTF_ERR)
			dt_dprintf("Could not create 'void' CTF entry.\n");
	} else {
		/*
		 * Any module other than 'vmlinux' inherits the types from
		 * 'vmlinux'.
		 * Shared types are 1 through btf->parent->type_cnt - 1
		 * (base).
		 * A module's types are base through base + btf->type_cnt - 1,
		 * but the types are stored in the BTF types array with indexes
		 * 1 through btf->type_cnt - 1.
		 */
		if (btf->parent != NULL && btf->parent->type_cnt > 0)
			base = btf->parent->type_cnt - 1;
	}

	for (i = 1; i < btf->type_cnt; i++) {
		int	type_id = i + base;

		if (dt_btf_add_to_ctf(dtp, btf, ctf, type_id) == CTF_ERR) {
			dt_dprintf("BTF-to-CTF error: %s\n",
				   ctf_errmsg(dtp->dt_ctferr));
			break;
		}
	}

out:
	if (ctf_update(ctf) == CTF_ERR) {
		dt_ctf_error(dtp, ctf);
		ctf_close(ctf);
		return NULL;
	}

	return ctf;
}
#endif

ctf_dict_t *
dt_btf_module_ctf(dtrace_hdl_t *dtp, dt_module_t *dmp)
{
	ctf_dict_t	*ctf = NULL;
#ifdef HAVE_LIBCTF
	dt_btf_t	*btf = dmp->dm_btf;
#endif

	/* If we already have CTF data, return it. */
	if (dmp->dm_ctfp)
		return dmp->dm_ctfp;

#ifdef HAVE_LIBCTF
	/*
	 * The shared module owns the dictionary generated from vmlinux BTF.
	 * vmlinux itself, like every other module, imports this dictionary.
	 */
	if (dmp == dtp->dt_exec && dt_module_shared_ctf(dtp) == NULL) {
		dt_module_t *shared = dt_module_create(dtp, "shared_ctf");

		if (shared == NULL) {
			dt_set_errno(dtp, EDT_NOMEM);
			return NULL;
		}
		shared->dm_ctfp = dt_btf_to_ctf(dtp, NULL, btf);
		if (shared->dm_ctfp == NULL)
			return NULL;
		ctf_setspecific(shared->dm_ctfp, shared);
		shared->dm_flags |= DT_DM_LOADED;
	}

	ctf = dt_btf_to_ctf(dtp, dmp, btf);

	dt_dprintf("Generated %s CTF from BTF (%d types).\n", dmp->dm_name,
		   btf->type_cnt);
#endif

	return ctf;
}

static uint32_t
dt_btf_lookup_name_kind2(const dt_btf_t *btf, int32_t base, const char *name,
			 uint32_t kind)
{
	uint32_t	i;

	/* Look through the type data for a match. */
	for (i = 1; i < btf->type_cnt; i++) {
		const btf_type_t	*type = btf->types[i];
		const char		*str;

		if (BTF_INFO_KIND(type->info) != kind)
			continue;

		str = dt_btf_get_string(btf, type->name_off);
		if (str == NULL)
			continue;
		if (strcmp(name, str) == 0)
			return i + base;
	}

	return BTF_TYPE_NONE;
}

uint32_t
dt_btf_lookup_name_kind(dtrace_hdl_t *dtp, dt_module_t *dmp, const char *name,
			uint32_t kind)
{
	dt_btf_t	*btf;
	const dt_btf_t	*parent;
	int32_t		base = 0, id;

	if (kind == BTF_KIND_UNKN)
		return BTF_TYPE_NONE;
	if (strcmp(name, "void") == 0)
		return 0;

	/* Get BTF data for the module. */
	btf = dt_btf_load_module(dtp, dmp);

	/* No type lookup without BTF data. */
	if (!btf)
		return BTF_TYPE_NONE;

	/*
	 * BTF type data for a module is split between two BTF data source:
	 *   shared	[ 0 .. parent->type_cnt - 1 ]
	 *   local	base + [ 1 .. type_cnt ]
	 *   			with base = parent->type_cnt - 1
	 */
	parent = btf->parent;
	if (parent != NULL && parent->type_cnt > 0)
		base = parent->type_cnt - 1;

	id = dt_btf_lookup_name_kind2(btf, base, name, kind);
	if (id == BTF_TYPE_NONE && parent != NULL)
		id = dt_btf_lookup_name_kind2(parent, 0, name, kind);

	return id;
}

int
dt_btf_get_module_ids(dtrace_hdl_t *dtp)
{
	uint32_t	id = 0;

	for (;;) {
		int		rc, fd;
		btf_info_t	info;
		char		name[PATH_MAX];
		uint32_t	size = sizeof(info);
		dt_module_t	*dmp;

		rc = dt_bpf_btf_get_next_id(id, &id);
		if (rc) {
			if (errno == EPERM)
				return 0;	/* Not supported. */
			if (errno == ENOENT)
				return 0;	/* Done. */

			return dt_set_errno(dtp, errno);
		}

		fd = dt_bpf_btf_get_fd_by_id(id);
		if (fd < 0) {
			if (errno == ENOENT)
				continue;	/* Module got unloaded? */

			return dt_set_errno(dtp, errno);
		}

		memset(&info, 0, size);
		info.name = (uint64_t)name;
		info.name_len = sizeof(name);
		rc = dt_bpf_btf_get_info_by_fd(fd, &info, &size);
		close(fd);
		if (rc)
			return dt_set_errno(dtp, errno);

		dmp = dt_module_lookup_by_name(dtp, name);
		if (dmp == NULL || dmp->dm_btf_id != 0)
			continue;

		dmp->dm_btf_id = id;
	}
}

int
dt_btf_module_fd(const dt_module_t *dmp)
{
	return dmp->dm_btf_id ? dt_bpf_btf_get_fd_by_id(dmp->dm_btf_id) : 0;
}

int
dt_btf_func_argc(const dt_btf_t *btf, uint32_t id)
{
	btf_type_t	*type = dt_btf_real_type_by_id(btf, id);

	if (type == NULL)
		return -1;

	if (BTF_INFO_KIND(type->info) != BTF_KIND_FUNC_PROTO)
		return -1;

	return BTF_INFO_VLEN(type->info);
}

int
dt_btf_func_is_void(const dt_btf_t *btf, uint32_t id)
{
	btf_type_t	*type = dt_btf_real_type_by_id(btf, id);

	if (type != NULL &&
	    BTF_INFO_KIND(type->info) == BTF_KIND_FUNC_PROTO && type->type == 0)
		return 1;

	return 0;
}

/*
 * Return 1 if the function referenced by the BTF id can be traced using the
 * fprobe/fexit facility.  Specifically, that the function is not variadic and
 * that none of its arguments exceeds the acceptable value size (16 bytes).
 * (See btf_distill_func_proto() in kernel/bpf/btf.c for reference.)
 */
#define MAX_BPF_FUNC_REG_REGS	5
int
dt_btf_func_is_traceable(const dt_btf_t *btf, uint32_t id)
{
	btf_type_t	*type = dt_btf_real_type_by_id(btf, id);
	int		argc;

	/* If no prototype is found, BPF fprobes do not work. */
	if (!type || BTF_INFO_KIND(type->info) != BTF_KIND_FUNC_PROTO)
		return 0;

	/* If the return type is a struct or union, BPF fprobes do not work. */
	if (type->type != 0) {
		btf_type_t	*rtype = dt_btf_real_type_by_id(btf, type->type);

		if (rtype == NULL)
			return 0;

		switch (BTF_INFO_KIND(rtype->info)) {
		case BTF_KIND_STRUCT:
		case BTF_KIND_UNION:
			return 0;
		}
	}

	/*
	 * BPF fprobes do not support functions that are variadic or that have
	 * any arguments passed by value that are a struct or union of size
	 * greater than 16.
	 */
	argc = BTF_INFO_VLEN(type->info);
	if (argc > 0) {
		btf_param_t	*args = (btf_param_t *)(type + 1);
		int		i;

		if (args[argc - 1].type == 0)		/* variadic */
			return 0;

		for (i = 0; i < argc; i++) {
			type = dt_btf_real_type_by_id(btf, args[i].type);
			if (type == NULL)
				return 0;

			switch (BTF_INFO_KIND(type->info)) {
			case BTF_KIND_STRUCT:
			case BTF_KIND_UNION:
				if (type->size > 16)	/* value size > 16 */
					return 0;
			}
		}
	}

	return 1;
}
