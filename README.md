# bsd-rngd
Userland daemon like rng-tools rngd, to feed additional entropy via randomdev_accumulate.  Tested on FreeBSD 11.1 with a Ubld.it TrueRNGPro device.

TODO:
  * Use Capsicum
  * Split up writes of 16 bytes or greater into multiple 8 byte chunks to optimize amount of entropy written ino random pools
  * Health tests for input from trng source
