#include <linux/string.h>
//#include <stdio.h>
//#include <stdlib.h>

// Ramdisk definitions, in bytes
#define RAMDISK_SIZE 2097152
#define RAMDISK_BLOCK_SIZE 256


// Index node definitions, in bytes
#define INDEX_NODE_SIZE 64
#define INDEX_NODE_COUNT 1024

#define OPENFILES 100

//forward definitions (just the ones that i need)
void* allocate_block(int);
void* get_free_block();
int get_dir_entry(int, char *, int);
void split_dir_file(char *, char **, char **);
void my_set_bit(unsigned int*, int);

// Type definitions
typedef struct i_node {
	int id;
	short type; //0 if not in use, 1 for directory, 2 for regular file
	int size;
	int allocated; //0 if free, 1 if in use, might not be necessary since type will basically take the same role
	void* block_pointer[10];
	int bp_count;
} i_node;

typedef struct single_indirect{
	void* block_pointer[64];
} single_indirect;

typedef struct double_indirect{
	single_indirect* indirect_block_pointer[64];
} double_indirect;

typedef struct superblock{
    int free_blocks;
    int free_inodes;
	char *bitmap; //address of the bitmap
	void *free_block_addr; //address of where the freeblocks start
} superblock;

typedef struct dir_entry{
	char filename[14]; //14 bytes, including \0 terminator
	short inode_number;
} dir_entry;

typedef struct fd_entry{
	int offset; //current offset in file
	i_node * inode_pointer;
}fd_entry;

//each process has a node
typedef struct pnode_fd_table{
	int pid;
	fd_entry table[OPENFILES]; //each process can open up to 100 files
	struct pnode_fd_table *next;
} pnode_fd_table;

///////////////////////
//GLOBAL DECLARATIONS
void* disk;
superblock * s_block; //superblock
i_node* inode_array;
char* block_bitmap;
void* free_block_addr;

//////////////////////
//FUNCTIONS

void init_inode(int index, int type){
	inode_array[index].allocated = 1;
	inode_array[index].type = type;
	inode_array[index].size = 0;
	inode_array[index].bp_count = 1;
	inode_array[index].block_pointer[0] = get_free_block();
}


void init_fd_entry(fd_entry *entry){
	entry->offset = -1;
	entry->inode_pointer = NULL;
}


//traverse an ABSOLUTE PATHNAME and its associated dir_entries, returning the inode number for that pathname, can be either regular file or directory
int get_dir_inode(char *path_name, int type){

	//according to: docs.sun.com/app/docs/doc/806-4422/6jdgkltde?l=en&a=view
	//strsep will segfault if it is not used this way...

	char** pathname = &path_name;
	char* tmp = path_name;
	//tmp = strdup(path_name);
	pathname = &tmp;

	//ex. pathname: "/usr/example/folder1
	//printf("spath: %s\n",*pathname);
	int num = 0;
	int inode_index = 0;
	int check = strcmp(*pathname, "/");
	if(check == 0){
		//printf("this is a root node\n");
		num = 0;
		return num;
	}

	char* current_dir;
	strsep(pathname, "/");
	//needs to be called once before, since there is technically null before the first "/"
	current_dir = strsep(pathname, "/");
	//printf("current_directory: %s\n", current_dir);
	//printf("x: %x\n", current_dir);


	while(strlen(current_dir) != 0){
		//printf("in: %s\n", current_dir);
		num = get_dir_entry(inode_index, current_dir, type);
		if(num == -1)
			return -1;
		else{
			inode_index = num;
			current_dir = strsep(pathname, "/");
		}
	}
	return inode_index;
}

//checks if a file exists in a given block in the ram_disk
int is_block_entry(void *addr, char *file_name, int type){
	void* traverse = addr;
	//number of directory entries in a single block
	int entries = 256/sizeof(dir_entry);
	//iterate over each directory entry in a block
	int i;
	int tmp_inode_num;
	int tmp_inode_type;
	char* tmp_file_name;

	for(i = 0; i < entries; i++){
		tmp_inode_num = ((dir_entry*)traverse)->inode_number;
		tmp_file_name = ((dir_entry*)traverse)->filename;
		//printf("tmp_file_name: %s\n", tmp_file_name);
		//tmp_inode_type = ((dir_entry*)traverse)->type;
		tmp_inode_type = inode_array[tmp_inode_num].type;
		if(strcmp(file_name, tmp_file_name) == 0){
			//check the type
			if(type == 0 || type == tmp_inode_type){
				//return the inode number if we have a match
				return tmp_inode_num;
			}
		}
		traverse = traverse + sizeof(dir_entry);
	}
	return -1; //nothing found
}

