/*--------------------------------------------------------------*/
/*---------------------------- mkfs.c --------------------------*/
/*--------------------------------------------------------------*/

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <time.h>
#include <linux/fs.h>
#include <linux/xattr.h>
#include <linux/posix_acl.h>
#include <linux/posix_acl_xattr.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include "../kern/ux_fs.h"

int
main(int argc, char **argv)
{
        struct ux_dirent        dir;
        struct ux_superblock    sb;
        struct ux_inode         inode;
        time_t                  tm;
        off_t                   nsectors = UX_MAXBLOCKS;
        int                     devfd, error, i;
        int                     map_blks;
        char                    block[UX_BSIZE];
        struct posix_acl* acl;
        struct simple_xattrs xattr_list;
        void* xattr = malloc(BLOCK_SIZE);

        if (argc != 2) {
                fprintf(stderr, "uxmkfs: Need to specify device\n");
                exit(1);
        }
        devfd = open(argv[1], O_WRONLY);
        if (devfd < 0) {
                fprintf(stderr, "uxmkfs: Failed to open device\n");
                exit(1);
        }
        error = lseek(devfd, (off_t)(nsectors * 512), SEEK_SET);
        if (error == -1) {
                fprintf(stderr, "uxmkfs: Cannot create filesystem"
                        " of specified size\n");
                exit(1);
        }
        lseek(devfd, 0, SEEK_SET);

        /*
         * Fill in the fields of the superblock and write
         * it out to the first block of the device.
         */

        sb.s_magic = UX_MAGIC;
        sb.s_mod = UX_FSCLEAN;
        sb.s_nifree = UX_MAXFILES - 4;  
        sb.s_nbfree = UX_MAXBLOCKS - 4;

        /*
         * First 4 inodes are in use. Inodes 0 and 1 are not
         * used by anything, 2 is the root directory and 3 is
         * lost+found.
         */

        sb.s_inode[0]  = UX_INODE_INUSE;
        sb.s_inode[1]  = UX_INODE_INUSE;
        sb.s_inode[2]  = UX_INODE_INUSE;
        sb.s_inode[3]  = UX_INODE_INUSE;

        /*
         * The rest of the inodes are marked unused
         */

        for (i = 4 ; i < UX_MAXFILES ; i++) {
                sb.s_inode[i] = UX_INODE_FREE;
        }

        /*
         * The first two blocks are allocated for the entries
         * for the root and lost+found directories.
         */

        sb.s_block[0] = UX_BLOCK_INUSE;
        sb.s_block[1] = UX_BLOCK_INUSE;
        sb.s_block[2] = UX_BLOCK_INUSE;
        sb.s_block[3] = UX_BLOCK_INUSE;

        /*
         * The rest of the blocks are marked unused
         */

        for (i = 4 ; i < UX_MAXBLOCKS ; i++) {
                sb.s_block[i] = UX_BLOCK_FREE;
        }

        write(devfd, (char *)&sb, sizeof(struct ux_superblock));

        /*
         * The root directory and lost+found directory inodes
         * must be initialized.
         */

        time(&tm);
        memset((void *)&inode, 0, sizeof(struct ux_inode));
        inode.i_mode = S_IFDIR | 0755;
        inode.i_nlink = 3;        /* ".", ".." and "lost+found" */
        inode.i_atime = tm;
        inode.i_mtime = tm;
        inode.i_ctime = tm;
        inode.i_uid = 0;
        inode.i_gid = 0;
        inode.i_size = UX_BSIZE;
        inode.i_blocks = 1;
        inode.i_addr[0] = UX_FIRST_DATA_BLOCK;
        inode.i_xattr_blk_addr = inode.i_addr[0] + 1;
        acl = posix_acl_from_mode(inode.i_mode, GFP_KERNEL);
        memset(xattr, 0, BLOCK_SIZE);
        inode.i_xattr_size = posix_acl_xattr_size(acl->a_count);
        posix_acl_to_xattr(&init_user_ns, acl, xattr, inode.i_xattr_size);
        memset(&xattr_list, 0, sizeof(struct simple_xattrs));
        simple_xattrs_init(&xattr_list);
        simple_xattr_set(&xattr_list, XATTR_NAME_POSIX_ACL_DEFAULT, acl, inode.i_xattr_size, XATTR_CREATE);

        lseek(devfd, UX_INODE_BLOCK * UX_BSIZE + 1024, SEEK_SET);
        write(devfd, (char *)&inode, sizeof(struct ux_inode));
        lseek(devfd, inode.i_xattr_blk_addr * UX_BSIZE, SEEK_SET);
        write(devfd, (char *)&xattr_list, inode.i_xattr_size);

        memset((void *)&inode, 0 , sizeof(struct ux_inode));
        inode.i_mode = S_IFDIR | 0755;
        inode.i_nlink = 2;        /* "." and ".." */
        inode.i_atime = tm;
        inode.i_mtime = tm;
        inode.i_ctime = tm;
        inode.i_uid = 0;
        inode.i_gid = 0;
        inode.i_size = UX_BSIZE;
        inode.i_blocks = 1;
        inode.i_addr[0] = UX_FIRST_DATA_BLOCK + 2;
        inode.i_xattr_blk_addr = inode.i_addr[0] + 1;
        acl = posix_acl_from_mode(inode.i_mode, GFP_KERNEL);
        memset(xattr, 0, BLOCK_SIZE);
        posix_acl_to_xattr(&init_user_ns, acl, xattr, posix_acl_xattr_size(acl->a_count));
        memset(&xattr_list, 0, sizeof(struct simple_xattrs));
        simple_xattrs_init(&xattr_list);
        simple_xattr_set(&xattr_list, XATTR_NAME_POSIX_ACL_DEFAULT, acl, inode.i_xattr_size, XATTR_CREATE);
        char *xattr_names = malloc(512);
        ssize_t xattr_names_size = simple_xattr_list(NULL, &xattr_list, xattr_names, 512);
        

        lseek(devfd, UX_INODE_BLOCK * UX_BSIZE + 1536, SEEK_SET);
        write(devfd, (char *)&inode, sizeof(struct ux_inode));
        lseek(devfd, inode.i_xattr_blk_addr * UX_BSIZE, SEEK_SET);
        write(devfd, (char *)&xattr_list, inode.i_xattr_size);

        /*
         * Fill in the directory entries for root 
         */

        lseek(devfd, UX_FIRST_DATA_BLOCK * UX_BSIZE, SEEK_SET);
        memset((void *)&block, 0, UX_BSIZE);
        write(devfd, block, UX_BSIZE);
        lseek(devfd, UX_FIRST_DATA_BLOCK * UX_BSIZE, SEEK_SET);
        dir.d_ino = 2;
        strcpy(dir.d_name, ".");
        write(devfd, (char *)&dir, sizeof(struct ux_dirent));
        dir.d_ino = 2;
        strcpy(dir.d_name, "..");
        write(devfd, (char *)&dir, sizeof(struct ux_dirent));
        dir.d_ino = 3;
        strcpy(dir.d_name, "lost+found");
        write(devfd, (char *)&dir, sizeof(struct ux_dirent));

        /*
         * Fill in the directory entries for lost+found 
         */

        lseek(devfd, UX_FIRST_DATA_BLOCK * UX_BSIZE + UX_BSIZE,
              SEEK_SET);
        memset((void *)&block, 0, UX_BSIZE);
        write(devfd, block, UX_BSIZE);
        lseek(devfd, UX_FIRST_DATA_BLOCK * UX_BSIZE + UX_BSIZE, 
              SEEK_SET);
        dir.d_ino = 2;
        strcpy(dir.d_name, ".");
        write(devfd, (char *)&dir, sizeof(struct ux_dirent));
        dir.d_ino = 2;
        strcpy(dir.d_name, "..");
        write(devfd, (char *)&dir, sizeof(struct ux_dirent));
}

