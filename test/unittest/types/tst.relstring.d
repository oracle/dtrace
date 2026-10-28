/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	Verify relational operators with strings
 *
 * SECTION: Types, Operators, and Expressions/Relational Operators
 *
 */

#pragma D option quiet


dtrace:::BEGIN
{
	string_1 = "abcde";
	string_2 = "aabcde";
	string_3 = "abcdef";
}

profile:::tick-1
/string_1 <= string_2 || string_2 >= string_1 || string_1 == string_2/
{
	printf("Shouldn't end up here (1)\n");
	printf("string_1 = %s string_2 = %s string_3 = %s\n",
		string_1, string_2, string_3);
	exit(1);
}

profile:::tick-1
/string_3 < string_1 || string_1 > string_3 || string_3 == string_1/
{
	printf("Shouldn't end up here (2)\n");
	printf("string_1 = %s string_2 = %s string_3 = %s n",
		string_1, string_2, string_3);
	exit(1);
}

profile:::tick-1
/string_3 > string_1 && string_1 > string_2 &&
	string_1 != string_2 && string_2 != string_3/
{
	exit(0);
}
