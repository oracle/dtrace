#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# ASSERTION: System-wide uprobes accept defined function symbols in stripped
# DSOs, and reject data, undefined, and nonexistent symbols.
#

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1
DIRNAME="$tmpdir/uprobe-symbols.$$.$RANDOM"
mkdir -p "$DIRNAME/lib"
cd "$DIRNAME" || exit 1

cat > symbols.c <<'EOF'
extern void undefined_function(void);
__attribute__((visibility("default"))) long exported_data = 123;
__attribute__((noinline, visibility("default"))) long exported_function(void)
{
	if (exported_data == -1)
		undefined_function();
	return exported_data;
}
EOF

${CC:-cc} $test_cppflags -fPIC -shared -o lib/libswsymbols.so symbols.c || exit 1
${STRIP:-strip} --strip-unneeded lib/libswsymbols.so || exit 1
export LD_LIBRARY_PATH="$PWD/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

if ! $dtrace $dt_flags -e -n \
    'uprobe:libswsymbols.so:exported_function:entry' >/dev/null 2>&1; then
	echo ERROR: exported function was not found in stripped DSO
	exit 1
fi

for symbol in exported_data undefined_function does_not_exist; do
	if $dtrace $dt_flags -e -n \
	    "uprobe:libswsymbols.so:$symbol:entry" >/dev/null 2>&1; then
		echo "ERROR: non-function or undefined symbol $symbol was accepted"
		exit 1
	fi
done

echo success
exit 0
