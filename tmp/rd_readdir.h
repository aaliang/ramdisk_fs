#include <ramdisk.h>

int rd_readdir(int fd, char* address) {
	int addr, offset;
	char* file_addr;
    pnode_fd_table* fd_t;
    fd_entry fd_e;
    if (fd < 0) {
        return -1; // check if fd is valid
    }

    fd_t = find_fd_list(fd_table, getpid());
    fd_e = fd_t->table[fd];
	offset = fd_e.offset;

    if (fd_t == NULL) {
        return -1; // check if file exists
    }
    if ((fd_e.inode_pointer->type) == 1) {
        return -1; // make sure not a dir
    }

	if (fd_e.inode_pointer->size == 0) {
		return 0; // empty dir
	}
	if (fd_e.inode_pointer->size < 0) {
		return -1; // dir corrupted
	}
	if ((offset % 16) != 0) {
		return -1; // offset pointer corrupted
	}
	if (offset == fd_e.inode_pointer->size) {
		return 0; // end of file
	}

	// Set pointer to the directory entry.
	addr = copy_contents_address(fd_e.inode_pointer, offset, &file_addr);
	if (file_addr == NULL) {
		return -1;
	}
	if (RAMDISK_BLOCK_SIZE - addr < 0) {
		return -1;
	}

	memcpy(address, file_addr, 16); // copy file
	fd_t->table[fd].offset += 16; // increase offset

    return 1;
}
