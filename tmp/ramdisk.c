// not in kernel space right now #include <linux/vmalloc.h>

#include "ramdisk.h"

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
	fd_table = (pnode_fd_table *) malloc(sizeof(pnode_fd_table));
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
	disk = malloc(RAMDISK_SIZE);
	if(disk == NULL){
		printf("ramdisk allocation failed\n");
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
	pnode = (pnode_fd_table *) malloc(sizeof(pnode_fd_table));
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
		printf("error: dir already exists\n");
		return -1;
	}
	//get ready to allocate inode
	int index = get_free_inode();
	if(index == -1){
		printf("no free inodes\n");
		return -1;
	}

	s_block->free_inodes = s_block->free_inodes - 1;

	init_inode(index, 1);

	inode_array[dir_inode_num].size = inode_array[dir_inode_num].size + sizeof(dir_entry);
	dir_entry* new_dir = get_free_dir_entry(dir_inode_num);

	new_dir->inode_number = index;

	strcpy(new_dir->filename, filename);		

	printf("rd_mkdir created a directory\n");
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
		printf("error: dir already exists\n");
		return -1;
	}
	//get ready to allocate inode
	int index = get_free_inode();
	if(index == -1){
		printf("no free inodes\n");
		return -1;
	}
	s_block->free_inodes = s_block->free_inodes - 1;
	init_inode(index, 2); //type = 2, regular file
	
	inode_array[dir_inode_num].size = inode_array[dir_inode_num].size + sizeof(dir_entry);
	dir_entry* new_dir = get_free_dir_entry(dir_inode_num);
	new_dir->inode_number = index;
	strcpy(new_dir->filename, filename);		

	printf("rd_creat created a file\n");
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
		printf("rd_open success\n");
		return new_fd;
	}
	
	int find_inode = get_dir_inode(dir_path, 0);
	//search if such a directory exists
	if(find_inode > -1){
		find_inode = get_dir_entry(find_inode, filename, 0);
		if(find_inode > -1){
			int new_fd = allocate_fd(pid, find_inode);
			printf("rd_open success\n");
			return new_fd;
		}
	}
	printf("rd_open failed\n");
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
	printf("rd_close success\n");
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
    printf("rd_lseek has moved the file to %d\n", fd_lseek->table[fd].offset);

    return 0;
}

int rd_write(int fd, char* address, int num_bytes){
	printf("num_bytes: %d\n", num_bytes);
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
					printf("ddderror\n");
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
		printf("c_a: %s\n", current_addr);
		addr_offset = addr_offset + bytes_to_copy;
		//printf("char: %c\n", current_addr[0]);
		//printf("string: %s\n", current_addr);
		//printf("current_addr: %s\n", current_addr);
		//printf("address: %s\n", address);

		bytes_to_write = bytes_to_write - bytes_to_copy;
		total_bytes = total_bytes + bytes_to_copy;
		printf("TOTAL BYTES: %d\n", total_bytes);
	
		pid_table->table[fd].offset = pid_table->table[fd].offset + bytes_to_copy;
		pid_table->table[fd].inode_pointer->size += bytes_to_copy;
		current_addr = NULL;
		//printf("current_addr: %x\n", current_addr);
	}while(bytes_to_write > 0);
	//address = current_addr;
	printf("returning\n");
	
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
	printf("addr\n");
	printf("file_addr: %s\n", file_addr);
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

	printf("rd_unlink %s is now unlinked\n", pathname);
	return 0;
}

