#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# ASSERTION: A system-wide uprobe fires for existing and newly-created,
# unrelated processes, and supplies entry arguments and return values.
#
# @@timeout: 30

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1
DIRNAME="$tmpdir/uprobe-systemwide.$$.$RANDOM"
mkdir -p "$DIRNAME/lib"
cd "$DIRNAME" || exit 1

cat > probe.c <<'EOF'
__attribute__((noinline, visibility("default")))
long
sw_probe(long a, long b, long c, long d, long e, long f)
{
	return a + b + c + d + e + f;
}
EOF

cat > worker.c <<'EOF'
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

extern long sw_probe(long, long, long, long, long, long);

int
main(int argc, char **argv)
{
	char ch;
	long base, got;
	int fd;

	if (argc != 3)
		return 2;

	fd = open(argv[1], O_RDONLY);
	if (fd == -1)
		return 3;
	if (read(fd, &ch, 1) == -1 && errno != EINTR)
		return 4;
	close(fd);

	base = strtol(argv[2], NULL, 0);
	got = sw_probe(base, base + 1, base + 2, base + 3, base + 4,
	    base + 5);

	return got == base * 6 + 15 ? 0 : 5;
}
EOF

${CC:-cc} $test_cppflags -fPIC -shared -o lib/libswuprobe.so probe.c || exit 1
${CC:-cc} $test_cppflags -Llib -o worker worker.c -lswuprobe || exit 1

export LD_LIBRARY_PATH="$PWD/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
mkfifo gate

pids=
dtrace_pid=
cleanup()
{
	[ -n "$dtrace_pid" ] && kill "$dtrace_pid" 2>/dev/null
	[ -n "$pids" ] && kill $pids 2>/dev/null
}
trap cleanup EXIT

# These workers exist before DTrace attaches.  They are unrelated to DTrace
# and wait until the tracing process has had time to enable its perf events.
./worker gate 10 & p1=$!; pids="$pids $p1"
./worker gate 20 & p2=$!; pids="$pids $p2"

$dtrace $dt_flags -qn '
dtrace:::BEGIN
{
	bad = 0;
}

uprobe:libswuprobe.so:sw_probe:entry
{
	entries++;
	self->expected = arg0 + arg1 + arg2 + arg3 + arg4 + arg5;
	bad += arg1 != arg0 + 1 || arg2 != arg0 + 2 ||
	    arg3 != arg0 + 3 || arg4 != arg0 + 4 || arg5 != arg0 + 5;
	bad += probemod != "libswuprobe.so" || probefunc != "sw_probe" ||
	    probename != "entry";
	printf("E %d %d %ld\n", pid, tid, self->expected);
}

/* A second clause must share the underlying uprobe and still be called. */
uprobe:libswuprobe.so:sw_probe:entry
{
	extra_entries++;
}

uprobe:libswuprobe.so:sw_probe:return
{
	returns++;
	bad += arg1 != self->expected;
	self->expected = 0;
	printf("R %d %d %ld\n", pid, tid, arg1);
}

uprobe:libswuprobe.so:sw_probe:return
/ returns == 3 /
{
	exit(bad != 0 || entries != 3 || extra_entries != 3);
}

profile:::tick-12s
{
	exit(1);
}
' > dtrace.out 2> dtrace.err &
dtrace_pid=$!

sleep 2
printf 'xx' > gate

# This worker is created only after the probe has been enabled.
./worker /dev/null 30 & p3=$!; pids="$pids $p3"

wait "$p1" || exit 1
wait "$p2" || exit 1
wait "$p3" || exit 1
pids=

if ! wait "$dtrace_pid"; then
	cat dtrace.err
	cat dtrace.out
	exit 1
fi
dtrace_pid=

if [ "$(awk '$1 == "E" { n++ } END { print n + 0 }' dtrace.out)" != 3 ] ||
   [ "$(awk '$1 == "R" { n++ } END { print n + 0 }' dtrace.out)" != 3 ] ||
   [ "$(awk '$1 == "E" { print $2 }' dtrace.out | sort -u | wc -l)" != 3 ]; then
	echo ERROR: expected one entry and return from each of three processes
	cat dtrace.err
	cat dtrace.out
	exit 1
fi

echo success
exit 0
