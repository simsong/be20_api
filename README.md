# be20_api

|Branch|Coverage|
|------|--------|
|[main](https://github.com/simsong/be20_api/blob/main/README.md)|[![codecov](https://codecov.io/gh/simsong/be20_api/branch/slg-dev/graph/badge.svg?token=Nj8q8eo3Ji)](https://codecov.io/gh/simsong/be20_api)|
|[slg-dev](https://github.com/simsong/be20_api/blob/slg-dev/README.md)| [![codecov](https://codecov.io/gh/simsong/be20_api/branch/slg-dev/graph/badge.svg?token=Nj8q8eo3Ji)](https://codecov.io/gh/simsong/be20_api)|


This is the framework for the [bulk_extractor](https://github.com/simsong/bulk_extractor)  plug-in API.
It is called *be20_api* because the API was developed for Bulk_Extractor version 1.3. The API has been
used without change in Bulk_Extractor versions 1.4 and 1.5, and will be used without change in Bulk_Extractor version 2.0

The Bulk_Extractor API is a plug-in API for bulk_extractor "scanners." Scanners are implemented
as `extern "C"` functions which are called from the bulk_extractor C++ framework. All bulk_extractor
scanners are implemented using the API. Scanners can either be compiled into the bulk_extractor executable, or they can be loaded at run-time from the plug-ins directory. The directory contains zero or more shared libraries (on Unix/Linux/MacOS) or DLLs (on Windows).

There is no differnece in functionality between scanners that are
compiled into the program (e.g. bulk_extractor or tcpflow) and those that are loaded at runtime.

## Normal Usage

The API defines functions for:

1. Creating a `scanner_set`.  This creates the scanner_set's `feature_recorder_set`.

2. Loading scanners into a scanner set.  When each scanner is loaded:

  2.1 Any feature recorders that it specifies will be created and
  added to the `feature_recorder_set` if they do not already exist.

3. Entering the scanning phase.

4. Scanning one or more `sbuf`s, which may cause scanners to create child sbufs
   and recursively scan them.

5. Exiting the scanning phase and running the histogram phase, which
   causes the scanner_set to collect from the scanner all of the
   specified histograms (by `feature_recorder` name and regular
   expression). Each feature recorder is then asked to make its
   histograms (this process can be parallelized too, and will be
   parallelized in the future!)

6. Finally, the `scanner_set` shuts down and everything is de-allocated.

## Path Printing

The API also defines functions for "path printing," which uses the scanners to decode and print a forensic path.

|Path|Action|
|----|------|
|0-PRINT|Prints the contents of location 0|
|0-PRINT/r|Raw dumps the length of the buffer in decimal, a \r\n, and then the contents of location 0|
|0-PRINT/h|Hext dump the contents of location 0|


## Working with this repo.
This repo can used in three ways:

1. As a stand-alone repo for testing the API modules.
2. As a stand-alone repo for developing and testing scanners.
3. As a submodule repo to bulk_extractor or tcplow

The autotools implementation is this repo is designed to either be included in the parent's `configure.ac` file or to use its own `configure.ac` file. It makes a library called `be20_api.a` which can then be linked into the bulk_extractor program or the testing program.

Use the  `bootstrap.sh` program in *this* repo to compile the test programs.

### Help on git submodules

Git submodules are complicated. Basically, the parent module is linked to a paritcular commit point, and not to a particular branch. This isolates parent modules from changes in the submodule until the parent module wants to accept the change.

Update to this repository to master:

    (cd be20_api; git pull origin master)

# Major changes with BE20 v. 2.0:
* `scanner_set` now controls the recursive scanning process. Scanner
  set holds the configuration information for the scan and the scanners.

* sbuf now keeps track of the depth.
* max_depth is now defined for the `scanner_set`, not per scanner. An
  individual scanner can just look at the depth in the sbuf and abort
  if the scanner things have gone on too long.

Scanner Activation
------------------
* scanner_commands is created from reading the command-line
  arguments. It contains enable and disable commands for each scanner.

* For each scanner, we can then scan the scanner_commands to determine
  if the scanner should be initialized, and if we should, we
  initialize it.

* The scanners are then sent

BE20_API STATUS REPORT
======================
BE13_API has been renamed BE20_API and is largley complete.

Next on the agenda is rewriting tcpflow to use be20_api from be13_api.
