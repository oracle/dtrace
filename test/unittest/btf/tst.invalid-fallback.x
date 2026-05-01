#!/bin/bash

if [ ! -r /lib/modules/$(uname -r)/kernel/vmlinux.ctfa ]; then
	echo "No vmlinux CTF archive found."
	exit 2
fi

exit 0
