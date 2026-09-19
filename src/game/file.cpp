// game/file: thin wrapper over the SN ProDG host PC file system (D:/Bio4/Prog/file.cpp): open /
// close / read / write / seek / exist on the development host, rooted at d:\bio4. Every call is a
// no-op unless System_flg 0x20000 (host connection detected at boot).
#include "types.h"
#include "global.h"
#include "fileserver.h"
#include "file.h"

extern "C" {
void OSReport(const char* fmt, ...);
int sprintf(char* buf, const char* fmt, ...);
}

static int usb_fd = -1;
static void* usb_buf = (void*) 0x81800000;
static int usb_size = 0;
static int usb_pos = 0;

// Never called in this build; only its strings survive in .rodata.
static inline void usb_connect(int port)
{
    OSReport("USB server connect wait...\n");
    OSReport("connect done\n");
    OSReport("USB port: %d\n", port);
}

// Boot: initialises the PC file server link and sets its root directory to d:\bio4.
int InitFile()
{
    PCinit();
    OSReport("SN PC file system Initialize\n");
    PCopen("SETROOT:d:\\bio4", 0, 0);
    return 1;
}

// Opens a host file: mode 0/2 create+write (FILE_OPEN_WRITE / RDWR), 1 read. Returns the fd, 0 on failure.
int file_open(const char* name, int mode)
{
    int fd;

    if (SysFlagChk(pG, SYS_SN_PC_READ)) {
        if (mode == 0 || mode == 2) {
            fd = PCcreat(name, 0);
            if (fd == -1) {
                return 0;
            }
        } else {
            if (mode != 0) {
                mode--;
            }
            fd = PCopen(name, mode, 0);
            if (fd == -1) {
                fd = 0;
            }
        }
        return fd;
    }
    return 0;
}

// Closes a host fd; 0 ok, -1 failure.
int file_close(int fd)
{
    int ret;

    if (SysFlagChk(pG, SYS_SN_PC_READ)) {
        if (PCclose(fd) != 0) {
            ret = -1;
            return ret;
        }
        ret = 0;
    } else {
        ret = -1;
    }
    return ret;
}

// Reads size bytes; returns the count.
int file_read(int fd, void* buf, int size)
{
    int ret = 0;

    if (SysFlagChk(pG, SYS_SN_PC_READ)) {
        ret = PCread(fd, buf, size);
    }
    return ret;
}

// Writes size bytes; returns the count.
int file_write(int fd, const void* buf, int size)
{
    int ret = 0;

    if (SysFlagChk(pG, SYS_SN_PC_READ)) {
        ret = PCwrite(fd, buf, size);
    }
    return ret;
}

// Seeks (whence 0 set, 1 cur, 2 end); returns the new position, -1 without host.
int file_seek(int fd, int offset, int whence)
{
    int ret = -1;

    if (SysFlagChk(pG, SYS_SN_PC_READ)) {
        ret = PClseek(fd, offset, whence);
    }
    return ret;
}

// 1 when the host file can be opened for reading.
int file_exist(const char* name)
{
    int fd;

    if (!SysFlagChk(pG, SYS_SN_PC_READ)) {
        return 0;
    }
    fd = PCopen(name, 0, 0);
    if (fd == -1) {
        return 0;
    }
    PCclose(fd);
    return 1;
}

// Changes the host root directory ("SETROOT:dir").
int file_path(const char* dir)
{
    char buf[64];

    if (!SysFlagChk(pG, SYS_SN_PC_READ)) {
        return 0;
    }
    sprintf(buf, "SETROOT:%s", dir);
    PCopen(buf, 0, 0);
    return 1;
}
