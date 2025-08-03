#include "initguid.h"
#include "hid.h"

#include "util/dprintf.h"

#include <stdio.h>
#include <winreg.h>
#include <string.h>

HRESULT GetDeviceInterfaceFromRegistry(const char* hardwareId, char* path, size_t* path_size) {
    HKEY hKey;
    DWORD dwIndex = 0;
    char subKeyName[1024];
    DWORD subKeyNameSize;
    int i, j;
    
    dprintf("Registry: Looking for hardware ID: %s\n", hardwareId);
    
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, 
                     "SYSTEM\\CurrentControlSet\\Control\\DeviceClasses\\{4d1e55b2-f16f-11cf-88cb-001111000030}", 
                     0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        dprintf("Registry: Failed to open device classes key\n");
        return S_FALSE;
    }
    
    while (TRUE) {
        subKeyNameSize = sizeof(subKeyName);
        if (RegEnumKeyEx(hKey, dwIndex++, subKeyName, &subKeyNameSize, 
                        NULL, NULL, NULL, NULL) != ERROR_SUCCESS) {
            break;
        }
        
        dprintf("Registry: Checking subkey: %s\n", subKeyName);
        
        // 检查是否是HID设备
        if (!strstr(subKeyName, "HID#")) {
            continue;
        }
        
        // 准备用于比较的硬件ID（转换 \ 为 #，并移除REV部分）
        char tempHardwareId[1024];
        strcpy_s(tempHardwareId, sizeof(tempHardwareId), hardwareId);
        
        // 移除REV部分（如果存在）
        char* revPos = strstr(tempHardwareId, "&REV_");
        if (revPos) {
            char* nextAmp = strchr(revPos + 1, '&');
            if (nextAmp) {
                // 将REV后面的部分移动到REV位置
                memmove(revPos, nextAmp, strlen(nextAmp) + 1);
            } else {
                // REV是最后一部分，直接截断
                *revPos = '\0';
            }
        }
        
        for (i = 0; tempHardwareId[i]; i++) {
            if (tempHardwareId[i] == '\\') {
                tempHardwareId[i] = '#';
            }
        }
        
        // dprintf("Registry: Hardware ID after processing: %s\n", tempHardwareId);
        
        // 创建副本用于大小写无关比较
        char upperSubKey[1024];
        char upperHardwareId[1024];
        strcpy_s(upperSubKey, sizeof(upperSubKey), subKeyName);
        strcpy_s(upperHardwareId, sizeof(upperHardwareId), tempHardwareId);
        
        _strupr_s(upperSubKey, sizeof(upperSubKey));
        _strupr_s(upperHardwareId, sizeof(upperHardwareId));
        
        // dprintf("Registry: Comparing '%s' with '%s'\n", upperSubKey, upperHardwareId);
        
        if (strstr(upperSubKey, upperHardwareId)) {
            // dprintf("Registry: Found matching device!\n");
            
            // 注册表中的subKeyName格式类似：
            // ##?#HID#VID_8088&PID_0101&MI_05#8&7134EB&0&0000#{4D1E55B2-F16F-11CF-88CB-001111000030}
            // 我们需要转换为正确的设备路径格式：
            // \\?\HID#VID_8088&PID_0101&MI_05#8&7134EB&0&0000#{4d1e55b2-f16f-11cf-88cb-001111000030}
            
            char devicePath[1024];
            strcpy_s(devicePath, sizeof(devicePath), subKeyName);
            
            // 将开头的 ## 替换为 '\\'
            if (devicePath[0] == '#' && devicePath[1] == '#') {
                devicePath[0] = '\\';
                devicePath[1] = '\\';
            }
            
            // 将第3个字符的 ? 保持不变，将第4个字符的 # 替换为 '\\'
            if (devicePath[3] == '#') {
                devicePath[3] = '\\';
            }
            
            // 后面的部分保持原样，因为已经是正确的格式
            
            // dprintf("Registry: Constructed device path: %s\n", devicePath);
            
            size_t pathLen = strlen(devicePath);
            if (pathLen >= *path_size) {
                RegCloseKey(hKey);
                return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
            }
            
            strncpy_s(path, *path_size, devicePath, _TRUNCATE);
            *path_size = pathLen;
            
            RegCloseKey(hKey);
            return S_OK;
        }
    }
    
    RegCloseKey(hKey);
    return S_FALSE;
}

