#include <ramdisk.h>

int rd_lseek(int fd, int offset) {
    fileDescriptorNode* fd_lseek;
	if (fd < 0) {
		return -1;
	}

    fd_lseek = find_fd(fd_table, getpid());
    if (fd_lseek == NULL) {
        return -1;
    }

    // check if the file is open
    if (fd_lseek->fd_table[fd].inode_pointer == NULL) {
        return -1;
    }

    // lseek doesn't work on dir (denoted by 1)
    if ((fd_lseek->fd_table[fd].inode_pointer->type) == 1) {
        return -1;
    }

    // double check the offset
    int filesize = fd_lseek->fd_table[fd].inode_pointer->size;
    if (offset < 0) {
        offset = 0;
    }
    if (offset > filesize) {
        fd_lseek->fd_table[fd].offset = filesize;
    }
    else {
        fd_lseek->fd_table[fd].offset = offset;
    }
    printk("rd_lseek has moved the file to %d\n", fd_lseek->fd_table[fd].offset);

    return 0;
}