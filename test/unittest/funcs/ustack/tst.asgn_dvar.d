/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Test assignment of ustack() to a dynamic variable (assoc element).
 */

/* @@trigger: ustack-tst-basic */

pid$target:a.out:myfunc_z:entry
{
	assoc["a"] = ustack(3);
	printf("%k", assoc["a"]);

	exit(0);
}

dtrace:::ERROR
{
	exit(1);
}
