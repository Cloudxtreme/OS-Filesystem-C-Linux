/*
 * file:        homework.c
 * description: skeleton file for CS 5600 homework 4
 * Author: Peter Desnoyers, Vignesh Kumar Subramanian, Shao Hang
 */

#define FUSE_USE_VERSION 27

#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <fuse.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "cs5600fs.h"
#include "blkdev.h"

/* 
 * disk access - the global variable 'disk' points to a blkdev
 * structure which has been initialized to access the image file.
 *
 * NOTE - blkdev access is in terms of 512-byte SECTORS, while the
 * file system uses 1024-byte BLOCKS. Remember to multiply everything
 * by 2.
 */

extern struct blkdev *disk;

/* init - this is called once by the FUSE framework at startup.
 * This might be a good place to read in the super-block and set up
 * any global variables you need. You don't need to worry about the
 * argument or the return value.
 */
struct cs5600fs_super *super=NULL;
struct cs5600fs_entry *fat=NULL;
struct cs5600fs_dirent directory[16];
char name[44];

void* hw4_init(struct fuse_conn_info *conn)
{
	// initialize file system
	// read in superblock
	super = calloc(1, 1024);
	disk->ops->read(disk, 0, 2, (void*) super);
	// read in fat block
	fat = calloc(1024, sizeof(struct cs5600fs_entry));
	disk->ops->read(disk, 1 * 2, super->fat_len*2, (void*) fat);
    return NULL;

}	
 
/* strwrd - helper function 
    used to split the string into words
 */ 
char *strwrd(char *s, char *buf, size_t len, char *delim)
{
	s += strspn(s, delim);
	int n = strcspn(s, delim);
	if (len - 1 < n)
		n = len - 1;
	memset(buf, 0, len);
	memcpy(buf, s, n);
	s += n;
	return (*s == 0) ? NULL : s;
}


/* find file name in the directory
 * return proper errors
 */

int lookupEntry(char *name, int blk_num)
{
	int i;
  	for(i = 0; i < 16; i++)
    {
    	// find it
    	if(1 == directory[i].valid && !(strcmp(name, directory[i].name)))
			return i;
	}
	return -1;				
}

int new_blk_num;
int new_current_blk_num;

/* parsePath 
 * parse the path and return the entry number
 */
int parsePath(char *path, int blk_num)
{
	new_blk_num = blk_num;
	int entry = 0;
	// root directory
	if(strlen(path) == 1)
	{
		disk->ops->read(disk, new_blk_num * 2, 2, (void*)directory);
		return entry;
	}
	
	while(path != NULL)
	{
		memset(name,0,sizeof(name));
		path = strwrd((char*)path, name, sizeof(name),"/");
		disk->ops->read(disk, new_blk_num * 2, 2, (void*)directory);
		entry = lookupEntry(name, new_blk_num);
		if (entry == -1) 
			return -ENOENT;
		if (entry <0)
			return entry;
		new_current_blk_num = new_blk_num;
		new_blk_num = directory[entry].start;
	}
	
	return entry;
}
/* convertAttr- helper function
 * convert the file attributes of the cs5600fs to 
 * attributes of the Linux 
 */
void convertAttr(int entry, struct stat *sb)
{
	sb->st_dev = 0;
	sb->st_ino = 0;
	sb->st_rdev = 0;
	sb->st_nlink = 1;
	sb->st_blksize = 1024;
	 
	if(directory[entry].isDir)
		sb->st_blocks = 0;
	else 
		sb->st_blocks = directory[entry].length / 1024 + 1;
	
	sb->st_uid = directory[entry].uid;
	sb->st_gid = directory[entry].gid;
	sb->st_size = directory[entry].length;
	sb->st_mtime = directory[entry].mtime;
	sb->st_atime = directory[entry].mtime;
	sb->st_ctime = directory[entry].mtime;	
	sb->st_mode = directory[entry].mode | (directory[entry].isDir ? S_IFDIR : S_IFREG);
}

/* Note on path translation errors:
 * In addition to the method-specific errors listed below, almost
 * every method can return one of the following errors if it fails to
 * locate a file or directory corresponding to a specified path.
 *
 * ENOENT - a component of the path is not present.
 * ENOTDIR - an intermediate component of the path (e.g. 'b' in
 *           /a/b/c) is not a directory
 */

