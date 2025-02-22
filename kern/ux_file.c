/*--------------------------------------------------------------*/
/*--------------------------- ux_file.c ------------------------*/
/*--------------------------------------------------------------*/

#include <linux/fs.h>
#include <linux/buffer_head.h>
#include "ux_fs.h"
#include "ux_xattr.h"
#include "ux_acl.h"

const struct file_operations ux_file_operations = {
	.llseek		= generic_file_llseek,
	.read_iter	= generic_file_read_iter,
	.write_iter	= generic_file_write_iter,
	.mmap		= generic_file_mmap,
};

int ux_get_block(struct inode *inode, sector_t block,
		struct buffer_head *bh_result, int create)
{
	struct super_block *sb = inode->i_sb;
	struct ux_inode *uip = (struct ux_inode *)inode->i_private;
	__u32 blk, acl_blk_num;

	/*
	 * First check to see if the file can be extended.
	 */
	if (block >= UX_DIRECT_BLOCKS) {
		return -EFBIG;
	}

	/*
	 * If we're creating, we must allocate a new block.
	 */
	if (create) {
		blk = ux_data_alloc(sb);
		if (blk == 0) {
			return -ENOSPC;
		}

		acl_blk_num = ux_data_alloc(sb);
		if (!acl_blk_num) {
			iput(inode);
			return -ENOSPC;
		}

		uip->i_addr[block] = blk;
		uip->i_acl_blk_addr = acl_blk_num;
		inode->i_blocks++;
		uip->i_size = inode->i_size;
		mark_inode_dirty(inode);
	}

	map_bh(bh_result, sb, uip->i_addr[block]);
	if (create) {
		set_buffer_new(bh_result);
	}

	return 0;
}

int ux_writepage(struct page *page, struct writeback_control *wbc)
{
	return block_write_full_page(page, ux_get_block, wbc);
}

int ux_readpage(struct file *file, struct page *page)
{
	return block_read_full_page(page, ux_get_block);
}

int ux_write_begin(struct file *file, struct address_space *mapping,
			loff_t pos, unsigned int len, unsigned int flags,
			struct page **pagep, void **fsdata)
{
	return block_write_begin(mapping, pos, len, flags, pagep, ux_get_block);
}

sector_t ux_bmap(struct address_space *mapping, sector_t block)
{
	return generic_block_bmap(mapping, block, ux_get_block);
}

const struct address_space_operations ux_aops = {
	.readpage	= ux_readpage,
	.writepage	= ux_writepage,
	.write_begin	= ux_write_begin,
	.write_end	= generic_write_end,
	.bmap		= ux_bmap,
};

const struct inode_operations ux_file_inops = {
	.link	= ux_link,
	.unlink	= ux_unlink,
	.listxattr	= generic_listxattr,
	.get_acl	= ux_get_acl,
	.set_acl	= ux_set_acl,
};
