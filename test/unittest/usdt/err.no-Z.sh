#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2024, 2026, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# This test verifies that dtrace will not wait for USDT processes to
# appear if -Z is not used.

dtrace=$1

$dtrace $dt_flags -qn '
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
