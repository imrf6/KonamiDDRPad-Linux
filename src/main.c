/*
 * MIT License
 * 
 * Copyright (c) 2020 imrf6 and dmillard
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include <fcntl.h>
#include <linux/uinput.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <hidapi/hidapi.h>
#include <stdint.h>

#define MAX_STR 1024
#define KONAMI_VENDOR_ID 0x1ccf
#define KONAMI_PRODUCT_ID 0x1010

/*
 * The Konami DDR Pad streams output that is 27 bytes long.
 *
 *  According to the HID descriptor, the bit layout is:
 *  num x size: Purpose
 *  ------------------------------
 *   13 x    1: Buttons
 *    3 x    1: Constant (padding)
 *    1 x    4: Hat Switch (0x39)
 *    1 x    4: Constant (padding)
 *    4 x    8: X, Y, Z, Z-rot (0x30, 0x31, 0x32, 0x35)
 *   12 x    8: Reserved (0x20 - 0x2b)
 *    4 x   16: Reserved (0x2c - 0x2f)
 *
 *  There is a feature stream available:
 *  num x size: Purpose
 *  ------------------------------
 *    8 x    8: Feature
 *
 *  There is also an output available:
 *  num x size: Purpose
 *  ------------------------------
 *    8 x    8: Unknown
 *
 *  3rd byte: 
 *  right left  up  down  
 *  2     6     0   4
 *  leftup  rightup leftdown  rightdown
 *  7       1       5         3
 *  leftright updown
 *  8         8
 *
 * nth byte (1-indexed): 
 *  8 right
 *  9 left
 *  10 up
 *  11 down 
 *  12 triangle
 *  13 O
 *  14 X
 *  15 square
 *
 *  2nd byte: 
 *  02 start
 *  01 select
 *  03 (both)
 *  10 mode 
 */

void Emit(int fd, int type, int code, int val) {
  struct input_event ie;
  ie.code = code;
  ie.type = type;
  ie.value = val;
  // Note: Time seems to be ignored.
  ie.time.tv_sec = 0;
  ie.time.tv_usec = 0;

  write(fd, &ie, sizeof(ie));
}

int CreateVirtualDevice(char* name, uint16_t vendor_id, uint16_t product_id) {
  // Note: Values of vendor_id and product_id are arbitrary and can be anything.

  int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
  if (fd == -1) {
    perror("Could not open uinput device");
    exit(1);
  }
  // Tell the kernel that we will send key events.
  ioctl(fd, UI_SET_EVBIT, EV_KEY);

  // Tell the kernel which key events we will send.
  ioctl(fd, UI_SET_KEYBIT, BTN_DPAD_UP);
  ioctl(fd, UI_SET_KEYBIT, BTN_DPAD_DOWN);
  ioctl(fd, UI_SET_KEYBIT, BTN_DPAD_LEFT);
  ioctl(fd, UI_SET_KEYBIT, BTN_DPAD_RIGHT);
  ioctl(fd, UI_SET_KEYBIT, BTN_SELECT);
  ioctl(fd, UI_SET_KEYBIT, BTN_START);
  ioctl(fd, UI_SET_KEYBIT, BTN_NORTH);
  ioctl(fd, UI_SET_KEYBIT, BTN_SOUTH);
  ioctl(fd, UI_SET_KEYBIT, BTN_EAST);
  ioctl(fd, UI_SET_KEYBIT, BTN_WEST);
  ioctl(fd, UI_SET_KEYBIT, BTN_MODE);

  struct uinput_setup usetup;
  memset(&usetup, 0, sizeof(usetup));

  usetup.id.bustype = BUS_USB;
  usetup.id.vendor = vendor_id;
  usetup.id.product = product_id;    
  strcpy(usetup.name, name);

  ioctl(fd, UI_DEV_SETUP, &usetup);
  ioctl(fd, UI_DEV_CREATE);

  return fd;
}

void DestroyVirtualDevice(int fd) {
  ioctl(fd, UI_DEV_DESTROY);
  close(fd);
} 

