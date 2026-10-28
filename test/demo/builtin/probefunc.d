dtrace:::BEGIN {
	trace(probefunc);
	exit(0);
}
