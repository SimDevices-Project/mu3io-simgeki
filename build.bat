mkdir build
gcc -shared -o build/simgeki_io.dll mu3io.c hid.c -I. -lsetupapi