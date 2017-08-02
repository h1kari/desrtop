desrtop
=======

This code allows you to use the DES rainbow tables provided by crack.sh to crack DES keys for the specific 1122334455667788 plaintext which is used by default by [SMB Capture](https://crack.sh/netntlm) and [Responder](https://crack.sh/netntlm). Currently the code is designed to use an FPGA using the [FPGA code on github](https://github.com/h1kari/desrtfpga) to compute the chains, but should be easily modified to work on a CPU or GPU. The current version only works on 1 table per 1 FPGA with a worst-case runtime of around 12 seconds and 99.65% measured success rate when using all 12 tables.

Install Notes
-------------

This project makes use of [Pico AC-510](http://picocomputing.com/ac-510-superprocessor-module/) modules and the [Pico Computing framework](https://picocomputing.zendesk.com/hc/en-us) for communication, which you will need to have installed to build the project:

```
$ sudo dpkg -i picocomputing_5.6.0.0_all.deb
$ export PICOBASE=/usr/src/picocomputing-5.6.0.0
$ cd desrtop
$ make
```

Getting the tables
------------------

We'll have hard drives with the tables available at the [SHA2017 conference](https://sha2017.org) to anyone that wants to make copies. Each table is 512,104,771,584 bytes, and we've found most 6TB drives are just short of being able to store 6,145,257,259,008 bytes. We're currently trying to figure out the best way to distribute the tables. If anyone has hosting that they're willing to provide to seed torrents or has a good way of distributing 6TB of data, please contact me.

Running op
----------

Now that you've built the project, you'll first need to load the fpga, and then run it on a ciphertext. Based on the table specified, you will need a corresponding `<table>.dat` file. The program has been tested on and runs optimally (especially when using with an FPGA) when this is pointed to a file or raw device that supports extremely fast random IO speeds like an NVMe drive or RAM disk.

```
# reboot fpga and immediately exit
$ ./op -f 1 -r -x

# now run with a random ciphertext
$ ./op -f 1 -t 1 -c cf1ae6e3236cde2f
*** FOUND KEY ce98c3e6dd24da ***
```

Bug tracker
-----------

Have a bug? Please create an issue here on GitHub!

https://github.com/h1kari/desrtop/issues

Copyright
---------

Copyright 2017 David Hulton

Licensed under the BSD 3-Clause License: https://opensource.org/licenses/BSD-3-Clause
