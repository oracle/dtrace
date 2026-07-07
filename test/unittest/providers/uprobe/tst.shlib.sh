#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# ASSERTION: A system-wide uprobe probe in a shared library works.
#
# @@timeout: 45

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1
DIRNAME="$tmpdir/uprobe-shlib.$$.$RANDOM"
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

if ! $dtrace $dt_flags -c "./worker 17" -qn '
uprobe:libswchurn.so:churn_probe:entry
{
	printf("%ld = PID %d\n", arg0, pid);
	exit(arg0 != 17);
}

profile:::tick-1s
{
	exit(1);
}
' > dtrace.out 2> dtrace.err; then
	cat dtrace.err
	cat dtrace.out
	exit 1
fi

if ! grep -Eq '^17 = PID [0-9]+$' dtrace.out; then
	echo "ERROR: shared-library probe did not report the expected argument"
	cat dtrace.err
	cat dtrace.out
	exit 1
fi

echo success
exit 0
