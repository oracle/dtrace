#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

#
# ASSERTION: Created and attached targets expose libraries selected by the
# loader through RPATH/RUNPATH, including $ORIGIN and indirect dependencies.
#

dtrace=$1
dir=$tmpdir/scope-user-library-path.$$
CC=${CC:-/usr/bin/gcc}
pid=

cleanup()
{
	if [[ -n $pid ]]; then
		kill "$pid" 2>/dev/null
		wait "$pid" 2>/dev/null
	fi
	rm -rf "$dir"
}

mkdir -p "$dir/other" || exit 1
trap cleanup EXIT

cat > "$dir/child.c" <<'EOF'
int child_symbol(void) { return 0; }
EOF

cat > "$dir/parent.c" <<'EOF'
extern int child_symbol(void);
int parent_symbol(void) { return child_symbol(); }
EOF

cat > "$dir/main.c" <<'EOF'
#include <stdio.h>
#include <unistd.h>
extern int parent_symbol(void);
int main(int argc, char **argv)
{
	FILE *fp;
	if (parent_symbol() != 0)
		return 1;
	if (argc > 1) {
		if ((fp = fopen(argv[1], "w")) == NULL)
			return 1;
		fclose(fp);
		for (;;)
			pause();
	}
	return 0;
}
EOF

# These competing files must not override the loader's actual choices.
cp /bin/true "$dir/other/scope_parent.so" || exit 1
cp /bin/true "$dir/other/scope_child.so" || exit 1
export PATH="$dir/other:$PATH"

for mode in runpath rpath; do
	base=$dir/$mode
	mkdir -p "$base/bin" "$base/lib/deps" || exit 1
	"$CC" -fPIC -shared -Wl,-soname,scope_child.so \
	    -o "$base/lib/deps/scope_child.so.1.0" "$dir/child.c" || exit 1
	ln -s scope_child.so.1.0 "$base/lib/deps/scope_child.so" || exit 1

	# RUNPATH must be supplied by each parent.  RPATH on the executable
	# also finds the indirect dependency without a path on the parent.
	parent_flags=()
	if [[ $mode == runpath ]]; then
		parent_flags=(-Wl,--enable-new-dtags,-rpath,'$ORIGIN/deps')
		main_flags=(-Wl,--enable-new-dtags,-rpath,'$ORIGIN/../lib')
	else
		main_flags=(-Wl,--disable-new-dtags
		    -Wl,-rpath,'$ORIGIN/../lib:$ORIGIN/../lib/deps')
	fi
	"$CC" -fPIC -shared -Wl,-soname,scope_parent.so \
	    "${parent_flags[@]}" -o "$base/lib/scope_parent.so.1.0" \
	    "$dir/parent.c" -L"$base/lib/deps" -l:scope_child.so || exit 1
	ln -s scope_parent.so.1.0 "$base/lib/scope_parent.so" || exit 1
	"$CC" -o "$base/bin/program" "$dir/main.c" -L"$base/lib" \
	    -l:scope_parent.so -Wl,-rpath-link,"$base/lib/deps" \
	    "${main_flags[@]}" || exit 1

	cat > "$dir/check.d" <<'EOF'
dtrace:::BEGIN
{
	parent = &scope_parent.so``parent_symbol;
	child = &scope_child.so``child_symbol;
}
EOF
	"$dtrace" $dt_flags -e -xlinkmode=static -c "$base/bin/program" \
	    -s "$dir/check.d" || exit 1

	"$base/bin/program" "$base/ready" &
	pid=$!
	for ((i = 0; i < 100; i++)); do
		[[ -f $base/ready ]] && break
		kill -0 "$pid" 2>/dev/null || exit 1
		sleep 0.1
	done
	[[ -f $base/ready ]] || exit 1
	"$dtrace" $dt_flags -e -xlinkmode=static -p "$pid" \
	    -s "$dir/check.d" || exit 1
	kill "$pid"
	wait "$pid" 2>/dev/null
	pid=
done
