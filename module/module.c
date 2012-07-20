/*#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/vmalloc.h>
#include <linux/version.h>
#include <linux/ioctl.h>
#include <linux/proc_fs.h>
*/

#include <linux/vmalloc.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/utsname.h>
#include <linux/string.h>
#include <linux/sched.h>

#include <linux/errno.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <asm/string.h>
#include <asm/unistd.h>
//#include <sys/ioctl.h>
#include "ramdisk.h"
#include "ioctl.h"

//returns the address of the block number, each block is 256 bytes
//block numbers start at 0

//addresses
/* these are declared in ramdisk.h:
void* disk;
superblock * s_block; //superblock
i_node* inode_array;
char* block_bitmap;
void* free_blocks;
*/

pnode_fd_table* fd_table; //essentially a linked list of file descriptor tables for each process

void* getblockoffset(int block_num, void *disk_address){
	void* addr = disk_address+(256*block_num); //sizeof void is only 1 byte, if we change it to a pointer to int or anything else, it will actually be 64*block_num
	return addr;
}

int init_fd_table(){
	fd_table = (pnode_fd_table *) vmalloc(sizeof(pnode_fd_table));
	fd_table->pid = 0;
	int i;
	for(i = 0; i < OPENFILES; i++){
		init_fd_entry(&(fd_table->table[i]));
		fd_table->next = NULL;
	return 1;
	}
}


//initializes the ramdisk, 1 if sucessful, -1 if failed
int init_ramdisk(){
	disk = vmalloc(RAMDISK_SIZE);
	if(disk == NULL){
		printk("ramdisk allocation failed\n");
		return -1;
	}
	s_block = disk;
	inode_array = getblockoffset(1, disk);
	block_bitmap = getblockoffset(257, disk);
	free_block_addr = getblockoffset(260, disk);

	s_block->free_blocks = (RAMDISK_SIZE-256-64-256*4)/256;
	s_block->free_inodes = (65536/64)-1;//number of inodes

	//probably not necessary since there's already globals for block_bitmap and free_blocks
	s_block->bitmap = block_bitmap;
	s_block->free_block_addr = free_block_addr;

	//initialize the block_bitmap
	int i; //set everything to zero
	for(i = 0; i < 1024; i++){
		*block_bitmap = 0;
		block_bitmap++;
	}

	//initialize the inode_array
	for(i = 0; i < INDEX_NODE_COUNT; i++){
		inode_array[i].id = i;
		inode_array[i].allocated = 0;
		inode_array[i].type = 0;
		inode_array[i].size = 0;
		int j;
		for(j = 0; j < 10; j++){
			inode_array[i].block_pointer[j] = NULL;
		}
		inode_array[i].bp_count = 0;
	}

	//need a notion of a root directory:
	inode_array[0].allocated = 1;
	inode_array[0].type = 1;
	inode_array[0].block_pointer[0] = get_free_block();
	inode_array[0].bp_count = 1;
	return 1;
}


//returns a new valid file descriptor associated with an inode_id
int allocate_fd(int pid, int inode_id){
	pnode_fd_table *pnode = NULL;
	pnode_fd_table *traverse = fd_table;
	do{
		if(traverse->pid == pid){
			pnode = traverse;
			break;
		}
		traverse = traverse->next;
	}while(traverse != NULL);
	
	if(pnode != NULL){
		//exists already
		if(pnode->pid == pid){
			int i;
			int index = -1;
			for(i = 0; i < OPENFILES; i++){
				if(pnode->table[i].offset == -1){
					index = i;
					break;
				}
			}
			if(index == -1)
				return -1;
			pnode->table[index].offset = 0;
			pnode->table[index].inode_pointer = &inode_array[inode_id];
			return index;
		}
		return -1;
	}
	pnode = (pnode_fd_table *) vmalloc(sizeof(pnode_fd_table));
	int i;
	for(i = 0; i < OPENFILES; i++){
		init_fd_entry(&(pnode->table[i]));
	}
	pnode->pid = pid;
	pnode->next = fd_table; //insert it into the table
	fd_table = pnode;
	int index = -1;
	for(i = 0; i <OPENFILES; i++){
		if(pnode->table[i].offset == -1){
			index = i;
			break;
		}
	}
	if(index == -1)
		return -1;
	pnode->table[index].inode_pointer = &inode_array[inode_id];
	pnode->table[index].offset = 0;
	return index;					
}

