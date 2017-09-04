#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>


void entropy_feed(char *dev, int n)
{
	syslog(LOG_NOTICE, "bsd-rngd: entropy gathering daemon started");

	char buf[n];
	int rnddev = open("/dev/random", O_WRONLY);
	if (rnddev < 0)
	{
		syslog(LOG_ERR, "Unable to open device /dev/random: %s", strerror(errno));
		exit(-1);
	}
	/* main loop to do the thing */
	while(1)
	{
		ssize_t rv = 0;
		int trng = open(dev, O_RDONLY);
		if (trng < 0)
		{
			syslog(LOG_ERR, "Unable to open device %s: %s", dev, strerror(errno));
			exit(-1);
		}
		flock(trng, LOCK_EX);
		read(trng,buf,n);
		if (rv < 0)
		{
			syslog(LOG_ERR, "Unable to read from device %s: %s", dev, strerror(errno));
			exit(-1);
		}
		rv = write(rnddev,buf,n);
		if (rv < 0)
		{
			syslog(LOG_ERR, "Unable to write to /dev/random: %s", strerror(errno));
			exit(-1);
		}
		flock(trng,LOCK_UN);
		close(trng);
	}
}


int
main(void)
{
	// some boiler-plate daemonization code
	pid_t pid, sid;
	pid = fork();

	if (pid < 0)
		exit(-1);
	if (pid > 0)
		exit(0);
	sid = setsid();
	if (sid < 0)
		exit(-1);
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);

	// run the real logic here. Takes an trng device node and a number of bytes as arguments
	entropy_feed("/dev/cuaU0", 64);
}
