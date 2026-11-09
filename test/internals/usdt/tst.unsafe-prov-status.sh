#!/bin/bash
# Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
# ASSERTION: The unsafe-provider test fails on helper errors or invalid PIDs,
#            and reaches its filesystem assertion after a successful helper.

dir="$tmpdir/unsafe-prov-status.$$"
mkdir -p "$dir/stash" "$dir/probes" || exit 1
trap 'rm -rf "$dir"' EXIT
script="$PWD/test/unittest/usdt/err.unsafe-prov.sh"

# Replace compilation only: exercise the real test's result handling without
# requiring a helper device or issuing a hostile registration to the daemon.
cat > "$dir/cc" <<'EOF'
#!/bin/bash
cat > unsafe_prov <<'HELPER'
#!/bin/bash
case $helper_case in
    failure) exit 1 ;;
    empty) exit 0 ;;
    invalid) echo invalid ;;
    valid) echo 12345 ;;
    traversal) mkdir -p "$DTRACE_OPT_DOFSTASHPATH/unsafe12345"; echo 12345 ;;
esac
HELPER
chmod +x unsafe_prov
EOF
chmod +x "$dir/cc" || exit 1

for helper_case in failure empty invalid valid traversal; do
	export helper_case
	CC="$dir/cc" tmpdir="$dir" DTRACE_OPT_DOFSTASHPATH="$dir" \
	    bash "$script" > "$dir/out" 2>&1
	status=$?
	expected=0
	[[ $helper_case == valid ]] && expected=1
	if [[ $status != $expected ]] ||
	    { [[ $helper_case != valid ]] && grep -q 'All OK' "$dir/out"; }; then
		echo "incorrect result for helper case $helper_case: $status"
		cat "$dir/out"
		exit 1
	fi
done
