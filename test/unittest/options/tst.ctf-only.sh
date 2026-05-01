#!/bin/bash
#
# ASSERTION: Kernel types still load from the CTF archive when BTF is disabled.
#

dtrace=$1
dir=$tmpdir/ctf-only.$$
mkdir -p "$dir" || exit 1

"$dtrace" $dt_flags -e -xdebug -xbtfpath=none -n '
BEGIN
{
	this->size = sizeof(struct vmlinux`task_struct);
}' >"$dir/output" 2>&1
rc=$?
if [ "$rc" -ne 0 ]; then
	cat "$dir/output"
	exit 1
fi

if ! grep -q 'Loaded shared CTF from archive' "$dir/output" ||
   grep -q 'Generated vmlinux CTF from BTF' "$dir/output"; then
	cat "$dir/output"
	exit 1
fi

exit 0
