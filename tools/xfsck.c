#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <limits.h>
#include <string.h>
#include <math.h>

#define stat xv6_stat  // avoid clash with host struct stat
#define dirent xv6_dirent  // avoid clash with host struct stat
#include "types.h"
#include "fs.h"
#include "stat.h"
#undef stat
#undef dirent

// To compile from xv6/tools directory, run command:
//  gcc -iquote ../include -Wall -Werror -ggdb -o xfsck xfsck.c
// To run xfsck, run command:
//  xfsck file_system_image
// e.g.: $ xfsck xfsckTest/Good

// If nothing is printed in console, the file system img has no error.
// Testing img are in directory xfsckTest.
// Visualization: 
//  https://shawnzhong.github.io/xv6-file-system-visualizer/


int roundUp(double fraction) 
{
    return fraction + 0.99; 
}


int main(int argc, char *argv[]) {
	int fd;
    // Usage is something like <my prog> <fs.img>
	if (argc == 2) {
		fd = open(argv[1], O_RDONLY);
	} else {
        fprintf(stderr, "image not found.\n");
        exit(1);
    }

    if (fd < 0) {
        fprintf(stderr, "image not found.\n");
        exit(1);
    }

    struct stat sbuf;
	fstat(fd, &sbuf);

    void *img_ptr = mmap(NULL, sbuf.st_size, 
        PROT_READ, MAP_PRIVATE, fd, 0);

    struct superblock *sb = (struct superblock *) (img_ptr + BSIZE);
    struct dinode *dip = (struct dinode *) (img_ptr + 2 * BSIZE);
    
    //printf("nblocks = %d\nninodes = %d\n", sb->nblocks, sb->ninodes);
    
    // Number of blocks for ... :
    uint nb_inodes = roundUp((double) sb->ninodes/IPB); 
    uint nb_bitmap = roundUp((double) sb->nblocks/BPB);
    uint nb_total = sb->nblocks + nb_inodes + nb_bitmap + 3;
    uint index_idb = NDIRECT; // index of the indirect data block
        // index 0 to NDIRECT-1 are direct data blocks

    // index of the starting blocks for ... :
    int ib_inode = 2; // after first block and super block
    int ib_bitmap = ib_inode + nb_inodes + 1; // after inode blocks
    int ib_data = ib_bitmap + nb_bitmap; // after bitmap blocks
    int ib_end = ib_data + sb->nblocks - 1; // after data blocks

    int num_dirent = BSIZE / (DIRSIZ + sizeof(ushort));
    int num_inodes = nb_inodes * IPB;


    // 1. Superblock corruption: 
    // Correct if: filesystem_size_in_superblock >
    //      num_data_blocks + num_blocks_for_inode + num_blocks_for_bitmap + 1
    if (sb->size < nb_total)
    {
        fprintf(stderr, "ERROR: superblock is corrupted.\n");
        //printf("sb->size == %d, nb_total == %d\n", sb->size, nb_total);
        exit(1);
    }

    // 2. Inode type check:
    // Correct if: Each inode is either unallocated 
    //      or one of the valid types: File, Dir, or Dev
    for (int i = 0; i < num_inodes; ++i) {
        if (dip[i].type != 0 
            && dip[i].type != 1
            && dip[i].type != 2
            && dip[i].type != 3)
        {
            //printf("inode [%d] type = %d", i, dip[i].type);
            fprintf(stderr, "ERROR: bad inode.\n");
            exit(1);
        }
    }

    // 3. Address validity: 
    // Correct if: for in-use inodes, each address that is used by inode 
    //      is valid (points to a valid datablock address within the image).
    for (int i = 0; i < num_inodes; ++i) {
        int num_links = 0;
        if (dip[i].type != 0)
        {
            // Check direct links:
            for (int j = 0; j < NDIRECT; ++j)
            {
                //printf("addr = %d\n", dip[i].addrs[j]);
                if (dip[i].addrs[j] != 0) 
                {
                    if (dip[i].addrs[j] < ib_data || dip[i].addrs[j] > ib_end) 
                    {
                        fprintf(stderr, "ERROR: bad direct address in inode.\n");
                        //printf("inode [%d] addrs [%d] == %d\n", i, j, dip[i].addrs[j]);
                        exit(1);
                    }
                    if (dip[i].addrs[j] != 0) 
                        num_links ++;
                }
            }
            

            // Check indirect links:
            if (dip[i].addrs[index_idb] != 0) 
            {
                if (dip[i].addrs[index_idb] < ib_data || dip[i].addrs[index_idb] > ib_end)
                {
                    fprintf(stderr, "ERROR: bad indirect address in inode.\n");
                    //printf("%d\n", dip[i].addrs[NDIRECT+1]);
                    exit(1);
                } 
                else 
                {
                    num_links ++;
                    uint* addr_indirect = (uint*) (img_ptr + BSIZE * dip[i].addrs[index_idb]);
                    for (int j=0; j < NINDIRECT; ++j)
                    {
                        if (addr_indirect[j] != 0)
                        {
                            if (addr_indirect[j] < ib_data || addr_indirect[j] > ib_end) 
                            {
                                fprintf(stderr, "ERROR: bad indirect address in inode.\n");
                                //printf("%d\n", addr_indirect[j]);
                                exit(1);
                            }
                        }
                    }
                }
            }
        }
    }

    // 4. Directory content correctness: 
    // Correct if: Each directory contains . and .. entries, 
    //      and the . entry points to the directory itself. 
    for (int i = 0; i < num_inodes; ++i) {
        if (dip[i].type == 1)
        {
            uint data_block_addr = dip[i].addrs[0]; 
	        struct xv6_dirent *entry = (struct xv6_dirent *)(img_ptr + data_block_addr * BSIZE);
            int foundSelf = 0;
            int foundParent = 0;
            for (int j = 0; j < num_dirent; ++j) 
            {
                //printf("entry name = %s\n", entry[j].name);
                if (strcmp(entry[j].name, ".") == 0) 
                {
                    foundSelf = 1;
                    // Check if "." entry has inode number points to itself:
                    if (entry[j].inum != i)
                    {
                        fprintf(stderr, "ERROR: directory not properly formatted.\n");
                        //printf("inode not pointing to self\n");
                        exit(1);
                    }
                }
                else if (strcmp(entry[j].name, "..") == 0)
                {
                    foundParent = 1;
                }
            }

            // Check if both "." and ".." are present among entries:
            if (foundSelf == 0 || foundParent == 0)
            {
                fprintf(stderr, "ERROR: directory not properly formatted.\n");
                //printf("i = %d, . and .. not presented\n", i);
                exit(1);
            }
        } 
    }

    
    uchar *bitmap = (uchar *)(img_ptr + BBLOCK(0, sb->ninodes)*BSIZE);


    // 5. Bitmap correctness:
    // Correct if: For in-use inodes, each address in use 
    //      is also marked in use in the bitmap.
    for (int i = 0; i < num_inodes; ++i) 
    {   
        if (dip[i].type != 0)
        {
            for (int j = 0; j < NDIRECT+1; ++j) 
            {
                uint addr_db = dip[i].addrs[j]; 
                    // address of data block pointed by the jth addr in inode i
                
                if (addr_db != 0)
                {
                    //int bit_index = addr_db % BPB;
                    uchar bit = (bitmap[addr_db/8] & (0x1 << (addr_db%8))) >> (addr_db%8);
                    if (bit == 0)
                    {
                        fprintf(stderr, "ERROR: address used by inode but marked free in bitmap.\n");
                        //printf("inode [%d] addrs [%d] == %d\n", i, j, dip[i].addrs[j]);
                        exit(1);
                    }
                }
            }
            uint* addr_indir = (uint*)(img_ptr + BSIZE * dip[i].addrs[index_idb]);
            for (int j = 0; j < NINDIRECT; ++j)
            {
                uchar bit = (bitmap[addr_indir[j]/8] & (0x1 << (addr_indir[j]%8))) >> (addr_indir[j]%8);
                if (bit == 0)
                {
                    fprintf(stderr, "ERROR: address used by inode but marked free in bitmap.\n");
                    //printf("inode [%d] addrs [%d] == %d\n", i, j, dip[i].addrs[j]);
                    exit(1);
                }
            }
        }
    }

    // 6. Bitmap correctness: 
    // Correct if: For blocks marked in-use in bitmap, the block should  
    //      actually be in-use in an inode or indirect block somewhere. 
    for(int i = 29; i < sb->nblocks; i++) 
    {
        uchar bit = (bitmap[i/8] & (0x1 << (i%8))) >> (i%8);
        if (bit == 1)
        {
            int isUsed = 0;
            for (int j = 0; j < num_inodes; j++)
            {
                for (int k = 0; k < NDIRECT+1; k++)
                {
                    //if (dip[j].addrs[k] != 0)
                        //printf("dip %d addrs %d == %d\n", j, k, dip[j].addrs[k]);
                    if (dip[j].addrs[k] == i)
                    {
                        isUsed = 1;
                        break;
                    }
                }
                uint* addr_indir = (uint*)(img_ptr + BSIZE * dip[j].addrs[index_idb]);
                for (int k = 0; k < NINDIRECT; k++)
                {
                    if (addr_indir[k] == i)
                    {
                        isUsed = 1;
                        break;
                    }
                }
                if (isUsed == 1)
                    break;
            }

            if (isUsed == 0)
            {
                fprintf(stderr, "ERROR: bitmap marks block in use but it is not in use.\n");
                //printf("block %d is not in use\n", i);
                exit(1);
            }
        }
    }
    

    // 7. Single usage of direct addr:
    // Correct if: For in-use inodes, each direct address in use is only used once.
    for (int i = 0; i < num_inodes; ++i) {
        if (dip[i].type != 0)
        {
            for (int j = 0; j < NDIRECT; ++j)
            {
                uint addr_data = dip[i].addrs[j];
                if (addr_data != 0)
                {
                    for (int k = i; k < nb_inodes; ++k)
                    {
                        for (int h = 0; h < NDIRECT; ++h)
                        {
                            if (k != i || h != j)
                            {
                                if (addr_data == dip[k].addrs[h])
                                {
                                    fprintf(stderr, "ERROR: direct address used more than once.\n");
                                    //printf("inode[%d].addrs[%d] == inode[%d].addrs[%d] == %d\n", i, j, k, h, dip[i].addrs[j]);
                                    exit(1);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // 8. Correct file size stored
    // Correct if: For in-use inodes, the file size stored must be within 
    //      the actual number of blocks used for storage. 
    //      That is if b blocks are used with block size s, 
    //      then the file size must be > (b-1)*s and <= b*s.
    for (int i = 0; i < num_inodes; ++i) {
        if (dip[i].type != 0 && dip[i].size != 0)
        {
            int num_db = 0; // number of data blocks used
            for (int j = 0; j < NDIRECT; ++j)
            {
                if (dip[i].addrs[j] != 0) 
                    num_db ++;
            }
            if (dip[i].addrs[index_idb] != 0) 
            {
                uint* addr_indirect = (uint*) (img_ptr + BSIZE * dip[i].addrs[index_idb]);
                for (int j=0; j < NINDIRECT; ++j)
                {
                    if (addr_indirect[j] != 0) 
                        num_db ++;
                }
            }
            if (dip[i].size > num_db * BSIZE || dip[i].size < (num_db-1) * BSIZE)
            {
                fprintf(stderr, "ERROR: incorrect file size in inode.\n");
                //printf("dip[%d].size = %d, %d data blocks used\n", i, dip[i].size, num_db);
                exit(1);
            }
        }
    }

    // Array of addresses for all directory inodes:
    int dir_inodes[sb->ninodes]; 
    int num_dir = 0;
    for (int i = 0; i < num_inodes; ++i) {
        if (dip[i].type == 1)
        {
            dir_inodes[num_dir] = i; //+ ib_inode;
            num_dir ++;
        }
    }

    // 9. No lose end:
    // Correct if: For all inodes marked in use, 
    //      each must be referred to in at least one directory.
    for (int i = 0; i < num_inodes; ++i) { 
        if (dip[i].type != 0) 
        {
            int isReferred = 0; 
            for (int j = 0; j < num_dir; ++j) 
            {
                struct dinode *dir = (struct dinode *) &dip[dir_inodes[j]]; 
                for (int k = 0; k < NDIRECT; ++k)
                {
                    struct xv6_dirent *entry = (struct xv6_dirent *)(img_ptr + BSIZE * dir->addrs[k]);
                    for (int h = 0; h < num_dirent; ++h)
                    {
                        //printf("inum = %d\n", entry[h].inum);
                        if (entry[h].inum == (ushort)i) 
                        {
                            isReferred = 1;
                            break;
                        }
                    }
                    if (isReferred == 1)
                        break;
                } 
                
                uint *addr_indirect = (uint*) (img_ptr + BSIZE * dir->addrs[index_idb]);
                for (int k = 0; k < NINDIRECT; ++k)
                {
                    struct xv6_dirent *entry = (struct xv6_dirent *)(img_ptr + BSIZE * addr_indirect[k]);
                    for (int h = 0; h < num_dirent; ++h)
                    {
                        //printf("inum = %d\n", entry[h].inum);
                        if (entry[h].inum == (ushort)i) 
                        {
                            isReferred = 1;
                            break;
                        }
                    }
                    if (isReferred == 1)
                        break;
                } 

                if (isReferred == 1)
                    break;
                
            } 
            if (isReferred == 0)
            {
                fprintf(stderr, "ERROR: inode marked used but not found in a directory.\n");
                //printf("dip[%d] is never referred\n", i);
                exit(1);
            }
            
        }
        
    }

    //  10. Matching status of inode:
    // Correct if: For each inode number that is referred to in a valid directory, 
    //      it is actually marked in use. 
    for (int j = 0; j < num_dir; ++j)
    {   
        struct dinode *dir = (struct dinode *) &dip[dir_inodes[j]]; 
        for (int k = 0; k < NDIRECT; ++k)
        {
            struct xv6_dirent *entry = (struct xv6_dirent *)(img_ptr + BSIZE * dir->addrs[k]);
            for (int h = 0; h < num_dirent; ++h)
            {
                //printf("inum = %d\n", entry[h].inum);
                if (entry[h].inum > 0 && dip[entry[h].inum].type == 0) 
                {
                    fprintf(stderr, "ERROR: inode referred to in directory but marked free.\n");
                    //printf("dip[%d] is referred but not in use\n", entry[h].inum);
                    exit(1);
                }
            }
        } 
        
        uint *addr_indirect = (uint*) (img_ptr + BSIZE * dir->addrs[index_idb]);
        for (int k = 0; k < NINDIRECT; ++k)
        {
            struct xv6_dirent *entry = (struct xv6_dirent *)(img_ptr + BSIZE * addr_indirect[k]);
            for (int h = 0; h < num_dirent; ++h)
            {
                //printf("inum = %d\n", entry[h].inum);
                if (entry[h].inum > 0 && dip[entry[h].inum].type == 0) 
                {
                    fprintf(stderr, "ERROR: inode referred to in directory but marked free.\n");
                    //printf("dip[%d] is referred but not in use\n", entry[h].inum);
                    exit(1);
                }
            }
        } 
    }
    
    // 11. Correct reference count:
    // Correct if: For every file (could be saved in multiple directories), 
    // check if nlink == number of the the directories link to it
    for (int i = 0; i < num_inodes; ++i)
    {
        if (dip[i].type == 2)
        {
            int count_link = 0;
            
            for (int j = 0; j < num_dir; ++j)
            {
                struct dinode *dir = (struct dinode *) &dip[dir_inodes[j]]; 
                for (int k = 0; k < NDIRECT; ++k)
                {
                    struct xv6_dirent* entry = (struct xv6_dirent*)(img_ptr + BSIZE * dir->addrs[k]);
                    for (int h = 0; h < num_dirent; ++h)
                    { 
                        if (entry[h].inum == i) {
                            count_link ++;
                            //printf("Refer to %d: dir %d, db %d, entry %d\n", i, j, k, h);
                        }
                    }
                }

                uint* addr_indir = (uint*)(img_ptr + BSIZE * dir->addrs[index_idb]);
                    // addr of the indirect data block
                for (int k = 0; k < NINDIRECT; ++k) 
                {
                    struct xv6_dirent* entry = (struct xv6_dirent*)
                        (img_ptr + BSIZE * addr_indir[k]); 
                        // entries in each data block pointed by the indirect block
                    for (int h = 0; h < num_dirent; ++h)
                    {
                        if (entry[h].inum == i) {
                            count_link ++;
                            //printf("Refer to %d: dir %d, indir db %d, entry %d\n", i, j, k, h);
                        }
                    }
                }
            }
            if (count_link != dip[i].nlink)
            {
                fprintf(stderr, "ERROR: bad reference count for file.\n");
                //printf("inode %d has nlink = %d, \nbut is referenced %d times\n", i, dip[i].nlink, count_link);
                exit(1);
            }
        }
    }

    // 12. No extra links allowed for directories
    // Correct if: each directory only appears in one other directory.
    for (int i = 0; i < num_dir; i++)
    {
        int count_link = 0;
        for (int j = 0; j < num_dir; j++)
        {
            struct dinode *dir = (struct dinode *) &dip[dir_inodes[j]]; 
            for (int k = 0; k < NDIRECT; ++k)
            {
                struct xv6_dirent* entry = (struct xv6_dirent*)
                    (img_ptr + BSIZE * dir->addrs[k]);
                for (int h = 2; h < num_dirent; ++h)
                {
                    if (entry[h].inum == dir_inodes[i]) {
                        count_link ++;
                    }
                }
            }

            uint* addr_indir = (uint*)(img_ptr + BSIZE * dir->addrs[index_idb]);
                // addr of the indirect data block
            for (int k = 0; k < NINDIRECT; ++k) 
            {
                struct xv6_dirent* entry = (struct xv6_dirent*)
                    (img_ptr + BSIZE * addr_indir[k]); 
                    // entries in each data block pointed by the indirect block
                for (int h = 2; h < num_dirent; ++h)
                {
                    if (entry[h].inum == dir_inodes[i]) {
                        count_link ++;
                    }
                }
            }
        }
        if (i == 0)
            count_link ++;
            // root dir

        if (count_link != 1 || count_link != dip[dir_inodes[i]].type)
        {
            fprintf(stderr, "ERROR: directory appears more than once in file system.\n");
            //printf("dir inode %d has nlink = %d, \nbut is referenced %d times\n", i, dip[i].nlink, count_link);
            exit(1);
        }
    }

    // Extra 1:
    // Correct if: Each .. entry in a directory refers to the proper parent inode 
    //      and parent inode points back to it.
    for (int i = 0; i < num_dir; ++i)
    {
        struct dinode *dir = (struct dinode *) &dip[dir_inodes[i]];
        int foundParent = 0;
        for (int k = 0; k < NDIRECT; ++k)
        {
            struct xv6_dirent* entry = (struct xv6_dirent*)
                (img_ptr + BSIZE * dir->addrs[k]);
            for (int h = 0; h < num_dirent; ++h)
            {
                if (strcmp(entry[h].name, "..") == 0) {

                    // Check if parent contain this dir inode:
                    struct dinode *parent = (struct dinode *) &dip[entry[h].inum];
                    for (int j = 0; j < NDIRECT; ++j)
                    {
                        struct xv6_dirent* entry_parent = (struct xv6_dirent*)
                            (img_ptr + BSIZE * parent->addrs[j]);
                        for (int g = 0; g < num_dirent; ++g)
                        {
                            if (entry_parent[g].inum == dir_inodes[i])
                            {
                                foundParent = 1;
                                break;
                            }
                        }
                        if (foundParent == 1)
                            break;
                    }
                    if (foundParent == 1)
                        break;
                }
            }
            if (foundParent == 1)
                break;
        }
        if (foundParent == 0)
        {
            fprintf(stderr, "ERROR: parent directory mismatch\n");
            printf("inode %d cannot match parent\n", dir_inodes[i]);
            exit(1);
        }
    }

    return 0;
}