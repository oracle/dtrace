#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# ASSERTION: Exact and globbed system-wide uprobe descriptions create probes
# with canonical entry and return names.
#

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1
DIRNAME="$tmpdir/uprobe-probespec.$$.$RANDOM"
mkdir -p "$DIRNAME/lib"
cd "$DIRNAME" || exit 1

cat > probe.c <<'EOF'
__attribute__((noinline, visibility("default")))
long named_probe(long value) { return value + 1; }
EOF
${CC:-cc} $test_cppflags -fPIC -shared -o lib/libswpspec.so probe.c || exit 1
export LD_LIBRARY_PATH="$PWD/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

check()
{
	spec=$1
	shift

	if ! $dtrace $dt_flags -l -n "$spec" > list.out 2> list.err; then
		cat list.err
		exit 1
	fi

	awk '$2 == "uprobe" && $3 == "libswpspec.so" &&
	    $4 == "named_probe" { print $5 }' list.out | sort -u > names.out
	printf '%s\n' "$@" | sort -u > expected.out
	if ! diff -u expected.out names.out; then
		echo "ERROR: wrong probes for $spec"
		cat list.err
		cat list.out
		exit 1
	fi
}

check 'uprobe:libswpspec.so:named_probe:entry' entry
check 'uprobe:libswpspec.so:named_probe:return' return
check 'uprobe:libswpspec.so:named_probe:e*' entry
check 'uprobe:libswpspec.so:named_probe:ret*' return
check 'uprobe:libswpspec.so:named_probe:*' entry return
check 'uprobe:libswpspec.so:named_probe:' entry return

for spec in \
    'uprobe:libswpspec.so:named_probe:offset' \
    'uprobe:libswpspec.so:no_such_function:entry' \
    'uprobe:no_such_module.so:named_probe:entry'; do
	if $dtrace $dt_flags -e -n "$spec" > /dev/null 2>&1; then
		echo "ERROR: unexpectedly matched $spec"
		exit 1
	fi
done

echo success
exit 0