//checks if the parent directory for a given pathname exists -- not used so far
int parent_dir_exists(char* pathname){
	char* file_name;
	char* dir;
	split_dir_file(pathname, &dir, &file_name);
	int dir_inode_num = get_dir_inode(dir, 0);
	if(dir_inode_num == -1)
		return -1;
	//the directory exists

}


//find
void* get_free_slot(void *addr){
	//void *directory_entry = addr;
	int directory_entries = RAMDISK_BLOCK_SIZE/sizeof(dir_entry);
	dir_entry * traverse = (dir_entry *) addr;
	int i;
	for(i = 0; i < directory_entries; i++){
		char *filename = traverse->filename;
		int inode_index = traverse->inode_number;
		if(strcmp(filename, "/") != 0 && inode_index == 0){
			return (void *) traverse;
		}
		traverse = ((void*)traverse) + sizeof(dir_entry);
	}
	return NULL;
}

//returns a pointer to a free directory entry given an index to the inode_array
dir_entry* get_free_dir_entry(int inode_index){
	//printf("parent: %d\n", inode_index);

	//need to worry about direct, single-indirect, and doubly indirects
	//first 8 block pointers: direct
	//9th block pointer: single indirect
	//10th pointer: double indirect

	//if we cant find it on the direct level, move on to single, double
	void* block_pointer;

	int bp_count = inode_array[inode_index].bp_count;
	//printf("bp_count: %d\n", bp_count);
	if(bp_count < 9){
		void* addr = inode_array[inode_index].block_pointer[bp_count-1];
		dir_entry* slot = (dir_entry *)get_free_slot(addr);
		if(slot != NULL)
			return slot;
	}

	//nothing free, go on to single indirect

	if(bp_count > 8){
		single_indirect *si = (single_indirect *)inode_array[inode_index].block_pointer[8];
		int i;
		for(i = 0; i < 64; i++){
			void* dir_ent = si->block_pointer[i];
			if(dir_ent == NULL)
				break;
			dir_entry *slot = (dir_entry *) get_free_slot(dir_ent);
			if(slot != NULL)
				return slot;
		}
	}
	//double indirect now
	if(bp_count > 9){
		double_indirect* di = (double_indirect *) inode_array[inode_index].block_pointer[9];
		int i;
		for(i = 0; i < 64; i++){
			single_indirect *si = (single_indirect*)(di->indirect_block_pointer[i]);
			if(si == NULL)
				break;
			int j;
			for(j = 0; j < 64; j++){
				void* dir_ent = si->block_pointer[j];
				if(dir_ent == NULL)
					break;
				dir_entry *slot = (dir_entry *) get_free_slot(dir_ent);
				if(slot != NULL)
					return slot;
			}
		}
	}

	//need to allocate a new block
	void* b_index = allocate_block(inode_index);
	return (dir_entry *) b_index;
}



//gets the inode number of a specified directory entry name, starting at a specific inode_index

int get_dir_entry(int inode_index, char *dir_name, int type){

	//need to worry about direct, single-indirect, and doubly indirects
	//first 8 block pointers: direct
	//9th block pointer: single indirect
	//10th pointer: double indirect

	//if we cant find it on the direct level, move on to single, double
	int direct;
	void* block_pointer;

	int bp_count = inode_array[inode_index].bp_count;
	if(bp_count < 8){
		direct = bp_count;
	}
	else
		direct = 8;
	int i;
	int inode_num = -1;
	for(i = 0; i < direct; i ++){
		block_pointer = inode_array[inode_index].block_pointer[i];
		inode_num = is_block_entry(block_pointer, dir_name, type);
		//printf("inode_num: %d", inode_num);
		if(inode_num != -1)
			return inode_num;
	}

	inode_num = -1;

	//take care of single indirect pointer
	if(bp_count > 8){
		//point block_pointer to the 9th pointer
		block_pointer = inode_array[inode_index].block_pointer[8];
		//single indirect points to 64 pointers
		for(i = 0; i < 64; i ++){
			void* tmp_bp = ((single_indirect *)block_pointer)->block_pointer[i];
			if(tmp_bp == NULL)
				break;
			inode_num = is_block_entry(tmp_bp, dir_name, type);
			if(inode_num != -1)
				return inode_num;
		}
	}
	//go on to double indirect if we have to

	if(bp_count > 9){
		block_pointer = inode_array[inode_index].block_pointer[9];
		//basically do the single indirect within another level of indirection
		single_indirect *si;
		for(i = 0; i < 64; i ++){
			//heres a single indirect pointer
			si = ((double_indirect *)block_pointer)->indirect_block_pointer[i];
			if(si == NULL)
				return -1;
			int j;
			for(j = 0; j < 64; j++){
				if(si->block_pointer[j]==NULL)
					return -1;
				inode_num = is_block_entry(si->block_pointer[j], dir_name, type);
				if(inode_num != -1)
					return inode_num;
			}
		}
	}
	//if we progressed this far, it doesn't exist
	return -1;
}

