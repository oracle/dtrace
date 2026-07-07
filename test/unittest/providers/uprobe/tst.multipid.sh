#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# ASSERTION: A system-wide uprobe remains active for many short-lived
# processes created after tracing starts.
#
# @@timeout: 45

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1
NWORKERS=20
DIRNAME="$tmpdir/uprobe-multipid.$$.$RANDOM"
mkdir -p "$DIRNAME/lib"
cd "$DIRNAME" || exit 1

cat > probe.c <<'EOF'
__attribute__((noinline, visibility("default")))
long
churn_probe(long value)
{
	return value + 1;
}
EOF

cat > worker.c <<'EOF'
#include <stdlib.h>

extern long churn_probe(long);

int
main(int argc, char **argv)
{
	long value, got;

	if (argc != 2)
		return 2;

	value = strtol(argv[1], NULL, 0);
	got = churn_probe(value);

	return got == value + 1 ? 0 : 3;
}
EOF

${CC:-cc} $test_cppflags -fPIC -shared -o lib/libswchurn.so probe.c || exit 1
${CC:-cc} $test_cppflags -Llib -o worker worker.c -lswchurn || exit 1

export LD_LIBRARY_PATH="$PWD/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

dtrace_pid=
pids=
cleanup()
{
	[ -n "$dtrace_pid" ] && kill "$dtrace_pid" 2>/dev/null
	[ -n "$pids" ] && kill $pids 2>/dev/null
}
trap cleanup EXIT

$dtrace $dt_flags -qn '
uprobe:libswchurn.so:churn_probe:entry
/seen[pid] == 0/
{
	seen[pid] = 1;
	hits++;
	printf("%ld = PID %d\n", arg0, pid);
}

profile:::tick-1s
/hits >= $1/
{
	exit(0);
}

profile:::tick-20s
{
	exit(1);
}
' $NWORKERS > dtrace.out &
dtrace_pid=$!

sleep 2

i=0
while [ $i -lt $NWORKERS ]; do
	./worker "$i" &
	pids="$pids $!"
	i=$((i + 1))
done

for pid in $pids; do
	wait "$pid" || exit 1
done
pids=

if ! wait "$dtrace_pid"; then
	cat dtrace.out
	exit 1
fi
dtrace_pid=

cat dtrace.out
if [ "$(grep PID dtrace.out | sort -u | wc -l)" != "$NWORKERS" ]; then
	echo "ERROR: expected $NWORKERS distinct traced processes"
	cat dtrace.out
	exit 1
fi

echo success
exit 0
