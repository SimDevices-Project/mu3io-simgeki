#include <windows.h>

#include <devguid.h>

#include <setupapi.h>

#include <hidclass.h>
#include <stdint.h>

HRESULT GetHidPathByVidPidMi(const char* vid,
                             const char* pid,
                             const char* mi,
                             char* path,
                             size_t* path_size);