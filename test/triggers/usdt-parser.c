/*
 * Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <usdt_parser.h>

#define CHECK(x) do { if (!(x)) { \
	fprintf(stderr, "line %d: %s (errno %d)\n", __LINE__, #x, errno); \
	exit(1); \
} } while (0)

static void
replies(void)
{
	union {
		dof_parsed_t align;
		unsigned char bytes[512];
	} storage;
	unsigned char *buf = storage.bytes;
	dof_parsed_t *msg = &storage.align, *reply;
	int fds[2], status, type;
	size_t sizes[] = { DIT_PROVIDER_HEADSZ, DIT_PROBE_HEADSZ,
		DIT_TRACEPOINT_HEADSZ, DIT_ERR_HEADSZ, DIT_EOF_HEADSZ,
		DIT_ARGS_NATIVE_HEADSZ, DIT_ARGS_XLAT_HEADSZ, DIT_ARGS_MAP_HEADSZ };
	pid_t child;

	/* Every message kind: valid minimum, short body, missing terminator. */
	for (type = DIT_PROVIDER; type <= DIT_ARGS_MAP; type++) {
		int bad;
		for (bad = 0; bad < 3; bad++) {
			memset(buf, 0, sizeof(storage.bytes));
			msg->type = type;
			msg->size = sizes[type] + (type != DIT_EOF);
			if (bad == 1)
				msg->size--;
			if (bad == 2 && type != DIT_EOF && type != DIT_ARGS_MAP)
				buf[msg->size - 1] = 'x';
			CHECK(pipe(fds) == 0);
			CHECK(usdt_parser_write_one(fds[1], buf, msg->size) == 0);
			close(fds[1]);
			errno = 0;
			reply = usdt_parser_host_read(fds[0], 1);
			if (bad == 1 || (bad == 2 && type != DIT_EOF && type != DIT_ARGS_MAP))
				CHECK(reply == NULL && errno == EPROTO);
			else
				CHECK(reply != NULL && reply->type == type);
			free(reply);
			close(fds[0]);
		}
	}

	/* Invalid size/type and truncated frames, including a partial size. */
	for (type = 0; type < 5; type++) {
		memset(buf, 0, sizeof(storage.bytes));
		msg->size = type == 0 ? DIT_BASE_HEADSZ - 1 :
		    type == 1 ? (size_t)DIT_MAX_SIZE + 1 : sizeof(*msg);
		msg->type = (dof_parsed_info_t)99;
		CHECK(pipe(fds) == 0);
		CHECK(usdt_parser_write_one(fds[1], buf,
		    type == 3 ? sizeof(size_t) - 1 : type == 4 ? sizeof(size_t) : sizeof(*msg)) == 0);
		close(fds[1]);
		reply = usdt_parser_host_read(fds[0], 1);
		CHECK(reply == NULL);
		close(fds[0]);
	}

	/* A large fragmented frame must not consume the next frame. */
	memset(buf, 'x', sizeof(storage.bytes));
	msg->size = sizeof(storage.bytes);
	msg->type = DIT_PROVIDER;
	msg->provider.nprobes = 0;
	buf[sizeof(storage.bytes) - 1] = 0;
	CHECK(pipe(fds) == 0);
	CHECK((child = fork()) >= 0);
	if (child == 0) {
		dof_parsed_t eof = { .size = DIT_EOF_HEADSZ, .type = DIT_EOF };
		size_t i;
		close(fds[0]);
		for (i = 0; i < sizeof(storage.bytes); i++) {
			CHECK(usdt_parser_write_one(fds[1], buf + i, 1) == 0);
			usleep(100);
		}
		CHECK(usdt_parser_write_one(fds[1], &eof, eof.size) == 0);
		close(fds[1]);
		_exit(0);
	}
	close(fds[1]);
	reply = usdt_parser_host_read(fds[0], 2);
	CHECK(reply && reply->size == sizeof(storage.bytes) &&
	    memcmp(reply, buf, sizeof(storage.bytes)) == 0);
	free(reply);
	reply = usdt_parser_host_read(fds[0], 2);
	CHECK(reply && reply->type == DIT_EOF);
	free(reply);
	CHECK(usdt_parser_host_read(fds[0], 1) == NULL);
	close(fds[0]);
	CHECK(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0);

	/* Keep the writer open and silent: poll timeout must not enter read(). */
	CHECK(pipe(fds) == 0);
	alarm(5);
	errno = 0;
	CHECK(usdt_parser_host_read(fds[0], 1) == NULL && errno == ETIMEDOUT);
	alarm(0);
	close(fds[0]);
	close(fds[1]);
	CHECK(signal(SIGPIPE, SIG_IGN) != SIG_ERR);
	CHECK(pipe(fds) == 0);
	close(fds[0]);
	CHECK(usdt_parser_write_one(fds[1], "x", 1) == -EPIPE);
	close(fds[1]);
}