//will give you the directory and filename given an absolute pathname
void split_dir_file(char *pathname, char **dir, char **file_name){
	char* cpname;
	char* wd;
	int wd_index;
	char *tmp;
	int length = strlen(pathname);
	cpname = (char*) vmalloc(length+1);
	strcpy(cpname, pathname);
	wd = strrchr(cpname, '/'); //we are interested in the last occurrence of the slash, aka the working directory
	wd_index = wd - cpname;
	*dir = (char*) vmalloc(wd_index + 2);
	memcpy(*dir, cpname, wd_index+1);
	tmp = *dir;
	tmp = wd_index + 1 + tmp;
	*tmp = '\0';

	*file_name = (char*) vmalloc(1 * (length - wd_index));
	strcpy(*file_name, wd + 1);
}

//gets you the index to a free inode in the inode_array
int get_free_inode(){
	int i = -1;
	for(i = 0; i < INDEX_NODE_COUNT; i++){
		if(inode_array[i].allocated == 0){
			return i;
		}
	}
	return i;
}

//updates the block_pointer for an inode
void* allocate_block(int inode_number){
	i_node* node = &inode_array[inode_number];
	if(node->bp_count < 8){
		node->block_pointer[node->bp_count] = get_free_block();
		int tmp = node->bp_count;
		node->bp_count++;
		return node->block_pointer[tmp];
	}
	//go on to single indirect
	if(node->bp_count >= 8){
		single_indirect * si = (single_indirect *) node->block_pointer[8];
		//first single indirect
		if(node->bp_count == 8){
			si = get_free_block();
			int m;
			for(m = 0; m < 64; m++){
				si->block_pointer[m] = NULL;			
			}
			si->block_pointer[0] = get_free_block();
			node->bp_count++;
			node->block_pointer[8] = si;
			return si->block_pointer[0];
		}
		int i;
		for(i = 0; i < 64; i++){
			//get the first free first indirect block pointer
			if(si->block_pointer[i] == NULL){
				si->block_pointer[i] = get_free_block();
				return si->block_pointer[i];
			}
		}
		//go on to double indirect
		if(node->bp_count == 9){
			//need a double indirect
			node->block_pointer[9] = get_free_block();
			double_indirect * di = (double_indirect *) node->block_pointer[9];
			di->indirect_block_pointer[0] = (single_indirect *) get_free_block();
			di->indirect_block_pointer[0]->block_pointer[0] = get_free_block();
			node->bp_count++;
		}
		if(node->bp_count == 10){
			//double indirect already exists
			double_indirect * di = (double_indirect *) node->block_pointer[9];
			int i;
			for(i = 0; i < 64; i++){
				if(di->indirect_block_pointer[i] == NULL){
					di->indirect_block_pointer[i] = get_free_block();
					di->indirect_block_pointer[i]->block_pointer[0] = get_free_block();
					((double_indirect *) node->block_pointer[9])->indirect_block_pointer[i] =  di->indirect_block_pointer[i];
					return di->indirect_block_pointer[i]->block_pointer[0];
				}
				single_indirect *si = di->indirect_block_pointer[i];
				int j;
				for(j = 0; j < 64; j++){
					if(si->block_pointer[j] == NULL){
						si->block_pointer[j] = get_free_block();
						return si->block_pointer[j];
					}
				}
			}
		}
	}
	return NULL;
}

