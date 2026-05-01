#!/bin/bash
#
# ASSERTION: A kernel module without its own BTF inherits vmlinux types.
#

dtrace=$1
dir=$tmpdir/btf-module-parent.$$
mkdir -p "$dir" || exit 1
ln -s /sys/kernel/btf/vmlinux "$dir/vmlinux" || exit 1

module=
for path in /sys/kernel/btf/*; do
	[ "$path" = /sys/kernel/btf/vmlinux ] && continue
	[ -r "$path" ] || continue
	name=${path##*/}
	[[ $name =~ ^[A-Za-z_][A-Za-z0-9_]*$ ]] || continue
	module=$name
	break
done

if [ -z "$module" ]; then
	echo "No loaded kernel module with BTF data."
	exit 1
fi

# Deliberately leave $dir/$module absent.  This exercises the inherited
# parent-only BTF container, including its CTF import for scoped lookup.
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

if ! grep -q "Cannot open BTF file $dir/$module:" "$dir/output" ||
   ! grep -q "Generated $module CTF from BTF (0 types)" "$dir/output"; then
	cat "$dir/output"
	exit 1
fi

exit 0
