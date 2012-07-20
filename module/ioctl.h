//#include <string.h>
//#include <unistd.h>
#include <linux/ioctl.h>
//#include <sys/ioctl.h>


// info on ioctls and IO/IOR/IOW/IOWR
// http://www.imamu.edu.sa/topics/IT/IT%202/Communicating%20with%20Hardware%20on%20the%20LINUX%20Platform.ps
//

typedef struct {
    char* pathname; // creat, mkdir, unlink
    int path_length;
    int fd; // close, read, write, lseek, readdir
    char* address; // read, write, readdir
    int addr_length;
    int num_bytes; // read, write
    int offset; // lseek
} ramdisk_ioctl;

#define MKDIR_IOWR _IOWR(250, 0, ramdisk_ioctl)
#define CREAT_IOWR _IOWR(250, 1, ramdisk_ioctl)
#define OPEN_IOWR _IOWR(250, 2, ramdisk_ioctl)
#define CLOSE_IOWR _IOWR(250, 3, ramdisk_ioctl)
#define READ_IOWR _IOWR(250, 4, ramdisk_ioctl)
#define LSEEK_IOWR _IOWR(250, 5, ramdisk_ioctl)
#define WRITE_IOWR _IOWR(250, 6, ramdisk_ioctl)
#define READDIR_IOWR _IOWR(250, 7, ramdisk_ioctl)
#define UNLINK_IOWR _IOWR(250, 8, ramdisk_ioctl)
