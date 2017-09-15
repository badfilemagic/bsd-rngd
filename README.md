# bsd-rngd
Userland daemon like rng-tools rngd, to feed additional entropy via randomdev_accumulate.  Tested on FreeBSD 11.1 with a Ubld.it TrueRNGPro device.

copy the binary to /usr/local/sbin
copy the config to /usr/local/etc/ and edit it as necessary
copy the rc script to /usr/local/etc/rc.d
edit /etc/rc.conf and add the line:
	bsdrngd_enable="YES"
start with 'service bsdrngd start'

TODO:
  * Health tests for input from trng source
