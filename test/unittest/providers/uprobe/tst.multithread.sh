#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# ASSERTION: System-wide uprobe entry and return probes preserve per-thread
# self state under concurrent calls from multiple threads.
#
# @@timeout: 45

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1
NTHREADS=4
NITER=20
EXPECTED=$((NTHREADS * NITER))
DIRNAME="$tmpdir/uprobe-multithread.$$.$RANDOM"
mkdir -p "$DIRNAME/lib"
cd "$DIRNAME" || exit 1

cat > probe.c <<'EOF'
__attribute__((noinline, visibility("default")))
long
thread_probe(long thread_id, long iter)
{
	return thread_id * 100000 + iter;
}
EOF

cat > worker.c <<EOF
#include <pthread.h>
#include <stdint.h>
#include <unistd.h>

#define NTHREADS $NTHREADS
#define NITER $NITER

extern long thread_probe(long, long);

static void *
run_thread(void *arg)
{
	long id = (long)(intptr_t)arg;
	long i;

	for (i = 0; i < NITER; i++) {
		long got = thread_probe(id, i);

		if (got != id * 100000 + i)
			return (void *)1;
		usleep(1000);
	}

	return NULL;
}

int
main(void)
{
	pthread_t threads[NTHREADS];
	long i;

	for (i = 0; i < NTHREADS; i++) {
		if (pthread_create(&threads[i], NULL, run_thread,
		    (void *)(intptr_t)i) != 0)
			return 2;
	}

	for (i = 0; i < NTHREADS; i++) {
		void *status = NULL;

		if (pthread_join(threads[i], &status) != 0 || status != NULL)
			return 3;
	}

	return 0;
}
EOF

${CC:-cc} $test_cppflags -fPIC -shared -o lib/libswthread.so probe.c || exit 1
${CC:-cc} $test_cppflags -Llib -o worker worker.c -lswthread -pthread || exit 1
export LD_LIBRARY_PATH="$PWD/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

dtrace_pid=
worker_pid=
cleanup()
{
	[ -n "$dtrace_pid" ] && kill "$dtrace_pid" 2>/dev/null
	[ -n "$worker_pid" ] && kill "$worker_pid" 2>/dev/null
}
trap cleanup EXIT

$dtrace $dt_flags -qn '
dtrace:::BEGIN
{
	entries = 0;
	bad = 0;
	tids = 0;
}

uprobe:libswthread.so:thread_probe:entry
{
	entries++;
	self->active = 1;
	self->expected = arg0 * 100000 + arg1;
}

uprobe:libswthread.so:thread_probe:entry
/seen[tid] == 0/
{
	seen[tid] = 1;
	tids++;
}

uprobe:libswthread.so:thread_probe:return
/self->active/
{
	returns++;
	bad += arg1 != self->expected;
	self->active = 0;
}

uprobe:libswthread.so:thread_probe:return
/returns == $1/
{
	exit(bad != 0 || entries != $1 || tids != $2);
}

profile:::tick-20s
{
	exit(1);
}
' $EXPECTED $NTHREADS > dtrace.out 2> dtrace.err &
dtrace_pid=$!

sleep 2
./worker &
worker_pid=$!

if ! wait "$worker_pid"; then
	cat dtrace.err
	cat dtrace.out
	exit 1
fi
worker_pid=

if ! wait "$dtrace_pid"; then
	cat dtrace.err
	cat dtrace.out
	exit 1
fi
dtrace_pid=

echo success
exit 0
