mkdir build
gcc -m64 -shared -o build/simgeki_io.dll mu3io.c config.c hid.c util/dprintf.c -I. -lsetupapi