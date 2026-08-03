/*
 * USB????? - HID + CDC + WinUSB ???
 */

#ifndef __USB_DESC_H
#define __USB_DESC_H

#include "usb_type.h"

#define USB_DEVICE_DESCRIPTOR_TYPE                 0x01
#define USB_CONFIGURATION_DESCRIPTOR_TYPE          0x02
#define USB_STRING_DESCRIPTOR_TYPE                 0x03
#define USB_INTERFACE_DESCRIPTOR_TYPE              0x04
#define USB_ENDPOINT_DESCRIPTOR_TYPE               0x05
#define USB_INTERFACE_ASSOCIATION_DESCRIPTOR_TYPE  0x0B
#define USB_BOS_DESCRIPTOR_TYPE                    0x0F
#define USB_DEVICE_CAPABILITY_DESCRIPTOR_TYPE      0x10

#define HID_DESCRIPTOR_TYPE                        0x21
#define REPORT_DESCRIPTOR                          0x22

#define USB_HID_DEV_SIZ_DEVICE_DESC               18
#define USB_HID_DEV_SIZ_CONFIG_DESC               132
#define USB_HID_DEV_SIZ_REPORT_DESC               51
#define USB_HID_DEV_SIZ_HID_DESC                  9
#define USB_HID_DEV_SIZ_BOS_DESC                  33
#define USB_HID_DEV_SIZ_MSOS20_DESC               182
#define USB_HID_DEV_SIZ_MSOS10_COMPAT_DESC        40
#define USB_HID_DEV_OFF_HID_DESC                  18

#define USB_HID_DEV_SIZ_STRING_LANGID             4
#define USB_HID_DEV_SIZ_STRING_VENDOR             18
#define USB_HID_DEV_SIZ_STRING_PRODUCT            30
#define USB_HID_DEV_SIZ_STRING_SERIAL             40

#define STANDARD_ENDPOINT_DESC_SIZE               0x09
#define WINUSB_MS_VENDOR_CODE                     0x07
#define WINUSB_REQUEST_GET_DESCRIPTOR_SET         0x07

extern const u8 UsbHidDev_DeviceDescriptor[USB_HID_DEV_SIZ_DEVICE_DESC];
extern const u8 UsbHidDev_ConfigDescriptor[USB_HID_DEV_SIZ_CONFIG_DESC];
extern const u8 UsbHidDev_ReportDescriptor[USB_HID_DEV_SIZ_REPORT_DESC];
extern const u8 UsbHidDev_StringLangID[USB_HID_DEV_SIZ_STRING_LANGID];
extern const u8 UsbHidDev_StringVendor[USB_HID_DEV_SIZ_STRING_VENDOR];
extern const u8 UsbHidDev_StringProduct[USB_HID_DEV_SIZ_STRING_PRODUCT];
extern const u8 UsbHidDev_StringSerial[USB_HID_DEV_SIZ_STRING_SERIAL];
extern const u8 UsbHidDev_StringMSOS[18];
extern const u8 UsbHidDev_BOSDescriptor[USB_HID_DEV_SIZ_BOS_DESC];
extern const u8 UsbHidDev_MSOS20Descriptor[USB_HID_DEV_SIZ_MSOS20_DESC];
extern const u8 UsbHidDev_MSOS10CompatDescriptor[USB_HID_DEV_SIZ_MSOS10_COMPAT_DESC];

#endif
