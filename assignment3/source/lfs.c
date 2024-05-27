#include <fuse.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <math.h>
#include "lfs.h"


// used for touch when making a file
int	lfs_mknod(const char * path, mode_t mode, dev_t device){
	printf("mknod: (path=%s)\n", path);
	
	return createEntry(path, OmegaTable, 1);
}

// deletes a file
int lfs_unlink(const char * path){
	printf("unlink: (path=%s)\n", path);

	struct entry* ptr = getEntry(path);

	if(ptr->data){
		free(ptr->data);
	}
	int tmpID = ptr->ID;
	int tmpsID = ptr->dirID;
	struct table* dir = OmegaTable->entries[ptr->parentID]->dirTable;
	
	

	rmEntry(OmegaTable, tmpID);
 	rmEntry(dir, tmpsID);

	freeDir(ptr);
	

	return 0;
}

// removes a directory
int lfs_rmdir(const char* path){
	printf("rmdir: (path=%s)\n", path);

	struct entry* ptr = getEntry(path);

	if(ptr->dirTable){
		if(ptr->dirTable->size > -1){
			return -1;
		}
	}

	int tmpID = ptr->ID;
	int tmpsID = ptr->dirID;
	struct table* dir = OmegaTable->entries[ptr->parentID]->dirTable;
	

	rmEntry(OmegaTable, tmpID);
 	rmEntry(dir, tmpsID);
	freeDir(ptr);
	
	return 0;
}

// creates a directory
int lfs_mkdir(const char *path, mode_t mode){
	printf("mkdir: (path=%s)\n", path);
	return createEntry(path, OmegaTable, 0);
}

// renames a file entry
int lfs_rename(const char * path, const char * buf){
	printf("rename: (path=%s)\n", path);
	struct entry* ptr = getEntry(path);


	char* token,*str, *lastStr;
	str = strdup(buf); // something about permission, leaving commented out in case i need down the road. Important
	// iterates through path in slices. removes a "/" each iteration.
	lastStr = str;

	while ( (token = strsep(&str, "/")) != NULL && str != NULL){
		lastStr = str;	
	}
	
	// get a pointer pointing to new struct entry
	
	size_t size = sizeof(char)*strlen(lastStr) + (sizeof(int)) +1;
	if(ptr->ID > 9){
		snprintf(ptr->name, size+1, "%d-%s", ptr->ID, lastStr);

	}else{
		snprintf(ptr->name, size+2, "0%d-%s", ptr->ID, lastStr);
	}
	return 0;
}

int lfs_utime(const char * path, struct utimbuf* time){
	printf("utime: (path=%s)\n", path);
	struct entry* ptr = getEntry(path);
	ptr->accT = time->actime;
	ptr->modT = time->modtime;
	return 0;
}

// currently the 0 index is always empty, and thus getattr will return a null struct everytime it finds one. Should inf another way?
// Burde fungerer ordentligt for nu.
int lfs_getattr( const char *path, struct stat *stbuf ) {
	int res = 0;
	printf("getattr: (path=%s)\n", path);
	
	struct entry* tmpFile = getEntry(path);

	

	memset(stbuf, 0, sizeof(struct stat));
	if( strcmp( path, "/" ) == 0 ) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 1;
	} else if( tmpFile != NULL) {
		printf("ID gotten : %d\n", tmpFile->ID);
		if(tmpFile->name != 0){	
			// if the file contains data it's a regular file
			if(tmpFile->size > -1){
				// printf("found a file\n");
				// printf("size of found file: %d\n",tmpFile->size);
				stbuf->st_mode = S_IFREG | 0777;
				stbuf->st_nlink = 1;
				stbuf->st_size = tmpFile->size;
			// if data is NOT(true) = NULL it's a directory
			}else if(!tmpFile->data){
				// printf("found a dir\n");
				stbuf->st_mode = S_IFDIR | 0755;
				stbuf->st_nlink = 1;
			}else{
				res = -ENOENT;}
			//	adding attributes to stbuf

			stbuf->st_uid = tmpFile->ownerID;
			stbuf->st_gid = tmpFile->groupID;
			stbuf->st_atime = tmpFile->accT;
			stbuf->st_mtime = tmpFile->modT;
		}
	} else{
		res = -ENOENT;}
	
	return res;
}

