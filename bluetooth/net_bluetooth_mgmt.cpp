/*
 * Copyright (C) 2022 The Android Open Source Project
 * Copyright (C) 2024 KonstaKANG
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "android.hardware.bluetooth.service.rpi"

#include "net_bluetooth_mgmt.h"

#include <fcntl.h>
#include <log/log.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>

// Definitions imported from <linux/net/bluetooth/bluetooth.h>
#define BTPROTO_HCI 1

// Definitions imported from <linux/net/bluetooth/hci_sock.h>
#define HCI_CHANNEL_USER 1
#define HCI_CHANNEL_CONTROL 3
#define HCI_DEV_NONE 0xffff

struct sockaddr_hci {
  sa_family_t hci_family;
  unsigned short hci_dev;
  unsigned short hci_channel;
};

// Definitions imported from <linux/net/bluetooth/mgmt.h>
#define MGMT_OP_READ_INDEX_LIST 0x0003
#define MGMT_EV_INDEX_ADDED 0x0004
#define MGMT_EV_CMD_COMPLETE 0x0001
#define MGMT_PKT_SIZE_MAX 1024
#define MGMT_INDEX_NONE 0xFFFF
#define WRITE_NO_INTR(fn) \
  do {                  \
  } while ((fn) == -1 && errno == EINTR)

struct mgmt_pkt {
  uint16_t opcode;
  uint16_t index;
  uint16_t len;
  uint8_t data[MGMT_PKT_SIZE_MAX];
} __attribute__((packed));

struct mgmt_ev_read_index_list {
  uint16_t opcode;
  uint8_t status;
  uint16_t num_controllers;
  uint16_t index[];
} __attribute__((packed));

namespace aidl::android::hardware::bluetooth::impl {

// Wait indefinitely for the selected HCI interface to be enabled in the
// bluetooth driver.
int NetBluetoothMgmt::waitHciDev(int hci_interface) {
  constexpr int kPollSliceMs = 250;
  constexpr int kMaxPollAttempts = 8;        // 2 seconds total.
  constexpr int kIndexListRetryEvery = 4;   // Re-query every 1 second.

  ALOGI("waiting for hci interface %d", hci_interface);

  int ret = -1;
  struct mgmt_pkt cmd;
  struct pollfd pollfd;
  struct sockaddr_hci hci_addr = {
      .hci_family = AF_BLUETOOTH,
      .hci_dev = HCI_DEV_NONE,
      .hci_channel = HCI_CHANNEL_CONTROL,
  };

  // Open and bind a socket to the bluetooth control interface in the
  // kernel driver, used to send control commands and receive control
  // events.
  int fd = socket(PF_BLUETOOTH, SOCK_RAW, BTPROTO_HCI);
  if (fd < 0) {
    ALOGE("unable to open raw bluetooth socket: %s", strerror(errno));
    return -1;
  }

  if (bind(fd, (struct sockaddr*)&hci_addr, sizeof(hci_addr)) < 0) {
    ALOGE("unable to bind bluetooth control channel: %s", strerror(errno));
    goto end;
  }

  // Send the control command [Read Index List].
  cmd = {
      .opcode = MGMT_OP_READ_INDEX_LIST,
      .index = MGMT_INDEX_NONE,
      .len = 0,
  };

  if (write(fd, &cmd, 6) != 6) {
    ALOGE("error writing mgmt command: %s", strerror(errno));
    goto end;
  }

  // ARDA_PI5_BT_BOUNDED_HCI_WAIT_V1
  //
  // Never block the single AIDL binder thread indefinitely while waiting for
  // the kernel HCI index. A wedged initialize() makes the registered HAL
  // service unable to answer even interface-descriptor or dumpsys requests.
  //
  // Use short poll slices with a hard 10 second deadline. Periodically repeat
  // READ_INDEX_LIST so a missed INDEX_ADDED event cannot wedge initialization.
  pollfd = {.fd = fd, .events = POLLIN};

  for (int poll_attempt = 0; poll_attempt < kMaxPollAttempts;
       ++poll_attempt) {
    ret = poll(&pollfd, 1, kPollSliceMs);

    // Poll interrupted, try again.
    if (ret == -1 && (errno == EINTR || errno == EAGAIN)) {
      continue;
    }

    // Poll failure, abandon.
    if (ret == -1) {
      ALOGE("poll error: %s", strerror(errno));
      break;
    }

    // Poll slice expired. Re-query the current controller list periodically
    // in case INDEX_ADDED was missed between the initial query and this wait.
    if (ret == 0) {
      if ((poll_attempt + 1) % kIndexListRetryEvery == 0) {
        const ssize_t written = write(fd, &cmd, 6);
        if (written != 6) {
          ALOGW("error re-querying mgmt index list: %s", strerror(errno));
        }
      }
      continue;
    }

    // A control socket error cannot recover by waiting forever.
    if ((pollfd.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
      ALOGE("bluetooth mgmt poll revents=0x%x", pollfd.revents);
      ret = -1;
      break;
    }

    // Spurious wakeup without readable data.
    if ((pollfd.revents & POLLIN) == 0) {
      continue;
    }

    // Read the next control event.
    struct mgmt_pkt ev {};
    ret = read(fd, &ev, sizeof(ev));
    if (ret < 0) {
      ALOGE("error reading mgmt event: %s", strerror(errno));
      goto end;
    }

    // Received [Read Index List] command response.
    if (ev.opcode == MGMT_EV_CMD_COMPLETE) {
      struct mgmt_ev_read_index_list* data =
          (struct mgmt_ev_read_index_list*)ev.data;

      // Prefer the exact hci_interface
      for (int i = 0; i < data->num_controllers; i++) {
        if (data->index[i] == hci_interface) {
          ALOGI("hci interface %d found", data->index[i]);
          ret = data->index[i];
          goto end;
        }
      }

      // Accept a larger one if we can't find the exact one
      for (int i = 0; i < data->num_controllers; i++) {
        if (data->index[i] >= hci_interface) {
          ALOGI("hci interface %d found", data->index[i]);
          ret = data->index[i];
          goto end;
        }
      }
    }

    // Received [Index Added] event. Keep this consistent with the
    // READ_INDEX_LIST fallback above, which accepts a larger controller index
    // if the exact requested index is not present.
    if (ev.opcode == MGMT_EV_INDEX_ADDED && ev.index >= hci_interface) {
      ALOGI("hci interface %d added", ev.index);
      ret = ev.index;
      goto end;
    }
  }

  ALOGE("timed out waiting for hci interface %d after %d ms",
        hci_interface, kPollSliceMs * kMaxPollAttempts);
  ret = -1;

end:
  ::close(fd);
  return ret;
}

int NetBluetoothMgmt::findRfKill() {
  // ARDA_PI5_BT_DYNAMIC_RFKILL_V1_1
  //
  // rfkill indexes are not stable across a serdev reprobe. A missing
  // rfkill0 must not terminate discovery because Bluetooth may legitimately
  // reappear as rfkill1/rfkill2/etc.
  if (rfkill_state_ != NULL) return 0;

  constexpr int kMaxRfkillDevices = 64;

  for (int i = 0; i < kMaxRfkillDevices; ++i) {
    char rfkill_type[64];
    char type[16] = {};

    snprintf(rfkill_type, sizeof(rfkill_type),
             "/sys/class/rfkill/rfkill%d/type", i);

    int fd = open(rfkill_type, O_RDONLY);
    if (fd < 0) {
      if (errno != ENOENT) {
        ALOGW("open(%s) failed: %s (%d)", rfkill_type,
              strerror(errno), errno);
      }
      continue;
    }

    const ssize_t size = read(fd, type, sizeof(type) - 1);
    ::close(fd);

    if (size >= 9 && !memcmp(type, "bluetooth", 9)) {
      ::asprintf(&rfkill_state_,
                 "/sys/class/rfkill/rfkill%d/state", i);
      ALOGI("Bluetooth rfkill state node: %s", rfkill_state_);
      return 0;
    }
  }

  ALOGE("no Bluetooth rfkill state node found");
  return -1;
}

int NetBluetoothMgmt::rfKill(int block) {
  int fd;
  char on = (block)?'1':'0';
  if (findRfKill() != 0) return 0;

  fd = open(rfkill_state_, O_WRONLY);
  if (fd < 0) {
    ALOGE( "Unable to open /dev/rfkill");
    return -1;
  }
  ssize_t len;
  WRITE_NO_INTR(len = write(fd, &on, 1));
  if (len < 0) {
    ALOGE( "Failed to change rfkill state");
    ::close(fd);
    return -1;
  }
  ::close(fd);
  return 0;
}

int NetBluetoothMgmt::openHci(int hci_interface) {
  ALOGI("opening hci interface %d", hci_interface);

  // Block Bluetooth.
  rfkill_state_ = NULL;
  rfKill(1);

  // Wait for the HCI interface to complete initialization or to come online.
  int hci = waitHciDev(hci_interface);
  if (hci < 0) {
    ALOGE("hci interface %d not found", hci_interface);
    return -1;
  }

  // Open the raw HCI socket.
  int fd = socket(AF_BLUETOOTH, SOCK_RAW, BTPROTO_HCI);
  if (fd < 0) {
    ALOGE("unable to open raw bluetooth socket: %s", strerror(errno));
    return -1;
  }

  struct sockaddr_hci hci_addr = {
      .hci_family = AF_BLUETOOTH,
      .hci_dev = static_cast<uint16_t>(hci),
      .hci_channel = HCI_CHANNEL_USER,
  };

  // Bind the socket to the selected interface.
  if (bind(fd, (struct sockaddr*)&hci_addr, sizeof(hci_addr)) < 0) {
    ALOGE("unable to bind bluetooth user channel: %s", strerror(errno));
    ::close(fd);
    return -1;
  }

  ALOGI("hci interface %d ready", hci);
  bt_fd_ = fd;
  return fd;
}

void NetBluetoothMgmt::closeHci() {
  if (bt_fd_ != -1) {
    ::close(bt_fd_);
    bt_fd_ = -1;
  }

  // Unblock Bluetooth.
  rfKill(0);
  free(rfkill_state_);
}

}  // namespace aidl::android::hardware::bluetooth::impl