HRESULT GetHidPathByVidPidMi(const char* vid,
                             const char* pid,
                             const char* mi,
                             char* path,
                             size_t* path_size) {
  HDEVINFO deviceInfoSet;
  SP_DEVINFO_DATA deviceInfoData;
  DWORD i;
  char hardwareId[1024];

  if (vid == NULL || pid == NULL || mi == NULL) {
    return E_INVALIDARG;
  }

  deviceInfoSet = SetupDiGetClassDevs(&GUID_DEVINTERFACE_HID, NULL, NULL,
                                      DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
  if (deviceInfoSet == INVALID_HANDLE_VALUE) {
    return S_FALSE;
  }

  for (i = 0;; i++) {
    deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
    if (!SetupDiEnumDeviceInfo(deviceInfoSet, i, &deviceInfoData)) {
      break;
    }

    if (!SetupDiGetDeviceRegistryProperty(
            deviceInfoSet, &deviceInfoData, SPDRP_HARDWAREID, NULL,
            (PBYTE)hardwareId, sizeof(hardwareId), NULL)) {
      continue;
    }

    dprintf("Testing device: %s\n", hardwareId);

    if (strstr(hardwareId, vid) && strstr(hardwareId, pid) &&
        strstr(hardwareId, mi)) {
      dprintf("Testing Passed: %s\n", hardwareId);
      
      SP_DEVICE_INTERFACE_DATA deviceInterfaceData = {sizeof(SP_DEVICE_INTERFACE_DATA)};
      if (0 && SetupDiEnumDeviceInterfaces(deviceInfoSet, &deviceInfoData,
                                      &GUID_DEVINTERFACE_HID, 0,
                                      &deviceInterfaceData)) {
        dprintf("EnumDeviceInterfaces Succeeded.\n");
        
        DWORD requiredSize = 0;
        SetupDiGetDeviceInterfaceDetail(deviceInfoSet, &deviceInterfaceData,
                                        NULL, 0, &requiredSize, NULL);

        PSP_DEVICE_INTERFACE_DETAIL_DATA detailData =
            (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(requiredSize);
        if (detailData == NULL) {
          SetupDiDestroyDeviceInfoList(deviceInfoSet);
          return E_OUTOFMEMORY;
        }

        detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

        if (SetupDiGetDeviceInterfaceDetail(deviceInfoSet, &deviceInterfaceData,
                                            detailData, requiredSize, NULL,
                                            NULL)) {
          dprintf("Device Path: %s\n", detailData->DevicePath);
          size_t pathLen = strlen(detailData->DevicePath);
          if (pathLen >= *path_size) {
            free(detailData);
            SetupDiDestroyDeviceInfoList(deviceInfoSet);
            return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
          }

          strncpy_s(path, *path_size, detailData->DevicePath, _TRUNCATE);
          *path_size = pathLen;
          free(detailData);
          SetupDiDestroyDeviceInfoList(deviceInfoSet);
          return S_OK;
        }
        free(detailData);
      } else {
        dprintf("SetupDiEnumDeviceInterfaces Hacked, trying registry method...\n");
        
        char devicePath[1024];
        size_t pathSize = sizeof(devicePath);
        HRESULT hr = GetDeviceInterfaceFromRegistry(hardwareId, devicePath, &pathSize);
        
        if (SUCCEEDED(hr)) {
          dprintf("Device Path from registry: %s\n", devicePath);
          size_t pathLen = strlen(devicePath);
          if (pathLen >= *path_size) {
            SetupDiDestroyDeviceInfoList(deviceInfoSet);
            return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
          }
          
          strncpy_s(path, *path_size, devicePath, _TRUNCATE);
          *path_size = pathLen;
          SetupDiDestroyDeviceInfoList(deviceInfoSet);
          return S_OK;
        }
        
        DWORD err = GetLastError();
        dprintf("ERR: EnumDeviceInterfaces failed: 0x%08X\n", err);
      }
    }
  }

  SetupDiDestroyDeviceInfoList(deviceInfoSet);
  return S_FALSE;
}