hid_device* OpenDancePad(uint16_t vendor_id, uint16_t product_id) {
  int res;
  wchar_t wstr[MAX_STR];

  // Open device with the given IDs using NULL as serial number.
  hid_device* handle = hid_open(vendor_id, product_id, NULL);
  if (handle == NULL) {
    wprintf(L"Could not open device.\n");
    exit(1);
  }

  // Get manufacturer info of device.
  res = hid_get_manufacturer_string(handle, wstr, MAX_STR);
  if (res == -1) {
    wprintf(L"Could not read manufacturer string: %ls.\n", hid_error(handle));
    exit(1);
  }
  wprintf(L"Manufacturer: %ls\n", wstr);

  // Get product info of device.
  res = hid_get_product_string(handle, wstr, MAX_STR);
  if (res == -1) {
    wprintf(L"Could not read product string: %ls.\n", hid_error(handle));
    exit(1);
  }
  wprintf(L"Product: %ls\n", wstr);

  // Get serial number of device.
  res = hid_get_serial_number_string(handle, wstr, MAX_STR);
  if (res == -1) {
    wprintf(L"Could not read serial number string: %ls\n", hid_error(handle));
    exit(1);
  }
  wprintf(L"Serial Number: (%d) %ls\n", wstr[0], &wstr[1]);

  return handle;
} 

void CloseDancePad(hid_device* handle) {
  hid_close(handle);
}

int ReadReport(hid_device* handle, uint8_t* report, int size) {
  int res;
  res = hid_read(handle, report, size);
  if (res == -1) {
    wprintf(L"Failed to read from device: %ls\n", hid_error(handle));
    return -1;
  }
  return 0;
}

void CheckReportValueAndEmit(uint8_t current_value, uint8_t previous_value, int
    code, int fd) { 
  if (current_value > previous_value) {
    // The key has been pressed; emit press event.
    Emit(fd, EV_KEY, code, 1);
  }
  else if (current_value < previous_value) {
    // The key has been released; emit release event.
    Emit(fd, EV_KEY, code, 0); 
  }
}


void Loop(hid_device* handle, int fd) {
  uint8_t report[27];
  uint8_t previous_report[27];
  ReadReport(handle, previous_report, 27);
  while(1) {
    if (ReadReport(handle, report, 27) != 0) {
      return;
    }
    CheckReportValueAndEmit(report[7], previous_report[7], BTN_DPAD_RIGHT, fd);
    CheckReportValueAndEmit(report[8], previous_report[8], BTN_DPAD_LEFT, fd);
    CheckReportValueAndEmit(report[9], previous_report[9], BTN_DPAD_UP, fd);
    CheckReportValueAndEmit(report[10], previous_report[10], BTN_DPAD_DOWN, fd);
    CheckReportValueAndEmit(report[11], previous_report[11], BTN_NORTH, fd);
    CheckReportValueAndEmit(report[12], previous_report[12], BTN_EAST, fd);
    CheckReportValueAndEmit(report[13], previous_report[13], BTN_SOUTH, fd);
    CheckReportValueAndEmit(report[14], previous_report[14], BTN_WEST, fd);
    CheckReportValueAndEmit(report[1] & 0x02, previous_report[1] & 0x02, BTN_START, fd);
    CheckReportValueAndEmit(report[1] & 0x01, previous_report[1] & 0x01, BTN_SELECT, fd);
    CheckReportValueAndEmit(report[1] & 0x10, previous_report[1] & 0x10, BTN_MODE, fd);
    // Send sync report. 
    // Note: Unsure if this must be sent after each emit or not.
    Emit(fd, EV_SYN, SYN_REPORT, 0);
    memcpy(previous_report, report, 27);
  }
}

int main(int argc, char** argv) {

  int res = hid_init();
  if (res == -1) {
    wprintf(L"Could not initialize hidapi.\n");
    exit(1);
  }

  hid_device* handle = OpenDancePad(KONAMI_VENDOR_ID, KONAMI_PRODUCT_ID);
  int fd = CreateVirtualDevice("Fake DDR pad", 0x1234, 0x5678);

  Loop(handle, fd);

  DestroyVirtualDevice(fd);
  CloseDancePad(handle);
  res = hid_exit();

  return 0;
}
