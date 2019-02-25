/* $Id$ */
/** @file
 * vboxsf - Linux Shared Folders VFS, internal header.
 */

/*
 * Copyright (C) 2006-2019 Oracle Corporation
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef GA_INCLUDED_SRC_linux_sharedfolders_vfsmod_h
#define GA_INCLUDED_SRC_linux_sharedfolders_vfsmod_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#if 0 /* Enables strict checks. */
# define RT_STRICT
# define VBOX_STRICT
#endif

#define LOG_GROUP LOG_GROUP_SHARED_FOLDERS
#include "the-linux-kernel.h"
#include <iprt/list.h>
#include <iprt/asm.h>
#include <VBox/log.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
# include <linux/backing-dev.h>
#endif

#include <VBox/VBoxGuestLibSharedFolders.h>
#include <VBox/VBoxGuestLibSharedFoldersInline.h>
#include "vbsfmount.h"

#define DIR_BUFFER_SIZE (16*_1K)

/* per-shared folder information */
struct sf_glob_info {
	VBGLSFMAP map;
	struct nls_table *nls;
	int ttl;
	int uid;
	int gid;
	int dmode;
	int fmode;
	int dmask;
	int fmask;
	/** Maximum number of pages to allow in an I/O buffer with the host.
	 * This applies to read and write operations.  */
	uint32_t cMaxIoPages;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0) && LINUX_VERSION_CODE < KERNEL_VERSION(4, 11, 0)
	struct backing_dev_info bdi;
#endif
	char tag[32];		/**< Mount tag for VBoxService automounter.  @since 6.0 */
};

/**
 * For associating inodes with host handles.
 *
 * This is necessary for address_space_operations::sf_writepage and allows
 * optimizing stat, lookups and other operations on open files and directories.
 */
struct sf_handle {
	/** List entry (head sf_inode_info::HandleList). */
	RTLISTNODE              Entry;
	/** Host file/whatever handle. */
	SHFLHANDLE              hHost;
	/** SF_HANDLE_F_XXX */
	uint32_t                fFlags;
	/** Reference counter.
	 * Close the handle and free the structure when it reaches zero. */
	uint32_t volatile       cRefs;
#ifdef VBOX_STRICT
	/** For strictness checks. */
	struct sf_inode_info   *pInodeInfo;
#endif
};

/** @name SF_HANDLE_F_XXX - Handle summary flags (sf_handle::fFlags).
 * @{  */
#define SF_HANDLE_F_READ        UINT32_C(0x00000001)
#define SF_HANDLE_F_WRITE       UINT32_C(0x00000002)
#define SF_HANDLE_F_APPEND      UINT32_C(0x00000004)
#define SF_HANDLE_F_FILE        UINT32_C(0x00000010)
#define SF_HANDLE_F_ON_LIST     UINT32_C(0x00000080)
#define SF_HANDLE_F_MAGIC_MASK  UINT32_C(0xffffff00)
#define SF_HANDLE_F_MAGIC     	UINT32_C(0x75030700) /**< Maurice Ravel (1875-03-07). */
#define SF_HANDLE_F_MAGIC_DEAD  UINT32_C(0x19371228)
/** @} */

/**
 * VBox specific per-inode information.
 */
struct sf_inode_info {
	/** Which file */
	SHFLSTRING *path;
	/** Some information was changed, update data on next revalidate */
	int force_restat;
	/** directory content changed, update the whole directory on next sf_getdent */
	int force_reread;

	/** handle valid if a file was created with sf_create_aux until it will
	 * be opened with sf_reg_open()
	 * @todo r=bird: figure this one out...  */
	SHFLHANDLE handle;

	/** List of open handles (struct sf_handle), protected by g_SfHandleLock. */
	RTLISTANCHOR 	        HandleList;
#ifdef VBOX_STRICT
	uint32_t                u32Magic;
# define SF_INODE_INFO_MAGIC            UINT32_C(0x18620822) /**< Claude Debussy */
# define SF_INODE_INFO_MAGIC_DEAD       UINT32_C(0x19180325)
#endif
};

struct sf_dir_info {
	/** @todo sf_handle. */
	struct list_head info_list;
};

struct sf_dir_buf {
	size_t cEntries;
	size_t cbFree;
	size_t cbUsed;
	void *buf;
	struct list_head head;
};

/**
 * VBox specific infor fore a regular file.
 */
struct sf_reg_info {
	/** Handle tracking structure. */
	struct sf_handle        Handle;
};

/* globals */
extern VBGLSFCLIENT client_handle;
extern spinlock_t g_SfHandleLock;


/* forward declarations */
extern struct inode_operations sf_dir_iops;
extern struct inode_operations sf_lnk_iops;
extern struct inode_operations sf_reg_iops;
extern struct file_operations sf_dir_fops;
extern struct file_operations sf_reg_fops;
extern struct dentry_operations sf_dentry_ops;
extern struct address_space_operations sf_reg_aops;

