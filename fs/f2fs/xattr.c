/*
 * fs/f2fs/xattr.c
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *             http://www.samsung.com/
 *
 * Portions of this code from linux/fs/ext2/xattr.c
 *
 * Copyright (C) 2001-2003 Andreas Gruenbacher <agruen@suse.de>
 *
 * Fix by Harrison Xing <harrison@mountainviewdata.com>.
 * Extended attributes for symlinks and special files added per
 *  suggestion of Luka Renko <luka.renko@hermes.si>.
 * xattr consolidation Copyright (c) 2004 James Morris <jmorris@redhat.com>,
 *  Red Hat Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/rwsem.h>
#include <linux/f2fs_fs.h>
#include <linux/security.h>
#include <linux/posix_acl_xattr.h>
#include "f2fs.h"
#include "xattr.h"

static int f2fs_xattr_generic_get(const struct xattr_handler *handler,
		struct dentry *unused, struct inode *inode,
		const char *name, void *buffer, size_t size)
{
	struct f2fs_sb_info *sbi = F2FS_SB(inode->i_sb);

	switch (handler->flags) {
	case F2FS_XATTR_INDEX_USER:
		if (!test_opt(sbi, XATTR_USER))
			return -EOPNOTSUPP;
		break;
	case F2FS_XATTR_INDEX_TRUSTED:
		if (!capable(CAP_SYS_ADMIN))
			return -EPERM;
		break;
	case F2FS_XATTR_INDEX_SECURITY:
		break;
	default:
		return -EINVAL;
	}
	return f2fs_getxattr(inode, handler->flags, name,
			     buffer, size, NULL);
}

static int f2fs_xattr_generic_set(const struct xattr_handler *handler,
		struct dentry *unused, struct inode *inode,
		const char *name, const void *value,
		size_t size, int flags)
{
	struct f2fs_sb_info *sbi = F2FS_SB(inode->i_sb);

	switch (handler->flags) {
	case F2FS_XATTR_INDEX_USER:
		if (!test_opt(sbi, XATTR_USER))
			return -EOPNOTSUPP;
		break;
	case F2FS_XATTR_INDEX_TRUSTED:
		if (!capable(CAP_SYS_ADMIN))
			return -EPERM;
		break;
	case F2FS_XATTR_INDEX_SECURITY:
		break;
	default:
		return -EINVAL;
	}
	return f2fs_setxattr(inode, handler->flags, name,
					value, size, NULL, flags);
}

static bool f2fs_xattr_user_list(struct dentry *dentry)
{
	struct f2fs_sb_info *sbi = F2FS_SB(dentry->d_sb);

	return test_opt(sbi, XATTR_USER);
}

static bool f2fs_xattr_trusted_list(struct dentry *dentry)
{
	return capable(CAP_SYS_ADMIN);
}

static int f2fs_xattr_advise_get(const struct xattr_handler *handler,
		struct dentry *unused, struct inode *inode,
		const char *name, void *buffer, size_t size)
{
	if (buffer)
		*((char *)buffer) = F2FS_I(inode)->i_advise;
	return sizeof(char);
}

static int f2fs_xattr_advise_set(const struct xattr_handler *handler,
		struct dentry *unused, struct inode *inode,
		const char *name, const void *value,
		size_t size, int flags)
{
	if (!inode_owner_or_capable(inode))
		return -EPERM;
	if (value == NULL)
		return -EINVAL;

	F2FS_I(inode)->i_advise |= *(char *)value;
	f2fs_mark_inode_dirty_sync(inode, true);
	return 0;
}

#ifdef CONFIG_F2FS_FS_SECURITY
static int f2fs_initxattrs(struct inode *inode, const struct xattr *xattr_array,
		void *page)
{
	const struct xattr *xattr;
	int err = 0;

	for (xattr = xattr_array; xattr->name != NULL; xattr++) {
		err = f2fs_setxattr(inode, F2FS_XATTR_INDEX_SECURITY,
				xattr->name, xattr->value,
				xattr->value_len, (struct page *)page, 0);
		if (err < 0)
			break;
	}
	return err;
}

int f2fs_init_security(struct inode *inode, struct inode *dir,
				const struct qstr *qstr, struct page *ipage)
{
	return security_inode_init_security(inode, dir, qstr,
				&f2fs_initxattrs, ipage);
}
#endif

const struct xattr_handler f2fs_xattr_user_handler = {
	.prefix	= XATTR_USER_PREFIX,
	.flags	= F2FS_XATTR_INDEX_USER,
	.list	= f2fs_xattr_user_list,
	.get	= f2fs_xattr_generic_get,
	.set	= f2fs_xattr_generic_set,
};

const struct xattr_handler f2fs_xattr_trusted_handler = {
	.prefix	= XATTR_TRUSTED_PREFIX,
	.flags	= F2FS_XATTR_INDEX_TRUSTED,
	.list	= f2fs_xattr_trusted_list,
	.get	= f2fs_xattr_generic_get,
	.set	= f2fs_xattr_generic_set,
};

const struct xattr_handler f2fs_xattr_advise_handler = {
	.name	= F2FS_SYSTEM_ADVISE_NAME,
	.flags	= F2FS_XATTR_INDEX_ADVISE,
	.get    = f2fs_xattr_advise_get,
	.set    = f2fs_xattr_advise_set,
};

const struct xattr_handler f2fs_xattr_security_handler = {
	.prefix	= XATTR_SECURITY_PREFIX,
	.flags	= F2FS_XATTR_INDEX_SECURITY,
	.get	= f2fs_xattr_generic_get,
	.set	= f2fs_xattr_generic_set,
};

static const struct xattr_handler *f2fs_xattr_handler_map[] = {
	[F2FS_XATTR_INDEX_USER] = &f2fs_xattr_user_handler,
#ifdef CONFIG_F2FS_FS_POSIX_ACL
	[F2FS_XATTR_INDEX_POSIX_ACL_ACCESS] = &posix_acl_access_xattr_handler,
	[F2FS_XATTR_INDEX_POSIX_ACL_DEFAULT] = &posix_acl_default_xattr_handler,
#endif
	[F2FS_XATTR_INDEX_TRUSTED] = &f2fs_xattr_trusted_handler,
#ifdef CONFIG_F2FS_FS_SECURITY
	[F2FS_XATTR_INDEX_SECURITY] = &f2fs_xattr_security_handler,
#endif
	[F2FS_XATTR_INDEX_ADVISE] = &f2fs_xattr_advise_handler,
};

const struct xattr_handler *f2fs_xattr_handlers[] = {
	&f2fs_xattr_user_handler,
#ifdef CONFIG_F2FS_FS_POSIX_ACL
	&posix_acl_access_xattr_handler,
	&posix_acl_default_xattr_handler,
#endif
	&f2fs_xattr_trusted_handler,
#ifdef CONFIG_F2FS_FS_SECURITY
	&f2fs_xattr_security_handler,
#endif
	&f2fs_xattr_advise_handler,
	NULL,
};

static inline const struct xattr_handler *f2fs_xattr_handler(int index)
{
	const struct xattr_handler *handler = NULL;

	if (index > 0 && index < ARRAY_SIZE(f2fs_xattr_handler_map))
		handler = f2fs_xattr_handler_map[index];
	return handler;
}

static struct f2fs_xattr_entry *__find_xattr(void *base_addr,
				void *last_base_addr, int index,
				size_t len, const char *name)
{
	struct f2fs_xattr_entry *entry;

	list_for_each_xattr(entry, base_addr) {
		if ((void *)(entry) + sizeof(__u32) > last_base_addr ||
			(void *)XATTR_NEXT_ENTRY(entry) > last_base_addr)
			return NULL;

		if (entry->e_name_index != index)
			continue;
		if (entry->e_name_len != len)
			continue;
		if (!memcmp(entry->e_name, name, len))
			break;
	}
	return entry;
}

static struct f2fs_xattr_entry *__find_inline_xattr(void *base_addr,
					void **last_addr, int index,
					size_t len, const char *name)
{
	struct f2fs_xattr_entry *entry;
	unsigned int inline_size = F2FS_INLINE_XATTR_ADDRS << 2;

	list_for_each_xattr(entry, base_addr) {
		if ((void *)entry + sizeof(__u32) > base_addr + inline_size ||
			(void *)XATTR_NEXT_ENTRY(entry) + sizeof(__u32) >
			base_addr + inline_size) {
			*last_addr = entry;
			return NULL;
		}
		if (entry->e_name_index != index)
			continue;
		if (entry->e_name_len != len)
			continue;
		if (!memcmp(entry->e_name, name, len))
			break;
	}
	return entry;
}

static int read_inline_xattr(struct inode *inode, struct page *ipage,
							void *txattr_addr)
{
	struct f2fs_sb_info *sbi = F2FS_I_SB(inode);
	unsigned int inline_size = inline_xattr_size(inode);
	struct page *page = NULL;
	void *inline_addr;

	if (ipage) {
		inline_addr = inline_xattr_addr(ipage);
	} else {
		page = get_node_page(sbi, inode->i_ino);
		if (IS_ERR(page))
			return PTR_ERR(page);

		inline_addr = inline_xattr_addr(page);
	}
	memcpy(txattr_addr, inline_addr, inline_size);
	f2fs_put_page(page, 1);

	return 0;
}

static int read_xattr_block(struct inode *inode, void *txattr_addr)
{
	struct f2fs_sb_info *sbi = F2FS_I_SB(inode);
	nid_t xnid = F2FS_I(inode)->i_xattr_nid;
	unsigned int inline_size = inline_xattr_size(inode);
	struct page *xpage;
	void *xattr_addr;

	/* The inode already has an extended attribute block. */
	xpage = get_node_page(sbi, xnid);
	if (IS_ERR(xpage))
		return PTR_ERR(xpage);

	xattr_addr = page_address(xpage);
	memcpy(txattr_addr + inline_size, xattr_addr, VALID_XATTR_BLOCK_SIZE);
	f2fs_put_page(xpage, 1);

	return 0;
}