// path is simply a string
// buf is the buffer *the command prints
// filler not sure exac	tly. 
// offset?
// fi is file info
int lfs_readdir( const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi ) {
	(void) offset;
	(void) fi;
	printf("readdir: (path=%s)\n", path);
	// 	struct entry* ptr;

	// I need a way to find the current directory from the given path
	// calloc initializes memory by making it zeros.

	// string compare of path and "/" so if the directory is not root, it will return an error

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);
	struct entry* ptr = getEntry(path);
	
	if(ptr == NULL){
		return 0;
	}

	// Am i looping throug all directories the directories of the parent, or are they not sored here.
	// loop through current directory and name them all.
	if(ptr->dirTable == NULL){
		return 0;
	}
	struct entry* tmpPtr;
	for(int i = 0;i < ptr->dirTable->size+1; i++){
		tmpPtr = ptr->dirTable->entries[i];
		
		if(tmpPtr != NULL){
			filler(buf, tmpPtr->name, NULL, 0);
		}
	}
	return 0;
}

//Permission
int lfs_open( const char *path, struct fuse_file_info *fi ) {
    printf("open: (path=%s)\n", path);
	// calloc sets all memory to 0
	// thus i don't have to worry about reading "old" memory
	struct entry* file = calloc(1, sizeof(struct entry));

	file->accT = time(NULL);

	if((file = getEntry(path))){
		fi->fh = (uint64_t)file;
	}

	return 0;
}

int lfs_read( const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi ) {
    printf("read: (path=%s)\n", path);
	
	struct entry* ptr = (struct entry*) fi->fh;
	if(ptr->size == 0){
		return 0;
	}
	memcpy(buf, ptr->data, size);
	return size;
}


int write_data(struct entry* ptr, const char* buf, size_t buf_size){

	// allocate space for buf data + old data
	void* tmp = calloc(buf_size + ptr->size, 1);

	// write old data from ptr, size of ptr->size to new allocation:
	memcpy(tmp, ptr->data, ptr->size);

	// now print to where old data left off ptr->size
	memcpy(tmp+ptr->size, buf, buf_size);

	// a buffer to read from, and the size i'm reading. 
	ptr->size = (int) buf_size + ptr->size;

	// free old memory
	free(ptr->data);

	// have to remember to return the amount written
	ptr->data = tmp;

	ptr->modT = time(NULL);

	return (int)buf_size;
}

// need to rewrite write, such that it adds to the end of the data, fiarly sure the point is you write to what is last, and then call several times. 
int lfs_write(const char *path, const char* buf, size_t buf_size, off_t offset, struct fuse_file_info* fi){
	printf("write: (path=%s)\n", path);

	// I'm given at path to the file i want to write to, however i should be writing to fi->fh
	struct entry* ptr = (struct entry*) fi->fh;

	return 	write_data(ptr, buf, buf_size);;
}


int lfs_release(const char *path, struct fuse_file_info *fi) {
	printf("release: (path=%s)\n", path);
	return 0;
}

int intSize(int x){
 	if (x >= 1000000000) return 10;
    if (x >= 100000000)  return 9;
    if (x >= 10000000)   return 8;
    if (x >= 1000000)    return 7;
    if (x >= 100000)     return 6;
    if (x >= 10000)      return 5;
    if (x >= 1000)       return 4;
    if (x >= 100)        return 3;
    if (x >= 10)         return 2;
    return 1;
}

// the given pointer has to be empty
// using fread moves ptr each time. 
int lfs_read_once( void** buf, FILE* ptr){

	int size = 0;
	int read; 
	read = fread(&size, 1, 1, ptr);
	*buf = malloc(size);
	printf("%d",size);
	read = fread(*buf, size, 1, ptr);

	return read;
}

// maybe reqiret to two versions, one for char and one for int
int lfs_write_once(void* data, int size, void* wp){

	printf("%d", size);
	// struct size_t is always the size 8, 64 bits or 8 bytes.
	fwrite(&size, 1, 1, wp);
	fwrite(data, size, 1, wp);
	return size +1;
	}

