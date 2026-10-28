/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *  Test assigning a variable two different incompatible types.
 *
 * SECTION:  Variables/Clause-Local Variables
 *
 */

dtrace:::BEGIN
{
	this->x = curthread;
	this->x = pid;
}
