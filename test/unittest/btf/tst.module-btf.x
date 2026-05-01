#!/bin/bash

for path in /sys/kernel/btf/*; do
	[ "$path" = /sys/kernel/btf/vmlinux ] && continue
	[ -r "$path" ] || continue
	module=${path##*/}
	if [[ $module =~ ^[A-Za-z_][A-Za-z0-9_]*$ ]]; then
		exit 0
	fi
done

echo "No suitable loaded kernel module with BTF data."
exit 2
