/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Test the kinds of probe definition clause grammar rules.
 *
 * SECTION: Program Structure/Probe Clauses and Declarations;
 * 	Program Structure/Actions
 */


dtrace:::BEGIN
{}

dtrace:::BEGIN
/1/
{}

profile:::tick-1
{
	exit(0);
}