//rd_mkdir creates a directory in a PREXISTING parent directory
//TODO: FINISH
int rd_mkdir(char* pathname){
	char *dir_path;
	char *filename;
	split_dir_file(pathname, &dir_path, &filename);
	int dir_inode_num = get_dir_inode(dir_path, 1); //check if dir_path exists
	//int dir_inode_num = get_dir_inode(pathname, 1);
	//printf("dir_inode_num: %d\n", dir_inode_num);
	if(dir_inode_num == -1)
		return -1;
	int tmp = get_dir_entry(dir_inode_num, filename, 1);
	//printf("here\n");
	if(tmp != -1){
		//directory already exists
		printk("error: dir already exists\n");
		return -1;
	}
	//get ready to allocate inode
	int index = get_free_inode();
	if(index == -1){
		printk("no free inodes\n");
		return -1;
	}

	s_block->free_inodes = s_block->free_inodes - 1;

	init_inode(index, 1);

	inode_array[dir_inode_num].size = inode_array[dir_inode_num].size + sizeof(dir_entry);
	dir_entry* new_dir = get_free_dir_entry(dir_inode_num);

	new_dir->inode_number = index;

	strcpy(new_dir->filename, filename);		

	printk("rd_mkdir created a directory\n");
	return 1;
}

int rd_creat(char* pathname){
	char *dir_path;
	char *filename;
	split_dir_file(pathname, &dir_path, &filename);
	int dir_inode_num = get_dir_inode(dir_path, 1); //check if dir_path exists
	if(dir_inode_num == -1){
		return -1;
	}
	int tmp = get_dir_entry(dir_inode_num, filename, 1);
	if(tmp != -1){
		//directory already exists
		printk("error: dir already exists\n");
		return -1;
	}
	//get ready to allocate inode
	int index = get_free_inode();
	if(index == -1){
		printk("no free inodes\n");
		return -1;
	}
	s_block->free_inodes = s_block->free_inodes - 1;
	init_inode(index, 2); //type = 2, regular file
	
	inode_array[dir_inode_num].size = inode_array[dir_inode_num].size + sizeof(dir_entry);
	dir_entry* new_dir = get_free_dir_entry(dir_inode_num);
	new_dir->inode_number = index;
	strcpy(new_dir->filename, filename);		

	printk("rd_creat created a file\n");
	return 1;
}


//rd_open, will require a pid, really only accessible via the kernel
int rd_open(char* pathname){
	char *dir_path;
	char *filename;

	int pid = 5; //for now its five, fix this later when we aren't in userspace


	split_dir_file(pathname, &dir_path, &filename);

	int root_check = strcmp(dir_path, "/");
	if(filename != NULL && root_check == 0){
		int new_fd = allocate_fd(pid, 0);
		printk("rd_open success\n");
		return new_fd;
	}
	
	int find_inode = get_dir_inode(dir_path, 0);
	//search if such a directory exists
	if(find_inode > -1){
		find_inode = get_dir_entry(find_inode, filename, 0);
		if(find_inode > -1){
			int new_fd = allocate_fd(pid, find_inode);
			printk("rd_open success\n");
			return new_fd;
		}
	}
	printk("rd_open failed\n");
	return -1;
}

int rd_close(int fd){
	if(fd < 0)
		return -1;
	//TODO: pid needs to non manually be put in
	int pid = 5;
	pnode_fd_table* list = find_fd_list(fd_table, 5);
	if(list == NULL)
		return -1;
	if(list->table[fd].inode_pointer == NULL)
		return -1;
	list->table[fd].inode_pointer = NULL;
	list->table[fd].offset = -1;
	printk("rd_close success\n");
	return 0;
}	