static int lookup_all_xattrs(struct inode *inode, struct page *ipage,
				unsigned int index, unsigned int len,
				const char *name, struct f2fs_xattr_entry **xe,
				void **base_addr, int *base_size)
{
	void *cur_addr, *txattr_addr, *last_txattr_addr;
	void *last_addr = NULL;
	nid_t xnid = F2FS_I(inode)->i_xattr_nid;
	unsigned int inline_size = inline_xattr_size(inode);
	int err = 0;

	if (!xnid && !inline_size)
		return -ENODATA;

	*base_size = XATTR_SIZE(xnid, inode) + XATTR_PADDING_SIZE;
	txattr_addr = kzalloc(*base_size, GFP_F2FS_ZERO);
	if (!txattr_addr)
		return -ENOMEM;

	last_txattr_addr = (void *)txattr_addr + XATTR_SIZE(xnid, inode);

	/* read from inline xattr */
	if (inline_size) {
		err = read_inline_xattr(inode, ipage, txattr_addr);
		if (err)
			goto out;

		*xe = __find_inline_xattr(txattr_addr, &last_addr,
						index, len, name);
		if (*xe) {
			*base_size = inline_size;
			goto check;
		}
	}

	/* read from xattr node block */
	if (xnid) {
		err = read_xattr_block(inode, txattr_addr);
		if (err)
			goto out;
	}

	if (last_addr)
		cur_addr = XATTR_HDR(last_addr) - 1;
	else
		cur_addr = txattr_addr;

	*xe = __find_xattr(cur_addr, last_txattr_addr, index, len, name);
	if (!*xe) {
		err = -EFAULT;
		goto out;
	}
check:
	if (IS_XATTR_LAST_ENTRY(*xe)) {
		err = -ENODATA;
		goto out;
	}

	*base_addr = txattr_addr;
	return 0;
out:
	kzfree(txattr_addr);
	return err;
}

