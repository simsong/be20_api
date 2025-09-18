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
* Generate the `config.h`
```
./bootstrap.sh
```
* Configure
```
./configure
```
* Then make the executable  
```
make
```
* Time for tests
```shell
./test_be20_api.exe    

make check  || (for fn in test*.log ; do echo ""; echo $fn ; cat $fn ; done; exit 1)
```
Done!