extern void              sf_handle_drop_chain(struct sf_inode_info *pInodeInfo);
extern struct sf_handle *sf_handle_find(struct sf_inode_info *pInodeInfo, uint32_t fFlagsSet, uint32_t fFlagsClear);
extern uint32_t          sf_handle_release_slow(struct sf_handle *pHandle, struct sf_glob_info *sf_g, const char *pszCaller);
extern void              sf_handle_append(struct sf_inode_info *pInodeInfo, struct sf_handle *pHandle);

/**
 * Releases a handle.
 *
 * @returns New reference count.
 * @param   pHandle         The handle to release.
 * @param   sf_g            The info structure for the shared folder associated
 *      		    with the handle.
 * @param   pszCaller       The caller name (for logging failures).
 */
DECLINLINE(uint32_t) sf_handle_release(struct sf_handle *pHandle, struct sf_glob_info *sf_g, const char *pszCaller)
{
	uint32_t cRefs;

	Assert((pHandle->fFlags & SF_HANDLE_F_MAGIC_MASK) == SF_HANDLE_F_MAGIC);
	Assert(pHandle->pInodeInfo);
	Assert(pHandle->pInodeInfo && pHandle->pInodeInfo->u32Magic == SF_INODE_INFO_MAGIC);

	cRefs = ASMAtomicDecU32(&pHandle->cRefs);
	Assert(cRefs < _64M);
	if (cRefs)
		return cRefs;
	return sf_handle_release_slow(pHandle, sf_g, pszCaller);
}

extern void sf_init_inode(struct sf_glob_info *sf_g, struct inode *inode,
			  PSHFLFSOBJINFO info);
extern int sf_stat(const char *caller, struct sf_glob_info *sf_g,
		   SHFLSTRING * path, PSHFLFSOBJINFO result, int ok_to_fail);
extern int sf_inode_revalidate(struct dentry *dentry);
int sf_inode_revalidate_with_handle(struct dentry *dentry, SHFLHANDLE hHostFile, bool fForced);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
# if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
extern int sf_getattr(const struct path *path, struct kstat *kstat,
		      u32 request_mask, unsigned int query_flags);
# else
extern int sf_getattr(struct vfsmount *mnt, struct dentry *dentry,
		      struct kstat *kstat);
# endif
extern int sf_setattr(struct dentry *dentry, struct iattr *iattr);
#endif
extern int sf_path_from_dentry(const char *caller, struct sf_glob_info *sf_g,
			       struct sf_inode_info *sf_i,
			       struct dentry *dentry, SHFLSTRING ** result);
extern int sf_nlscpy(struct sf_glob_info *sf_g, char *name,
		     size_t name_bound_len, const unsigned char *utf8_name,
		     size_t utf8_len);
extern void sf_dir_info_free(struct sf_dir_info *p);
extern void sf_dir_info_empty(struct sf_dir_info *p);
extern struct sf_dir_info *sf_dir_info_alloc(void);
extern int sf_dir_read_all(struct sf_glob_info *sf_g,
			   struct sf_inode_info *sf_i, struct sf_dir_info *sf_d,
			   SHFLHANDLE handle);
extern int sf_init_backing_dev(struct super_block *sb, struct sf_glob_info *sf_g);
extern void sf_done_backing_dev(struct super_block *sb, struct sf_glob_info *sf_g);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
# define STRUCT_STATFS  struct statfs
#else
# define STRUCT_STATFS  struct kstatfs
#endif
int sf_get_volume_info(struct super_block *sb, STRUCT_STATFS * stat);

#ifdef __cplusplus
# define CMC_API __attribute__ ((cdecl, regparm (0)))
#else
# define CMC_API __attribute__ ((regparm (0)))
#endif

#if 1
# define TRACE()          LogFunc(("tracepoint\n"))
# define SFLOGFLOW(aArgs) Log(aArgs)
#else
# define TRACE()          RTLogBackdoorPrintf("%s: tracepoint\n", __FUNCTION__)
# define SFLOGFLOW(aArgs) RTLogBackdoorPrintf aArgs
#endif

/* Following casts are here to prevent assignment of void * to
   pointers of arbitrary type */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
# define GET_GLOB_INFO(sb)       ((struct sf_glob_info *) (sb)->u.generic_sbp)
# define SET_GLOB_INFO(sb, sf_g) (sb)->u.generic_sbp = sf_g
#else
# define GET_GLOB_INFO(sb)       ((struct sf_glob_info *) (sb)->s_fs_info)
# define SET_GLOB_INFO(sb, sf_g) (sb)->s_fs_info = sf_g
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19) || defined(KERNEL_FC6)
/* FC6 kernel 2.6.18, vanilla kernel 2.6.19+ */
# define GET_INODE_INFO(i)       ((struct sf_inode_info *) (i)->i_private)
# define SET_INODE_INFO(i, sf_i) (i)->i_private = sf_i
#else
/* vanilla kernel up to 2.6.18 */
# define GET_INODE_INFO(i)       ((struct sf_inode_info *) (i)->u.generic_ip)
# define SET_INODE_INFO(i, sf_i) (i)->u.generic_ip = sf_i
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0)
# define GET_F_DENTRY(f)        (f->f_path.dentry)
#else
# define GET_F_DENTRY(f)        (f->f_dentry)
#endif

#endif /* !GA_INCLUDED_SRC_linux_sharedfolders_vfsmod_h */
