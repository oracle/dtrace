#!/bin/bash
#
# ASSERTION: Invalid raw BTF is rejected and a usable CTF archive is loaded.
#

dtrace=$1
dir=$tmpdir/btf-invalid-fallback.$$
mkdir -p "$dir" || exit 1

# Preserve the BTF magic, but truncate the file at the end of its header.
# This enters the raw BTF loader and fails header validation.
head -c 24 /sys/kernel/btf/vmlinux >"$dir/vmlinux" || exit 1

"$dtrace" $dt_flags -e -xdebug -xbtfpath="$dir" -n '
BEGIN
{
	this->size = sizeof(struct vmlinux`task_struct);
}' >"$dir/output" 2>&1
rc=$?
if [ "$rc" -ne 0 ]; then
	cat "$dir/output"
	exit 1
fi

if ! grep -q "Cannot decode BTF data $dir/vmlinux" "$dir/output" ||
   ! grep -q 'Loaded shared CTF from archive' "$dir/output"; then
	cat "$dir/output"
	exit 1
fi

exit 0
