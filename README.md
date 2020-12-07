# be13_api
slg-dev: [![codecov](https://codecov.io/gh/simsong/be13_api/branch/slg-dev/graph/badge.svg?token=Nj8q8eo3Ji)](https://codecov.io/gh/simsong/be13_api)

This is the framework for the [bulk_extractor](https://github.com/simsong/bulk_extractor)  plug-in API.
It is called *be13_api* because the API was developed for Bulk_Extractor version 1.3. The API has been
used without change in Bulk_Extractor versions 1.4 and 1.5, and will be used without change in Bulk_Extractor version 2.0

The Bulk_Extractor API is a plug-in API for bulk_extractor "scanners." Scanners are implemented
as `extern "C"` functions which are called from the bulk_extractor C++ framework. All bulk_extractor
scanners are implemented using the API. Scanners can either be compiled into the bulk_extractor executable, or they can be loaded at run-time from the plug-ins directory. The directory contains zero or more shared libraries (on Unix/Linux/MacOS) or DLLs (on Windows).

There is no differnece in functionality between scanners that are compiled into bulk_extractor and those that are loaded at runtime.

The API defines functions for:

1. Initializing the scanner.
2. Scanning an `sbuf`.
3. Recording features that are discovered in `sbuf`s.
4. Creating new `sbuf`s with data from the current `sbuf`.
5. Requesting scans of the newly created `sbuf`s.
6. Shutting down.

## Working with this repo
This repo can used in two ways:

1. As a stand-alone repo for developing and testing scanners.
2. As a submodule repo to bulk_extractor.

The autotools implementation is this repo is designed to either be included in the parent's `configure.ac` file or to use its own `configure.ac` file. It makes a library called `be13_api.a` which can then be linked into the bulk_extractor program or the testing program.

### Help on git submodules

Git submodules are complicated. Basically, the parent module is linked to a paritcular commit point, and not to a particular branch. This isolates parent modules from changes in the submodule until the parent module wants to accept the change.

Here are some things to help you understand this:

References:
  * http://stackoverflow.com/questions/7259535/git-setting-up-a-remote-origin
  * http://stackoverflow.com/questions/5828324/update-git-submodule

Update to this repository to master:

    git pull origin master

Push changes in this repository to master:

    git push origin master

If you get this error:

    error: failed to push some refs to 'git@github.com:simsong/be13_api.git'
    hint: Updates were rejected because a pushed branch tip is behind its remote
    hint: counterpart. If you did not intend to push that branch, you may want to
    hint: specify branches to push or set the 'push.default' configuration
    hint: variable to 'current' or 'upstream' to push only the current branch.
    $ 

Do this:

    $ git checkout -b tmp  ; git fetch ; git checkout -b $USER-dev ; git merge master ; git merge tmp ; git push origin $USER-dev


BE13_API STATUS REPORT
======================
I continue to port bulk_extractor, tcpflow, be13_api and dfxml to modern C++. After surveying the standards I’ve decided to go with C++17 and not C++14, as support for 17 is now widespread. (I probably don’t need 20). I am sticking with autotools, although there seems a strong reason to move to CMake. I am keeping be13_api and dfxml as a modules that are included, python-style, rather than making them stand-alone libraries that are linked against. I’m not 100% sure that’s the correct decision, though.

The project is taking longer than anticipated because I am also doing a general code refactoring. The main thing that is taking time is figuring out how to detangle all of the C++ objects having to do with parser options and configuration. 

Given that tcpflow and bulk_extractor both use be13_api, my attention has shifted to using tcpflow to get be13_api operational, as it is a simpler program. I’m about three quarters of the way through now. I anticipate having something finished before the end of 2020.

--- Simson Garfinkel, October 18, 2020
