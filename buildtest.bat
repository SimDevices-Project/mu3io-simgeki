mkdir build
gcc -m64 hid.c mu3io.c test.c util/dprintf.c -o build/test.exe -lsetupapi