/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Basic test for translators in /usr/lib/dtrace/procfs.d
 *
 * SECTION: Translators/Translator Declarations
 * SECTION: Translators/Translate Operator
 * SECTION: Translators/Process Model Translators
 *
 */

#pragma D option quiet

dtrace:::BEGIN
{
	mypr_addr = xlate < psinfo_t > (curthread).pr_addr;
	printf("pr_addr: %p", mypr_addr);
	exit(0);
}

dtrace:::ERROR
{
	exit(1);
}