//same exact thing as allocate_block except instead of the inode index from the array as an index, we pass the address of the node
void* allocate_block_addr(i_node * node){
	//printf("allocating\n");
	//printf("bp_count: %d\n", node->bp_count);
	if(node->bp_count < 8){
		//printf("here\n");
		node->block_pointer[node->bp_count] = get_free_block();
		int tmp = node->bp_count;
		node->bp_count++;
		//printf("get free block scucss\n");
		return node->block_pointer[tmp];
	}
	//go on to single indirect
	if(node->bp_count >= 8){
		single_indirect * si = (single_indirect *) node->block_pointer[8];
		//printf("getting single indirect pointer\n");
		//first single indirect
		if(node->bp_count == 8){
			si = get_free_block();
			int m;
			for(m = 0; m < 64; m++){
				si->block_pointer[m] = NULL;
			}
			//printf("si allocated\n");
			si->block_pointer[0] = NULL;
			//printf("requesting\n");
			void* tmp = get_free_block();
			//printf("tmp: %x\n", tmp);
			si->block_pointer[0] = tmp;
			//si->block_pointer[0] = tmp;
			//printf("got free block\n");
			node->bp_count++;
			node->block_pointer[8] = si;
			return si->block_pointer[0];
		}
		int i;
		for(i = 0; i < 64; i++){
			//printf("looking for free indirect block pointer, i: %d\n", i);
			//get the first free first indirect block pointer
			if(si->block_pointer[i] == NULL){
				si->block_pointer[i] = get_free_block();
				//printf("here\n");
				return si->block_pointer[i];
			}
		}
		//printf("moving on to double indirect\n");
		//go on to double indirect
		if(node->bp_count == 9){
			//need a double indirect
			node->block_pointer[9] = get_free_block();
			double_indirect * di = (double_indirect *) node->block_pointer[9];
			di->indirect_block_pointer[0] = (single_indirect *) get_free_block();
			di->indirect_block_pointer[0]->block_pointer[0] = get_free_block();
			node->bp_count++;
		}
		if(node->bp_count == 10){
			//double indirect already exists
			double_indirect * di = (double_indirect *) node->block_pointer[9];
			int i;
			for(i = 0; i < 64; i++){
				if(di->indirect_block_pointer[i] == NULL){
					di->indirect_block_pointer[i] = (single_indirect *) get_free_block();
					di->indirect_block_pointer[i]->block_pointer[0] = get_free_block();
					((double_indirect *) node->block_pointer[9])->indirect_block_pointer[i] = di->indirect_block_pointer[i];
					return di->indirect_block_pointer[i]->block_pointer[0];
				}
				single_indirect *si = di->indirect_block_pointer[i];
				int j;
				for(j = 0; j < 64; j++){
					if(si->block_pointer[j] == NULL){
						si->block_pointer[j] = get_free_block();
						return si->block_pointer[j];
					}
				}
			}
		}
	}
	return NULL;
}





unsigned int bit_position(unsigned int index){
	return (0x01 << index); //position of the bit
}

void my_set_bit(unsigned int* value, int position) {
    *value |= position;
}

void* get_free_block(){
	//printf("getting free block\n");
	int bitmap_size = RAMDISK_BLOCK_SIZE*4; //bitmap is 4 blocks big
	int i;
	//go through all the bytes in the block
	
	for(i = 0; i < bitmap_size; i++){
		int j; //bit operations, 8 bits to a byte
		for(j = 0; j < 8; j++){
			unsigned int bits = *(s_block->bitmap + i);
			unsigned int bits2 = bits&bit_position(j);
			if(bits2 == 0){
				//printf("here\n");
				//we are free
				//printf("free_blocks: %d", s_block->free_blocks);
				s_block->free_blocks = s_block->free_blocks - 1;

				//printf("bit_position j %d", bit_position(j));
				int nn = bit_position(j);
				//set the bitmap
				//*bits |= j;
				my_set_bit((unsigned int *)(s_block->bitmap + i), nn);
				int block_pos = (i * 8) + j;
				//printf("free_block_addr: %x\n", s_block->free_block_addr);
				char* block_addr = s_block->free_block_addr + (block_pos * RAMDISK_BLOCK_SIZE);
				memset(block_addr, 0, RAMDISK_BLOCK_SIZE);
				return block_addr;
			}
		}
	}
	//printf("cannot get free block\n");
	return NULL;

}

