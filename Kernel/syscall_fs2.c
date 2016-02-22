#include <kernel.h>
#include <version.h>
#include <kdata.h>
#include <printf.h>

/*
 *	File system related calls that are not continually used (so
 *	we can potentially bank them out if we do a 32K/32K mode)
 */

static arg_t chdiroot_op(inoptr ino, inoptr * p)
{
	if (getmode(ino) != MODE_R(F_DIR)) {
		udata.u_error = ENOTDIR;
		i_deref(ino);
		return (-1);
	}
	i_deref(*p);
	*p = ino;
	return 0;
}

/*******************************************
  chdir (dir)                      Function 10
  char *dir;
 ********************************************/
#define dir (char *)udata.u_argn

arg_t _chdir(void)
{
	inoptr newcwd;

	if (!(newcwd = n_open(dir, NULLINOPTR)))
		return (-1);
	return chdiroot_op(newcwd, &udata.u_cwd);
}

#undef dir

/*******************************************
  fchdir(fd)                      Function 48
  int fd;
 ********************************************/
#define fd (int16_t)udata.u_argn

arg_t _fchdir(void)
{
	inoptr newcwd;

	if ((newcwd = getinode(fd)) == NULLINODE)
		return (-1);
	i_ref(newcwd);
	return chdiroot_op(newcwd, &udata.u_cwd);
}

#undef fd

/*******************************************
  chroot (dir)                      Function 46
  char *dir;
 ********************************************/
#define dir (char *)udata.u_argn

arg_t _chroot(void)
{
	inoptr newroot;

	if (esuper())
		return (-1);
	if (!(newroot = n_open(dir, NULLINOPTR)))
		return (-1);
	return chdiroot_op(newroot, &udata.u_root);
}

#undef dir


/*******************************************
  mknod (name, mode, dev)           Function 4
  char  *name;
  int16_t mode;
  int16_t dev;
 ********************************************/
#define name (char *)udata.u_argn
#define mode (int16_t)udata.u_argn1
#define dev  (int16_t)udata.u_argn2

arg_t _mknod(void)
{
	inoptr ino;
	inoptr parent;
	char fname[FILENAME_LEN + 1];

	udata.u_error = 0;

	if (!super() && ((mode & F_MASK) != F_PIPE)) {
		udata.u_error = EPERM;
		goto nogood3;
	}
	if ((ino = n_open(name, &parent)) != NULL) {
		udata.u_error = EEXIST;
		goto nogood;
	}

	if (!parent) {
		udata.u_error = ENOENT;
		return (-1);
	}

	filename(name, fname);
	ino = newfile(parent, fname);
	if(!ino)
		goto nogood3;	/* parent inode is derefed in newfile. SN */

	/* Initialize mode and dev */
	ino->c_node.i_mode = mode & ~udata.u_mask;
	ino->c_node.i_addr[0] = isdevice(ino) ? dev : 0;
	setftime(ino, A_TIME | M_TIME | C_TIME);
	wr_inode(ino);

	i_deref(ino);
	return (0);

      nogood:
	i_deref(ino);
	i_deref(parent);
      nogood3:
	return (-1);
}

#undef name
#undef mode
#undef dev

/*******************************************
  access (path, mode)              Function 12        ?
  char  *path;
  int16_t mode;
 ********************************************/
#define path (char *)udata.u_argn
#define mode (int16_t)udata.u_argn1

arg_t _access(void)
{
	inoptr ino;
	uint16_t euid;
	uint16_t egid;
	int16_t retval;

	if ((mode & 07) && !ugetc(path)) {
		udata.u_error = ENOENT;
		return (-1);
	}

	/* Temporarily make eff. id real id. */
	euid = udata.u_euid;
	egid = udata.u_egid;
	udata.u_euid = udata.u_ptab->p_uid;
	udata.u_egid = udata.u_gid;

	if (!(ino = n_open(path, NULLINOPTR))) {
		retval = -1;
		goto nogood;
	}

	retval = 0;
	if (~getperm(ino) & (mode & 07)) {
		udata.u_error = EACCES;
		retval = -1;
	}

	i_deref(ino);
      nogood:
	udata.u_euid = euid;
	udata.u_egid = egid;

	return (retval);
}

#undef path
#undef mode

#define mode (int16_t)udata.u_argn1

static arg_t chmod_op(inoptr ino)
{
	if (ino->c_node.i_uid != udata.u_euid && esuper())
		return (-1);
	if (ino->c_flags & CRDONLY) {
		udata.u_error = EROFS;
		return -1;
	}

	ino->c_node.i_mode =
	    (mode & MODE_MASK) | (ino->c_node.i_mode & F_MASK);
	setftime(ino, C_TIME);
	return 0;
}

#undef mode

/*******************************************
  chmod (path, mode)               Function 13
  char  *path;
  int16_t mode;
 ********************************************/
#define path (char *)udata.u_argn

