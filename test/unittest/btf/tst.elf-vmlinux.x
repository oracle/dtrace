#!/bin/bash

if ! command -v "${OBJCOPY:-objcopy}" >/dev/null 2>&1; then
	echo "objcopy is needed to create an ELF BTF fixture."
	exit 2
fi

exit 0
