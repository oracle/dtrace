#!/bin/bash
# Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.

# Fault injection is restricted to the daemon and stash owned by this run.
if [[ -z $dtprobed_pid || $test_libdir == installed ||
      $DTRACE_OPT_DOFSTASHPATH != "$tmpdir/"* ||
      $(readlink "/proc/$dtprobed_pid/exe") != "$PWD/build/dtprobed" ]]; then
	echo "requires the test runner's private dtprobed"
	exit 2
fi
exit 0