/* getattr - get file or directory attributes. For a description of
 *  the fields in 'struct stat', see 'man lstat'.
 *
 * Note - fields not provided in CS5600fs are:
 *    st_nlink - always set to 1
 *    st_atime, st_ctime - set to same value as st_mtime
 *
 * errors - path translation, ENOENT
 */
 
static int hw4_getattr(const char *path, struct stat *sb)
{
	char *sub_path = (char*)calloc(16*44,sizeof(char));
	strcpy(sub_path, path);
	
	int start = super->root_dirent.start;
	int entry;
	
	// find the block and entry of the path

	while(sub_path != NULL)
 	{			
		memset(name,0,sizeof(name));
		sub_path = strwrd(sub_path, name, sizeof(name), "/");
		disk->ops->read(disk, start * 2, 2, (void*)directory);
		entry = lookupEntry(name, start);		
		if(entry == -1) 
			return -ENOENT;
		start = directory[entry].start;
	}

	convertAttr(entry, sb);
	free(sub_path);

    return 0;
}

/* readdir - get directory contents.
 *
 * for each entry in the directory, invoke the 'filler' function,
 * which is passed as a function pointer, as follows:
 *     filler(buf, <name>, <statbuf>, 0)
 * where <statbuf> is a struct stat, just like in getattr.
 *
 * Errors - path resolution, ENOTDIR, ENOENT
 */


static int hw4_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info *fi)
{
	int len = strlen(path);
	int i;
	
	struct stat sb;
	memset(&sb, 0, sizeof(sb));
	int start = super->root_dirent.start;

	// find the block number of the path  
	int entry = parsePath((void*)path, start);

	if(entry == -1) 
		return -ENOENT;
		
	start = new_blk_num;

	disk->ops->read(disk, start * 2, 2, (void*)directory);
	while(1)
	{
		if(!fat[start].inUse)  
			return 0;
		for(i = 0; i < 16; i++)
 		{
      		if (!directory[i].valid) 
			  continue;
			  
			sb.st_dev = 0;
			sb.st_ino = 0;
			sb.st_rdev = 0;
			sb.st_blocks = directory[i].length / 1024+1;
			sb.st_blksize = 1024;
			sb.st_uid = directory[i].uid;
			sb.st_gid = directory[i].gid;
			sb.st_size = directory[i].length;
			sb.st_mtime = sb.st_atime= sb.st_ctime = directory[i].mtime;
			sb.st_nlink=1;
			if (len == 1)
				sb.st_mode = directory[i].mode | S_IFDIR;
			else
				sb.st_mode = directory[entry].mode | (directory[entry].isDir ? S_IFDIR : S_IFREG);
			strcpy(name, directory[i].name);
			filler(buf, name, &sb,0);
		}
		if (fat[start].eof) 
			return 0;
		start = fat[start].next;
		disk->ops->read(disk, start * 2, 2, (void*)directory);
	}

	return 0;
}

/*
 * Cereate the entry depending on the isDir
 */
int createEntry(const char *path, mode_t mode, int isDir)
{
	
	int entry=0;

	char *new_name = (char*)calloc(44,sizeof(char));

	int i, j;

	// save the path in paths
	// save the name in new_name
	char paths[20][44];
	int k = 0;

	while(1)
	{
		path = strwrd((char*)path, paths[k], sizeof(paths[k]), "/");
		strcpy(new_name, paths[k]);
		if (path == NULL)
			break;
		k++;
	}

	
	int blk_num = super->root_dirent.start;
	// find the location of the path
	for(i = 0; i < k; i++)
	{
		disk->ops->read(disk, blk_num * 2, 2, (void*)directory);
		entry = lookupEntry(paths[i], blk_num);
		if(entry == -1) 
			return -ENOENT;
		blk_num = directory[entry].start;
	}

	disk->ops->read(disk, blk_num * 2, 2, (void*)directory);
	
	// check if the entry is exsisted
	for(i = 0; i < 16; i++)
		if(strcmp(directory[i].name, paths[k]) == 0)
			return -EEXIST;

	// find an entry that is free
	for(i = 0; i < 16; i++)
		if(directory[i].valid == 0)
			break;
	
	// find a free block
	for(j = 0; j < 1024; j++)
	{
		if(fat[j].inUse == 0)
		{
			fat[j].inUse = 1;
			fat[j].eof = 1;		
			break;
		}
	}
	
	if(j==1024) 
		return -ENOSPC;

	// initialize the file
	directory[i].valid = 1;
	directory[i].isDir = isDir;
	directory[i].length = 0;
	directory[i].start = j;
	directory[i].mode = mode;
	directory[i].mtime = time(NULL);
	directory[i].gid = getgid();
	directory[i].uid = getuid();	
	directory[i].pad = 0;
	strcpy(directory[i].name, new_name);
	
	// write back
	disk->ops->write(disk,blk_num*2,2,(void*)directory);
	disk->ops->write(disk,2,8,(void*)fat);	

	free(new_name);
    return 0;
}

