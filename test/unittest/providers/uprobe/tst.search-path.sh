#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# ASSERTION: Userspace modules are resolved in LD_LIBRARY_PATH/PATH order,
# including empty path components, the current directory, and relative search
# paths.
#

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1
DIRNAME="$tmpdir/uprobe-search-path.$$.$RANDOM"
mkdir -p "$DIRNAME/A" "$DIRNAME/B" "$DIRNAME/bin" "$DIRNAME/sub"
cd "$DIRNAME" || exit 1

cat > a.c <<'EOF'
__attribute__((noinline, visibility("default"))) void from_a(void) {}
EOF
cat > b.c <<'EOF'
__attribute__((noinline, visibility("default"))) void from_b(void) {}
EOF
cat > cwd.c <<'EOF'
__attribute__((noinline, visibility("default"))) void from_cwd(void) {}
EOF
cat > path.c <<'EOF'
__attribute__((noinline, visibility("default"))) void from_path(void) {}
int main(void) { from_path(); return 0; }
EOF
cat > relative.c <<'EOF'
__attribute__((noinline, visibility("default"))) void from_relative(void) {}
EOF

${CC:-cc} $test_cppflags -fPIC -shared -o A/libchoice.so a.c || exit 1
${CC:-cc} $test_cppflags -fPIC -shared -o B/libchoice.so b.c || exit 1
${CC:-cc} $test_cppflags -fPIC -shared -o libcwd.so cwd.c || exit 1
${CC:-cc} $test_cppflags -o bin/swpathprog path.c || exit 1
${CC:-cc} $test_cppflags -fPIC -shared -o sub/librelative.so relative.c || exit 1

# Retain the runner's path to libdtrace after the test's search directories.
match()
{
	env_ld="$1${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
	env_path=$2
	spec=$3
	if ! LD_LIBRARY_PATH="$env_ld" PATH="$env_path" \
	    "$dtrace" $dt_flags -e -n "$spec" > dtrace.out 2> dtrace.err; then
		echo "ERROR: did not resolve $spec"
		cat dtrace.err
		exit 1
	fi
}

no_match()
{
	env_ld="$1${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
	env_path=$2
	spec=$3
	if LD_LIBRARY_PATH="$env_ld" PATH="$env_path" \
	    "$dtrace" $dt_flags -e -n "$spec" > dtrace.out 2> dtrace.err; then
		echo "ERROR: unexpectedly resolved $spec"
		exit 1
	fi
}

sys_path=${PATH:-/usr/bin:/bin}

# Search order must select one of two objects with the same module name.
match "$PWD/A:$PWD/B" "$sys_path" 'uprobe:libchoice.so:from_a:entry'
no_match "$PWD/A:$PWD/B" "$sys_path" 'uprobe:libchoice.so:from_b:entry'
match "$PWD/B:$PWD/A" "$sys_path" 'uprobe:libchoice.so:from_b:entry'
no_match "$PWD/B:$PWD/A" "$sys_path" 'uprobe:libchoice.so:from_a:entry'

# Empty components denote '.', and duplicate components are harmless.
match ":$PWD/A::$PWD/A" "$sys_path" 'uprobe:libcwd.so:from_cwd:entry'

# Executables can be found through PATH.
match "" "$PWD/bin:$sys_path" 'uprobe:swpathprog:from_path:entry'

# Relative search paths resolve modules by their basename.
no_match "" "$sys_path" 'uprobe:librelative.so:from_relative:entry'
match "sub" "$sys_path" 'uprobe:librelative.so:from_relative:entry'

echo success
exit 0