arg_t _chmod(void)
{
	inoptr ino;
	int ret;

	if (!(ino = n_open(path, NULLINOPTR)))
		return (-1);
	ret = chmod_op(ino);
	i_deref(ino);
	return ret;
}

#undef path

/*******************************************
  fchmod (path, mode)               Function 49
  int fd;
  int16_t mode;
 ********************************************/
#define fd (int16_t)udata.u_argn

arg_t _fchmod(void)
{
	inoptr ino;

	if ((ino = getinode(fd)) == NULLINODE)
		return (-1);

	return chmod_op(ino);
}

#undef fd

#define owner (int16_t)udata.u_argn1
#define group (int16_t)udata.u_argn2

static int chown_op(inoptr ino)
{
	if (ino->c_node.i_uid != udata.u_euid && esuper())
		return (-1);
	if (ino->c_flags & CRDONLY) {
		udata.u_error = EROFS;
		return -1;
	}
	ino->c_node.i_uid = owner;
	ino->c_node.i_gid = group;
	setftime(ino, C_TIME);
	return 0;
}

#undef owner
#undef group

/*******************************************
  chown (path, owner, group)       Function 14        ?
  char *path;
  int  owner;
  int  group;
 ********************************************/
#define path (char *)udata.u_argn

arg_t _chown(void)
{
	inoptr ino;
	int ret;

	if (!(ino = n_open(path, NULLINOPTR)))
		return (-1);
	ret = chown_op(ino);
	i_deref(ino);
	return ret;
}

#undef path

/*******************************************
  fchown (path, owner, group)       Function 50
  int fd;
  int  owner;
  int  group;
 ********************************************/
#define fd (int16_t)udata.u_argn

arg_t _fchown(void)
{
	inoptr ino;

	if ((ino = getinode(fd)) == NULLINODE)
		return (-1);
	return chown_op(ino);
}

#undef fd


/*******************************************
  utime (file, buf)                Function 43
  char *file;
  char *buf;
 ********************************************/
#define file (char *)udata.u_argn
#define buf (char *)udata.u_argn1

arg_t _utime(void)
{
	inoptr ino;
	time_t t[2];

	if (!(ino = n_open(file, NULLINOPTR)))
		return (-1);
	if (ino->c_flags & CRDONLY) {
		udata.u_error = EROFS;
		goto out2;
	}
	/* Special case in the Unix API - NULL means now */
	if (buf) {
	        if (ino->c_node.i_uid != udata.u_euid && esuper())
			goto out;
		if (!valaddr(buf, 2 * sizeof(time_t)))
			goto out2;
		uget(buf, t, 2 * sizeof(time_t));
	} else {
	        if (!(getperm(ino) & OTH_WR))
			goto out;
	        rdtime(&t[0]);
	        memcpy(&t[1], &t[0], sizeof(t[1]));
        }
	/* FIXME: needs updating once we pack top bits
	   elsewhere in the inode */
	ino->c_node.i_atime = t[0].low;
	ino->c_node.i_mtime = t[1].low;
	setftime(ino, C_TIME);
	i_deref(ino);
	return (0);
out:
	udata.u_error = EPERM;
out2:
	i_deref(ino);
	return -1;
}

#undef file
#undef buf

/*******************************************
  acct(fd)	                 Function 61
  int16_t fd;
  
  Process accounting. Differs from SYS5 in
  that we pass an fd and hide the opening
  in the C library code.
 *******************************************/
#define fd (int16_t)udata.u_argn

arg_t _acct(void)
{
#ifdef CONFIG_ACCT
        inoptr inode;
        if (esuper())
                return -1;
        if (acct_fh != -1)
                oft_deref(acct_fh);
        if (fd != -1) {
                if ((inode = getinode(fd)) == NULLINODE)
                        return -1;
                if (getmode(inode) != MODE_R(F_REG)) {
                        udata.u_error = EINVAL;
                        return -1;
                }
		if (inode->c_flags & CRDONLY) {
			udata.u_error = EROFS;
			return -1;
		}
        	acct_fh = udata.u_files[fd];
        	++of_tab[acct_fh].o_refs;
        }
	return 0;
#else
        udata.u_error = EINVAL;
        return -1;
#endif        
}

#undef fd

/* Special system call returns super-block of given filesystem
 * for users to determine free space, etc.  Should be replaced
 * with a sync() followed by a read of block 1 of the device.
 */
/*******************************************
  getfsys (dev, buf)               Function 22
  int16_t  dev;
  struct filesys *buf;
 ********************************************/
#define dev (uint16_t)udata.u_argn
#define buf (struct filesys *)udata.u_argn1

arg_t _getfsys(void)
{
        struct mount *m = fs_tab_get(dev);
	if (m == NULL || m->m_dev == NO_DEVICE) {
		udata.u_error = ENXIO;
		return (-1);
	}
	return uput((char *) m->m_fs, (char *) buf, sizeof(struct filesys));
}

#undef dev
#undef buf
