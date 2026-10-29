#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2023, 2026, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

dtrace=$1

DIRNAME="$tmpdir/modpath.$$.$RANDOM"
mkdir -p $DIRNAME
cd $DIRNAME
# Exercise CTF archive lookup without falling back to BTF.
dt_flags="$dt_flags -xbtfpath=none"

# Create the directory where the CTF archive will end up.
mkdir -p mydirectory/kernel

# Basic check that DTrace works.
$dtrace $dt_flags                       -lvP rawtp > basic.out
if [ $? -ne 0 ]; then
	echo ERROR: basic check on DTrace failed
	cat basic.out
	exit 1
else
	echo basic check on DTrace passed
fi

# Without CTF or BTF, listing rawtp probes must fail with missing type data.
if $dtrace $dt_flags -xmodpath=mydirectory -lvP rawtp > empty.out 2> empty.err; then
	echo ERROR: empty check on DTrace unexpectedly succeeded
	cat empty.out empty.err
	exit 1
elif ! grep -Fq 'No type data (CTF or BTF) found for vmlinux' empty.err; then
	echo ERROR: empty check on DTrace failed for an unexpected reason
	cat empty.out empty.err
	exit 1
else
	echo empty check on DTrace passed
fi

# Link a CTF archive to our directory.
ln -s /lib/modules/$(uname -r)/kernel/vmlinux.ctfa mydirectory/kernel/vmlinux.ctfa

# Verify that modpath now works.
$dtrace $dt_flags -xmodpath=mydirectory -lvP rawtp > final.out
if [ $? -ne 0 ]; then
	echo ERROR: final check on DTrace failed
	cat final.out
	exit 1
else
	echo final check on DTrace passed
fi

# Check the amount of CTF info we got from each run by seeing how many
# rawtp args have non-trivial type info.  Both nbasic and nfinal should be
# substantial and likely identical (but we allow a generous tolerance).

nbasic=`gawk '/^[	 ]*args\[/ && !/uint64_t/ && !/void \*/' basic.out | wc -l`
nfinal=`gawk '/^[	 ]*args\[/ && !/uint64_t/ && !/void \*/' final.out | wc -l`

if [ $nbasic -lt 1000 ]; then
	echo ERROR: basic check turned up little CTF info
	cat basic.out
	echo got nbasic $nbasic nfinal $nfinal
	exit 1
fi
if [ $nfinal -lt 1000 ]; then
	echo ERROR: final check turned up little CTF info
	cat final.out
	echo got nbasic $nbasic nfinal $nfinal
	exit 1
fi
if [ $nbasic -gt $(($nfinal + 200)) -o $nfinal -gt $((nbasic + 200)) ]; then
	echo $nbasic $nfinal out of range
	echo ==== basic.out:
	cat basic.out
	echo ==== final.out:
	cat final.out
	echo got nbasic $nbasic nfinal $nfinal
	exit 1
fi

echo all checks pass
exit 0
