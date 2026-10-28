/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * Use 'this' variables in associative array.
 *
 * SECTION:  Variables/Scalar Variables
 *
 */

this int y;
this int z;
this int res;

dtrace:::BEGIN
{
	this->x[this->y, this->z] = 123;
	this->res = this->x[this->y, this->z]++;
	exit (0);
}
