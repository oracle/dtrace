#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

#
# ASSERTION: A dotted userspace module name resolves both external symbols
# and types.
#

dtrace=$1
dir=$tmpdir/scope-user-dotted.$$

mkdir -p "$dir" || exit 1
trap 'rm -rf "$dir"' EXIT
cp test/triggers/testprobe "$dir/scope.user" || exit 1
cd "$dir" || exit 1

PATH=.: "$dtrace" $dt_flags -e -xlinkmode=static -c scope.user -n '
dtrace:::BEGIN
{
	symbol = &scope.user``main;
	type = (struct scope.user``dtrace_scope_test_type *)NULL;
	exit(0);
}'
