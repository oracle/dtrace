#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

#
# ASSERTION: An empty trailing entry in PATH or LD_LIBRARY_PATH denotes the
# current directory when resolving a userspace module.
#

dtrace=$1
dir=$tmpdir/scope-user-path-empty.$$

mkdir -p "$dir" || exit 1
trap 'rm -rf "$dir"' EXIT
cp test/triggers/testprobe "$dir/testprobe" || exit 1
cd "$dir" || exit 1

# Make sure . (current directory) is not part of PATH or LD_LIBRARY_PATH.
# TODO

# Now add an empty training entry.
PATH=$PATH: LD_LIBRARY_PATH=$LD_LIBRARY_PATH: "$dtrace" -xlinkmode=static $dt_flags -c testprobe -n '
dtrace:::BEGIN
{
	trace(&testprobe``main);
	exit(0);
}'
