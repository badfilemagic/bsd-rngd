#include <sys/capsicum.h>
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
typedef struct conf {
	char		entropy_device[MAX_DEV_NAME_LEN];
	char		read_bytes[2];
	char		sleep_seconds[1];	

} conf_t;



void
read_entropy(char *dev, char *buf, uint32_t n)
{
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


void entropy_feed(char *dev, uint32_t n, uint32_t s)
{
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



int
main(void)
{
	conf_t config;
	read_config(&config, "/etc/bsd-rngd.conf");
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
	entropy_feed(config.entropy_device, (uint32_t)atoi(config.read_bytes), (uint32_t)atoi(config.sleep_seconds));
}
