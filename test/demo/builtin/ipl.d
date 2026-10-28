/* @@xfail: dtv2: need ipl support */

dtrace:::BEGIN {
	trace(ipl);
	exit(0);
}