int main(int argc, char ** argv){
	init_ramdisk();
	init_fd_table();
	//void *disk = malloc(DISKSIZE);
	//printf("disk: %d\n", disk);
	//void *of1 = getblockoffset(1, disk);

	//void *inode_array = getblockoffset(1, disk); //pointer to the beginning of the inode array
	//void *block_bitmap = getblockoffset(257, disk); //pointer to the beginning of the bitmap array
	//void *files = getblockoffset(260, disk); //pointer to the beginning of the file partition
	char* parents;
	char* filename;
	split_dir_file("/test", &parents, &filename);
	

	int ret = rd_mkdir("/test");
	printf("ret: %d\n", ret);
	ret = rd_mkdir("/g");
	printf("ret: %d\n", ret);
	ret = rd_mkdir("/s/s/s/s/s");
	printf("ret:%d\n", ret);
	ret = rd_mkdir("/test/d1");
	printf("ret: %d\n", ret);
	printf("create file: poop.c in /test/d1\n");
	ret = rd_creat("/test/d1/poop.c");
	printf("ret: %d\n", ret);
	printf("create file: double.c in /test/d1\n");
	ret = rd_creat("/test/d1/double.c");
	printf("ret: %d\n", ret);
	int fd1 = rd_open("/test/d1/poop.c");
	printf("fd1: %d\n", fd1);
	int fd2 = rd_open("/does/not/exist.c");
	printf("fd2: %d\n", fd2);
	int fd3 = rd_open("/test");
	printf("fd3: %d\n", fd3);

	//char buffer[] = "Zeus, in Greek mythology, is the god of the sky and ruler of the Olympian gods. Zeus corresponds to the Roman god Jupiter. Zeus was considered, according to Homer, the father of the gods and of mortals (Martin 309). He did not create either gods or mortals; he was their father in the sense of being the protector and ruler both of the Olympian family and of the human race. He was lord of the sky, the rain god, and the cloud gatherer, who wielded the terrible thunderbolt. His breastplate was the aegis, his bird the eagle and his tree the oak. Zeus presided over the gods on Mount Olympus in Thessaly. His principle shrines were at Dodona, in Epirus, the land of the oak trees and the most ancient shrine, famous for its oracle, and at Olympia. Zeus was the youngest son of the Titans, Cronus and Rhea and the brother of deities Poseidon, Hades, Hestia, Demeter, and Hera. According to one of the ancient myths of the birth of Zeus, Cronus, fearing that he might be dethroned by one of his children, swallowed them as they were born (Martin 367). Upon the birth of Zeus, Rhea wrapped a stone in swaddling clothes for Cronus to swallow and concealed the infant god in Crete, where he was fed on the milk of the goat Amalthea and reared by nymphs. When Zeus grew to maturity, he forced Cronus to disgorge the other children, who were eager to take vengeance on their father. In the war that followed, the Titans fought on the side of Cronus, but Zeus and the other gods were successful, and the Titans were consigned to the abyss of Tatarus. Zeus henceforth ruled over the sky, and his brother Poseidon and Hades were given power over the sea and the underworld, respectively. The earth was to be ruled in common by all three. Beginning with the writings of the Greek poet Homer, Zeus is pictured in two very different ways. “He is the god of justice and mercy, the protector of the weak, and the punisher of the wicked” (Homer I1). As a husband to his sister Hera, he is the father of Ares, the god of war; Hebe, the goddess of youth; Hephaestus, the god of fire; and Eileithyia, the goddess of childbirth. At the same time, Zeus is described as falling in love with one woman after another and resorting to all kinds of tricks to hide his infidelity from his wife. Stories of his escapades were numerous in ancient mythology, and many of the offspring were a result of his love affairs with both goddesses and mortal women. It is believed that, with the development of a sense of ethics in Greek life, the idea of a lecherous, sometimes ridiculous father god became distasteful, so later legends tended to present Zeus in a more exalted light.";

	int k = 0;
	char buffer[10001];
	for(k = 0; k < 10001; k++){
		if(k%4 == 0)
			buffer[k] = 'w';
		else if(k%4 == 1)
			buffer[k] = 'x';
		else if(k%4 == 2)
			buffer[k] = 'y';
		else if(k%4 == 3)
			buffer[k] = 'z'; 
	}
	buffer[10000] = '\0'; //need to close it

	char buffer2[20000];
	
	for(k = 0; k < 20000; k++){
		if(k%4 == 0)
			buffer2[k] = 'w';
		else if(k%4 == 1)
			buffer2[k] = 'x';
		else if(k%4 == 2)
			buffer2[k] = 'y';
		else if(k%4 == 3)
			buffer2[k] = 'z'; 
	}
	buffer2[19999] = '\0';
	printf("buffer2, len: %d\n", strlen(buffer2));
	int fd5 = rd_open("/test/d1/double.c");
	printf("fd5: %d\n", fd5);

	int write2 = rd_write(fd5, buffer2, strlen(buffer2));
	printf("write: %d\n", write2);
	
	int seek2 = rd_lseek(fd5, 0);
	
	char* read2 = (char *) malloc(20000);
	int read_b = rd_read(fd5, read2, 20000);
	printf("%s\n", read2);
	printf("read %d bytes\n", read_b);

	int dir_fd = rd_open("/test/d1");
	printf("dir_fd: %d\n", dir_fd);

	char* dir = malloc(sizeof(dir_entry));
	char* dir_name;
	short inode_num;
	dir_entry* entry;

	int dir_ret = rd_readdir(dir_fd, (char *)dir);
	entry = (dir_entry *) dir;
	

	printf("dir_ret: %d\n", dir_ret);
	printf("entry name: %s\n", entry->filename);
	printf("inode_num: %d\n", entry->inode_number);
	printf("l: %c\n", entry[0]);
	
	
	
/*
	int len = strlen(buffer);
	printf("len: %d\n", len);
	
	int write = rd_write(fd1, buffer, len);
	printf("write: %d\n", write);
	int num_bytes = len;

	//seek back to the beginning so we can read it again
	int seek = rd_lseek(fd1, 0);
	printf("seek: %d\n", seek);

	
	char* read = (char *)malloc(num_bytes);
	int read_bytes = rd_read(fd1, read, num_bytes);
	printf("read_bytes: %d\n", read_bytes);
	printf("%s\n", read);
	*/

	
	//get_dir_inode("/test/example/f1");
}