static int read_all_xattrs(struct inode *inode, struct page *ipage,
							void **base_addr)
{
	struct f2fs_xattr_header *header;
	nid_t xnid = F2FS_I(inode)->i_xattr_nid;
	unsigned int size = VALID_XATTR_BLOCK_SIZE;
	unsigned int inline_size = inline_xattr_size(inode);
	void *txattr_addr;
	int err;

	txattr_addr = kzalloc(inline_size + size + XATTR_PADDING_SIZE,
							GFP_F2FS_ZERO);
	if (!txattr_addr)
		return -ENOMEM;

	/* read from inline xattr */
	if (inline_size) {
		err = read_inline_xattr(inode, ipage, txattr_addr);
		if (err)
			goto fail;
	}

	/* read from xattr node block */
	if (xnid) {
		err = read_xattr_block(inode, txattr_addr);
		if (err)
			goto fail;
	}

	header = XATTR_HDR(txattr_addr);

	/* never been allocated xattrs */
	if (le32_to_cpu(header->h_magic) != F2FS_XATTR_MAGIC) {
		header->h_magic = cpu_to_le32(F2FS_XATTR_MAGIC);
		header->h_refcount = cpu_to_le32(1);
	}
	*base_addr = txattr_addr;
	return 0;
fail:
	kzfree(txattr_addr);
	return err;
}

