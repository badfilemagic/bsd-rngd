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

#include <sys/capsicum.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#define MAX_DEV_NAME_LEN	16
#define MAX_CONF_LINE_BUF	24
#define DELIMETER		"="	

/* structure containing configuration file items */
typedef struct conf {
	char		entropy_device[MAX_DEV_NAME_LEN];
	char		read_bytes[4];	
	char		sleep_seconds[1];	

} conf_t;

void write_pid(pid_t);

void
usage(void)
{
	fprintf(stderr,"USAGE: bsdrngd [-d -c /path/to/config]\nBy default, will run in the foreground and load its config from /usr/local/etc/bsd-rngd.conf\n");
	exit(1);
}




/* read entropy from the trng device */
void
read_entropy(int fd, char *buf, uint32_t n)
{
	ssize_t rv;
	
	flock(fd, LOCK_EX);
	rv = read(fd,buf,n);
	if ( rv < 0 ) 
	{
		syslog(LOG_ERR, "Error reading bytes from entropy source: %s", strerror(errno));
		exit(-1);
	}
	flock(fd, LOCK_UN);
}

void
write_entropy(int d, char *buf, int n)
{	
	ssize_t rv = 0;
		rv = write(d,buf,n);
	if ( rv < 0 ) 
	{
		syslog(LOG_ERR, "Unable to write to /dev/random: %s", strerror(errno));
		exit(-1);
	}
}

/* main daemon child loop */
void entropy_feed(char *dev, uint32_t n, uint32_t s)
{
	
	cap_rights_t rights;	
	write_pid(getpid());

	FILE *trng = fopen(dev,"r");
	if (trng == NULL)
	{
		syslog(LOG_ERR, "Unable to open trng device for read: %s", strerror(errno));
		exit(-1);
	}
	cap_rights_init(&rights, CAP_FSTAT, CAP_READ);
	if (cap_rights_limit(fileno(trng), &rights) < 0 && errno != ENOSYS)
	{
		syslog(LOG_ERR, "Unable to set capsicum rights limit on trng file handle: %s", strerror(errno));
		exit(-1);
	}
	FILE *rnd = fopen("/dev/random", "w");
	if (rnd == NULL)
	{
		syslog(LOG_ERR, "Unable to open /dev/random for writing: %s", strerror(errno));
		exit(-1);
	}
	cap_rights_init(&rights, CAP_FSTAT, CAP_WRITE);
	if (cap_rights_limit(fileno(rnd), &rights) < 0 && errno != ENOSYS)
	{
		syslog(LOG_ERR, "Unable to set capsicum rights limut on /dev/random file handle: %s", strerror(errno));
		exit(-1);
	}
	syslog(LOG_NOTICE, "bsd-rngd: entropy gathering daemon started for device %s", dev);
	
	cap_enter();
	
	char buf[n];
	explicit_bzero(buf,n);
	/* main loop to do the thing */
	while(1)
	{
		read_entropy(fileno(trng),buf,n);
		fpurge(trng);
		if (n <= 16)
		{
			write_entropy(fileno(rnd),buf,n);
		}
		else
		{
			char ent[8];
			explicit_bzero(ent,8);
			for(int i = 0; i < (n - 8); i += 8)
			{
				memcpy(ent,buf,8);
				write_entropy(fileno(rnd),ent,8);
			}
			explicit_bzero(ent,8);
		}
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
			memcpy(c->read_bytes, conf_item, 4);	
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
		exit(-1);
	}
	flock(fileno(fh), LOCK_EX);
	fprintf(fh,"%d\n",p);
	flock(fileno(fh), LOCK_UN);
	fclose(fh);
}

int
main(int argc, char *argv[])
{
	int ch;
	conf_t config;
	int c = 0;
	int daemonize = 0;


	while((ch = getopt(argc, argv, "hdc:")) != -1)
	{
		switch(ch)
		{
			case 'h':
				usage();
				break;
			case 'd':
				daemonize = 1;
				break;
			case 'c':
				read_config(&config,optarg);
				c = 1;
				break;
			default:
				usage();
		}
		
	}
	if (c == 0)
		read_config(&config, "/usr/local/etc/bsd-rngd.conf");
	uint32_t bytes = 0;
	const char *errstr;
	bytes = (uint32_t)strtonum(config.read_bytes,8,4096,&errstr);
	if (errstr != NULL)
	{
		errx(1, "BYTES value is out of range (8, 4096): %u :%s", bytes, errstr);
		exit(-1);
	}
	printf("%d\n", bytes);
	if ((bytes % 8) != 0)
	{
		if (daemonize == 0)
		{
			fprintf(stderr,"Error: specified value for read_bytes %u is not a multiple of 8!\n", bytes);
			exit(-1);
		}
		else
		{
			syslog(LOG_ERR, "specified value for read_bytes %u is not a multiple of 8!\n", bytes);
			exit(-1);
		}
	}
	if (daemonize == 1)
		daemon(0,0);
	/* get to doing work */
	entropy_feed(config.entropy_device, bytes, (uint32_t)atoi(config.sleep_seconds));
}
