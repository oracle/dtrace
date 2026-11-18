#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2026, d-Matrix Corporation. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# Check how libproc classifies a root process without a terminal when it
# does not ask systemd: a standard stream to a regular file outside /var
# means it is not a system daemon, one under /var does not.
#
# libproc asks systemd whenever /run/systemd/system exists, and for a
# process in a systemd slice, as this one is, the slice decides and these
# rules are never reached.  So on a systemd system, dtrace runs under
# unshare -m, in a mount namespace of its own, with a tmpfs mounted over
# /run/systemd: libproc does not find systemd and uses these rules.  The
# namespace is made private first, so the rest of the system still sees
# /run/systemd.
#

dtrace=$1

DIRNAME=$tmpdir/system-daemon-files.$$.$RANDOM
mkdir -p $DIRNAME
cd $DIRNAME

if [[ $DIRNAME == /var/* ]]; then
	echo "$DIRNAME is under /var"
	exit 1
fi
VARFILE=$(mktemp /var/tmp/system-daemon-files.XXXXXX) || exit 1
trap "rm -f $VARFILE" EXIT

# main PIDFILE: leave the terminal's session, write the pid to PIDFILE and
# sleep for a minute
cat > main.c <<EOT
#include <stdio.h>
#include <unistd.h>

int main(int argc, char **argv)
{
	FILE *f;
	int i;

	if (setsid() < 0 || (f = fopen(argv[1], "w")) == NULL)
		return 1;
	fprintf(f, "%d\n", getpid());
	fclose(f);
	for (i = 0; i < 600; i++)
		usleep(100000);
	return 0;
}
EOT
$CC -o main main.c || exit 1

# run_dtrace DTRACE ARGS: run dtrace, with /run/systemd hidden if it exists
run_dtrace() {
	if [ -d /run/systemd/system ]; then
		unshare -m sh -c 'mount --make-rprivate / &&
		    mount -t tmpfs none /run/systemd && exec "$@"' sh "$@"
	else
		"$@"
	fi
}

# verdict LABEL FILE: run main with stdout to FILE and print LABEL and
# libproc's verdict
verdict() {
	local pid out

	rm -f pid
	./main pid < /dev/null > $2 2> /dev/null &
	for i in $(seq 50); do
		[ -s pid ] && break
		sleep 0.1
	done
	pid=$(cat pid)

	out=$(DTRACE_DEBUG=t run_dtrace $dtrace $dt_flags \
	    -ln "pid$pid:a.out:main:entry" 2>&1)
	if grep -q " $pid is a system daemon process" <<< "$out"; then
		echo "$1: system daemon"
	elif grep -q "^ *[0-9]* *pid$pid " <<< "$out"; then
		echo "$1: not a system daemon"
	else
		echo "$1: no verdict"
		grep -v DEBUG <<< "$out" | tail -2
	fi
	kill $pid
}

verdict "outside /var" $DIRNAME/out.txt
verdict "under /var" $VARFILE

exit 0
