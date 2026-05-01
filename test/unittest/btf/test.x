#!/bin/bash

if [ ! -r /sys/kernel/btf/vmlinux ]; then
	echo "No vmlinux BTF data found."
	exit 2
fi

exit 0
