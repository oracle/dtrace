dtrace:::BEGIN {
	trace(gid);
	exit(0);
}
