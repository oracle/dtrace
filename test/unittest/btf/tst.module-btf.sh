#!/bin/bash
#
# ASSERTION: Split module BTF loads with vmlinux as its parent and converts
#            into a CTF container usable for scoped parent type lookup.
#

dtrace=$1
dir=$tmpdir/btf-module.$$
mkdir -p "$dir" || exit 1
ln -s /sys/kernel/btf/vmlinux "$dir/vmlinux" || exit 1

module=
for path in /sys/kernel/btf/*; do
	[ "$path" = /sys/kernel/btf/vmlinux ] && continue
	[ -r "$path" ] || continue
	name=${path##*/}
	[[ $name =~ ^[A-Za-z_][A-Za-z0-9_]*$ ]] || continue
	module=$name
	ln -s "$path" "$dir/$module" || exit 1
	break
done

if [ -z "$module" ]; then
	echo "No suitable loaded kernel module with BTF data."
	exit 1
fi

"$dtrace" $dt_flags -e -xdebug -xctfpath="$dir/missing.ctfa" \
	-xbtfpath="$dir" -n "
BEGIN
{
	this->size = sizeof(struct ${module}\`task_struct);
}" >"$dir/output" 2>&1
rc=$?
if [ "$rc" -ne 0 ]; then
	cat "$dir/output"
	exit 1
fi

if ! grep -q "BTF file $dir/$module: [1-9][0-9]* types" "$dir/output" ||
   ! grep -q "Generated $module CTF from BTF" "$dir/output"; then
	cat "$dir/output"
	exit 1
fi

exit 0
