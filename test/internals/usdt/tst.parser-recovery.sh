#!/bin/bash
# Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
# ASSERTION: dtprobed replaces a dead or stalled parser and can subsequently
#            register and fire a valid USDT probe.
# @@tags: uninstalled
# @@timeout: 60

# Check again when invoked directly, without runtest.sh's .x processing.
test/internals/usdt/tst.parser-recovery.x || exit 67
dtrace=$1
dir="$tmpdir/parser-recovery.$$"
mkdir -p "$dir" || exit 1
old_parser=

cleanup()
{
	# Do not leave a stopped parser behind if an assertion fails.
	if [[ -n $old_parser ]] &&
	    [[ $(awk '/^PPid:/ {print $2}' "/proc/$old_parser/status" 2>/dev/null) == "$dtprobed_pid" ]]; then
		kill -CONT "$old_parser" 2>/dev/null
	fi
	rm -rf "$dir"
}
trap cleanup EXIT
trap 'exit 1' HUP INT TERM

cat > "$dir/prov.d" <<'EOF'
provider recovery {
	probe go();
};
EOF
cat > "$dir/main.c" <<'EOF'
#include "prov.h"
int main(void)
{
	RECOVERY_GO();
	return 0;
}
EOF

"$dtrace" $dt_flags -h -o "$dir/prov.h" -s "$dir/prov.d" || exit 1
${CC:-cc} $test_cppflags -c -o "$dir/main.o" "$dir/main.c" || exit 1
"$dtrace" $dt_flags -G -o "$dir/prov.o" -s "$dir/prov.d" "$dir/main.o" || exit 1
${CC:-cc} $test_ldflags -o "$dir/victim" "$dir/main.o" "$dir/prov.o" || exit 1

check_probe()
{
	"$dtrace" $dt_flags -q -c "$dir/victim" -n '
recovery$target:::go { seen++; }
dtrace:::END { exit(seen != 1); }'
}

parser_child()
{
	local children
	read -ra children < "/proc/$dtprobed_pid/task/$dtprobed_pid/children"
	if [[ ${#children[@]} != 1 ]]; then
		echo "expected one private parser child, found ${#children[@]}" >&2
		return 1
	fi
	echo "${children[0]}"
}

# Establish that probe compilation and registration work before injecting faults.
check_probe || exit 1
for signal in KILL STOP; do
	old_parser=$(parser_child) || exit 1
	kill -"$signal" "$old_parser" || exit 1
	# An initial registration may fail while the daemon detects a broken pipe.
	# A silent, stopped parser exercises the reply timeout and retry instead.
	# The constructor's status is not evidence of successful registration; the
	# subsequent DTrace invocation must actually observe the probe firing.
	if ! timeout --signal=TERM --kill-after=2 15 "$dir/victim" > "$dir/register.out" 2>&1; then
		echo "registration hung after parser SIG$signal"
		cat "$dir/register.out"
		exit 1
	fi
	kill -0 "$dtprobed_pid" || exit 1
	new_parser=$(parser_child) || exit 1
	if [[ $new_parser == "$old_parser" ]]; then
		echo "dtprobed did not replace parser after SIG$signal"
		exit 1
	fi
	old_parser=
	check_probe || exit 1
done
