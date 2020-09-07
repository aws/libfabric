#include <efa.h>

INT64 align_down_to_power_of_2(INT64 n)
{
	INT64 idx;
	return BitScanReverse64(&idx, n) ? (1 << idx) : 0;
}

uid_t getuid(void)
{
	// This is a linux command not available on windows, and is used by the EFA provider
	// for addressing when the local and remote peers are on the same node with SHM enabled.
	// Because the provider disables this SHM communication, this function is not called.
	return 0;
}

ssize_t process_vm_writev(pid_t pid,
	const struct iovec *local_iov,
	unsigned long liovcnt,
	const struct iovec *remote_iov,
	unsigned long riovcnt,
	unsigned long flags)
{
	// This is a linux command not available on windows, used to check if the SHM provider can be used.
	// Doing nothing and returning ensures the SHM provider is disabled on windows.
	return 0;
}

pid_t fork(void)
{
	// This is a linux command not available on windows used to check if the SHM provider can be used.
	// Returning -1 ensures the SHM provider is disabled on windows.
	return -1;
}

pid_t wait(int *wstatus)
{
	// This is a linux command not available on windows, and is used to check if the SHM provider can be used.
	// Returning -1 ensures the SHM provider is disabled on windows.
	return -1;
}

pid_t getppid(void)
{
	// This is a linux command that is not available on windows. It is used to get a parent process id after
	// calling fork(). As fork is made to fail on the windows provider, this function is never called.
	return -1;
}