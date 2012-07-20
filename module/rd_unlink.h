int rd_unlink(char* pathname) {
	int pid = 5;
    char* dir;
    char* file;
	char* file_type;
    int parent_dir;
    int file_inode;
	int i, direct_count;
	int rem_inode, bp_count;
	pnode_fd_table* fd_unlink;
	single_indirect* single_block;
    double_indirect* double_block;

	// (4) error if root
    if (strcmp(pathname, "/") == 0) {
        return -1;
    }
    split_dir_file(pathname, &dir, &file);
    parent_dir = parent_dir_exists(dir);
    // (1) error if pathname/file doesn't exist
	if (parent_dir == -1) {
        vfree(dir); 
		vfree(file);
        return -1;
    }
    file_inode = get_dir_entry(parent_dir, file, 2);
	if (file_inode == -1) {
        vfree(dir); 
		vfree(file);
        return -1;
    }
	// (2) error if dir is non-empty
    file_type = inode_array[file_inode].type;
    if ((file_type == 1) && (inode_array[file_inode].size != 0)) {
        return -1;
    }
	// (3) error if trying to unlink open file
	fd_unlink = find_fd_list(fd_table, pid);
	if (fd_unlink->table[file_inode].inode_pointer == NULL) {
		vfree(dir);
		vfree(file);
		return -1;
	}
	rem_inode = unlinker(parent_dir, file, inode_array[file_inode].type);  

    inode_array[parent_dir].size -= 16;
    s_block->free_inodes += 1;
    inode_array[rem_inode].id = rem_inode;
    inode_array[rem_inode].allocated = 0;
    inode_array[rem_inode].size = 0;
    inode_array[rem_inode].type = 0;

	bp_count = inode_array[rem_inode].bp_count;
	if (bp_count > 8){
		direct_count = 8;
	} 
	else {direct_count = bp_count;}

	for(i = 0; i < direct_count; i++) {
		if (inode_array[rem_inode].block_pointer[i] == NULL){
			break;
		}
		memset(inode_array[rem_inode].block_pointer[i], 0, RAMDISK_BLOCK_SIZE);
	}
	// single indirect
    if (bp_count > 8) {
        single_block = (single_indirect*) inode_array[file_inode].block_pointer[8];
        for (i = 0; i < 64; i++) {
            if (!single_block->block_pointer[i]) {
                break;
            }

			memset(single_block->block_pointer[i], 0, RAMDISK_BLOCK_SIZE);
        }
    }
    //double indirect
    if (bp_count == 10) {
        double_block = (double_indirect*) inode_array[file_inode].block_pointer[9];
        for (i = 0; i < 64; i++) {
            if (!double_block->indirect_block_pointer[i]) {
                break;
            }
            for (i = 0; i < 64; i++) {
                if (!single_block->block_pointer[i]) {
                    break;
                }

				memset(single_block->block_pointer[i], 0, RAMDISK_BLOCK_SIZE);	
            }
        }
    }
    
    inode_array[rem_inode].bp_count = 0;

	printf("rd_unlink %s is now unlinked\n", pathname);
	return 0;
}