static inline int write_all_xattrs(struct inode *inode, __u32 hsize,
				void *txattr_addr, struct page *ipage)
{
	struct f2fs_sb_info *sbi = F2FS_I_SB(inode);
	size_t inline_size = inline_xattr_size(inode);
	void *xattr_addr;
	struct page *xpage;
	nid_t new_nid = 0;
	int err;

	if (hsize > inline_size && !F2FS_I(inode)->i_xattr_nid)
		if (!alloc_nid(sbi, &new_nid))
			return -ENOSPC;

	/* write to inline xattr */
	if (inline_size) {
		struct page *page = NULL;
		void *inline_addr;

		if (ipage) {
			inline_addr = inline_xattr_addr(ipage);
			f2fs_wait_on_page_writeback(ipage, NODE, true);
			set_page_dirty(ipage);
		} else {
			page = get_node_page(sbi, inode->i_ino);
			if (IS_ERR(page)) {
				alloc_nid_failed(sbi, new_nid);
				return PTR_ERR(page);
			}
			inline_addr = inline_xattr_addr(page);
			f2fs_wait_on_page_writeback(page, NODE, true);
		}
		memcpy(inline_addr, txattr_addr, inline_size);
		f2fs_put_page(page, 1);

		/* no need to use xattr node block */
		if (hsize <= inline_size) {
			err = truncate_xattr_node(inode, ipage);
			alloc_nid_failed(sbi, new_nid);
			return err;
		}
	}

	/* write to xattr node block */
	if (F2FS_I(inode)->i_xattr_nid) {
		xpage = get_node_page(sbi, F2FS_I(inode)->i_xattr_nid);
		if (IS_ERR(xpage)) {
			alloc_nid_failed(sbi, new_nid);
			return PTR_ERR(xpage);
		}
		f2fs_bug_on(sbi, new_nid);
		f2fs_wait_on_page_writeback(xpage, NODE, true);
	} else {
		struct dnode_of_data dn;
		set_new_dnode(&dn, inode, NULL, NULL, new_nid);
		xpage = new_node_page(&dn, XATTR_NODE_OFFSET);
		if (IS_ERR(xpage)) {
			alloc_nid_failed(sbi, new_nid);
			return PTR_ERR(xpage);
		}
		alloc_nid_done(sbi, new_nid);
	}

	xattr_addr = page_address(xpage);
	memcpy(xattr_addr, txattr_addr + inline_size, VALID_XATTR_BLOCK_SIZE);
	set_page_dirty(xpage);
	f2fs_put_page(xpage, 1);

	return 0;
}