// attempts to read an entry from the given file pointer
// returns NULL, if it doesn't pass the check for entry
int lfs_read_entry(FILE* wp, struct entry* ptr){
	//printf("ID: --------- %d\n", (int) sizeof(ptr->ID));
	//printf("name: ------- %d\n", (int) sizeof(ptr->name));
	//printf("pathName: --- %d\n", (int) sizeof(ptr->pathName));

	void* buf = 0;
	int x;
	char* y;
	int read;
	
	// I need to write a check, if there is an entry in the block.
	char* entry = calloc(1,1);
	read = fread(entry, 1, 1, wp);	
	if(*entry != 101){
		fseek( wp, -1, SEEK_CUR );
		return 0;
	}
	printf("e");

	// ptr->ID
	read = lfs_read_once(&buf, wp);
	x = *(int*)buf;
	printf("%d", x);

	ptr->ID = x;

	// wp->name
	read = lfs_read_once(&buf, wp);
	y = (char*)buf;
	printf("%s", y);

	ptr->name = malloc(sizeof(char)*strlen(y));
	ptr->name = y;

	// wp->pathName
	read = lfs_read_once(&buf, wp);
	y = (char*)buf;
	printf("%s", y);
	
	ptr->pathName = malloc(sizeof(char)*strlen(y));
	ptr->pathName = y;

	

	// wp->parent
	read = lfs_read_once(&buf, wp);
	x = *(int*)buf;
	printf("%d", x);
	ptr->parentID = x;

	// wp->dirID
	read = lfs_read_once(&buf, wp);
	x = *(int*)buf;
	printf("%d", x);
	ptr->dirID = x;

	// wp->groupID
	read = lfs_read_once(&buf, wp);
	x = *(int*)buf;
	printf("%d", x);
	ptr->groupID = x;


	// wp->ownerID
	read = lfs_read_once(&buf, wp);
	x = *(int*)buf;
	printf("%d", x);
	ptr->ownerID = x;


	long int z;	
	// wp->modT - long int ld
	read = lfs_read_once(&buf, wp);
	z = *(long int*)buf;
	printf("%ld", z);
	ptr->modT = z;


	// wp->accT - long int ld
	read = lfs_read_once(&buf, wp);
	z = *(long int*)buf;
	printf("%ld", z);
	ptr->accT = z;

	//	printf("\n");

	//read check first
	// wp->data
	read = lfs_read_once(&buf, wp);
	y = (char*)buf;
	printf("%s", (char*)y);

	// if there is data stored
	if( *y > 110 ){
		// there is data to be read. 
		// i should read data as an integer, and save it as size, and later load data.


		// size = (wp->dirTable->size+1) * sizeof(int);
		read = lfs_read_once(&buf, wp);
		y = (char*)buf;

		char* tmpptr;
		x = strtol(y, &tmpptr, 10);
		printf("%d", x);
		
		if(x != 0){
			// set data pointer
			ptr->data = y;
			// get the size
			read = lfs_read_once(&buf, wp);
			x = *(int*)buf;
			printf("%d", x);
			ptr->size = x;
		}else{
			char* string = "0";
			ptr->data = malloc(sizeof(char)*strlen(string));
			ptr->data = string;
			printf("%s", string);
			ptr->size = 0;
		}
	}else{
		ptr->data = NULL;
		ptr->size = -1;
	}
	return read;
}
int lfs_write_entry(FILE* wp, struct entry* ptr){

	// I need to write a check, if there is an entry in the block.
	char* buf = "e";
	fwrite(buf, 1, 1, wp);	
	printf("e");
	int size;
	// ptr->ID
	// size = intSize(ptr->ID) * sizeof(int);
	size = sizeof(int);
	printf("%d", ptr->ID);
	lfs_write_once(&ptr->ID, size, wp);
	
	// ptr->name
	size = strlen(ptr->name) * sizeof(char);
 	printf("%s", ptr->name);
	lfs_write_once(ptr->name, size, wp);

	// ptr->pathName
	size = strlen(ptr->pathName) * sizeof(char);
	printf("%s", ptr->pathName);
	lfs_write_once(ptr->pathName, size, wp);

	// ptr->parentID
	size = sizeof(int);
	printf("%d", ptr->parentID);
	lfs_write_once(&ptr->parentID, size, wp);
	
	// ptr->dirID
	// printf("dirID: ------ %d\n", (int) sizeof(ptr->dirID));
	// size = intSize(ptr->dirID) * sizeof(int);
	size = sizeof(int);
	printf("%d", ptr->dirID);
	lfs_write_once(&ptr->dirID, size, wp);


	// ptr->groupID
	// printf("groupID: ---- %d\n", (int) sizeof(ptr->groupID));
	//size = intSize(ptr->groupID) * sizeof(int);
	size = sizeof(int);
	printf("%d", ptr->groupID);
	lfs_write_once(&ptr->groupID, size, wp);

	// ptr->ownerID
	// printf("ownerID: ---- %d\n", (int) sizeof(ptr->ownerID));
	// size = intSize(ptr->ownerID) * sizeof(int);
	size = sizeof(int);
	printf("%d", ptr->ownerID);
	lfs_write_once(&ptr->ownerID, size, wp);
	
	// ptr->modT
	// printf("modT: ------- %d\n", (int) sizeof(ptr->modT));
	size = sizeof(long int);
	printf("%ld", ptr->modT);
	lfs_write_once(&ptr->modT, size, wp);

	// ptr->accT
	// printf("accT: ------- %d\n", (int) sizeof(ptr->accT));
	size = sizeof(long int);
	printf("%ld", ptr->accT);
	lfs_write_once(&ptr->accT, size, wp);
	//printf("\n");


	// ptr->data
	// Need to rewrite this, such that i write the data block number
	if(ptr->size > -1){

		int yep = 121;
		size = sizeof(char);
		printf("%d", yep);
		lfs_write_once(&yep, sizeof(char), wp);

		// if it is a file, it contains something
		if(ptr->size != 0){
			// So now i need to retrieve the block number
			// this will be the data_pointer
			// i need to convert this to a string.
			char tmp[12];

			sprintf(tmp, "%d", diskc->data_pointer);
			printf("%d", diskc->data_pointer);

			lfs_write_once(tmp, 12, wp);
			printf("%d", ptr->size);
			lfs_write_once(&ptr->size, 12, wp);

			// this is the end of write_entry, thus i can move the pointer however i feel like. 
			fseek(wp, diskc->data_pointer, SEEK_SET);


			// write 512 bytes
			if(ptr->size < diskc->blocksize+1){
				printf("           -           %s", (char*)ptr->data);

				fwrite(ptr->data, 1, ptr->size, wp);
				// increment data  pointer by blocksize, for next entry that has data
				diskc->data_pointer += diskc->blocksize;
			}
			else{
				int leftover = ptr->size;
				while(leftover > 512){
					// write 512 bytes of data
					fwrite(ptr->data, 1, 512, wp);
					leftover -= 512;
					// increment data  pointer by blocksize, for next entry that has data
					diskc->data_pointer += diskc->blocksize;
				}
				if(leftover > 0){
					// write the remaining data
					fwrite(ptr->data, 1, leftover, wp);
					// increment data  pointer by blocksize, for next entry that has data
					diskc->data_pointer += diskc->blocksize;
				}
			}
		}else{
		// it does not contain anything, write a 0
		// an e has an actual int value, so i need something else
			char tmp[12];
			sprintf(tmp, "%d", 0);
			lfs_write_once("0", 12, wp);
			printf("0\n");

		}
	}else{
		// need to do a check here as well

		int yep = 110;
		size = sizeof(char);
	 	printf("%d\n", yep);
		lfs_write_once(&yep, sizeof(char), wp);
	}
	return 0;

}
// reading the first byte on the disk
// at some point should be used to read all of the disk
// currently only expects to read one integer from the disk
// Need to have some code specifying it is the correct type of filesystem
int lfs_load_disk(FILE* ptr){
	int read = 0;
	
	fseek(ptr, 0, SEEK_SET);

	char* check = calloc(3,1);
	if(!(fread(check, 3, 1, ptr))){
		return -1;
	}
	else{
		if(strcmp(check, "LFS") != 0){
			return -1;
		}
	}
	int tmp = 0;
	printf("Found an LFS filesystem\n\n");
	printf("LFS\n");
	// I will have a problem with renaming, i can clean up after creating them all,
	// by changing iterating through and changing name, and then change parentID for children

	diskc = malloc(sizeof(struct dc));
	diskc->blocksize = 512;
	diskc->entry_start = 3;


	// i don't know how large the filesystem is, and can't pinpoint the amount of entries either

	int offset = diskc->entry_start;
	
	struct entry* ent = malloc(sizeof(struct entry));

	// right now i read data with the entries, data should be seperated. 
	// i first need to write the data block number
	printf("\n");
	int i = 1;
	while((tmp = lfs_read_entry(ptr, ent))){

		// if i use SEEK_CUR as the offset starting point
		// i'll move blocksize away from where i last read
		// so i'll move a blocksize away from the middle of a block
		read += tmp;
		addEntry(&OmegaTable, ent);
		printf("\n");
		i++;
		if(i < 10){
			//printf("it's iD:       %d\n", ent->ID);
			//printf("it's parentID: %d\n\n", ent->parentID);
			//printf("Reading entry %d\n  ", i);
			
		}else if(i < 100){
			//printf("it's iD:      %d\n", ent->ID);
			//printf("it's parentID: %d\n\n", ent->parentID);
			//printf("Reading entry %d\n ", i);
			
		}else{
			//printf("it's iD:      %d\n", ent->ID);
			//printf("it's parentID: %d\n\n", ent->parentID);
			//printf("Reading entry %d\n", i);
			
		}
		ent = malloc(sizeof(struct entry));

		offset += diskc->blocksize;
		fseek(ptr, offset, SEEK_SET);
	}
	i = 1;
	int size = OmegaTable->size+1;
	printf("reading data - - - - - - - - - - - - - - - - - - - - - - - - - - - - - ");
	printf("\n\n");
	while(i < size){
		ent = OmegaTable->entries[i];
		int blockNum = 0;
		if(ent->size > 0){
			
			offset = strtol(ent->data, (char**)NULL, 10);
			// i was given a pointer to a block, and i should trust it
			blockNum = offset/512;
			printf("reading data from block number: %d\n", blockNum);
			free(ent->data);
			ent->data = malloc(sizeof(char) * ent->size);
			fseek(ptr, offset, SEEK_SET);
			offset = fread(ent->data, sizeof(char), ent->size, ptr);
			printf("%s\n", (char*)ent->data);
		}
		i++;
	}
	// find new parentID
	i = 1;
	
	while(i < size){
		ent = OmegaTable->entries[i];
		// Only here do i change the parent ID
		// if the parentID does not point to an entry with the same ID
		if(ent->parentID != OmegaTable->entries[ent->parentID]->ID){
			// find parent pointer byt iterating 
			int x = 0;
			while(x < size){
				// If the parent is found, but is not in it's right spot, change parent ID to the spot
				// Parent will have to change it's own ID at some point
				if(OmegaTable->entries[x]->ID == ent->parentID){
					ent->parentID = x;
				}
				x++;
			}
		}
		// parentID is found
		
		i++;
	}
	// clean names and ID's
	i = 1;
	while(i< size){
		// now change own ID and name
		ent = OmegaTable->entries[i];
		
		struct entry* parentPtr = OmegaTable->entries[ent->parentID];

		ent->dirID = addEntry(&parentPtr->dirTable, ent);

		// if saved ID does not match i, rename and change ID
		if(ent->ID != i){
			ent->ID = i;
			char id[13];
			sprintf(id, "/%d", ent->ID);
			lfs_rename(id, ent->pathName);
		}
		i++;
	}

	i = 0;
	while(i < size){
		i++;
	}
	printf("Succesfully loaded LFS filesystem\n\n");
	free(diskc);
	return read;
}


