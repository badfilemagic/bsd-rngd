# bsd-rngd
Userland daemon like rng-tools rngd, to feed additional entropy via randomdev_accumulate

TODO:
  * capsicumize
  * split up writes of 16 bytes or greater into multiple 8 byte chunks to optimize amount of entropy written ino random pools
