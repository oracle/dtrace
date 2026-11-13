#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

#
# ASSERTION: A command's executable is available as a userspace module even
# when its directory is outside the module search path.
#

dtrace=$1
dir=$tmpdir/scope-user-command.$$

mkdir -p "$dir/target" "$dir/other" || exit 1
trap 'rm -rf "$dir"' EXIT
cp test/triggers/testprobe "$dir/target/scope.command" || exit 1
cp /bin/true "$dir/other/scope.command" || exit 1
ln -s target/scope.command "$dir/command-link" || exit 1
cd "$dir" || exit 1

# The competing executable has no dtrace_scope_test_type.  Finding it instead
# of the command's executable must fail type lookup.
export PATH="$dir/other:/usr/bin:/bin"

check()
{
	"$dtrace" $dt_flags -e -xlinkmode=static -c "$1" -n '
dtrace:::BEGIN
{
	symbol = &scope.command``main;
	type = (struct scope.command``dtrace_scope_test_type *)NULL;
}' || exit 1
}

check ./target/scope.command
check target/scope.command
check "$dir/target/scope.command"
check ./command-link

cd other || exit 1
check ../target/scope.command

# Also exercise execvp() lookup through PATH, using a relative PATH entry.
export PATH="../target:$PATH"
check scope.command
