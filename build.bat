mkdir build
gcc -shared -o build/mu3io.dll mu3io.c hid.c -I. -lsetupapi