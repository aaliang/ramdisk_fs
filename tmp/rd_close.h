#include <ramdisk.h>

int rd_close(int fd) {
    if (fd < 0) {
        return -1;
    }

    pnode_fd_table* fd_t = find_fd(fd_table, getpid());

    if (fd_t == NULL) {
        return -1;
    }

    if (fd_t->table[fd].inode_pointer == NULL) {
		return -1;
    }

    fd_t->table[fd].offset = -1;
    fd_t->table[fd].inode_pointer = NULL;

    printk("rd_close success\n");

    return 0;
}