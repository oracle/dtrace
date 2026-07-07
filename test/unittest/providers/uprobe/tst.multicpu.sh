#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# ASSERTION: A system-wide uprobe can fires on every CPU allowed by the current
#	     cpuset.

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1
DIRNAME="$tmpdir/uprobe-allcpus.$$.$RANDOM"
mkdir -p "$DIRNAME/lib"
cd "$DIRNAME" || exit 1

cat > probe.c <<'EOF'
#include <unistd.h>

__attribute__((noinline, visibility("default")))
void
cpu_probe(unsigned long value)
{
	__asm__ volatile("" : : "r" (value) : "memory");
}

void
run_probes(void)
{
	int i;

	for (i = 0; i < 100; i++) {
		cpu_probe((unsigned long)i);
		usleep(10000);
	}
}
EOF

cat > worker.c <<'EOF'
extern void run_probes(void);
int main(void) { run_probes(); return 0; }
EOF

${CC:-cc} $test_cppflags -fPIC -shared -o lib/libswcpu.so probe.c || exit 1
${CC:-cc} $test_cppflags -Llib -o worker worker.c -lswcpu || exit 1

export LD_LIBRARY_PATH="$PWD/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

# Expand the affinity list and choose up to two actual allowed CPUs.  This is
# cpuset-safe and intentionally does not assume that CPU 0 is available.
cpuspec=$(awk '/^Cpus_allowed_list:/ { print $2 }' /proc/self/status)
cpus=()
IFS=',' read -ra ranges <<< "$cpuspec"
for range in "${ranges[@]}"; do
	if [[ $range == *-* ]]; then
		lo=${range%-*}; hi=${range#*-}
		for ((cpu = lo; cpu <= hi; cpu++)); do
			cpus+=("$cpu")
		done
	else
		cpus+=("$range")
	fi
done

[ ${#cpus[@]} -gt 0 ] || { echo ERROR: no allowed CPUs; exit 1; }
[ ${#cpus[@]} -gt 2 ] || { echo SKIPPED: not enough CPUs; exit 2; }

# We do not want to use CPU 0 because that is specified in perf_event_open()
cpu_a=${cpus[1]}
cpu_b=${cpus[${#cpus[@]}-1]}

dtrace_pid=
workers=
cleanup()
{
	[ -n "$dtrace_pid" ] && kill "$dtrace_pid" 2>/dev/null
	[ -n "$workers" ] && kill $workers 2>/dev/null
}
trap cleanup EXIT

$dtrace $dt_flags -qn '
dtrace:::BEGIN {
	printf("1st CPU is %d\n2nd CPU is %d\n", $1, $2);
}

uprobe:libswcpu.so:cpu_probe:entry
/seen[cpu] == 0/
{
	seen[cpu] = 1;
	printf("cpu %d\n", cpu);
}

uprobe:libswcpu.so:cpu_probe:entry
/seen[$1] > 0 && seen[$2] > 0/
{
	exit(0);
}

profile:::tick-12s
{
	exit(1);
}
' $cpu_a $cpu_b > dtrace.out &
dtrace_pid=$!

sleep 2
taskset -c "$cpu_a" ./worker & w1=$!; workers="$workers $w1"
if [ "$cpu_b" != "$cpu_a" ]; then
	taskset -c "$cpu_b" ./worker & w2=$!; workers="$workers $w2"
fi

wait "$w1" || exit 1
if [ -n "${w2:-}" ]; then
	wait "$w2" || exit 1
fi
workers=

mv $DIRNAME /tmp/check-it

if ! wait "$dtrace_pid"; then
	cat dtrace.out
	exit 1
fi
dtrace_pid=

cat dtrace.out
for cpu in "$cpu_a" "$cpu_b"; do
	if ! grep -qx "cpu $cpu" dtrace.out; then
		echo "ERROR: no uprobe fired on allowed CPU $cpu"
		exit 1
	fi
done

exit 0
