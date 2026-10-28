dtrace:::BEGIN {
	trace(ppid);
	exit(0);
}
