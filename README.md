[NB: 
	this is a locally modified plan9ports
	changes acme and devdraw to
	- use accel srolling with the wheel. Nice w/ apple mouse.
	- use F1/F2/F3 as mouse B1/B2/B3. I map left ctrl,cmd,option to those.
	- add a Size cmd for acme to resize columns.
	- make all windows with names starting with "+" be considered temp.

	I'd just clone the actual p9p from its site and then
	simply add the changes found here in ./src/cmd/{acme, devdraw}.

	HTH. -nemo
].


This is a port of many Plan 9 libraries and programs to Unix.

Installation
------------

To install, run ./INSTALL.  It builds mk and then uses mk to
run the rest of the installation.

For more details, see install(1), at install.txt in this directory
and at https://9fans.github.io/plan9port/man/man1/install.html.

Documentation
-------------

See https://9fans.github.io/plan9port/man/ for more documentation.
(Documentation is also in this tree, but you need to run
a successful install first.  After that, "9 man 1 intro".)

Intro(1) contains a list of man pages that describe new features
or differences from Plan 9.

Helping out
-----------

If you'd like to help out, great!

If you port this code to other architectures, please share your changes
so others can benefit.

Git
---

You can use Git to keep your local copy up-to-date as we make
changes and fix bugs.  See the git(1) man page here ("9 man git")
for details on using Git.

Status
------

[![Build Status](https://travis-ci.org/9fans/plan9port.svg?branch=master)](https://travis-ci.org/9fans/plan9port)
[![Coverity Scan Build Status](https://scan.coverity.com/projects/plan-9-from-user-space/badge.svg)](https://scan.coverity.com/projects/plan-9-from-user-space)


Contact
-------

* Mailing list: https://groups.google.com/group/plan9port-dev
* Issue tracker: https://github.com/9fans/plan9port/issues
* Submitting changes: https://github.com/9fans/plan9port/pulls

* Russ Cox <rsc@swtch.com>