int f2fs_getxattr(struct inode *inode, int index, const char *name,
		void *buffer, size_t buffer_size, struct page *ipage)
{
	struct f2fs_xattr_entry *entry = NULL;
	int error = 0;
	unsigned int size, len;
	void *base_addr = NULL;
	int base_size;

	if (name == NULL)
		return -EINVAL;

	len = strlen(name);
	if (len > F2FS_NAME_LEN)
		return -ERANGE;

	down_read(&F2FS_I(inode)->i_xattr_sem);
	error = lookup_all_xattrs(inode, ipage, index, len, name,
				&entry, &base_addr, &base_size);
	up_read(&F2FS_I(inode)->i_xattr_sem);
	if (error)
		return error;

	size = le16_to_cpu(entry->e_value_size);

	if (buffer && size > buffer_size) {
		error = -ERANGE;
		goto out;
	}

	if (buffer) {
		char *pval = entry->e_name + entry->e_name_len;

		if (base_size - (pval - (char *)base_addr) < size) {
			error = -ERANGE;
			goto out;
		}
		memcpy(buffer, pval, size);
	}
	error = size;
out:
	kzfree(base_addr);
	return error;
}

ssize_t f2fs_listxattr(struct dentry *dentry, char *buffer, size_t buffer_size)
{
	struct inode *inode = d_inode(dentry);
	nid_t xnid = F2FS_I(inode)->i_xattr_nid;
	struct f2fs_xattr_entry *entry;
	void *base_addr, *last_base_addr;
	int error = 0;
	size_t rest = buffer_size;

	down_read(&F2FS_I(inode)->i_xattr_sem);
	error = read_all_xattrs(inode, NULL, &base_addr);
	up_read(&F2FS_I(inode)->i_xattr_sem);
	if (error)
		return error;

	last_base_addr = (void *)base_addr + XATTR_SIZE(xnid, inode);

	list_for_each_xattr(entry, base_addr) {
		const struct xattr_handler *handler =
			f2fs_xattr_handler(entry->e_name_index);
		const char *prefix;
		size_t prefix_len;
		size_t size;

		if ((void *)(entry) + sizeof(__u32) > last_base_addr ||
			(void *)XATTR_NEXT_ENTRY(entry) > last_base_addr) {
			f2fs_msg(dentry->d_sb, KERN_ERR,
				 "inode (%lu) has corrupted xattr",
				 inode->i_ino);
			set_sbi_flag(F2FS_I_SB(inode), SBI_NEED_FSCK);
			error = -EFSCORRUPTED;
			goto cleanup;
		}

		if (!handler || (handler->list && !handler->list(dentry)))
			continue;

		prefix = handler->prefix ?: handler->name;
		prefix_len = strlen(prefix);
		size = prefix_len + entry->e_name_len + 1;
		if (buffer) {
			if (size > rest) {
				error = -ERANGE;
				goto cleanup;
			}
			memcpy(buffer, prefix, prefix_len);
			buffer += prefix_len;
			memcpy(buffer, entry->e_name, entry->e_name_len);
			buffer += entry->e_name_len;
			*buffer++ = 0;
		}
		rest -= size;
	}
	error = buffer_size - rest;
cleanup:
	kzfree(base_addr);
	return error;
}

static bool f2fs_xattr_value_same(struct f2fs_xattr_entry *entry,
					const void *value, size_t size)
{
	void *pval = entry->e_name + entry->e_name_len;

	return (le16_to_cpu(entry->e_value_size) == size) &&
					!memcmp(pval, value, size);
}

static int __f2fs_setxattr(struct inode *inode, int index,
			const char *name, const void *value, size_t size,
			struct page *ipage, int flags)
{
	struct f2fs_xattr_entry *here, *last;
	void *base_addr, *last_base_addr;
	nid_t xnid = F2FS_I(inode)->i_xattr_nid;
	int found, newsize;
	size_t len;
	__u32 new_hsize;
	int error = 0;

	if (name == NULL)
		return -EINVAL;

	if (value == NULL)
		size = 0;

	len = strlen(name);

	if (len > F2FS_NAME_LEN)
		return -ERANGE;

	if (size > MAX_VALUE_LEN(inode))
		return -E2BIG;

	error = read_all_xattrs(inode, ipage, &base_addr);
	if (error)
		return error;

	last_base_addr = (void *)base_addr + XATTR_SIZE(xnid, inode);

	/* find entry with wanted name. */
	here = __find_xattr(base_addr, last_base_addr, index, len, name);
	if (!here) {
		error = -EFAULT;
		goto exit;
	}