static int hw4_create(const char *path, mode_t mode,
			 struct fuse_file_info *fi)
{
	createEntry(path, mode, 0);
    return 0;
}

/* mkdir - create a directory with the given mode.
 * Errors - path resolution, EEXIST
 * Conditions for EEXIST are the same as for create.
 */ 
static int hw4_mkdir(const char *path, mode_t mode)
{
	createEntry(path, mode, 1);
    return 0;
}


/* unlink - delete a file
 *  Errors - path resolution, ENOENT, EISDIR
 */
static int hw4_unlink(const char *path)
{

	int blk_num = super->root_dirent.start;	
	int current_blk_num;

	int entry = parsePath((void*)path, blk_num);
	blk_num = new_blk_num;
	current_blk_num = new_current_blk_num;
	
	// check if it is a file or not
	if(directory[entry].isDir==1) 
		return -EISDIR;

	// set the valid to 0
	directory[entry].valid=0;

	while(1)
	{
		if(fat[blk_num].eof == 1) 
		{
			fat[blk_num].inUse = 0;			
			break;
		}

		fat[blk_num].inUse = 0;
		blk_num = fat[blk_num].next;
		
	}
	
	// write back
	disk->ops->write(disk, current_blk_num * 2, 2, (void*)directory);
	disk->ops->write(disk, 2, 8, (void*)fat);

    return 0;
}

/* rmdir - remove a directory
 *  Errors - path resolution, ENOENT, ENOTDIR, ENOTEMPTY
 */
static int hw4_rmdir(const char *path)
{

	int blk_num = super->root_dirent.start;	
	int current_blk_num;

	int i;

	int entry = parsePath((void*)path, blk_num);
	blk_num = new_blk_num;
	current_blk_num = new_current_blk_num;
	
	// check if it is a directory or not
	if(directory[entry].isDir == 0) 
		return -ENOTDIR;

	// check if the directory is empty
	disk->ops->read(disk, blk_num * 2, 2, (void*)directory);
	for(i = 0; i < 16; i++)
		if(directory[i].valid) 
			return -ENOTEMPTY;

	disk->ops->read(disk, current_blk_num * 2, 2, (void*)directory);

	directory[entry].valid = 0;
	fat[blk_num].inUse = 0;

	// write the changes back
	disk->ops->write(disk, current_blk_num * 2, 2, (void*)directory);
	disk->ops->write(disk,2,8,(void*)fat);
	
    return 0;
}

/* rename - rename a file or directory
 * Errors - path resolution, ENOENT, EINVAL, EEXIST
 *
 * ENOENT - source does not exist
 * EEXIST - destination already exists
 * EINVAL - source and destination are not in the same directory
 *
 * Note that this is a simplified version of the UNIX rename
 * functionality - see 'man 2 rename' for full semantics. In
 * particular, the full version can move across directories, replace a
 * destination file, and replace an empty directory with a full one.
 */
