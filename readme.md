# crux-lang

A memory managed, interpreted programming language.

## Building from source

### Windows 

To build from source on windows you need to have mingw (for gcc) installed, and in your PATH. 

you can get mingw [here](https://www.mingw-w64.org/).

```shell
mkdir build
cd build
cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release ../src
cmake --build . --config Release
```

### Linux 

```shell
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ../src
make
```