//not done
int rd_read(int fd, char* address, int num_bytes){
	int pid = 5;
	if(fd < 0)
		return -1;
	pnode_fd_table* pid_table = find_fd_list(fd_table, pid);
	if(pid_table == NULL)
		return -1;
	
	if(pid_table->table[fd].inode_pointer == NULL)
		return -1;
	if(pid_table->table[fd].inode_pointer->type != 2) //needs to be a regular file
		return -1;
	int bytes_to_read = num_bytes;
	int offset;
	char* current_addr = NULL;
	if(num_bytes > pid_table->table[fd].inode_pointer->size)
		num_bytes = pid_table->table[fd].inode_pointer->size;
	int bytes_to_copy;
	int total_bytes = 0;
	int addr_offset = 0;
	
	do{
		offset = pid_table->table[fd].offset;
		//determine how many bytes we can copy over
		//printf("old offset: %d\n", offset);
		int new_offset = copy_contents_address(pid_table->table[fd].inode_pointer, offset, &current_addr);
		//printf("new_offset: %d\n", new_offset);
		if(current_addr == NULL)
			return -1;
		if(bytes_to_read < (RAMDISK_BLOCK_SIZE - new_offset)){
			bytes_to_copy = bytes_to_read;
		}
		else	
			bytes_to_copy = RAMDISK_BLOCK_SIZE - new_offset;
		
		memcpy(address + addr_offset, current_addr, bytes_to_copy);
		addr_offset = addr_offset + bytes_to_copy;
		//printf("addr_offset:::::::%d\n", addr_offset);
		//printf("current_addr: %x\n", current_addr);
		//printf("current_addr contents: %s\n", current_addr);
		//printf("read_string: %s\n", current_addr);
		
		bytes_to_read = bytes_to_read - bytes_to_copy;
		pid_table->table[fd].offset += bytes_to_copy;
		current_addr = NULL;
		total_bytes = total_bytes + bytes_to_copy;
	}while(bytes_to_read > 0);

	
	return total_bytes;

}
//for now don't use pid as the current struct isn't supported outside of kernel context
int rd_lseek(int fd, int offset) {
	int pid = 5;
    pnode_fd_table* fd_lseek;
	if (fd < 0) {
		return -1;
	}
    fd_lseek = find_fd_list(fd_table, pid);
    if (fd_lseek == NULL) {
        return -1;
    }
    // check if the file is open
    if (fd_lseek->table[fd].inode_pointer == NULL) {
        return -1;
    }
    // lseek doesn't work on dir (denoted by 1)
    if ((fd_lseek->table[fd].inode_pointer->type) == 1) {
        return -1;
    }
    // double check the offset
    int filesize = fd_lseek->table[fd].inode_pointer->size;
    if (offset < 0) {
        offset = 0;
    }
    if (offset > filesize) {
        fd_lseek->table[fd].offset = filesize;
    }
    else {
        fd_lseek->table[fd].offset = offset;
    }
    printk("rd_lseek has moved the file to %d\n", fd_lseek->table[fd].offset);

    return 0;
}

int rd_write(int fd, char* address, int num_bytes){
	//printf("num_bytes: %d\n", num_bytes);
	int pid = 5;
	if(fd < 0)
		return -1;
	pnode_fd_table* pid_table = find_fd_list(fd_table, pid);
	
	if(pid_table == NULL)
		return -1;
	if(pid_table->table[fd].inode_pointer == NULL)
		return -1;
	if(pid_table->table[fd].inode_pointer->type != 2)
		return -1;
	if(num_bytes <= 0)
		return -1;
	int bytes_to_write = num_bytes;
	char *current_addr = NULL;
	int total_bytes = 0;

	int addr_offset = 0;
	do{
		//printf("writing\n");
		int offset = pid_table->table[fd].offset;
		//printf("offff: %d\n", offset);
		//check if we need to allocate a new block
		if((offset % RAMDISK_BLOCK_SIZE == 0) && (offset == pid_table->table[fd].inode_pointer->size)){
			if(offset >= 1067008){
				//printf("error here\n");
				return -1; //files above this size are not supported
			}
			if(offset > 0){
				//else we need to allocate new block
				//printf("new block required\n");
				if(allocate_block_addr(pid_table->table[fd].inode_pointer) == NULL){
					//printf("ddderror\n");
					return -1;
				}
			}
		}
		//printf("before current_addr: %x\n", current_addr);
		
		int new_offset = copy_contents_address(pid_table->table[fd].inode_pointer, offset, &current_addr);
		//printf("after current_addr: %x\n", current_addr);
		
		if(current_addr == NULL)
			return -1;
		int bytes_to_copy = RAMDISK_BLOCK_SIZE - new_offset;
		if(bytes_to_write < (RAMDISK_BLOCK_SIZE - new_offset))
			bytes_to_copy = bytes_to_write;
		//copy over to addr
		//printf("char: %c\n", address[0]);
		//printf("\ninode_addr: %x\n", current_addr);
		memcpy(current_addr, address + addr_offset, bytes_to_copy);
		//printf("c_a: %s\n", current_addr);
		addr_offset = addr_offset + bytes_to_copy;
		//printf("char: %c\n", current_addr[0]);
		//printf("string: %s\n", current_addr);
		//printf("current_addr: %s\n", current_addr);
		//printf("address: %s\n", address);

		bytes_to_write = bytes_to_write - bytes_to_copy;
		total_bytes = total_bytes + bytes_to_copy;
		//printf("TOTAL BYTES: %d\n", total_bytes);
	
		pid_table->table[fd].offset = pid_table->table[fd].offset + bytes_to_copy;
		pid_table->table[fd].inode_pointer->size += bytes_to_copy;
		current_addr = NULL;
		//printf("current_addr: %x\n", current_addr);
	}while(bytes_to_write > 0);
	//address = current_addr;
	//printf("returning\n");
	
	return total_bytes;
}

