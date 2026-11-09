#!/bin/bash
# Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
# ASSERTION: Malformed raw and ELF BTF is rejected without losing the split
#            parent, and a subsequent valid file can still be decoded.
dir="$tmpdir/btf-malformed.$$"
mkdir -p "$dir" || exit 1
trap 'rm -rf "$dir"' EXIT
test/triggers/btf-malformed "$dir/btf"
