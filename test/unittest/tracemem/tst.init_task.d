/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */


/*
 * ASSERTION:
 *  Test tracemem() by tracing out the contents of the initial
 *  task_struct as a raw stream of bytes.
 *
 * SECTION: Actions and Subroutines/tracemem()
 */

dtrace:::BEGIN
{
	i = 1;
}

profile:::tick-1
/i != 5/
{
	tracemem((void *)&`init_task, 20);
	i++;
}

profile:::tick-1
/i == 5/
{
	exit(0);
}
