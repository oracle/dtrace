/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, 2026, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: String comparisons work.
 *
 * SECTION:  Operators
 */

#pragma D option quiet

dtrace:::BEGIN
{
	nerrors = 0;

	s1 = "abcdefghi";
	s2 = "jklmnopqr";
	s3 = "stuvwxyz!";

	nerrors += (s1 <= s2 ? 0 : 1);
	nerrors += (s1 <  s2 ? 0 : 1);
	nerrors += (s1 == s2 ? 1 : 0);
	nerrors += (s1 != s2 ? 0 : 1);
	nerrors += (s1 >= s2 ? 1 : 0);
	nerrors += (s1 >  s2 ? 1 : 0);

	nerrors += (s2 <= s2 ? 0 : 1);
	nerrors += (s2 <  s2 ? 1 : 0);
	nerrors += (s2 == s2 ? 0 : 1);
	nerrors += (s2 != s2 ? 1 : 0);
	nerrors += (s2 >= s2 ? 0 : 1);
	nerrors += (s2 >  s2 ? 1 : 0);

	nerrors += (s3 <= s2 ? 1 : 0);
	nerrors += (s3 <  s2 ? 1 : 0);
	nerrors += (s3 == s2 ? 1 : 0);
	nerrors += (s3 != s2 ? 0 : 1);
	nerrors += (s3 >= s2 ? 0 : 1);
	nerrors += (s3 >  s2 ? 0 : 1);

	printf("%d errors\n", nerrors);
	exit(nerrors == 0 ? 0 : 1);
}
dtrace:::ERROR
{
	exit(1);
}
