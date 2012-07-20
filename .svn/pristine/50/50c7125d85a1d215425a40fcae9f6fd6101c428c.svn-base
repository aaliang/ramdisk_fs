#include <sys/ioctl.h>
#include <string.h>
#include <unistd.h>

int mkdir_io(int fd_io, char* pathname) {
    int ret_val;
	// info to be passed for mkdir
    ramdisk_ioctl rd_io;
    rd_io.pathname = pathname;
    rd_io.path_length = strlen(pathname);
	// device, request arg, extra info to be passed
    ret_val = ioctl(fd_io, MKDIR_IOWR, &rd_io);
    return ret_val;
}

int creat_io(int fd_io, char* pathname) {
    int ret_val;
	// info to be passed for creat
    ramdisk_ioctl rd_io;
    rd_io.pathname = pathname;
    rd_io.path_length = strlen(pathname);
	// device, request arg, extra info
    ret_val = ioctl(fd_io, CREAT_IOWR, &rd_io);
    return ret_val;
}

int open_io(int fd_io, char* pathname) {
    int ret_val;
	// info to be passed for open
    ramdisk_ioctl rd_io;
    rd_io.pathname = pathname;
    rd_io.path_length = strlen(pathname);
	// device, request arg, extra info
    ret_val = ioctl(fd_io, OPEN_IOWR, &rd_io);
    return ret_val;
}

int close_io(int fd_io, int fd) {
    int ret_val;
	// info to be passed for close
    ramdisk_ioctl rd_io;
    rd_io.fd = fd;
	// device, request arg, extra info
    ret_val = ioctl(fd_io, CLOSE_IOWR, &rd_io);
    return ret_val;
}

int read_io(int fd_io, int fd, char* address, int num_bytes) {
    int ret_val;
	// info to be passed for read
    ramdisk_ioctl rd_io;
    rd_io.address = address;
    rd_io.addr_length = num_bytes;
    rd_io.fd = fd;
    rd_io.num_bytes = num_bytes;
	// device, request arg, extra info
    ret_val = ioctl(fd_io, READ_IOWR, &rd_io);
    return ret_val;
}

int lseek_io(int fd_io, int fd, int offset) {
    int ret_val;
	// info to be passed for lseek
    ramdisk_ioctl rd_io;
    rd_io.fd = fd;
    rd_io.offset = offset;
	// device, request arg, extra info
    ret_val = ioctl(fd_io, LSEEK_IOWR, &rd_io);
    return ret_val;
}

int write_io(int fd_io, int fd, char* address, int num_bytes) {
    int ret_val;
	// info to be passed for write
    ramdisk_ioctl rd_io;
    rd_io.address = address;
    rd_io.fd = fd;
    rd_io.num_bytes = num_bytes;
	// device, request arg, extra info
    ret_val = ioctl(fd_io, WRITE_IOWR, &rd_io);
    return ret_val;
}

int readdir_io(int fd_io, int fd, char* address) {
    int ret_val;
    // info to be passed for readdir
    ramdisk_ioctl rd_io;                                                                                                                                                                       
    rd_io.address = address;
    rd_io.addr_length = 16;
    rd_io.fd = fd;
    // device, request arg, extra info                                                                                      
    ret_val = ioctl(fd_io, READDIR_IOWR, &rd_io);
    return ret_val;
}

int unlink_io(int fd_io, char* pathname) {
    int ret_val;
	// info to be passed for unlink
    ramdisk_ioctl rd_io;
    rd_io.pathname = pathname;
    rd_io.path_length = strlen(pathname);
	// device, request arg, extra info
    ret_val = ioctl(fd_io, UNLINK_IOWR, &rd_io);
    return ret_val;
}
