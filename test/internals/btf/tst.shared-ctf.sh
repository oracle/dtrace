#!/bin/bash
#
# Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# ASSERTION: A shared CTF module owns archive, BTF, and embedded dictionaries,
#            survives module discovery, and reports missing type data.
#

dir=$tmpdir/shared-ctf.$$
mkdir -p "$dir" || exit 1

test/triggers/shared-ctf "$dir"
