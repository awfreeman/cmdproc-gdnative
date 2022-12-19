# OFToast II's GDNative functions

Uses Fenteale's simple gdnative downloader, with some added misc functions.

## Building

This is a standard cmake project. It can either be built with command line or loaded directly in visual studio. Command line example is:

```
mkdir build
cd build
cmake ..
make
```

Output bins will be placed in the `project/gdnative` folder. Simply copy the bins into the respective oftoast 2 folders.

(CI uses vcpkg on Windows with MSVC to compile the file into a single dll, more ideal for distributing.)