// find a file descriptor list with a given pid
pnode_fd_table* find_fd_list(pnode_fd_table* fd_t, int pid) {
	while (fd_t) {
        if (fd_t->pid == pid) {
            return fd_t;
		}
        fd_t = fd_t->next;
    }
    return NULL;
}

//copies data over from a specified inode address over to current_addr, returns the new offset. basically points current_addr at the correct position in file
int copy_contents_address(i_node* inode_ptr, int offset, void** current_addr) {
    int block_num;
    char* blockPtr;

	//printf("offset::::::: %d\n", offset);

    if (offset < (256*8)) { //direct
        block_num = offset / RAMDISK_BLOCK_SIZE;

        // Point to the first address of the block
        void* block_pointer = inode_ptr->block_pointer[block_num];

       	int byte_shift = offset % RAMDISK_BLOCK_SIZE;  //remainder that carries over to offset data in the block
        block_pointer =  block_pointer + byte_shift; //advance the tmp addr by the block offset
        *current_addr = block_pointer;
		return byte_shift;
    }
    // Exists in single indirect, so filePositionTemp points to a block of pointers to blocks
    if (offset < (64*256+256*8)) {
		//printf("indirect, copy\n");
        single_indirect* si = (single_indirect*) inode_ptr->block_pointer[8];
        offset = offset - 256*8;

        int single_offset = offset / RAMDISK_BLOCK_SIZE;
		//printf("single_offset: %d\n", single_offset);
        void* block_pointer = si->block_pointer[single_offset];
	

        int byte_shift = offset % RAMDISK_BLOCK_SIZE;
        block_pointer = block_pointer + byte_shift;
        *current_addr = block_pointer;
		return byte_shift;
    }

    if (offset < (64*64*256+64*256+256*8)) {
        double_indirect * di = (double_indirect*) inode_ptr->block_pointer[9];

        // Subtract size of direct blocks and single indirect blocks
        offset = offset - 64*256;
        offset = offset - 256*8;

        // Divide by size of single indirect block to determine offset
        // withing double indirect block
        int indirect_shift = offset / (64*256);

        single_indirect *si = (single_indirect *) di->indirect_block_pointer[indirect_shift];
        // Decrement filePosition by the number of (single indirect * increment)
		offset = offset - (indirect_shift * 64 *256);

        // Divide by block size to determine increment in single indirect block
        indirect_shift = offset / RAMDISK_BLOCK_SIZE;
        void* block_pointer = si->block_pointer[indirect_shift];

        // Find the shift within the block containing data
        int byte_shift = offset % RAMDISK_BLOCK_SIZE;
        block_pointer = block_pointer + byte_shift;
        *current_addr = block_pointer;
		return  byte_shift;
    }
    return -1;
}

//helper for rd_unlink for deleting dir/file entry from parent node
int unlinker(int id, char* file, char* type) {
    int bp_count, direct_count, dir_entry;
    char* block_addr;
    single_indirect* single_block;
    double_indirect* double_block;
    int i, count;

    bp_count = inode_array[id].bp_count;
	direct_count = 0;
    if (bp_count > 8) {
        direct_count = 8;
    }
    else {
        direct_count = bp_count;
    }

	count = 0;
	// direct pointers
    while (count < direct_count) {
        block_addr = inode_array[id].block_pointer[count];
        dir_entry = is_block_entry(block_addr, file, type);
        if (dir_entry != -1) {
            return dir_entry;
        }
        count++;
    }
    // single indirect pointers
    if (bp_count > 8) {
        single_block = (single_indirect*) inode_array[id].block_pointer[8];
        for (i = 0; i < 64; i++) {
            if (!single_block->block_pointer[i]) {
                break;
            }
            dir_entry = is_block_entry(single_block->block_pointer[i], file, type);
            if (dir_entry != -1) {
                return dir_entry;
            }
        }
    }
    // double indirect pointers
    if (bp_count == 10) {
        double_block = (double_indirect*) inode_array[id].block_pointer[9];
        for (i = 0; i < 64; i++) {
            if (!double_block->indirect_block_pointer[i]) {
                break;
            }
            for (i = 0; i < 64; i++) {
                if (!single_block->block_pointer[i]) {
                    break;
                }
                dir_entry = is_block_entry(single_block->block_pointer[i], file, type);
                if (dir_entry != -1) {
                    return dir_entry;
                }
            }
        }
    }
    return -1;
}
