#!/bin/bash
# Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
# ASSERTION: Periodic test targets select the intended suites and preserve
#            output comparisons, both in-tree and in the installed makefile.

dir="$tmpdir/check-targets.$$"
mkdir -p "$dir" || exit 1
trap 'rm -rf "$dir"' EXIT
makecheck="$PWD/Makecheck"
export result="$dir/args"
cat > "$dir/runner" <<'EOF'
#!/bin/bash
printf '%s\n' "$@" > "$result"
EOF
chmod +x "$dir/runner" || exit 1

for mode in built installed; do
	{
		[[ $mode == installed ]] && echo 'always-installed := t'
		printf 'all:\n.git-version:\ninclude %s\n' "$makecheck"
	} > "$dir/driver.mk"
	for group in expensive stress-compare all; do
		case $group in
		    expensive) suites=expensive ;;
		    stress-compare) suites=unittest,internals,demo,smoke ;;
		    all) suites=unittest,internals,stress,demo,smoke,expensive ;;
		esac
		for verbose in '' verbose-; do
			target="check-$verbose$group"
			if ! make -s --no-print-directory -f "$dir/driver.mk" \
			    RUNTEST="$dir/runner" "$target" > "$dir/make.out" 2>&1; then
				cat "$dir/make.out"
				exit 1
			fi
			if ! grep -qx -- "--testsuites=$suites" "$result" ||
			    ! grep -qx -- '--comparison' "$result" ||
			    grep -qx -- '--no-comparison' "$result"; then
				echo "$mode $target selected incorrect arguments:"
				cat "$result"
				exit 1
			fi
			if [[ $mode == installed ]] && ! grep -qx -- '--use-installed' "$result"; then
				echo "$target lost installed-mode selection"
				exit 1
			fi
			if [[ -z $verbose ]] && ! grep -qx -- '--quiet' "$result"; then
				echo "$target lost quiet-mode selection"
				exit 1
			fi
		done
	done
done
