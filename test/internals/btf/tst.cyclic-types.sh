#!/bin/bash
#
# Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# ASSERTION: Cyclic BTF type references fail resolution, while a finite
#            chain still resolves to its function prototype.
#

test/triggers/btf-cyclic-types
rc=$?

echo "Result: $rc"

[[ $rc -eq 0 ]] || exit 1

exit 0