// at some point should be used to write all data to the disk
// currently expects the void pointer to be an integer of size one
// Need to have some code specifying it is the correct type of filesystem
	// Gonna use the string "LFS" string in capital letters
int lfs_write_disk(FILE* wp){
	
	diskc = malloc(sizeof(struct dc));
	diskc->blocksize = 512;

	// data gets written at end of entries
	
	diskc->entry_start = 3;
	diskc->entry_end = (OmegaTable->size)*diskc->blocksize + diskc->entry_start;
	// data_pointer points to newest written data offset, which is 0 at initialization
	diskc->data_pointer = diskc->entry_end;

	printf("\n\nWriting to disk:\n");
	char* check = "LFS";
	fwrite(check, 3, 1, wp);
	printf("\nLFS\n");
	
	int written = 0;

	// loop through all indexes of OmegatTable, and call lfs_write_entry
	int i = 1;

	// Each entry fills up a block
	// Once i've written entries, i should write data, at the end. 
	// The data written should be the block number, it points to, and the size
	// So when writing data, i jsut write straight into the next block. 
	int iDeleted = 0;
	int offset = diskc->entry_start;
	while(i < OmegaTable->size +1){
		
		// if i use SEEK_CUR as the offset starting point
		// i'll move blocksize away from where i last read
		// so i'll move a blocksize away from the middle of a block
		

		if(OmegaTable->sizeDeleted > -1 && OmegaTable->sizeDeleted > iDeleted-1){
			if(i == OmegaTable->deleted[iDeleted]){
				// printf("Called write on ID: %d\nBut no file stored here\n", i);
				iDeleted++;
			}else{
				/*
				if(i < 10){ printf("calling write on ID: %d  ", i); }
				else if(i < 100){ printf("calling write on ID: %d ", i);}
				else{ printf("calling write on ID: %d", i); }
				*/
				struct entry* ptr = OmegaTable->entries[i];
				written = lfs_write_entry(wp, ptr);
				freeDir(ptr);

			}
		}else{
			/*
			if(i < 10){ printf("calling write on ID: %d  ", i); }
			else if(i < 100){ printf("calling write on ID: %d ", i);}
			else{ printf("calling write on ID: %d", i); }
			*/
			struct entry* ptr = OmegaTable->entries[i];
			written = lfs_write_entry(wp, ptr);
			freeDir(ptr);

		}
		i++;
		offset += diskc->blocksize;
		fseek(wp, offset, SEEK_SET);

	}

	printf("Succesfully wrote to the disk: %s\n\n", fileName);
	return written;
}