static int hw4_rename(const char *src_path, const char *dst_path)
{
	char *sub_src_path = (char*)calloc(16*44,sizeof(char));
	char *sub_dst_path = (char*)calloc(16*44,sizeof(char));

	char path1[20][44];
	char path2[20][44];
	
	int k = 0;

	strcpy(sub_src_path, src_path);
	strcpy(sub_dst_path, dst_path);

	// save src path to path1
	// save dst path to path2
	while(1)
	{
		sub_src_path = strwrd(sub_src_path, path1[k], 44, " /");
		sub_dst_path = strwrd(sub_dst_path, path2[k], 44, " /");
		
		if (sub_src_path == NULL && sub_dst_path == NULL)
			break;
		
		if (sub_src_path == NULL || sub_dst_path == NULL)
			return -EINVAL;
			
		if (strcmp(path1[k], path2[k]) != 0)
			return -EINVAL;
			
		k++;
	}

	char *filename = (char*)malloc(sizeof(char)*44);
	strcpy(filename, path2[k]);
	
	int blk_num = super->root_dirent.start;
	int current_blk_num;

	// find the block and entry number of the src path
	int entry = parsePath((void*)src_path, blk_num);
	current_blk_num = new_current_blk_num;
	
	// find the entry number of the dst path
	int dst_entry = lookupEntry(filename, current_blk_num);

	// check if the dst file is existed
	if (dst_entry != -1)
		return -EEXIST;

	strcpy(directory[entry].name, filename);
	directory[entry].mtime = time(NULL);

	// write back
	disk->ops->write(disk, current_blk_num*2, 2, (void*)directory);

	free(filename);
	free(sub_src_path);
	free(sub_dst_path);
    return 0;
}

/* chmod - change file permissions
 * utime - change access and modification times
 *         (for definition of 'struct utimebuf', see 'man utime')
 *
 * Errors - path resolution, ENOENT.
 */

static int hw4_chmod(const char *path, mode_t mode)
{

	int blk_num = super->root_dirent.start;

	int entry = parsePath((char*)path, blk_num);
	blk_num = new_current_blk_num;
	
	// update the mode
	directory[entry].mode = mode;
	directory[entry].mtime = time(NULL);

	// write back
	disk->ops->write(disk, blk_num * 2, 2, (void*)directory);

    return 0;
}


int hw4_utime(const char *path, struct utimbuf *ut)
{
	int blk_num = super->root_dirent.start;

	// find the block and entry number of the path
	int entry = parsePath((void*)path, blk_num);
	blk_num = new_current_blk_num;
	
	// change the time
	directory[entry].mtime = time(NULL);

	// write back
	disk->ops->write(disk, blk_num * 2, 2, (void*)directory);

    return 0;
}

/* truncate - truncate file to exactly 'len' bytes
 * Errors - path resolution, ENOENT, EISDIR, EINVAL
 *    return EINVAL if len > 0.
 */
static int hw4_truncate(const char *path, off_t len)
{

	if (len < 0)
		return -EINVAL;	

	int blk_num = super->root_dirent.start;
	int current_blk_num;

	int entry = parsePath((void*)path, blk_num);
	blk_num = new_blk_num;
	current_blk_num = new_current_blk_num;
	
	// update the length and mtime
	directory[entry].length = len;
	directory[entry].mtime = time(NULL);
	disk->ops->write(disk, current_blk_num * 2, 2,(void*)directory);

	while (len >= 1024)
	{
		blk_num = fat[blk_num].next;
		len -= 1024;
	}

	if (fat[blk_num].eof == 1)
		return 0;
		
	fat[blk_num].eof = 1;
	
	// mark unused block
	while(1)
	{
		blk_num = fat[blk_num].next;
		fat[blk_num].inUse = 0;
		if (fat[blk_num].eof == 1)
			break;
	}

	// write back
	disk->ops->write(disk, 2, 2 * 4,(void*)fat);
	
	return 0;
}

/* read - read data from an open file.
 * should return exactly the number of bytes requested, except:
 *   - if offset >= len, return 0
 *   - on error, return <0
 * Errors - path resolution, ENOENT, EISDIR
 */

static int hw4_read(const char *path, char *buf, size_t len, off_t offset,
		    struct fuse_file_info *fi)
{
	int start = super->root_dirent.start;

	// find the block and entry number of the path
	int entry = parsePath((void*)path, start);
	start = new_blk_num;
	
	if(directory[entry].isDir) 
		return -EISDIR;
		
	if(offset >= directory[entry].length) 
		return 0;

	char *p = NULL;
	int length = directory[entry].length;
	char blk_buf[(length / 1024 + 1) * 1024];
	p = blk_buf;

