#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2024, 2026, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# This test verifies that dtrace will not run a destructive script
# for USDT probes if -w is not specified.

dtrace=$1

$dtrace $dt_flags -c test/triggers/usdt-tst-defer -qn '
dtrace:::BEGIN
{
	exit(0);
}

testprov*:::foo
{
	raise(SIGUSR1);
	exit(0);
}'
if [ $? -ne 0 ]; then
	echo expected failure
	exit 1
fi

echo unexpected success

exit 0
