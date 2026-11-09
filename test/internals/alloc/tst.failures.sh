#!/bin/bash
# Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
# ASSERTION: Allocation failures during BTF loading/conversion and module
#            range growth preserve ownership and permit subsequent retry.
dir="$tmpdir/allocation-failures.$$"
mkdir -p "$dir" || exit 1
trap 'rm -rf "$dir"' EXIT
test/triggers/allocation-failures "$dir/btf"
