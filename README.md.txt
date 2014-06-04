FILESYSTEM

Files in the project folder

1. homework.c - This is the .c file which is the implementation. Done by me and my partner. Skeleton provided by Professor.
2. cs5600fs.h - It is the structure of the MSDOS filesystem containing the superblock, FAT etc.
3. blkdev.h   - It is the block level device driver. It sits in between the disk and above filesystem. This structure contains
		function prototypes for read, write, mirroring, striping and RAID. 
4. image.c    - This contains the implementation of the functions prototypes in the blkdev.h. 
		  
5. misc.c     - This file is responsible for parsing the arguments, implementing the FUSE API Calls for all 
		commandline functions such as cd, ls, mkdir, pwd etc..
6. read-img.c - It is the image reader utility for reading the image files.

7. q1test.sh, q2test.sh, q3debug.sh, q3debug-setup.sh, q3test.sh are all test files.
8. hmework-4.pdf & homework-4-docs are the objectives and other information about implementing File system.


The project is to implement file system, a derivative of the MSDOS filesystem. I have used the FUSE library wherein
i and my partner have implemented the functions of the FUSE API calls which will basically help in accessing, reading and writing files 
across directories like in a normal filesystem.

Our Professor had provided some source files and which were worked earlier like blkdev.h, cs5600fs.h, image.c, misc.c etc which we 
had to incorporatein the empty fuction calls for FUSE API calls in homework.c.


MOstly part of the project was to call right function calls and complete the skeleton provided in the homework.c.