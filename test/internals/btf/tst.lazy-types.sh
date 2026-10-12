#!/bin/bash
#
# Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# ASSERTION: BTF converts only requested types and their dependencies,
#            preserving recursive types, split BTF, and failure recovery.
#

test/triggers/btf-lazy-types
rc=$?
echo "Result: $rc"
[[ $rc -eq 0 ]] || exit 1
exit 0