int rd_readdir(int fd, char* address) {
	int addr, offset;
	char* file_addr;
    pnode_fd_table* fd_t;
    fd_entry fd_e;
    if (fd < 0) {
        return -1; // check if fd is valid
    }
	int pid = 5;

    fd_t = find_fd_list(fd_table, pid);
    fd_e = fd_t->table[fd];
	offset = fd_e.offset;

    if (fd_t == NULL) {
        return -1; // check if file exists
    }
    if (fd_e.inode_pointer->type != 1) {
        return -1; // make sure it is a dir
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
	//printf("file_addr: %s\n", file_addr);
	if (file_addr == NULL) {
		return -1;
	}
	if (RAMDISK_BLOCK_SIZE - addr < 0) {
		return -1;
	}
	int d = strlen(file_addr);

	int i = 0;

	//memcpy(address, file_addr, strlen(file_addr)+1); // copy file
	memcpy(address, file_addr, 16);

	//printf("file_addr: %s\n", file_addr);
	fd_t->table[fd].offset += 16; // increase offset

    return 1;
}

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
        //vfree(dir); 
		//vfree(file);
        return -1;
    }
    file_inode = get_dir_entry(parent_dir, file, 2);
	if (file_inode == -1) {
       // vfree(dir); 
		//vfree(file);
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
		//vfree(dir);
		//vfree(file);
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

	printk("rd_unlink %s is now unlinked\n", pathname);
	return 0;
}
//ioctl wrapper, this could be horribly wrong
int my_ioctl(struct inode *inode, struct file *file, 
                         unsigned int cmd, unsigned long arg){
	

	ramdisk_ioctl params;
	char *path;
	int size;
	int ret;
	char *addr;

	copy_from_user(&params, (ramdisk_ioctl *)arg, sizeof(ramdisk_ioctl));

	switch(cmd){
		case MKDIR_IOWR:
			size = sizeof(char) * (params.path_length + 1);
			path = (char *) vmalloc(size);
			copy_from_user(path, params.pathname, size);
			ret = rd_mkdir(path);
			vfree(path);
			return ret;
			break;
		case CREAT_IOWR:
			size = sizeof(char) * (params.path_length + 1);
			path = (char *) vmalloc(size);
			copy_from_user(path, params.pathname, size);
			ret = rd_creat(path);
			vfree(path);
			return ret;
			break;
		case OPEN_IOWR:
			size = sizeof(char) * (params.path_length + 1);
			path = (char *) vmalloc(size);
			copy_from_user(path, params.pathname, size);
			ret = rd_open(path);
			vfree(path);
			return ret;
			break;
		case CLOSE_IOWR:
			ret = rd_close(params.fd);
			return ret;
			break;
		case READ_IOWR:
			addr = (char *) vmalloc(params.num_bytes);
			ret = rd_read(params.fd, addr, params.num_bytes);
			if(ret > -1)
				copy_to_user(params.address, addr, ret);	
			vfree(addr);
			return ret;
			break;
		case WRITE_IOWR:
			addr = (char *)vmalloc(params.num_bytes);
			copy_from_user(addr, params.address, (unsigned long) params.num_bytes);
			ret = rd_write(params.fd, addr, params.num_bytes);
			vfree(addr);
			return ret;
			break;
		case LSEEK_IOWR:
			ret = rd_lseek(params.fd, params.offset);
			return ret;
			break;
		case READDIR_IOWR:
			addr = (char *) vmalloc(16);
			ret = rd_readdir(params.fd, addr);
			copy_to_user(params.address, addr, 16);
			vfree(addr);
			return ret;
			break;
		case UNLINK_IOWR:
			size = sizeof(char) * (params.path_length + 1);
			path = (char*) vmalloc(size);
			copy_from_user(path, params.pathname, size);
			
			ret = rd_unlink(path);
			vfree(path);
			return ret;
			break;
		default:
			break;
	}
	return 0;

}	

int init_module(void)
{
	struct file_operations fops;
	printk(KERN_INFO "Hello world 1.\n");
	struct proc_dir_entry* ramdisk = create_proc_entry("ramdisk", 0644, NULL);
	if(!ramdisk){
		printk("cannot create /proc/ramdisk\n");
		return 1;
	}
	//ramdisk->owner = THIS_MODULE;
	ramdisk->proc_fops = &fops;
	fops.ioctl = my_ioctl;

	/* 
	 * A non 0 return means init_module failed; module can't be loaded. 
	 */
	init_ramdisk();
	init_fd_table();
	return 0;
}

void cleanup_module(void)
{
	printk(KERN_INFO "Goodbye world 1.\n");
}
