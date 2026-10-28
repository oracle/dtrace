/* @@xfail: dtv2: need vtimestamp support */

dtrace:::BEGIN {
	trace(vtimestamp);
	exit(0);
}
