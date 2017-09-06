/*
 *	Copyright (c) 2017, W. Dean Freeman
 *	All rights reserved.
 *
 * 	Redistribution and use in source and binary forms, with or without
 * 	modification, are permitted provided that the following conditions are met:
 *		* Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 *		* Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
 *       	* Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.
 *	
 *      THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *      AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *      IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *      DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 *      FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *      DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 *      SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *      CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 *      OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 *      OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>


#define MAX_DEV_NAME_LEN	16
#define MAX_CONF_LINE_BUF	24
#define MAX_BYTES 		64
#define DELIMETER		"="	

/* structure containing configuration file items */
typedef struct conf {
	char		entropy_device[MAX_DEV_NAME_LEN];
	char		read_bytes[2];
	char		sleep_seconds[1];	

} conf_t;

void write_pid(pid_t);


/* read entropy from the trng device */
void
read_entropy(char *dev, char *buf, uint32_t n)
{

	/* change this later */
	if (n > MAX_BYTES)
	{
		syslog(LOG_INFO, "WARN: Requested bytes greater than 64. Using 64");
		n = 64;
	}

	ssize_t rv = 0;
	int fd = open(dev, O_RDONLY);
	if ( fd < 0 )
	{
		syslog(LOG_ERR, "Unable to open device %s for writing: %s", dev, strerror(errno));
	       	exit(-1);
	}
	flock(fd, LOCK_EX);
	rv = read(fd,buf,n);
	if ( rv < 0 ) 
	{
		syslog(LOG_ERR, "Error reading bytes from entropy source: %s", strerror(errno));
		exit(-1);
	}
	flock(fd, LOCK_UN);
	close(fd);
}

void
write_entropy(char *buf, int n)
{
	ssize_t rv = 0;
	int fd = open("/dev/random", O_WRONLY);
	if ( fd < 0 )
	{
		syslog(LOG_ERR, "Unable to open /dev/random for writing: %s", strerror(errno));
		exit(-1);
	}
	rv = write(fd,buf,n);
	if ( rv < 0 ) 
	{
		syslog(LOG_ERR, "Unable to write to /dev/random: %s", strerror(errno));
		exit(-1);
	}
	close(fd);
}

/* main daemon child loop */
void entropy_feed(char *dev, uint32_t n, uint32_t s)
{
	write_pid(getpid());
	syslog(LOG_NOTICE, "bsd-rngd: entropy gathering daemon started for device %s", dev);
	char buf[n];
	explicit_bzero(buf,n);
	/* main loop to do the thing */
	while(1)
	{
		read_entropy(dev,buf,n);
		write_entropy(buf,n);
		explicit_bzero(buf,n);
		sleep(s);
	}
}

/* Perl-like utility function */
void
chomp(char *s)
{
	int i = 0;
	for (i = 0; i < strlen(s); i++)
	{
		if (s[i] == '\n')
			s[i] = '\0';
	}
	
}

/* read in the configuration file */
void
read_config(conf_t *c, char *f)
{
	FILE *fh = fopen(f,"r");
	if (fh == NULL)
	{
		syslog(LOG_ERR, "Unable to open bsd-rngd.conf for read: %s", strerror(errno));
		exit(-1);
	}
	flock(fileno(fh), LOCK_EX);
	char line[MAX_CONF_LINE_BUF];
	while(fgets(line,sizeof(line), fh) != NULL)
	{
		chomp(line);
		char *conf_item;
		conf_item = strstr((char*)line,DELIMETER);
		conf_item = conf_item + strlen(DELIMETER);
		if (strstr(line, "DEVICE") !=0)
		{
			memcpy(c->entropy_device, conf_item, MAX_DEV_NAME_LEN);
		}
		if (strstr(line, "BYTES") != 0)
		{
			memcpy(c->read_bytes, conf_item, 2);	
		}
		if (strstr(line, "INTERVAL") != 0)
		{
			memcpy(c->sleep_seconds, conf_item, 1);
	
		}
	}
	flock(fileno(fh),LOCK_UN);
	fclose(fh);
	
}

/* write pid file to /var/run */
void
write_pid(pid_t p)
{
	FILE *fh = fopen("/var/run/bsd-rngd.pid", "w");
	if (fh == NULL)
	{
		syslog(LOG_WARNING, "Unable to write pid file /var/run/bsd-rngd: %s", strerror(errno));
	}
	flock(fileno(fh), LOCK_EX);
	fprintf(fh,"%d\n",p);
	flock(fileno(fh), LOCK_UN);
	fclose(fh);
}

int
main(void)
{
	/* read the config */
	conf_t config;
	read_config(&config, "/etc/bsd-rngd.conf");
	
	/* some boiler plate daemonization code */
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

	/* get to doing work */
	entropy_feed(config.entropy_device, (uint32_t)atoi(config.read_bytes), (uint32_t)atoi(config.sleep_seconds));
}