static void
relocations(const dof_hdr_t *header)
{
	struct fixture {
		dof_hdr_t hdr;
		dof_sec_t sec[4];
		dof_relohdr_t reloc;
		dof_relodesc_t entry;
		uint64_t target;
		char strings[8];
	} good = { 0 }, data;
	dof_helper_t helper = { .dofhp_addr = 0x1000 };
	dof_parsed_t *reply;
	int i, fds[2];

	good.hdr = *header;
	good.hdr.dofh_secnum = 4;
	good.hdr.dofh_loadsz = good.hdr.dofh_filesz = sizeof(good);
	good.sec[0] = (dof_sec_t) { .dofs_type = DOF_SECT_STRTAB,
		.dofs_align = 1, .dofs_flags = DOF_SECF_LOAD,
		.dofs_offset = offsetof(struct fixture, strings), .dofs_size = 8 };
	good.sec[1] = (dof_sec_t) { .dofs_type = DOF_SECT_RELTAB,
		.dofs_align = 8, .dofs_flags = DOF_SECF_LOAD,
		.dofs_offset = offsetof(struct fixture, entry),
		.dofs_size = sizeof(dof_relodesc_t), .dofs_entsize = sizeof(dof_relodesc_t) };
	good.sec[2] = (dof_sec_t) { .dofs_type = DOF_SECT_PROFFS,
		.dofs_align = 8, .dofs_flags = DOF_SECF_LOAD,
		.dofs_offset = offsetof(struct fixture, target), .dofs_size = 8 };
	good.sec[3] = (dof_sec_t) { .dofs_type = DOF_SECT_URELHDR,
		.dofs_align = 4, .dofs_flags = DOF_SECF_LOAD,
		.dofs_offset = offsetof(struct fixture, reloc), .dofs_size = sizeof(dof_relohdr_t) };
	good.reloc = (dof_relohdr_t) { .dofr_strtab = 0, .dofr_relsec = 1, .dofr_tgtsec = 2 };
	good.entry.dofr_type = DOF_RELO_SETX;
	CHECK(pipe(fds) == 0);
	for (i = 0; i < 6; i++) {
		data = good;
		switch (i) {
		case 0: data.reloc.dofr_strtab = 4; break;
		case 1: data.reloc.dofr_relsec = 0; break;
		case 2: data.sec[1].dofs_entsize = 1; break;
		case 3: data.entry.dofr_offset = 8; break;
		case 4: data.entry.dofr_offset = 1; break;
		case 5: data.entry.dofr_type = UINT32_MAX; break;
		}
		CHECK(usdt_parse_dof(fds[1], &helper, &data.hdr) != 0);
		reply = usdt_parser_host_read(fds[0], 1);
		CHECK(reply && reply->type == DIT_ERR && reply->err.err_no == EINVAL);
		free(reply);
		data = good;
		CHECK(usdt_parse_dof(fds[1], &helper, &data.hdr) == 0);
		CHECK(data.target == helper.dofhp_addr);
	}
	close(fds[0]);
	close(fds[1]);
}

static void
malformed_dof(void)
{
	struct fixture {
		dof_hdr_t hdr;
		dof_sec_t sec;
		char strings[8];
	} good = { 0 }, data;
	dof_helper_t helper = { 0 };
	int i, fds[2];
	dof_parsed_t *reply;

	memcpy(good.hdr.dofh_ident, DOF_MAG_STRING, DOF_MAG_STRLEN);
	good.hdr.dofh_ident[DOF_ID_MODEL] = DOF_MODEL_LP64;
	good.hdr.dofh_ident[DOF_ID_ENCODING] = DOF_ENCODE_NATIVE;
	good.hdr.dofh_ident[DOF_ID_VERSION] = DOF_VERSION_3;
	good.hdr.dofh_ident[DOF_ID_DIFVERS] = DIF_VERSION_2;
	good.hdr.dofh_hdrsize = sizeof(dof_hdr_t);
	good.hdr.dofh_secsize = sizeof(dof_sec_t);
	good.hdr.dofh_secnum = 1;
	good.hdr.dofh_secoff = offsetof(struct fixture, sec);
	good.hdr.dofh_loadsz = good.hdr.dofh_filesz = sizeof(good);
	good.sec.dofs_type = DOF_SECT_STRTAB;
	good.sec.dofs_flags = DOF_SECF_LOAD;
	good.sec.dofs_align = 1;
	good.sec.dofs_offset = offsetof(struct fixture, strings);
	good.sec.dofs_size = sizeof(good.strings);

	CHECK(pipe(fds) == 0);
	for (i = 0; i < 10; i++) {
		data = good;
		switch (i) {
		case 0: data.hdr.dofh_loadsz = sizeof(dof_hdr_t) - 1; break;
		case 1: data.hdr.dofh_ident[DOF_ID_MAG0] = 0; break;
		case 2: data.hdr.dofh_ident[DOF_ID_VERSION] = 255; break;
		case 3: data.hdr.dofh_secsize = 1; break;
		case 4: data.hdr.dofh_secnum = UINT32_MAX; break;
		case 5: data.hdr.dofh_secoff++; break;
		case 6: data.sec.dofs_align = 3; break;
		case 7: data.sec.dofs_offset = sizeof(data) + 1; break;
		case 8: data.sec.dofs_size = UINT64_MAX; break;
		case 9: memset(data.strings, 'x', sizeof(data.strings)); break;
		}
		CHECK(usdt_parse_dof(fds[1], &helper, &data.hdr) != 0);
		reply = usdt_parser_host_read(fds[0], 1);
		CHECK(reply && reply->type == DIT_ERR && reply->err.err_no == EINVAL);
		free(reply);
		/* A bad request must not poison the next parse in the same process. */
		data = good;
		CHECK(usdt_parse_dof(fds[1], &helper, &data.hdr) == 0);
	}
	close(fds[0]);
	close(fds[1]);
	relocations(&good.hdr);
}

int
main(void)
{
	replies();
	malformed_dof();
	return 0;
}
