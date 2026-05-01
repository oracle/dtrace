#!/bin/bash

if ! command -v python3 >/dev/null 2>&1; then
	echo "Python 3 is needed to generate split BTF data."
	exit 2
fi

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
