#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# ASSERTION: System-wide uprobes use correct file offsets for shared objects,
# PIE executables, and non-PIE executables when the toolchain supports them.
#
# @@timeout: 45

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1
DIRNAME="$tmpdir/uprobe-elf-offsets.$$.$RANDOM"
mkdir -p "$DIRNAME/lib"
cd "$DIRNAME" || exit 1

cat > libprobe.c <<'EOF'
__attribute__((noinline, visibility("default")))
long
lib_offset_probe(long value)
{
	return value + 101;
}
EOF

cat > libworker.c <<'EOF'
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

extern long lib_offset_probe(long);

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
	if (wait_for_gate(argv[1]) != 0)
		return 3;

	value = strtol(argv[2], NULL, 0);
	got = lib_offset_probe(value);

	return got == value + 101 ? 0 : 4;
}
EOF

cat > pieprog.c <<'EOF'
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

__attribute__((noinline, visibility("default")))
long
pie_offset_probe(long value)
{
	return value + 202;
}

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
	if (wait_for_gate(argv[1]) != 0)
		return 3;

	value = strtol(argv[2], NULL, 0);
	got = pie_offset_probe(value);

	return got == value + 202 ? 0 : 4;
}
EOF

cat > nopieprog.c <<'EOF'
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

__attribute__((noinline, visibility("default")))
long
nopie_offset_probe(long value)
{
	return value + 303;
}

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
	if (wait_for_gate(argv[1]) != 0)
		return 3;

	value = strtol(argv[2], NULL, 0);
	got = nopie_offset_probe(value);

	return got == value + 303 ? 0 : 4;
}
EOF

${CC:-cc} $test_cppflags -fPIC -shared -o lib/libuoffset.so libprobe.c || exit 1
${CC:-cc} $test_cppflags -Llib -o libworker libworker.c -luoffset || exit 1
${CC:-cc} $test_cppflags -fPIE -pie -o pieprog pieprog.c || exit 1

have_nopie=0
if ${CC:-cc} $test_cppflags -fno-pie -no-pie -o nopieprog nopieprog.c; then
	have_nopie=1
else
	echo "non-PIE executable build failed: non-PIE subtest skipped"
fi

export LD_LIBRARY_PATH="$PWD/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export PATH="$PWD${PATH:+:$PATH}"
mkfifo gate

cat > trace.d <<'EOF'
uprobe:libuoffset.so:lib_offset_probe:entry
{
	libhit = 1;
	printf("lib %d %ld\n", pid, arg0);
}

uprobe:pieprog:pie_offset_probe:entry
{
	piehit = 1;
	printf("pie %d %ld\n", pid, arg0);
}
EOF

if [ "$have_nopie" = 1 ]; then
	cat >> trace.d <<'EOF'

uprobe:nopieprog:nopie_offset_probe:entry
{
	nopiehit = 1;
	printf("nopie %d %ld\n", pid, arg0);
}
EOF
fi

cat >> trace.d <<EOF

profile:::tick-1s
EOF
if [ "$have_nopie" = 1 ]; then
	cat >> trace.d <<'EOF'
/libhit && piehit && nopiehit/
EOF
else
	cat >> trace.d <<'EOF'
/libhit && piehit/
EOF
fi
cat >> trace.d <<'EOF'
{
	exit(0);
}

profile:::tick-12s
{
	exit(1);
}
EOF

dtrace_pid=
pids=
cleanup()
{
	[ -n "$dtrace_pid" ] && kill "$dtrace_pid" 2>/dev/null
	[ -n "$pids" ] && kill $pids 2>/dev/null
}
trap cleanup EXIT

./libworker gate 10 & pids="$pids $!"
./pieprog gate 20 & pids="$pids $!"
if [ "$have_nopie" = 1 ]; then
	./nopieprog gate 30 & pids="$pids $!"
fi

$dtrace $dt_flags -qs trace.d > dtrace.out 2> dtrace.err &
dtrace_pid=$!

sleep 2
expected=2
[ "$have_nopie" = 1 ] && expected=3
i=0
while [ $i -lt $expected ]; do
	printf x
	i=$((i + 1))
done > gate

for pid in $pids; do
	wait "$pid" || exit 1
done
pids=

if ! wait "$dtrace_pid"; then
	cat dtrace.err
	cat dtrace.out
	exit 1
fi
dtrace_pid=

for name in lib pie; do
	if ! grep -q "^$name " dtrace.out; then
		echo "ERROR: missing $name uprobe hit"
		cat dtrace.err
		cat dtrace.out
		exit 1
	fi
done
if [ "$have_nopie" = 1 ] && ! grep -q '^nopie ' dtrace.out; then
	echo "ERROR: missing nopie uprobe hit"
	cat dtrace.err
	cat dtrace.out
	exit 1
fi

echo success
exit 0
