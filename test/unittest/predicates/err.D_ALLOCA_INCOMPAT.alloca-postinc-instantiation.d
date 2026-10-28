/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: alloca assignment to variables in predicates doesn't
 *	      interfere with alloca/nonalloca compatibility checking.
 */

#pragma D option quiet

dtrace:::BEGIN / (new++ == 0) ? bar = alloca(10) : bar /
{
	trace(new);
        bar = &`max_pfn;
}

dtrace:::ERROR { exit(1); }
