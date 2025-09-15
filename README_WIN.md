## The joy of building with Make on Windows

* There are sub-modules here which require specific hydration
* `git clone --recurse-submodules https://github.com/simsong/be20_api`
   * If you've already cloned - `git submodule update --init --recursive` 
* Install mysys64 - https://www.msys2.org/
* Create toolchain using `pacman`

```sh
pacman -S \
  base-devel \
  mingw-w64-ucrt-x86_64-gcc \
  mingw-w64-ucrt-x86_64-make \
  mingw-w64-ucrt-x86_64-re2 \
  mingw-w64-ucrt-x86_64-abseil-cpp \
  mingw-w64-ucrt-x86_64-sqlite3 \
  mingw-w64-ucrt-x86_64-openssl \
  mingw-w64-ucrt-x86_64-expat
```
* Generate the next bit
```
autoreconf -fi
```
* Configure
```
./configure
```
* Then make should run 
```
make
```
