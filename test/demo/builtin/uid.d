dtrace:::BEGIN {
	trace(uid);
	exit(0);
}
