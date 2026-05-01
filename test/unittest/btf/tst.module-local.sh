#!/bin/bash
#
# ASSERTION: A module-local BTF type converts to CTF and the module also
#            inherits types from its vmlinux BTF parent.
#

dtrace=$1
dir=$tmpdir/btf-module-local.$$
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
[ -n "$module" ] || exit 1

# BTF name offsets in split data follow the parent string table.  Build a
# single local struct whose name is absent from the real kernel BTF.
python3 - "$dir/$module" <<'PY' || exit 1
import struct
import sys

with open('/sys/kernel/btf/vmlinux', 'rb') as parent:
    header = parent.read(24)
magic, version, flags, header_len, type_off, type_len, str_off, str_len = \
    struct.unpack('<HBBIIIII', header)
if magic != 0xeb9f or version != 1:
    raise ValueError('unsupported parent BTF header')

name = b'\0dtrace_btf_local_test\0'
type_data = struct.pack('<III', str_len + 1, 4 << 24, 8)
child_header = struct.pack('<HBBIIIII', 0xeb9f, 1, 0, 24,
                           0, len(type_data), len(type_data), len(name))
with open(sys.argv[1], 'wb') as child:
    child.write(child_header + type_data + name)
PY

"$dtrace" $dt_flags -e -xdebug -xctfpath="$dir/missing.ctfa" \
	-xbtfpath="$dir" -n "
BEGIN
{
	this->local_size = sizeof(struct ${module}\`dtrace_btf_local_test);
	this->parent_size = sizeof(struct ${module}\`task_struct);
}" >"$dir/output" 2>&1
rc=$?
if [ "$rc" -ne 0 ]; then
	cat "$dir/output"
	exit 1
fi

if ! grep -q "BTF file $dir/$module: 2 types" "$dir/output" ||
   ! grep -q "Generated $module CTF from BTF (2 types)" "$dir/output"; then
	cat "$dir/output"
	exit 1
fi

exit 0
