#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# ASSERTION: A system-wide uprobe fires for a library that is dlopen()ed only
# after tracing has started.
#
# @@timeout: 30

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1
DIRNAME="$tmpdir/uprobe-dlopen.$$.$RANDOM"
mkdir -p "$DIRNAME/lib"
cd "$DIRNAME" || exit 1

cat > probe.c <<'EOF'
__attribute__((noinline, visibility("default")))
long
lazy_probe(long value)
{
	return value * 3 + 1;
}
EOF

cat > worker.c <<'EOF'
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

typedef long (*probe_f)(long);

int
main(int argc, char **argv)
{
	char ch;
	int fd;
	long got;
	void *hdl;
	probe_f probe;

	if (argc != 2)
		return 2;

	fd = open(argv[1], O_RDONLY);
	if (fd == -1)
		return 3;
	if (read(fd, &ch, 1) == -1 && errno != EINTR)
		return 4;
	close(fd);

	hdl = dlopen("libswlazy.so", RTLD_NOW);
	if (hdl == NULL) {
		fprintf(stderr, "dlopen failed: %s\n", dlerror());
		return 5;
	}

	probe = (probe_f)dlsym(hdl, "lazy_probe");
	if (probe == NULL) {
		fprintf(stderr, "dlsym failed: %s\n", dlerror());
		return 6;
	}

	got = probe(17);
	dlclose(hdl);

	return got == 52 ? 0 : 7;
}
EOF

${CC:-cc} $test_cppflags -fPIC -shared -o lib/libswlazy.so probe.c || exit 1
if ! ${CC:-cc} $test_cppflags -o worker worker.c -ldl; then
	${CC:-cc} $test_cppflags -o worker worker.c || exit 1
fi
export LD_LIBRARY_PATH="$PWD/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
mkfifo gate

dtrace_pid=
worker_pid=
cleanup()
{
	[ -n "$dtrace_pid" ] && kill "$dtrace_pid" 2>/dev/null
	[ -n "$worker_pid" ] && kill "$worker_pid" 2>/dev/null
}
trap cleanup EXIT

./worker gate &
worker_pid=$!

# $dtrace $dt_flags -qn '
$dtrace $dt_flags -n '
uprobe:libswlazy.so:lazy_probe:entry
{
	self->pid = pid;
	self->expected = arg0 * 3 + 1;
	printf("E %ld\n", arg0);
}

uprobe:libswlazy.so:lazy_probe:return
/self->pid == pid/
{
	printf("R %ld (expected %ld)\n", arg1, self->expected);
	exit(self->expected == arg1 ? 0 : 1);
}

profile:::tick-5s
{
	exit(1);
}
' > dtrace.out 2> dtrace.err &
dtrace_pid=$!

sleep 2
printf x > gate

wait "$worker_pid" || exit 1
worker_pid=

if ! wait "$dtrace_pid"; then
	cat dtrace.err
	cat dtrace.out
	exit 1
fi
dtrace_pid=

echo success
exit 0
