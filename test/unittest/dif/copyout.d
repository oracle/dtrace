#pragma D option destructive

dtrace:::BEGIN
{
	i = 3;
	copyout((void *)i, 0, 5);
	exit(1);
}

dtrace:::ERROR
{
	exit(0);
}
