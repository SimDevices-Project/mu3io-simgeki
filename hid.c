#include "initguid.h"
#include "hid.h"

#include <stdio.h>

HRESULT GetHidPathByVidPidMi(const char* vid,
                             const char* pid,
                             const char* mi,
                             char* path,
                             size_t* path_size) {
  if (vid == NULL || pid == NULL || mi == NULL) {
    return E_INVALIDARG;
  }

  HDEVINFO deviceInfoSet;
  SP_DEVINFO_DATA deviceInfoData;
  DWORD i;
  char hardwareId[1024];

  // Get the device information set for all present devices
  deviceInfoSet = SetupDiGetClassDevs(&GUID_DEVINTERFACE_HID, NULL, NULL,
                                      DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
  if (deviceInfoSet == INVALID_HANDLE_VALUE) {
    return S_FALSE;
  }

  // Enumerate all devices in the set
  for (i = 0;; i++) {
    // 一定要先初始化 cbSize
    deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
    if (!SetupDiEnumDeviceInfo(deviceInfoSet, i, &deviceInfoData)) {
      // 没有更多设备或出错
      break;
    }

    // Get the hardware ID
    if (!SetupDiGetDeviceRegistryProperty(
            deviceInfoSet, &deviceInfoData, SPDRP_HARDWAREID, NULL,
            (PBYTE)hardwareId, sizeof(hardwareId), NULL)) {
      continue;
    }

    // printf("Testing device: %s\n", hardwareId);

    // Check if the hardware ID matches the device we are looking for
    if (strstr(hardwareId, vid) && strstr(hardwareId, pid) &&
        strstr(hardwareId, mi)) {
      // printf("Testing Passed: %s\n", hardwareId);
      // Get device interface data
      SP_DEVICE_INTERFACE_DATA deviceInterfaceData = {
          sizeof(SP_DEVICE_INTERFACE_DATA)};
      if (SetupDiEnumDeviceInterfaces(deviceInfoSet, &deviceInfoData,
                                      &GUID_DEVINTERFACE_HID, 0,
                                      &deviceInterfaceData)) {
        // printf("EnumDeviceInterfaces Succeeded.\n");
        // Get the required buffer size
        DWORD requiredSize = 0;
        SetupDiGetDeviceInterfaceDetail(deviceInfoSet, &deviceInterfaceData,
                                        NULL, 0, &requiredSize, NULL);

        // Allocate memory for the interface detail data
        PSP_DEVICE_INTERFACE_DETAIL_DATA detailData =
            (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(requiredSize);
        if (detailData == NULL) {
          // printf("Memory allocation failed.\n");
          SetupDiDestroyDeviceInfoList(deviceInfoSet);
          return E_OUTOFMEMORY;
        }

        detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

        // Get the interface detail data
        if (SetupDiGetDeviceInterfaceDetail(deviceInfoSet, &deviceInterfaceData,
                                            detailData, requiredSize, NULL,
                                            NULL)) {
          // Copy the device path
          // printf("Device Path: %s\n", detailData->DevicePath);
          size_t pathLen = strlen(detailData->DevicePath);
          if (pathLen >= *path_size) {
            free(detailData);
            SetupDiDestroyDeviceInfoList(deviceInfoSet);
            return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
          }

          strncpy_s(path, *path_size, detailData->DevicePath, _TRUNCATE);
          *path_size = pathLen;  // 更新为实际长度
          free(detailData);
          break;
        }
        free(detailData);
      } else {
        DWORD err = GetLastError();
        printf("ERR: EnumDeviceInterfaces failed: 0x%08X\n", err);
      }
      SetupDiDestroyDeviceInfoList(deviceInfoSet);
      return S_OK;
    }
  }

  SetupDiDestroyDeviceInfoList(deviceInfoSet);
  return S_FALSE;
}