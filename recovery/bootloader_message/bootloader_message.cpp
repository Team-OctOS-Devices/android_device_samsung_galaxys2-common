/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include <bootloader_message/bootloader_message.h>

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/system_properties.h>

#include <string>
#include <vector>

#include <android-base/file.h>
#include <android-base/stringprintf.h>
#include <android-base/unique_fd.h>
#include <fs_mgr.h>

#define CACHE_UPDATE_FILE "/cache/recovery/command"

static bool read_cache_file(void* p, size_t size, size_t offset, std::string* err) {
  android::base::unique_fd fd(open(CACHE_UPDATE_FILE, O_RDONLY));
  if (fd.get() == -1) {
    *err = android::base::StringPrintf("failed to open %s: %s", CACHE_UPDATE_FILE,
                                       strerror(errno));
    return false;
  }
  if (lseek(fd.get(), static_cast<off_t>(offset), SEEK_SET) != static_cast<off_t>(offset)) {
    *err = android::base::StringPrintf("failed to lseek %s: %s", CACHE_UPDATE_FILE,
                                       strerror(errno));
    return false;
  }
  if (!android::base::ReadFully(fd.get(), p, size)) {
    *err = android::base::StringPrintf("failed to read %s: %s", CACHE_UPDATE_FILE,
                                       strerror(errno));
    return false;
  }
  return true;
}

static bool write_cache_file(const void* p, size_t size, size_t offset, std::string* err) {
  android::base::unique_fd fd(open(CACHE_UPDATE_FILE, O_WRONLY | O_CREAT | O_SYNC));
  if (lseek(fd.get(), static_cast<off_t>(offset), SEEK_SET) != static_cast<off_t>(offset)) {
    *err = android::base::StringPrintf("failed to lseek %s: %s", CACHE_UPDATE_FILE,
                                       strerror(errno));
    return false;
  }
  if (!android::base::WriteFully(fd.get(), p, size)) {
    *err = android::base::StringPrintf("failed to write %s: %s", CACHE_UPDATE_FILE,
                                       strerror(errno));
    return false;
  }

  // TODO: O_SYNC and fsync duplicates each other?
  if (fsync(fd.get()) == -1) {
    *err = android::base::StringPrintf("failed to fsync %s: %s", CACHE_UPDATE_FILE,
                                       strerror(errno));
    return false;
  }
  return true;
}

bool read_bootloader_message(bootloader_message* boot, std::string* err) {
  return read_cache_file(boot, sizeof(*boot), BOOTLOADER_MESSAGE_OFFSET_IN_MISC, err);
}

bool write_bootloader_message(const bootloader_message& boot, std::string* err) {
  return write_cache_file(&boot, sizeof(boot), BOOTLOADER_MESSAGE_OFFSET_IN_MISC, err);
}

bool clear_bootloader_message(std::string* err) {
  bootloader_message boot = {};
  return write_bootloader_message(boot, err);
}

bool write_bootloader_message(const std::vector<std::string>& options, std::string* err) {
  bootloader_message boot = {};
  strlcpy(boot.command, "boot-recovery", sizeof(boot.command));
  strlcpy(boot.recovery, "recovery\n", sizeof(boot.recovery));
  for (const auto& s : options) {
    strlcat(boot.recovery, s.c_str(), sizeof(boot.recovery));
    if (s.back() != '\n') {
      strlcat(boot.recovery, "\n", sizeof(boot.recovery));
    }
  }
  return write_bootloader_message(boot, err);
}

bool read_wipe_package(std::string* package_data, size_t size, std::string* err) {
  package_data->resize(size);
  return read_cache_file(&(*package_data)[0], size, WIPE_PACKAGE_OFFSET_IN_MISC, err);
}

bool write_wipe_package(const std::string& package_data, std::string* err) {
  return write_cache_file(package_data.data(), package_data.size(),
                              WIPE_PACKAGE_OFFSET_IN_MISC, err);
}

extern "C" bool write_bootloader_message(const char* options) {
  std::string err;
  return write_bootloader_message({options}, &err);
}