	found = IS_XATTR_LAST_ENTRY(here) ? 0 : 1;

	if (found) {
		if ((flags & XATTR_CREATE)) {
			error = -EEXIST;
			goto exit;
		}

		if (f2fs_xattr_value_same(here, value, size))
			goto exit;
	} else if ((flags & XATTR_REPLACE)) {
		error = -ENODATA;
		goto exit;
	}

	last = here;
	while (!IS_XATTR_LAST_ENTRY(last)) {
		if ((void *)(last) + sizeof(__u32) > last_base_addr ||
			(void *)XATTR_NEXT_ENTRY(last) > last_base_addr) {
			set_sbi_flag(F2FS_I_SB(inode), SBI_NEED_FSCK);
			error = -EFSCORRUPTED;
			goto exit;
		}
		last = XATTR_NEXT_ENTRY(last);
	}

	newsize = XATTR_ALIGN(sizeof(struct f2fs_xattr_entry) + len + size);

	/* 1. Check space */
	if (value) {
		int free;
		/*
		 * If value is NULL, it is remove operation.
		 * In case of update operation, we calculate free.
		 */
		free = MIN_OFFSET(inode) - ((char *)last - (char *)base_addr);
		if (found)
			free = free + ENTRY_SIZE(here);

		if (unlikely(free < newsize)) {
			error = -E2BIG;
			goto exit;
		}
	}

	/* 2. Remove old entry */
	if (found) {
		/*
		 * If entry is found, remove old entry.
		 * If not found, remove operation is not needed.
		 */
		struct f2fs_xattr_entry *next = XATTR_NEXT_ENTRY(here);
		int oldsize = ENTRY_SIZE(here);

		memmove(here, next, (char *)last - (char *)next);
		last = (struct f2fs_xattr_entry *)((char *)last - oldsize);
		memset(last, 0, oldsize);
	}

	new_hsize = (char *)last - (char *)base_addr;

	/* 3. Write new entry */
	if (value) {
		char *pval;
		/*
		 * Before we come here, old entry is removed.
		 * We just write new entry.
		 */
		last->e_name_index = index;
		last->e_name_len = len;
		memcpy(last->e_name, name, len);
		pval = last->e_name + len;
		memcpy(pval, value, size);
		last->e_value_size = cpu_to_le16(size);
		new_hsize += newsize;
	}

	error = write_all_xattrs(inode, new_hsize, base_addr, ipage);
	if (error)
		goto exit;

	if (is_inode_flag_set(inode, FI_ACL_MODE)) {
		inode->i_mode = F2FS_I(inode)->i_acl_mode;
		inode->i_ctime = current_time(inode);
		clear_inode_flag(inode, FI_ACL_MODE);
	}
	if (index == F2FS_XATTR_INDEX_ENCRYPTION &&
			!strcmp(name, F2FS_XATTR_NAME_ENCRYPTION_CONTEXT))
		f2fs_set_encrypted_inode(inode);
	f2fs_mark_inode_dirty_sync(inode, true);
	if (!error && S_ISDIR(inode->i_mode))
		set_sbi_flag(F2FS_I_SB(inode), SBI_NEED_CP);
exit:
	kzfree(base_addr);
	return error;
}

int f2fs_setxattr(struct inode *inode, int index, const char *name,
				const void *value, size_t size,
				struct page *ipage, int flags)
{
	struct f2fs_sb_info *sbi = F2FS_I_SB(inode);
	int err;

	/* this case is only from init_inode_metadata */
	if (ipage)
		return __f2fs_setxattr(inode, index, name, value,
						size, ipage, flags);
	f2fs_balance_fs(sbi, true);

	f2fs_lock_op(sbi);
	/* protect xattr_ver */
	down_write(&F2FS_I(inode)->i_sem);
	down_write(&F2FS_I(inode)->i_xattr_sem);
	err = __f2fs_setxattr(inode, index, name, value, size, ipage, flags);
	up_write(&F2FS_I(inode)->i_xattr_sem);
	up_write(&F2FS_I(inode)->i_sem);
	f2fs_unlock_op(sbi);

	f2fs_update_time(sbi, REQ_TIME);
	return err;
}