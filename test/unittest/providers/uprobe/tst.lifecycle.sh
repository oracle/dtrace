#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# ASSERTION: A system-wide uprobe can be enabled, fired, detached, and enabled
# again repeatedly for the same ELF object.
#
# @@timeout: 45

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1
DIRNAME="$tmpdir/uprobe-lifecycle.$$.$RANDOM"
mkdir -p "$DIRNAME/lib"
cd "$DIRNAME" || exit 1

cat > probe.c <<'EOF'
__attribute__((noinline, visibility("default")))
long
life_probe(long value)
{
	return value + 1;
}
EOF

cat > worker.c <<'EOF'
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

extern long life_probe(long);

static int
wait_for_gate(const char *path)
{
	char ch;
	int fd = open(path, O_RDONLY);

	if (fd == -1)
		return 1;
	if (read(fd, &ch, 1) == -1 && errno != EINTR)
		return 1;
	close(fd);
	return 0;
}

int
main(int argc, char **argv)
{
	long value, got;

	if (argc != 3)
		return 2;

	value = strtol(argv[1], NULL, 0);
	if (wait_for_gate(argv[2]) != 0)
		return 3;

	got = life_probe(value);

	return got == value + 1 ? 0 : 4;
}
EOF

${CC:-cc} $test_cppflags -fPIC -shared -o lib/libswlife.so probe.c || exit 1
${CC:-cc} $test_cppflags -Llib -o worker worker.c -lswlife || exit 1
export LD_LIBRARY_PATH="$PWD/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

dtrace_pid=
worker_pid=
cleanup()
{
	[ -n "$dtrace_pid" ] && kill "$dtrace_pid" 2>/dev/null
	[ -n "$worker_pid" ] && kill "$worker_pid" 2>/dev/null
}
trap cleanup EXIT

i=1
while [ $i -le 5 ]; do
	gate="gate.$i"
	mkfifo "$gate"
	./worker "$i" "$gate" &
	worker_pid=$!

	$dtrace $dt_flags -qn '
uprobe:libswlife.so:life_probe:entry
{
	printf("%ld\n", arg0);
	exit(arg0 != $1);
}

profile:::tick-8s
{
	exit(1);
}
' $i > "dtrace.$i.out" 2> "dtrace.$i.err" &
	dtrace_pid=$!

	sleep 2
	printf x > "$gate"

	if ! wait "$worker_pid"; then
		cat "dtrace.$i.err"
		cat "dtrace.$i.out"
		exit 1
	fi
	worker_pid=

	if ! wait "$dtrace_pid"; then
		cat "dtrace.$i.err"
		cat "dtrace.$i.out"
		exit 1
	fi
	dtrace_pid=

	if ! grep -qx "$i" "dtrace.$i.out"; then
		echo "ERROR: iteration $i did not report the expected argument"
		cat "dtrace.$i.err"
		cat "dtrace.$i.out"
		exit 1
	fi

	i=$((i + 1))
done

echo success
exit 0
