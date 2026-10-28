/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */


/*
 * ASSERTION:
 *      +=, -= must handle invalid variables.
 *
 * SECTION: Types, Operators, and Expressions/Assignment Operators
 *
 */


dtrace:::BEGIN
{
	p -= trace(0);
	exit(0);
}
