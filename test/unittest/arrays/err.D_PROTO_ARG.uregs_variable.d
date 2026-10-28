/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: uregs[] must have a constant index.
 *
 * SECTION: User Process Tracing/uregs Array
 */

dtrace:::BEGIN
{
	i = R_SP;
	trace(uregs[i]);
	exit(1);
}
