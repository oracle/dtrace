#!/bin/bash

# Skip test if taskset is not available.
if ! command -v taskset > /dev/null; then
	echo "taskset not found, skipping test"
	exit 2
fi

exit 0
