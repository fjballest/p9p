[NB: this is plan9port with local changes by nemo, kept as commits
on top of upstream https://github.com/9fans/plan9port (see git log).

acme:
- Size command (also in the row tag) to make all columns equal width;
  Newcol/Delcol resize, and the initial column count depends on the screen width.
- accelerated scrolling: the wheel/Magic Mouse delta scrolls that many lines
  (devdraw on macOS passes the delta in the high bits of the button word).
- windows whose name contains + in the last element are scratch;
  *.out windows get Get in the tag.
- ! is a file name character.
- Getall (from Sape Mullender), $acmetags, autoindent tab after {.

lib9 shows /Users/nemo/u as /u; rcmain splits only on newlines.

Branch local-2023 is a snapshot of the sources installed at /u/plan9
from 2023 to 2026; old-master-2021 is the previous master.
-nemo]

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
[![Github Actions Build Status](https://github.com/9fans/plan9port/actions/workflows/actions.yaml/badge.svg)](https://github.com/9fans/plan9port/actions/workflows/actions.yaml)
[![Coverity Scan Build Status](https://scan.coverity.com/projects/plan-9-from-user-space/badge.svg)](https://scan.coverity.com/projects/plan-9-from-user-space)


Contact
-------

* Mailing list: https://groups.google.com/group/plan9port-dev
* Issue tracker: https://github.com/9fans/plan9port/issues
* Submitting changes: https://github.com/9fans/plan9port/pulls

* Russ Cox <rsc@swtch.com>
