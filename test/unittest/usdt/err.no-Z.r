expected failure
-- @@stderr --
dtrace: invalid probe specifier 
dtrace:::BEGIN
{
	exit(0);
}

testprov*:::foo
{
	raise(SIGUSR1);
	exit(0);
}: probe description testprov*:::foo does not match any probes