	while(length > 0)
	{		
		disk->ops->read(disk, start * 2, 2, (void*)p);
		start = fat[start].next;
		p += 1024;
		length -= 1024;
	}
	
	p = blk_buf;
	p += offset;

	//read len bytes data at offset from p to buffer 
	int read_len = (directory[entry].length < (offset + len))? (directory[entry].length - offset) : len;
	memcpy(buf, p, read_len);
	return read_len;
}


/* write - write data to a file
 * It should return exactly the number of bytes requested, except on
 * error.
 * Errors - path resolution, ENOENT, EISDIR
 *  return EINVAL if 'offset' is greater than current file length.
 */
static int hw4_write(const char *path, const char *buf, size_t len,
		     off_t offset, struct fuse_file_info *fi)
{
	int blk_num = super->root_dirent.start;	
	int current_blk_num;

	// find the block and entry number of the path
	int entry = parsePath((void*)path, blk_num);
	blk_num = new_blk_num;
	current_blk_num = new_current_blk_num;
	
	// check if it is a directory
	if(directory[entry].isDir==1) 
		return -EISDIR;		
	
	// check if the offset is valid
	if(offset > directory[entry].length)
		return -EINVAL;

	int current = (int)offset;
	int current_blk = blk_num;

	// find the block to write	
	while(current >= 1024)
	{
		current_blk = fat[current_blk].next;
		current -= 1024;
	}

	char *temp = (char*)malloc(sizeof(char)*1024);
	int remainNum = (int)len;
	//int wrt_len;
	int buf_offset = 0;
	int i;
	
	// write to block
	while(1)
	{
		disk->ops->read(disk, current_blk * 2, 2, (void*)temp); 
		int num;
		
		if((current + remainNum) <= 1024)
			num = remainNum;
		else
			num = 1024 - current;
			
		for (i = current; i < current + num; i++)
		{
			temp[i] = buf[buf_offset++];
		}

		// write back
		disk->ops->write(disk, current_blk * 2, 2, (void*)temp);

		// update the remainNum len and the current offset
		current = (current + num) % 1024;
		remainNum -= num;

		if(remainNum <= 0)
			break;

		// need to allocate more block or not
		if (fat[current_blk].eof == 1)
		{
			for(i = 0; i < 1024; i++)
			{
				if(fat[i].inUse == 0)
				{
					fat[i].inUse = 1;
					fat[i].eof = 1; 
					break;
				}
			}		
			if(i == 1024) 
				return -ENOSPC;
			
			fat[current_blk].eof = 0;	
			fat[current_blk].next = i;
			
			//disk->ops->write(disk, 2, 8, (void*)fat); 
		}
		current_blk = fat[current_blk].next;
	}
	
	if ((offset+len) > directory[entry].length)
		directory[entry].length = offset + len;

	directory[entry].mtime = time(NULL);
	
	// write back
	disk->ops->write(disk, 2, 8, (void*)fat);
	disk->ops->write(disk, current_blk_num * 2, 2, (void*)directory);

	free(temp);
	return len;
}

/* statfs - get file system statistics
 * see 'man 2 statfs' for description of 'struct statvfs'.
 * Errors - none. Needs to work.
 */
static int hw4_statfs(const char *path, struct statvfs *st)
{
	// set attribute values
	st->f_bsize = super->blk_size;
	st->f_blocks = super->fs_size;
	
	int numUsed=0;
	int i;
	for(i = 0; i < 1024; i++)
	{
		if(fat[i].inUse)
			numUsed++;
	}
	st->f_bfree = numUsed;	
	st->f_bavail = st->f_bfree;
	st->f_namemax = 43;
	return 0;
}


/* operations vector. Please don't rename it, as the skeleton code in
 * misc.c assumes it is named 'hw4_ops'.
 */
struct fuse_operations hw4_ops = {
    .init = hw4_init,
    .getattr = hw4_getattr,
    .readdir = hw4_readdir,
    .create = hw4_create,
    .mkdir = hw4_mkdir,
    .unlink = hw4_unlink,
    .rmdir = hw4_rmdir,
    .rename = hw4_rename,
    .chmod = hw4_chmod,
    .utime = hw4_utime,
    .truncate = hw4_truncate,
    .read = hw4_read,
    .write = hw4_write,
    .statfs = hw4_statfs,
};

