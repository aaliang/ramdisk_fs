#include <ramdisk.h>

static superblock* sb;

/*
	int rd_creat(char *pathname)

	Create a regular file with absolute pathname from the root of the directory tree,
	where each directory filename is delimited by a "/" character. On success, you
	should return 0, else if the file corresponding to pathname already exists you
	should return -1, indicating an error. Note that you need to update the parent
	directory file, to include the new entry.
*/
int rd_creat(char* pathname) 
{
    char* filename;
    
    dir_entry* free_dir_entry;

	// check if the Ramdisk is full
    if ((sb->free_inodes < 1) || (sb->free_blocks < 1)) 
	{
		printk("The ramdisk is full.\n");
        	
		return -1;
    }

    // TODO: 
	// check if the parent directory exists and the file doesn't already exist 

    // Get the filename
    filename = strrchr(pathname, '/');
    filename++;

	// TODO:
    // run this loop if the above is true
    if (// if parent directory/file doesn't exist) 
	{
		printk("File created successfully.\n");
        
	    return 1;
	}

	printk("Error creating requested file.\n");
    	
	return -1;
}
