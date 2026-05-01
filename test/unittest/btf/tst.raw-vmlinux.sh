#!/bin/bash
#
# ASSERTION: Raw vmlinux BTF is loaded and converted to usable CTF when the
#            CTF archive is unavailable, even if the host has an archive.
#

dtrace=$1
dir=$tmpdir/btf-raw-vmlinux.$$
mkdir -p "$dir" || exit 1
ln -s /sys/kernel/btf/vmlinux "$dir/vmlinux" || exit 1

"$dtrace" $dt_flags -e -xdebug -xctfpath="$dir/missing.ctfa" \
	-xbtfpath="$dir" -n '
BEGIN
{
	this->size = sizeof(struct vmlinux`task_struct);
}' >"$dir/output" 2>&1
rc=$?
if [ "$rc" -ne 0 ]; then
	cat "$dir/output"
	exit 1
fi

if ! grep -q "BTF file $dir/vmlinux: [1-9][0-9]* types" "$dir/output" ||
   ! grep -q 'Generated vmlinux CTF from BTF' "$dir/output"; then
	cat "$dir/output"
	exit 1
fi

exit 0