void lfs_destroy(void *private_data){

	FILE* wp = fopen(fileName, "w+b");
	if(!wp){
		wp = fopen("lfs.img", "w+b");
	}

	lfs_write_disk(wp);
	
}

// Once i'm going to read and write from .img file, i'll have to rewrite the initialization.
// Maybe just check if there is a root directory  or not, and then call this function if none. Maybe first bytes mark if filsystem already exists
void init(){

	
	OmegaTable = calloc(1, sizeof(struct table));
	OmegaTable->entries = calloc(1, sizeof(struct entry*));
	OmegaTable->size = 0;
	OmegaTable->sizeDeleted = -2;
	OmegaTable->entries[0] = calloc(1, sizeof(struct entry));
	OmegaTable->entries[0]->name = calloc(6, sizeof(char));
	OmegaTable->entries[0]->name = "0-root";
	OmegaTable->entries[0]->ID = 0;
	OmegaTable->entries[0]->size = -1;
	OmegaTable->entries[0]->dirID = 0;
	OmegaTable->entries[0]->data = NULL;
	OmegaTable->entries[0]->ownerID = getuid();
	OmegaTable->entries[0]->groupID = getgid();
	OmegaTable->entries[0]->accT = time( NULL );
	OmegaTable->entries[0]->modT = time( NULL );
}

// need to initialize the directory
int main( int argc, char *argv[] ) {

	

	// use last argument as filenmae
	char* filename = argv[argc-1];

	disk = fopen(filename, "r+b");

	// hvis disk ikke er null, findes filen
	if(disk){
		printf("1\n");
		// update arguments and their size,
		// and save filename for when closing disk
			argc--;	
			char* arg2[argc];
			// add old entries to new array
			int i = 0;
			while(i < argc){
				arg2[i] = argv[i];
				i++;
			}
			argv = arg2;
			fileName = malloc(sizeof(char)*strlen(filename));
			fileName = filename;
		// Only incremented for easier reading
		init();
	 	lfs_load_disk(disk);
		return fuse_main( argc, argv, &lfs_oper);
	}
	else{
		disk = fopen("lfs.img", "r+b");
		if(disk){
			fileName = malloc(sizeof(char)*strlen("data.txt"));
			fileName = "lfs.img";
			init();
			printf("lfs mounted succesfully\n");
			lfs_load_disk(disk);	
			return fuse_main( argc, argv, &lfs_oper);
		}else{
			init();
			printf("creating new system, lfs.img\n");

			return fuse_main( argc, argv, &lfs_oper);
		}
		
	}	
}

