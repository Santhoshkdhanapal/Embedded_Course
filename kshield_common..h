#ifndef KSHIELD_COMMON_H
#define KSHIELD_COMMON_H

#ifdef __KERNEL__
#include <linux/types.h>
#include <linux/ioctl.h>
#else
#include <cstdint>
#include <sys/ioctl.h>
#endif

#define KSHIELD_DEVICE_NAME   "kshield"
#define KSHIELD_DEVICE_PATH   "/dev/kshield"
#define KSHIELD_CLASS_NAME    "kshield_cls"
#define KSHIELD_RING_CAPACITY 128
#define KSHIELD_PATH_MAX      256
#define KSHIELD_COMM_MAX      16

enum kshield_event_type {
    KSHIELD_EVENT_EXECVE = 1,
    KSHIELD_EVENT_OPENAT = 2
};

struct kshield_telemetry {
    uint32_t event_type;
    uint32_t pid;
    uint32_t ppid;
    uint32_t uid;
    uint64_t timestamp_ns;
    char comm[KSHIELD_COMM_MAX];
    char target_path[KSHIELD_PATH_MAX];
};

#define KSHIELD_MAGIC 'k'
#define KSHIELD_IOCTL_GET_DROPPED_COUNT _IOR(KSHIELD_MAGIC, 1, uint64_t)
#define KSHIELD_IOCTL_PURGE_BUFFER      _IO(KSHIELD_MAGIC, 2)

#endif // KSHIELD_COMMON_H
