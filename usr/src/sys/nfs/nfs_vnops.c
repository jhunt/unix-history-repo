/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Rick Macklem at The University of Guelph.
 *
 * %sccs.include.redist.c%
 *
 *	@(#)nfs_vnops.c	7.52 (Berkeley) %G%
 */

/*
 * vnode op calls for sun nfs version 2
 */

#include "machine/mtpr.h"
#include "param.h"
#include "user.h"
#include "proc.h"
#include "kernel.h"
#include "mount.h"
#include "buf.h"
#include "malloc.h"
#include "mbuf.h"
#include "errno.h"
#include "file.h"
#include "conf.h"
#include "vnode.h"
#include "map.h"
#include "../ufs/quota.h"
#include "../ufs/inode.h"
#include "nfsv2.h"
#include "nfs.h"
#include "nfsnode.h"
#include "nfsmount.h"
#include "xdr_subs.h"
#include "nfsm_subs.h"
#include "nfsiom.h"

/* Defs */
#define	TRUE	1
#define	FALSE	0

/* Global vars */
int	nfs_lookup(),
	nfs_create(),
	nfs_mknod(),
	nfs_open(),
	nfs_close(),
	nfs_access(),
	nfs_getattr(),
	nfs_setattr(),
	nfs_read(),
	nfs_write(),
	vfs_noop(),
	vfs_nullop(),
	nfs_remove(),
	nfs_link(),
	nfs_rename(),
	nfs_mkdir(),
	nfs_rmdir(),
	nfs_symlink(),
	nfs_readdir(),
	nfs_readlink(),
	nfs_abortop(),
	nfs_lock(),
	nfs_unlock(),
	nfs_bmap(),
	nfs_strategy(),
	nfs_fsync(),
	nfs_inactive(),
	nfs_reclaim(),
	nfs_print(),
	nfs_islocked(),
	nfs_advlock();

struct vnodeops nfsv2_vnodeops = {
	nfs_lookup,		/* lookup */
	nfs_create,		/* create */
	nfs_mknod,		/* mknod */
	nfs_open,		/* open */
	nfs_close,		/* close */
	nfs_access,		/* access */
	nfs_getattr,		/* getattr */
	nfs_setattr,		/* setattr */
	nfs_read,		/* read */
	nfs_write,		/* write */
	vfs_noop,		/* ioctl */
	vfs_noop,		/* select */
	vfs_noop,		/* mmap */
	nfs_fsync,		/* fsync */
	vfs_nullop,		/* seek */
	nfs_remove,		/* remove */
	nfs_link,		/* link */
	nfs_rename,		/* rename */
	nfs_mkdir,		/* mkdir */
	nfs_rmdir,		/* rmdir */
	nfs_symlink,		/* symlink */
	nfs_readdir,		/* readdir */
	nfs_readlink,		/* readlink */
	nfs_abortop,		/* abortop */
	nfs_inactive,		/* inactive */
	nfs_reclaim,		/* reclaim */
	nfs_lock,		/* lock */
	nfs_unlock,		/* unlock */
	nfs_bmap,		/* bmap */
	nfs_strategy,		/* strategy */
	nfs_print,		/* print */
	nfs_islocked,		/* islocked */
	nfs_advlock,		/* advlock */
};

/* Special device vnode ops */
int	spec_lookup(),
	spec_open(),
	spec_read(),
	spec_write(),
	spec_strategy(),
	spec_bmap(),
	spec_ioctl(),
	spec_select(),
	spec_close(),
	spec_badop(),
	spec_nullop(),
	spec_advlock();

struct vnodeops spec_nfsv2nodeops = {
	spec_lookup,		/* lookup */
	spec_badop,		/* create */
	spec_badop,		/* mknod */
	spec_open,		/* open */
	spec_close,		/* close */
	nfs_access,		/* access */
	nfs_getattr,		/* getattr */
	nfs_setattr,		/* setattr */
	spec_read,		/* read */
	spec_write,		/* write */
	spec_ioctl,		/* ioctl */
	spec_select,		/* select */
	spec_badop,		/* mmap */
	spec_nullop,		/* fsync */
	spec_badop,		/* seek */
	spec_badop,		/* remove */
	spec_badop,		/* link */
	spec_badop,		/* rename */
	spec_badop,		/* mkdir */
	spec_badop,		/* rmdir */
	spec_badop,		/* symlink */
	spec_badop,		/* readdir */
	spec_badop,		/* readlink */
	spec_badop,		/* abortop */
	nfs_inactive,		/* inactive */
	nfs_reclaim,		/* reclaim */
	nfs_lock,		/* lock */
	nfs_unlock,		/* unlock */
	spec_bmap,		/* bmap */
	spec_strategy,		/* strategy */
	nfs_print,		/* print */
	nfs_islocked,		/* islocked */
	spec_advlock,		/* advlock */
};

#ifdef FIFO
int	fifo_lookup(),
	fifo_open(),
	fifo_read(),
	fifo_write(),
	fifo_bmap(),
	fifo_ioctl(),
	fifo_select(),
	fifo_close(),
	fifo_print(),
	fifo_badop(),
	fifo_nullop(),
	fifo_advlock();

struct vnodeops fifo_nfsv2nodeops = {
	fifo_lookup,		/* lookup */
	fifo_badop,		/* create */
	fifo_badop,		/* mknod */
	fifo_open,		/* open */
	fifo_close,		/* close */
	nfs_access,		/* access */
	nfs_getattr,		/* getattr */
	nfs_setattr,		/* setattr */
	fifo_read,		/* read */
	fifo_write,		/* write */
	fifo_ioctl,		/* ioctl */
	fifo_select,		/* select */
	fifo_badop,		/* mmap */
	fifo_nullop,		/* fsync */
	fifo_badop,		/* seek */
	fifo_badop,		/* remove */
	fifo_badop,		/* link */
	fifo_badop,		/* rename */
	fifo_badop,		/* mkdir */
	fifo_badop,		/* rmdir */
	fifo_badop,		/* symlink */
	fifo_badop,		/* readdir */
	fifo_badop,		/* readlink */
	fifo_badop,		/* abortop */
	nfs_inactive,		/* inactive */
	nfs_reclaim,		/* reclaim */
	nfs_lock,		/* lock */
	nfs_unlock,		/* unlock */
	fifo_bmap,		/* bmap */
	fifo_badop,		/* strategy */
	nfs_print,		/* print */
	nfs_islocked,		/* islocked */
	fifo_advlock,		/* advlock */
};
#endif /* FIFO */

extern u_long nfs_procids[NFS_NPROCS];
extern u_long nfs_prog, nfs_vers;
extern char nfsiobuf[MAXPHYS+NBPG];
struct map nfsmap[NFS_MSIZ];
struct buf nfs_bqueue;		/* Queue head for nfsiod's */
int nfs_asyncdaemons = 0;
struct proc *nfs_iodwant[NFS_MAXASYNCDAEMON];
static int nfsmap_want = 0;

/*
 * nfs null call from vfs.
 */
nfs_null(vp, cred)
	struct vnode *vp;
	struct ucred *cred;
{
	caddr_t bpos, dpos;
	u_long xid;
	int error = 0;
	struct mbuf *mreq, *mrep, *md, *mb;
	
	nfsm_reqhead(nfs_procids[NFSPROC_NULL], cred, 0);
	nfsm_request(vp, NFSPROC_NULL, u.u_procp, 0);
	nfsm_reqdone;
	return (error);
}

/*
 * nfs access vnode op.
 * Essentially just get vattr and then imitate iaccess()
 */
nfs_access(vp, mode, cred)
	struct vnode *vp;
	int mode;
	register struct ucred *cred;
{
	register struct vattr *vap;
	register gid_t *gp;
	struct vattr vattr;
	register int i;
	int error;

	/*
	 * If you're the super-user,
	 * you always get access.
	 */
	if (cred->cr_uid == 0)
		return (0);
	vap = &vattr;
	if (error = nfs_dogetattr(vp, vap, cred, 0))
		return (error);
	/*
	 * Access check is based on only one of owner, group, public.
	 * If not owner, then check group. If not a member of the
	 * group, then check public access.
	 */
	if (cred->cr_uid != vap->va_uid) {
		mode >>= 3;
		gp = cred->cr_groups;
		for (i = 0; i < cred->cr_ngroups; i++, gp++)
			if (vap->va_gid == *gp)
				goto found;
		mode >>= 3;
found:
		;
	}
	if ((vap->va_mode & mode) != 0)
		return (0);
	return (EACCES);
}

/*
 * nfs open vnode op
 * Just check to see if the type is ok
 */
/* ARGSUSED */
nfs_open(vp, mode, cred)
	struct vnode *vp;
	int mode;
	struct ucred *cred;
{
	register enum vtype vtyp;

	vtyp = vp->v_type;
	if (vtyp == VREG || vtyp == VDIR || vtyp == VLNK)
		return (0);
	else
		return (EACCES);
}

/*
 * nfs close vnode op
 * For reg files, invalidate any buffer cache entries.
 */
/* ARGSUSED */
nfs_close(vp, fflags, cred)
	register struct vnode *vp;
	int fflags;
	struct ucred *cred;
{
	register struct nfsnode *np = VTONFS(vp);
	int error = 0;

	if (vp->v_type == VREG && (np->n_flag & NMODIFIED)) {
		nfs_lock(vp);
		np->n_flag &= ~NMODIFIED;
		vinvalbuf(vp, TRUE);
		np->n_attrstamp = 0;
		if (np->n_flag & NWRITEERR) {
			np->n_flag &= ~NWRITEERR;
			error = np->n_error;
		}
		nfs_unlock(vp);
	}
	return (error);
}

/*
 * nfs getattr call from vfs.
 */
nfs_getattr(vp, vap, cred)
	register struct vnode *vp;
	struct vattr *vap;
	struct ucred *cred;
{
	return (nfs_dogetattr(vp, vap, cred, 0));
}

nfs_dogetattr(vp, vap, cred, tryhard)
	register struct vnode *vp;
	struct vattr *vap;
	struct ucred *cred;
	int tryhard;
{
	register caddr_t cp;
	register long t1;
	caddr_t bpos, dpos;
	u_long xid;
	int error = 0;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;
	
	/* First look in the cache.. */
	if (nfs_getattrcache(vp, vap) == 0)
		return (0);
	nfsstats.rpccnt[NFSPROC_GETATTR]++;
	nfsm_reqhead(nfs_procids[NFSPROC_GETATTR], cred, NFSX_FH);
	nfsm_fhtom(vp);
	nfsm_request(vp, NFSPROC_GETATTR, u.u_procp, tryhard);
	nfsm_loadattr(vp, vap);
	nfsm_reqdone;
	return (error);
}

/*
 * nfs setattr call.
 */
nfs_setattr(vp, vap, cred)
	register struct vnode *vp;
	register struct vattr *vap;
	struct ucred *cred;
{
	register struct nfsv2_sattr *sp;
	register caddr_t cp;
	register long t1;
	caddr_t bpos, dpos;
	u_long xid;
	int error = 0;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;
	struct nfsnode *np;

	nfsstats.rpccnt[NFSPROC_SETATTR]++;
	nfsm_reqhead(nfs_procids[NFSPROC_SETATTR], cred, NFSX_FH+NFSX_SATTR);
	nfsm_fhtom(vp);
	nfsm_build(sp, struct nfsv2_sattr *, NFSX_SATTR);
	if (vap->va_mode == 0xffff)
		sp->sa_mode = VNOVAL;
	else
		sp->sa_mode = vtonfs_mode(vp->v_type, vap->va_mode);
	if (vap->va_uid == 0xffff)
		sp->sa_uid = VNOVAL;
	else
		sp->sa_uid = txdr_unsigned(vap->va_uid);
	if (vap->va_gid == 0xffff)
		sp->sa_gid = VNOVAL;
	else
		sp->sa_gid = txdr_unsigned(vap->va_gid);
	sp->sa_size = txdr_unsigned(vap->va_size);
	sp->sa_atime.tv_sec = txdr_unsigned(vap->va_atime.tv_sec);
	sp->sa_atime.tv_usec = txdr_unsigned(vap->va_flags);
	txdr_time(&vap->va_mtime, &sp->sa_mtime);
	if (vap->va_size != VNOVAL || vap->va_mtime.tv_sec != VNOVAL ||
	    vap->va_atime.tv_sec != VNOVAL) {
		np = VTONFS(vp);
		if (np->n_flag & NMODIFIED) {
			np->n_flag &= ~NMODIFIED;
			vinvalbuf(vp, TRUE);
			np->n_attrstamp = 0;
		}
	}
	nfsm_request(vp, NFSPROC_SETATTR, u.u_procp, 1);
	nfsm_loadattr(vp, (struct vattr *)0);
	/* should we fill in any vap fields ?? */
	nfsm_reqdone;
	return (error);
}

/*
 * nfs lookup call, one step at a time...
 * First look in cache
 * If not found, unlock the directory nfsnode and do the rpc
 */
nfs_lookup(vp, ndp)
	register struct vnode *vp;
	register struct nameidata *ndp;
{
	register struct vnode *vdp;
	register u_long *p;
	register caddr_t cp;
	register long t1, t2;
	caddr_t bpos, dpos, cp2;
	u_long xid;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;
	struct vnode *newvp;
	long len;
	nfsv2fh_t *fhp;
	struct nfsnode *np;
	int lockparent, wantparent, flag, error = 0;

	ndp->ni_dvp = vp;
	ndp->ni_vp = NULL;
	if (vp->v_type != VDIR)
		return (ENOTDIR);
	lockparent = ndp->ni_nameiop & LOCKPARENT;
	flag = ndp->ni_nameiop & OPMASK;
	wantparent = ndp->ni_nameiop & (LOCKPARENT|WANTPARENT);
	if ((error = cache_lookup(ndp)) && error != ENOENT) {
		struct vattr vattr;
		int vpid;

#ifdef PARANOID
		if (vp == ndp->ni_rdir && ndp->ni_isdotdot)
			panic("nfs_lookup: .. through root");
#endif
		vdp = ndp->ni_vp;
		vpid = vdp->v_id;
		/*
		 * See the comment starting `Step through' in ufs/ufs_lookup.c
		 * for an explanation of the locking protocol
		 */
		if (vp == vdp) {
			VREF(vdp);
			error = 0;
		} else if (ndp->ni_isdotdot) {
			nfs_unlock(vp);
			error = vget(vdp);
		} else {
			error = vget(vdp);
			nfs_unlock(vp);
		}
		if (!error) {
			if (vpid == vdp->v_id) {
			   if (!nfs_dogetattr(vdp, &vattr, ndp->ni_cred, 0) &&
			       vattr.va_ctime.tv_sec == VTONFS(vdp)->n_ctime) {
				nfsstats.lookupcache_hits++;
				return (0);
			   } else {
				cache_purge(vdp);
				nfs_nput(vdp);
			   }
			} else {
				nfs_nput(vdp);
			}
		}
		ndp->ni_vp = NULLVP;
	} else
		nfs_unlock(vp);
	error = 0;
	nfsstats.lookupcache_misses++;
	nfsstats.rpccnt[NFSPROC_LOOKUP]++;
	len = ndp->ni_namelen;
	nfsm_reqhead(nfs_procids[NFSPROC_LOOKUP], ndp->ni_cred, NFSX_FH+NFSX_UNSIGNED+nfsm_rndup(len));
	nfsm_fhtom(vp);
	nfsm_strtom(ndp->ni_ptr, len, NFS_MAXNAMLEN);
	nfsm_request(vp, NFSPROC_LOOKUP, u.u_procp, 0);
nfsmout:
	if (error) {
		if (lockparent || (flag != CREATE && flag != RENAME) ||
		    *ndp->ni_next != 0)
			nfs_lock(vp);
		return (error);
	}
	nfsm_disect(fhp,nfsv2fh_t *,NFSX_FH);

	/*
	 * Handle DELETE and RENAME cases...
	 */
	if (flag == DELETE && *ndp->ni_next == 0) {
		if (!bcmp(VTONFS(vp)->n_fh.fh_bytes, (caddr_t)fhp, NFSX_FH)) {
			VREF(vp);
			newvp = vp;
			np = VTONFS(vp);
		} else {
			if (error = nfs_nget(vp->v_mount, fhp, &np)) {
				nfs_lock(vp);
				m_freem(mrep);
				return (error);
			}
			newvp = NFSTOV(np);
		}
		if (error =
		    nfs_loadattrcache(&newvp, &md, &dpos, (struct vattr *)0)) {
			nfs_lock(vp);
			if (newvp != vp)
				nfs_nput(newvp);
			else
				vrele(vp);
			m_freem(mrep);
			return (error);
		}
		ndp->ni_vp = newvp;
		if (lockparent || vp == newvp)
			nfs_lock(vp);
		m_freem(mrep);
		return (0);
	}

	if (flag == RENAME && wantparent && *ndp->ni_next == 0) {
		if (!bcmp(VTONFS(vp)->n_fh.fh_bytes, (caddr_t)fhp, NFSX_FH)) {
			nfs_lock(vp);
			m_freem(mrep);
			return (EISDIR);
		}
		if (error = nfs_nget(vp->v_mount, fhp, &np)) {
			nfs_lock(vp);
			m_freem(mrep);
			return (error);
		}
		newvp = NFSTOV(np);
		if (error =
		    nfs_loadattrcache(&newvp, &md, &dpos, (struct vattr *)0)) {
			nfs_lock(vp);
			nfs_nput(newvp);
			m_freem(mrep);
			return (error);
		}
		ndp->ni_vp = newvp;
		if (lockparent)
			nfs_lock(vp);
		m_freem(mrep);
		return (0);
	}

	if (!bcmp(VTONFS(vp)->n_fh.fh_bytes, (caddr_t)fhp, NFSX_FH)) {
		VREF(vp);
		newvp = vp;
		np = VTONFS(vp);
	} else if (ndp->ni_isdotdot) {
		if (error = nfs_nget(vp->v_mount, fhp, &np)) {
			nfs_lock(vp);
			m_freem(mrep);
			return (error);
		}
		newvp = NFSTOV(np);
	} else {
		if (error = nfs_nget(vp->v_mount, fhp, &np)) {
			nfs_lock(vp);
			m_freem(mrep);
			return (error);
		}
		newvp = NFSTOV(np);
	}
	if (error = nfs_loadattrcache(&newvp, &md, &dpos, (struct vattr *)0)) {
		nfs_lock(vp);
		if (newvp != vp)
			nfs_nput(newvp);
		else
			vrele(vp);
		m_freem(mrep);
		return (error);
	}
	m_freem(mrep);

	if (vp == newvp || (lockparent && *ndp->ni_next == '\0'))
		nfs_lock(vp);
	ndp->ni_vp = newvp;
	if (error == 0 && ndp->ni_makeentry) {
		np->n_ctime = np->n_vattr.va_ctime.tv_sec;
		cache_enter(ndp);
	}
	return (error);
}

/*
 * nfs read call.
 * Just call nfs_bioread() to do the work.
 */
nfs_read(vp, uiop, ioflag, cred)
	register struct vnode *vp;
	struct uio *uiop;
	int ioflag;
	struct ucred *cred;
{
	if (vp->v_type != VREG)
		return (EPERM);
	return (nfs_bioread(vp, uiop, ioflag, cred));
}

/*
 * nfs readlink call
 */
nfs_readlink(vp, uiop, cred)
	struct vnode *vp;
	struct uio *uiop;
	struct ucred *cred;
{
	if (vp->v_type != VLNK)
		return (EPERM);
	return (nfs_bioread(vp, uiop, 0, cred));
}

/*
 * Do a readlink rpc.
 * Called by nfs_doio() from below the buffer cache.
 */
nfs_readlinkrpc(vp, uiop, cred, procp)
	register struct vnode *vp;
	struct uio *uiop;
	struct ucred *cred;
	struct proc *procp;
{
	register u_long *p;
	register caddr_t cp;
	register long t1;
	caddr_t bpos, dpos, cp2;
	u_long xid;
	int error = 0;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;
	long len;

	nfsstats.rpccnt[NFSPROC_READLINK]++;
	nfs_unlock(vp);
	nfsm_reqhead(nfs_procids[NFSPROC_READLINK], cred, NFSX_FH);
	nfsm_fhtom(vp);
	nfsm_request(vp, NFSPROC_READLINK, procp, 0);
	nfsm_strsiz(len, NFS_MAXPATHLEN);
	nfsm_mtouio(uiop, len);
	nfsm_reqdone;
	nfs_lock(vp);
	return (error);
}

/*
 * nfs read rpc call
 * Ditto above
 */
nfs_readrpc(vp, uiop, cred, procp)
	register struct vnode *vp;
	struct uio *uiop;
	struct ucred *cred;
	struct proc *procp;
{
	register u_long *p;
	register caddr_t cp;
	register long t1;
	caddr_t bpos, dpos, cp2;
	u_long xid;
	int error = 0;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;
	struct nfsmount *nmp;
	long len, retlen, tsiz;

	nmp = VFSTONFS(vp->v_mount);
	tsiz = uiop->uio_resid;
	while (tsiz > 0) {
		nfsstats.rpccnt[NFSPROC_READ]++;
		len = (tsiz > nmp->nm_rsize) ? nmp->nm_rsize : tsiz;
		nfsm_reqhead(nfs_procids[NFSPROC_READ], cred, NFSX_FH+NFSX_UNSIGNED*3);
		nfsm_fhtom(vp);
		nfsm_build(p, u_long *, NFSX_UNSIGNED*3);
		*p++ = txdr_unsigned(uiop->uio_offset);
		*p++ = txdr_unsigned(len);
		*p = 0;
		nfsm_request(vp, NFSPROC_READ, procp, 1);
		nfsm_loadattr(vp, (struct vattr *)0);
		nfsm_strsiz(retlen, nmp->nm_rsize);
		nfsm_mtouio(uiop, retlen);
		m_freem(mrep);
		if (retlen < len)
			tsiz = 0;
		else
			tsiz -= len;
	}
nfsmout:
	return (error);
}

/*
 * nfs write call
 */
nfs_writerpc(vp, uiop, cred, procp)
	register struct vnode *vp;
	struct uio *uiop;
	struct ucred *cred;
	struct proc *procp;
{
	register u_long *p;
	register caddr_t cp;
	register long t1;
	caddr_t bpos, dpos;
	u_long xid;
	int error = 0;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;
	struct nfsmount *nmp;
	long len, tsiz;

	nmp = VFSTONFS(vp->v_mount);
	tsiz = uiop->uio_resid;
	while (tsiz > 0) {
		nfsstats.rpccnt[NFSPROC_WRITE]++;
		len = (tsiz > nmp->nm_wsize) ? nmp->nm_wsize : tsiz;
		nfsm_reqhead(nfs_procids[NFSPROC_WRITE], cred,
			NFSX_FH+NFSX_UNSIGNED*4);
		nfsm_fhtom(vp);
		nfsm_build(p, u_long *, NFSX_UNSIGNED*4);
		*(p+1) = txdr_unsigned(uiop->uio_offset);
		*(p+3) = txdr_unsigned(len);
		nfsm_uiotom(uiop, len);
		nfsm_request(vp, NFSPROC_WRITE, procp, 1);
		nfsm_loadattr(vp, (struct vattr *)0);
		m_freem(mrep);
		tsiz -= len;
	}
nfsmout:
	return (error);
}

/*
 * nfs mknod call
 * This is a kludge. Use a create rpc but with the IFMT bits of the mode
 * set to specify the file type and the size field for rdev.
 */
/* ARGSUSED */
nfs_mknod(ndp, vap, cred)
	struct nameidata *ndp;
	struct ucred *cred;
	register struct vattr *vap;
{
	register struct nfsv2_sattr *sp;
	register u_long *p;
	register caddr_t cp;
	register long t1, t2;
	caddr_t bpos, dpos;
	u_long xid;
	int error = 0;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;
	u_long rdev;

	if (vap->va_type == VCHR || vap->va_type == VBLK)
		rdev = txdr_unsigned(vap->va_rdev);
#ifdef FIFO
	else if (vap->va_type == VFIFO)
		rdev = 0xffffffff;
#endif /* FIFO */
	else {
		VOP_ABORTOP(ndp);
		vput(ndp->ni_dvp);
		return (EOPNOTSUPP);
	}
	nfsstats.rpccnt[NFSPROC_CREATE]++;
	nfsm_reqhead(nfs_procids[NFSPROC_CREATE], ndp->ni_cred,
	  NFSX_FH+NFSX_UNSIGNED+nfsm_rndup(ndp->ni_dent.d_namlen)+NFSX_SATTR);
	nfsm_fhtom(ndp->ni_dvp);
	nfsm_strtom(ndp->ni_dent.d_name, ndp->ni_dent.d_namlen, NFS_MAXNAMLEN);
	nfsm_build(sp, struct nfsv2_sattr *, NFSX_SATTR);
	sp->sa_mode = vtonfs_mode(vap->va_type, vap->va_mode);
	sp->sa_uid = txdr_unsigned(ndp->ni_cred->cr_uid);
	sp->sa_gid = txdr_unsigned(ndp->ni_cred->cr_gid);
	sp->sa_size = rdev;
	/* or should these be VNOVAL ?? */
	txdr_time(&vap->va_atime, &sp->sa_atime);
	txdr_time(&vap->va_mtime, &sp->sa_mtime);
	nfsm_request(ndp->ni_dvp, NFSPROC_CREATE, u.u_procp, 1);
	nfsm_reqdone;
	VTONFS(ndp->ni_dvp)->n_flag |= NMODIFIED;
	nfs_nput(ndp->ni_dvp);
	return (error);
}

/*
 * nfs file create call
 */
nfs_create(ndp, vap)
	register struct nameidata *ndp;
	register struct vattr *vap;
{
	register struct nfsv2_sattr *sp;
	register u_long *p;
	register caddr_t cp;
	register long t1, t2;
	caddr_t bpos, dpos, cp2;
	u_long xid;
	int error = 0;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;

	nfsstats.rpccnt[NFSPROC_CREATE]++;
	nfsm_reqhead(nfs_procids[NFSPROC_CREATE], ndp->ni_cred,
	  NFSX_FH+NFSX_UNSIGNED+nfsm_rndup(ndp->ni_dent.d_namlen)+NFSX_SATTR);
	nfsm_fhtom(ndp->ni_dvp);
	nfsm_strtom(ndp->ni_dent.d_name, ndp->ni_dent.d_namlen, NFS_MAXNAMLEN);
	nfsm_build(sp, struct nfsv2_sattr *, NFSX_SATTR);
	sp->sa_mode = vtonfs_mode(VREG, vap->va_mode);
	sp->sa_uid = txdr_unsigned(ndp->ni_cred->cr_uid);
	sp->sa_gid = txdr_unsigned(ndp->ni_cred->cr_gid);
	sp->sa_size = txdr_unsigned(0);
	/* or should these be VNOVAL ?? */
	txdr_time(&vap->va_atime, &sp->sa_atime);
	txdr_time(&vap->va_mtime, &sp->sa_mtime);
	nfsm_request(ndp->ni_dvp, NFSPROC_CREATE, u.u_procp, 1);
	nfsm_mtofh(ndp->ni_dvp, ndp->ni_vp);
	nfsm_reqdone;
	VTONFS(ndp->ni_dvp)->n_flag |= NMODIFIED;
	nfs_nput(ndp->ni_dvp);
	return (error);
}

/*
 * nfs file remove call
 * To try and make nfs semantics closer to ufs semantics, a file that has
 * other processes using the vnode is renamed instead of removed and then
 * removed later on the last close.
 * - If v_usecount > 1
 *	  If a rename is not already in the works
 *	     call nfs_sillyrename() to set it up
 *     else
 *	  do the remove rpc
 */
nfs_remove(ndp)
	register struct nameidata *ndp;
{
	register struct vnode *vp = ndp->ni_vp;
	register struct nfsnode *np = VTONFS(ndp->ni_vp);
	register u_long *p;
	register caddr_t cp;
	register long t1, t2;
	caddr_t bpos, dpos;
	u_long xid;
	int error = 0;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;

	if (vp->v_usecount > 1) {
		if (!np->n_sillyrename)
			error = nfs_sillyrename(ndp, REMOVE);
	} else {
		nfsstats.rpccnt[NFSPROC_REMOVE]++;
		nfsm_reqhead(nfs_procids[NFSPROC_REMOVE], ndp->ni_cred,
			NFSX_FH+NFSX_UNSIGNED+nfsm_rndup(ndp->ni_dent.d_namlen));
		nfsm_fhtom(ndp->ni_dvp);
		nfsm_strtom(ndp->ni_dent.d_name, ndp->ni_dent.d_namlen, NFS_MAXNAMLEN);
		nfsm_request(ndp->ni_dvp, NFSPROC_REMOVE, u.u_procp, 1);
		nfsm_reqdone;
		VTONFS(ndp->ni_dvp)->n_flag |= NMODIFIED;
		/*
		 * Kludge City: If the first reply to the remove rpc is lost..
		 *   the reply to the retransmitted request will be ENOENT
		 *   since the file was in fact removed
		 *   Therefore, we cheat and return success.
		 */
		if (error == ENOENT)
			error = 0;
	}
	np->n_attrstamp = 0;
	if (ndp->ni_dvp == vp)
		vrele(vp);
	else
		nfs_nput(ndp->ni_dvp);
	nfs_nput(vp);
	return (error);
}

/*
 * nfs file remove rpc called from nfs_inactive
 */
nfs_removeit(ndp)
	register struct nameidata *ndp;
{
	register u_long *p;
	register caddr_t cp;
	register long t1, t2;
	caddr_t bpos, dpos;
	u_long xid;
	int error = 0;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;

	nfsstats.rpccnt[NFSPROC_REMOVE]++;
	nfsm_reqhead(nfs_procids[NFSPROC_REMOVE], ndp->ni_cred,
		NFSX_FH+NFSX_UNSIGNED+nfsm_rndup(ndp->ni_dent.d_namlen));
	nfsm_fhtom(ndp->ni_dvp);
	nfsm_strtom(ndp->ni_dent.d_name, ndp->ni_dent.d_namlen, NFS_MAXNAMLEN);
	nfsm_request(ndp->ni_dvp, NFSPROC_REMOVE, u.u_procp, 1);
	nfsm_reqdone;
	VTONFS(ndp->ni_dvp)->n_flag |= NMODIFIED;
	return (error);
}

/*
 * nfs file rename call
 */
nfs_rename(sndp, tndp)
	register struct nameidata *sndp, *tndp;
{
	register u_long *p;
	register caddr_t cp;
	register long t1, t2;
	caddr_t bpos, dpos;
	u_long xid;
	int error = 0;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;

	nfsstats.rpccnt[NFSPROC_RENAME]++;
	nfsm_reqhead(nfs_procids[NFSPROC_RENAME], tndp->ni_cred,
		(NFSX_FH+NFSX_UNSIGNED)*2+nfsm_rndup(sndp->ni_dent.d_namlen)+
		nfsm_rndup(tndp->ni_dent.d_namlen)); /* or sndp->ni_cred?*/
	nfsm_fhtom(sndp->ni_dvp);
	nfsm_strtom(sndp->ni_dent.d_name,sndp->ni_dent.d_namlen,NFS_MAXNAMLEN);
	nfsm_fhtom(tndp->ni_dvp);
	nfsm_strtom(tndp->ni_dent.d_name,tndp->ni_dent.d_namlen,NFS_MAXNAMLEN);
	nfsm_request(sndp->ni_dvp, NFSPROC_RENAME, u.u_procp, 1);
	nfsm_reqdone;
	VTONFS(sndp->ni_dvp)->n_flag |= NMODIFIED;
	VTONFS(tndp->ni_dvp)->n_flag |= NMODIFIED;
	if (sndp->ni_vp->v_type == VDIR) {
		if (tndp->ni_vp != NULL && tndp->ni_vp->v_type == VDIR)
			cache_purge(tndp->ni_dvp);
		cache_purge(sndp->ni_dvp);
	}
	VOP_ABORTOP(tndp);
	if (tndp->ni_dvp == tndp->ni_vp)
		vrele(tndp->ni_dvp);
	else
		vput(tndp->ni_dvp);
	if (tndp->ni_vp)
		vput(tndp->ni_vp);
	VOP_ABORTOP(sndp);
	vrele(sndp->ni_dvp);
	vrele(sndp->ni_vp);
	/*
	 * Kludge: Map ENOENT => 0 assuming that it is a reply to a retry.
	 */
	if (error == ENOENT)
		error = 0;
	return (error);
}

/*
 * nfs file rename rpc called from nfs_remove() above
 */
nfs_renameit(sndp, tndp)
	register struct nameidata *sndp, *tndp;
{
	register u_long *p;
	register caddr_t cp;
	register long t1, t2;
	caddr_t bpos, dpos;
	u_long xid;
	int error = 0;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;

	nfsstats.rpccnt[NFSPROC_RENAME]++;
	nfsm_reqhead(nfs_procids[NFSPROC_RENAME], tndp->ni_cred,
		(NFSX_FH+NFSX_UNSIGNED)*2+nfsm_rndup(sndp->ni_dent.d_namlen)+
		nfsm_rndup(tndp->ni_dent.d_namlen)); /* or sndp->ni_cred?*/
	nfsm_fhtom(sndp->ni_dvp);
	nfsm_strtom(sndp->ni_dent.d_name,sndp->ni_dent.d_namlen,NFS_MAXNAMLEN);
	nfsm_fhtom(tndp->ni_dvp);
	nfsm_strtom(tndp->ni_dent.d_name,tndp->ni_dent.d_namlen,NFS_MAXNAMLEN);
	nfsm_request(sndp->ni_dvp, NFSPROC_RENAME, u.u_procp, 1);
	nfsm_reqdone;
	VTONFS(sndp->ni_dvp)->n_flag |= NMODIFIED;
	VTONFS(tndp->ni_dvp)->n_flag |= NMODIFIED;
	return (error);
}

/*
 * nfs hard link create call
 */
nfs_link(vp, ndp)
	register struct vnode *vp;
	register struct nameidata *ndp;
{
	register u_long *p;
	register caddr_t cp;
	register long t1, t2;
	caddr_t bpos, dpos;
	u_long xid;
	int error = 0;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;

	if (ndp->ni_dvp != vp)
		nfs_lock(vp);
	nfsstats.rpccnt[NFSPROC_LINK]++;
	nfsm_reqhead(nfs_procids[NFSPROC_LINK], ndp->ni_cred,
		NFSX_FH*2+NFSX_UNSIGNED+nfsm_rndup(ndp->ni_dent.d_namlen));
	nfsm_fhtom(vp);
	nfsm_fhtom(ndp->ni_dvp);
	nfsm_strtom(ndp->ni_dent.d_name, ndp->ni_dent.d_namlen, NFS_MAXNAMLEN);
	nfsm_request(vp, NFSPROC_LINK, u.u_procp, 1);
	nfsm_reqdone;
	VTONFS(vp)->n_attrstamp = 0;
	VTONFS(ndp->ni_dvp)->n_flag |= NMODIFIED;
	if (ndp->ni_dvp != vp)
		nfs_unlock(vp);
	nfs_nput(ndp->ni_dvp);
	/*
	 * Kludge: Map EEXIST => 0 assuming that it is a reply to a retry.
	 */
	if (error == EEXIST)
		error = 0;
	return (error);
}

/*
 * nfs symbolic link create call
 */
nfs_symlink(ndp, vap, nm)
	struct nameidata *ndp;
	struct vattr *vap;
	char *nm;		/* is this the path ?? */
{
	register struct nfsv2_sattr *sp;
	register u_long *p;
	register caddr_t cp;
	register long t1, t2;
	caddr_t bpos, dpos;
	u_long xid;
	int error = 0;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;

	nfsstats.rpccnt[NFSPROC_SYMLINK]++;
	nfsm_reqhead(nfs_procids[NFSPROC_SYMLINK], ndp->ni_cred,
	NFSX_FH+NFSX_UNSIGNED+nfsm_rndup(ndp->ni_dent.d_namlen)+NFSX_UNSIGNED);
	nfsm_fhtom(ndp->ni_dvp);
	nfsm_strtom(ndp->ni_dent.d_name, ndp->ni_dent.d_namlen, NFS_MAXNAMLEN);
	nfsm_strtom(nm, strlen(nm), NFS_MAXPATHLEN);
	nfsm_build(sp, struct nfsv2_sattr *, NFSX_SATTR);
	sp->sa_mode = vtonfs_mode(VLNK, vap->va_mode);
	sp->sa_uid = txdr_unsigned(ndp->ni_cred->cr_uid);
	sp->sa_gid = txdr_unsigned(ndp->ni_cred->cr_gid);
	sp->sa_size = txdr_unsigned(VNOVAL);
	txdr_time(&vap->va_atime, &sp->sa_atime);	/* or VNOVAL ?? */
	txdr_time(&vap->va_mtime, &sp->sa_mtime);	/* or VNOVAL ?? */
	nfsm_request(ndp->ni_dvp, NFSPROC_SYMLINK, u.u_procp, 1);
	nfsm_reqdone;
	VTONFS(ndp->ni_dvp)->n_flag |= NMODIFIED;
	nfs_nput(ndp->ni_dvp);
	/*
	 * Kludge: Map EEXIST => 0 assuming that it is a reply to a retry.
	 */
	if (error == EEXIST)
		error = 0;
	return (error);
}

/*
 * nfs make dir call
 */
nfs_mkdir(ndp, vap)
	register struct nameidata *ndp;
	struct vattr *vap;
{
	register struct nfsv2_sattr *sp;
	register u_long *p;
	register caddr_t cp;
	register long t1, t2;
	register int len;
	caddr_t bpos, dpos, cp2;
	u_long xid;
	int error = 0, firsttry = 1;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;

	len = ndp->ni_dent.d_namlen;
	nfsstats.rpccnt[NFSPROC_MKDIR]++;
	nfsm_reqhead(nfs_procids[NFSPROC_MKDIR], ndp->ni_cred,
	  NFSX_FH+NFSX_UNSIGNED+nfsm_rndup(len)+NFSX_SATTR);
	nfsm_fhtom(ndp->ni_dvp);
	nfsm_strtom(ndp->ni_dent.d_name, len, NFS_MAXNAMLEN);
	nfsm_build(sp, struct nfsv2_sattr *, NFSX_SATTR);
	sp->sa_mode = vtonfs_mode(VDIR, vap->va_mode);
	sp->sa_uid = txdr_unsigned(ndp->ni_cred->cr_uid);
	sp->sa_gid = txdr_unsigned(ndp->ni_cred->cr_gid);
	sp->sa_size = txdr_unsigned(VNOVAL);
	txdr_time(&vap->va_atime, &sp->sa_atime);	/* or VNOVAL ?? */
	txdr_time(&vap->va_mtime, &sp->sa_mtime);	/* or VNOVAL ?? */
	nfsm_request(ndp->ni_dvp, NFSPROC_MKDIR, u.u_procp, 1);
	nfsm_mtofh(ndp->ni_dvp, ndp->ni_vp);
	nfsm_reqdone;
	VTONFS(ndp->ni_dvp)->n_flag |= NMODIFIED;
	/*
	 * Kludge: Map EEXIST => 0 assuming that you have a reply to a retry
	 * if we can succeed in looking up the directory.
	 * "firsttry" is necessary since the macros may "goto nfsmout" which
	 * is above the if on errors. (Ugh)
	 */
	if (error == EEXIST && firsttry) {
		firsttry = 0;
		error = 0;
		nfsstats.rpccnt[NFSPROC_LOOKUP]++;
		ndp->ni_vp = NULL;
		nfsm_reqhead(nfs_procids[NFSPROC_LOOKUP], ndp->ni_cred,
		    NFSX_FH+NFSX_UNSIGNED+nfsm_rndup(len));
		nfsm_fhtom(ndp->ni_dvp);
		nfsm_strtom(ndp->ni_dent.d_name, len, NFS_MAXNAMLEN);
		nfsm_request(ndp->ni_dvp, NFSPROC_LOOKUP, u.u_procp, 1);
		nfsm_mtofh(ndp->ni_dvp, ndp->ni_vp);
		if (ndp->ni_vp->v_type != VDIR) {
			vput(ndp->ni_vp);
			error = EEXIST;
		}
		m_freem(mrep);
	}
	nfs_nput(ndp->ni_dvp);
	return (error);
}

/*
 * nfs remove directory call
 */
nfs_rmdir(ndp)
	register struct nameidata *ndp;
{
	register u_long *p;
	register caddr_t cp;
	register long t1, t2;
	caddr_t bpos, dpos;
	u_long xid;
	int error = 0;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;

	if (ndp->ni_dvp == ndp->ni_vp) {
		vrele(ndp->ni_dvp);
		nfs_nput(ndp->ni_dvp);
		return (EINVAL);
	}
	nfsstats.rpccnt[NFSPROC_RMDIR]++;
	nfsm_reqhead(nfs_procids[NFSPROC_RMDIR], ndp->ni_cred,
		NFSX_FH+NFSX_UNSIGNED+nfsm_rndup(ndp->ni_dent.d_namlen));
	nfsm_fhtom(ndp->ni_dvp);
	nfsm_strtom(ndp->ni_dent.d_name, ndp->ni_dent.d_namlen, NFS_MAXNAMLEN);
	nfsm_request(ndp->ni_dvp, NFSPROC_RMDIR, u.u_procp, 1);
	nfsm_reqdone;
	VTONFS(ndp->ni_dvp)->n_flag |= NMODIFIED;
	cache_purge(ndp->ni_dvp);
	cache_purge(ndp->ni_vp);
	nfs_nput(ndp->ni_vp);
	nfs_nput(ndp->ni_dvp);
	/*
	 * Kludge: Map ENOENT => 0 assuming that you have a reply to a retry.
	 */
	if (error == ENOENT)
		error = 0;
	return (error);
}

/*
 * nfs readdir call
 * Although cookie is defined as opaque, I translate it to/from net byte
 * order so that it looks more sensible. This appears consistent with the
 * Ultrix implementation of NFS.
 */
nfs_readdir(vp, uiop, cred, eofflagp)
	register struct vnode *vp;
	struct uio *uiop;
	struct ucred *cred;
	int *eofflagp;
{
	register struct nfsnode *np = VTONFS(vp);
	int tresid, error;
	struct vattr vattr;

	if (vp->v_type != VDIR)
		return (EPERM);
	/*
	 * First, check for hit on the EOF offset cache
	 */
	if (uiop->uio_offset != 0 && uiop->uio_offset == np->n_direofoffset &&
	    (np->n_flag & NMODIFIED) == 0 &&
	    nfs_dogetattr(vp, &vattr, cred, 0) == 0 &&
	    np->n_mtime == vattr.va_mtime.tv_sec) {
		*eofflagp = 1;
		nfsstats.direofcache_hits++;
		return (0);
	}

	/*
	 * Call nfs_bioread() to do the real work.
	 */
	tresid = uiop->uio_resid;
	error = nfs_bioread(vp, uiop, 0, cred);

	if (!error && uiop->uio_resid == tresid) {
		*eofflagp = 1;
		nfsstats.direofcache_misses++;
	} else
		*eofflagp = 0;
	return (error);
}

/*
 * Readdir rpc call.
 * Called from below the buffer cache by nfs_doio().
 */
nfs_readdirrpc(vp, uiop, cred, procp)
	register struct vnode *vp;
	struct uio *uiop;
	struct ucred *cred;
	struct proc *procp;
{
	register long len;
	register struct direct *dp;
	register u_long *p;
	register caddr_t cp;
	register long t1;
	long tlen, lastlen;
	caddr_t bpos, dpos, cp2;
	u_long xid;
	int error = 0;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;
	struct mbuf *md2;
	caddr_t dpos2;
	int siz;
	int more_dirs = 1;
	off_t off, savoff;
	struct direct *savdp;
	struct nfsmount *nmp;
	struct nfsnode *np = VTONFS(vp);
	long tresid;

	nmp = VFSTONFS(vp->v_mount);
	tresid = uiop->uio_resid;
	/*
	 * Loop around doing readdir rpc's of size uio_resid or nm_rsize,
	 * whichever is smaller, truncated to a multiple of DIRBLKSIZ.
	 * The stopping criteria is EOF or buffer full.
	 */
	while (more_dirs && uiop->uio_resid >= DIRBLKSIZ) {
		nfsstats.rpccnt[NFSPROC_READDIR]++;
		nfsm_reqhead(nfs_procids[NFSPROC_READDIR], cred, xid);
		nfsm_fhtom(vp);
		nfsm_build(p, u_long *, 2*NFSX_UNSIGNED);
		*p++ = txdr_unsigned(uiop->uio_offset);
		*p = txdr_unsigned(((uiop->uio_resid > nmp->nm_rsize) ?
			nmp->nm_rsize : uiop->uio_resid) & ~(DIRBLKSIZ-1));
		nfsm_request(vp, NFSPROC_READDIR, procp, 0);
		siz = 0;
		nfsm_disect(p, u_long *, NFSX_UNSIGNED);
		more_dirs = fxdr_unsigned(int, *p);
	
		/* Save the position so that we can do nfsm_mtouio() later */
		dpos2 = dpos;
		md2 = md;
	
		/* loop thru the dir entries, doctoring them to 4bsd form */
		off = uiop->uio_offset;
#ifdef lint
		dp = (struct direct *)0;
#endif /* lint */
		while (more_dirs && siz < uiop->uio_resid) {
			savoff = off;		/* Hold onto offset and dp */
			savdp = dp;
			nfsm_disecton(p, u_long *, 2*NFSX_UNSIGNED);
			dp = (struct direct *)p;
			dp->d_ino = fxdr_unsigned(u_long, *p++);
			len = fxdr_unsigned(int, *p);
			if (len <= 0 || len > NFS_MAXNAMLEN) {
				error = EBADRPC;
				m_freem(mrep);
				goto nfsmout;
			}
			dp->d_namlen = (u_short)len;
			nfsm_adv(len);		/* Point past name */
			tlen = nfsm_rndup(len);
			/*
			 * This should not be necessary, but some servers have
			 * broken XDR such that these bytes are not null filled.
			 */
			if (tlen != len) {
				*dpos = '\0';	/* Null-terminate */
				nfsm_adv(tlen - len);
				len = tlen;
			}
			nfsm_disecton(p, u_long *, 2*NFSX_UNSIGNED);
			off = fxdr_unsigned(off_t, *p);
			*p++ = 0;	/* Ensures null termination of name */
			more_dirs = fxdr_unsigned(int, *p);
			dp->d_reclen = len+4*NFSX_UNSIGNED;
			siz += dp->d_reclen;
		}
		/*
		 * If at end of rpc data, get the eof boolean
		 */
		if (!more_dirs) {
			nfsm_disecton(p, u_long *, NFSX_UNSIGNED);
			more_dirs = (fxdr_unsigned(int, *p) == 0);

			/*
			 * If at EOF, cache directory offset
			 */
			if (!more_dirs)
				np->n_direofoffset = off;
		}
		/*
		 * If there is too much to fit in the data buffer, use savoff and
		 * savdp to trim off the last record.
		 * --> we are not at eof
		 */
		if (siz > uiop->uio_resid) {
			off = savoff;
			siz -= dp->d_reclen;
			dp = savdp;
			more_dirs = 0;	/* Paranoia */
		}
		if (siz > 0) {
			lastlen = dp->d_reclen;
			md = md2;
			dpos = dpos2;
			nfsm_mtouio(uiop, siz);
			uiop->uio_offset = off;
		} else
			more_dirs = 0;	/* Ugh, never happens, but in case.. */
		m_freem(mrep);
	}
	/*
	 * Fill last record, iff any, out to a multiple of DIRBLKSIZ
	 * by increasing d_reclen for the last record.
	 */
	if (uiop->uio_resid < tresid) {
		len = uiop->uio_resid & (DIRBLKSIZ - 1);
		if (len > 0) {
			dp = (struct direct *)
				(uiop->uio_iov->iov_base - lastlen);
			dp->d_reclen += len;
			uiop->uio_iov->iov_base += len;
			uiop->uio_iov->iov_len -= len;
			uiop->uio_resid -= len;
		}
	}
nfsmout:
	return (error);
}

static char hextoasc[] = "0123456789abcdef";

/*
 * Silly rename. To make the NFS filesystem that is stateless look a little
 * more like the "ufs" a remove of an active vnode is translated to a rename
 * to a funny looking filename that is removed by nfs_inactive on the
 * nfsnode. There is the potential for another process on a different client
 * to create the same funny name between the nfs_lookitup() fails and the
 * nfs_rename() completes, but...
 */
nfs_sillyrename(ndp, flag)
	register struct nameidata *ndp;
	int flag;
{
	register struct nfsnode *np;
	register struct sillyrename *sp;
	register struct nameidata *tndp;
	int error;
	short pid;

	np = VTONFS(ndp->ni_dvp);
	cache_purge(ndp->ni_dvp);
	MALLOC(sp, struct sillyrename *, sizeof (struct sillyrename),
		M_TEMP, M_WAITOK);
	sp->s_flag = flag;
	bcopy((caddr_t)&np->n_fh, (caddr_t)&sp->s_fh, NFSX_FH);
	np = VTONFS(ndp->ni_vp);
	tndp = &sp->s_namei;
	tndp->ni_cred = crdup(ndp->ni_cred);

	/* Fudge together a funny name */
	pid = u.u_procp->p_pid;
	bcopy(".nfsAxxxx4.4", tndp->ni_dent.d_name, 13);
	tndp->ni_dent.d_namlen = 12;
	tndp->ni_dent.d_name[8] = hextoasc[pid & 0xf];
	tndp->ni_dent.d_name[7] = hextoasc[(pid >> 4) & 0xf];
	tndp->ni_dent.d_name[6] = hextoasc[(pid >> 8) & 0xf];
	tndp->ni_dent.d_name[5] = hextoasc[(pid >> 12) & 0xf];

	/* Try lookitups until we get one that isn't there */
	while (nfs_lookitup(ndp->ni_dvp, tndp, (nfsv2fh_t *)0) == 0) {
		tndp->ni_dent.d_name[4]++;
		if (tndp->ni_dent.d_name[4] > 'z') {
			error = EINVAL;
			goto bad;
		}
	}
	if (error = nfs_renameit(ndp, tndp))
		goto bad;
	nfs_lookitup(ndp->ni_dvp, tndp, &np->n_fh);
	np->n_sillyrename = sp;
	return (0);
bad:
	crfree(tndp->ni_cred);
	free((caddr_t)sp, M_TEMP);
	return (error);
}

/*
 * Look up a file name for silly rename stuff.
 * Just like nfs_lookup() except that it doesn't load returned values
 * into the nfsnode table.
 * If fhp != NULL it copies the returned file handle out
 */
nfs_lookitup(vp, ndp, fhp)
	register struct vnode *vp;
	register struct nameidata *ndp;
	nfsv2fh_t *fhp;
{
	register u_long *p;
	register caddr_t cp;
	register long t1, t2;
	caddr_t bpos, dpos, cp2;
	u_long xid;
	int error = 0;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;
	long len;

	nfsstats.rpccnt[NFSPROC_LOOKUP]++;
	ndp->ni_dvp = vp;
	ndp->ni_vp = NULL;
	len = ndp->ni_dent.d_namlen;
	nfsm_reqhead(nfs_procids[NFSPROC_LOOKUP], ndp->ni_cred, NFSX_FH+NFSX_UNSIGNED+nfsm_rndup(len));
	nfsm_fhtom(vp);
	nfsm_strtom(ndp->ni_dent.d_name, len, NFS_MAXNAMLEN);
	nfsm_request(vp, NFSPROC_LOOKUP, u.u_procp, 1);
	if (fhp != NULL) {
		nfsm_disect(cp, caddr_t, NFSX_FH);
		bcopy(cp, (caddr_t)fhp, NFSX_FH);
	}
	nfsm_reqdone;
	return (error);
}

/*
 * Kludge City..
 * - make nfs_bmap() essentially a no-op that does no translation
 * - do nfs_strategy() by faking physical I/O with nfs_readrpc/nfs_writerpc
 *   after mapping the physical addresses into Kernel Virtual space in the
 *   nfsiobuf area.
 *   (Maybe I could use the process's page mapping, but I was concerned that
 *    Kernel Write might not be enabled and also figured copyout() would do
 *    a lot more work than bcopy() and also it currently happens in the
 *    context of the swapper process (2).
 */
nfs_bmap(vp, bn, vpp, bnp)
	struct vnode *vp;
	daddr_t bn;
	struct vnode **vpp;
	daddr_t *bnp;
{
	if (vpp != NULL)
		*vpp = vp;
	if (bnp != NULL)
		*bnp = bn * btodb(vp->v_mount->mnt_stat.f_bsize);
	return (0);
}

/*
 * Strategy routine for phys. i/o
 * If the biod's are running, queue a request
 * otherwise just call nfs_doio() to get it done
 */
nfs_strategy(bp)
	register struct buf *bp;
{
	register struct buf *dp;
	register int i;
	struct proc *rp;
	int error = 0;
	int fnd = 0;

	/*
	 * Set b_proc. It seems a bit silly to do it here, but since bread()
	 * doesn't set it, I will.
	 * Set b_proc == NULL for asynchronous ops, since these may still
	 * be hanging about after the process terminates.
	 */
	if (bp->b_flags & B_ASYNC)
		bp->b_proc = (struct proc *)0;
	else
		bp->b_proc = u.u_procp;

	/*
	 * If the op is asynchronous and an i/o daemon is waiting
	 * queue the request, wake it up and wait for completion
	 * otherwise just do it ourselves.
	 */
	if (bp->b_proc == (struct proc *)NULL)
	    for (i = 0; i < nfs_asyncdaemons; i++) {
		if (rp = nfs_iodwant[i]) {
			/*
			 * Ensure that the async_daemon is still waiting here
			 */
			if (rp->p_stat != SSLEEP ||
			    rp->p_wchan != ((caddr_t)&nfs_iodwant[i])) {
				nfs_iodwant[i] = (struct proc *)0;
				continue;
			}
			dp = &nfs_bqueue;
			if (dp->b_actf == NULL) {
				dp->b_actl = bp;
				bp->b_actf = dp;
			} else {
				dp->b_actf->b_actl = bp;
				bp->b_actf = dp->b_actf;
			}
			dp->b_actf = bp;
			bp->b_actl = dp;
			fnd++;
			nfs_iodwant[i] = (struct proc *)0;
			wakeup((caddr_t)&nfs_iodwant[i]);
			break;
		}
	}
	if (!fnd)
		error = nfs_doio(bp);
	return (error);
}

/*
 * Fun and games with i/o
 * Essentially play ubasetup() and disk interrupt service routine by
 * mapping the data buffer into kernel virtual space and doing the
 * nfs read or write rpc's from it.
 * If the nfsiod's are not running, this is just called from nfs_strategy(),
 * otherwise it is called by the nfsiods to do what would normally be
 * partially disk interrupt driven.
 */
nfs_doio(bp)
	register struct buf *bp;
{
	register struct uio *uiop;
	register struct vnode *vp;
	struct nfsnode *np;
	struct ucred *cr;
	struct proc *rp;
	int error;
	struct uio uio;
	struct iovec io;
#if !defined(hp300) && !defined(i386)
	register struct pte *pte, *ppte;
	register caddr_t vaddr;
	int npf, npf2;
	int reg, o;
	caddr_t vbase;
	unsigned v;
#endif

	vp = bp->b_vp;
	np = VTONFS(vp);
	uiop = &uio;
	uiop->uio_iov = &io;
	uiop->uio_iovcnt = 1;
	uiop->uio_segflg = UIO_SYSSPACE;

	/*
	 * For phys i/o, map the b_addr into kernel virtual space using
	 * the Nfsiomap pte's
	 * Also, add a temporary b_rcred for reading using the process's uid
	 * and a guess at a group
	 */
	if (bp->b_flags & B_PHYS) {
		bp->b_rcred = cr = crget();
		rp = (bp->b_flags & B_DIRTY) ? &proc[2] : bp->b_proc;
		cr->cr_uid = rp->p_uid;
		cr->cr_gid = 0;		/* Anything ?? */
		cr->cr_ngroups = 1;
#if defined(hp300) || defined(i386)
		/* mapping was already done by vmapbuf */
		io.iov_base = bp->b_un.b_addr;
#else
		o = (int)bp->b_un.b_addr & PGOFSET;
		npf2 = npf = btoc(bp->b_bcount + o);

		/*
		 * Get some mapping page table entries
		 */
		while ((reg = rmalloc(nfsmap, (long)npf)) == 0) {
			nfsmap_want++;
			(void) tsleep((caddr_t)&nfsmap_want, PZERO-1, "nfsmap",
					0);
		}
		reg--;
		if (bp->b_flags & B_PAGET)
			pte = &Usrptmap[btokmx((struct pte *)bp->b_un.b_addr)];
		else {
			v = btop(bp->b_un.b_addr);
			if (bp->b_flags & B_UAREA)
				pte = &rp->p_addr[v];
			else
				pte = vtopte(rp, v);
		}

		/*
		 * Play vmaccess() but with the Nfsiomap page table
		 */
		ppte = &Nfsiomap[reg];
		vbase = vaddr = &nfsiobuf[reg*NBPG];
		while (npf != 0) {
			mapin(ppte, (u_int)vaddr, pte->pg_pfnum, (int)(PG_V|PG_KW));
#if defined(tahoe)
			mtpr(P1DC, vaddr);
#endif
			ppte++;
			pte++;
			vaddr += NBPG;
			--npf;
		}
		io.iov_base = vbase+o;
#endif /* !defined(hp300) */

		/*
		 * And do the i/o rpc
		 */
		io.iov_len = uiop->uio_resid = bp->b_bcount;
		uiop->uio_offset = bp->b_blkno * DEV_BSIZE;
		if (bp->b_flags & B_READ) {
			uiop->uio_rw = UIO_READ;
			nfsstats.read_physios++;
			bp->b_error = error = nfs_readrpc(vp, uiop,
				bp->b_rcred, bp->b_proc);
			(void) vnode_pager_uncache(vp);
		} else {
			uiop->uio_rw = UIO_WRITE;
			nfsstats.write_physios++;
			bp->b_error = error = nfs_writerpc(vp, uiop,
				bp->b_wcred, bp->b_proc);
		}

		/*
		 * Finally, release pte's used by physical i/o
		 */
		crfree(cr);
#if !defined(hp300) && !defined(i386)
		rmfree(nfsmap, (long)npf2, (long)++reg);
		if (nfsmap_want) {
			nfsmap_want = 0;
			wakeup((caddr_t)&nfsmap_want);
		}
#endif
	} else {
		if (bp->b_flags & B_READ) {
			io.iov_len = uiop->uio_resid = bp->b_bcount;
			io.iov_base = bp->b_un.b_addr;
			uiop->uio_rw = UIO_READ;
			switch (vp->v_type) {
			case VREG:
				uiop->uio_offset = bp->b_blkno * DEV_BSIZE;
				nfsstats.read_bios++;
				error = nfs_readrpc(vp, uiop, bp->b_rcred,
					bp->b_proc);
				break;
			case VLNK:
				uiop->uio_offset = 0;
				nfsstats.readlink_bios++;
				error = nfs_readlinkrpc(vp, uiop, bp->b_rcred,
						bp->b_proc);
				break;
			case VDIR:
				uiop->uio_offset = bp->b_lblkno;
				nfsstats.readdir_bios++;
				error = nfs_readdirrpc(vp, uiop, bp->b_rcred,
					    bp->b_proc);
				/*
				 * Save offset cookie in b_blkno.
				 */
				bp->b_blkno = uiop->uio_offset;
				break;
			};
			bp->b_error = error;
		} else {
			io.iov_len = uiop->uio_resid = bp->b_dirtyend
				- bp->b_dirtyoff;
			uiop->uio_offset = (bp->b_blkno * DEV_BSIZE)
				+ bp->b_dirtyoff;
			io.iov_base = bp->b_un.b_addr + bp->b_dirtyoff;
			uiop->uio_rw = UIO_WRITE;
			nfsstats.write_bios++;
			bp->b_error = error = nfs_writerpc(vp, uiop,
				bp->b_wcred, bp->b_proc);
			if (error) {
				np->n_error = error;
				np->n_flag |= NWRITEERR;
			}
			bp->b_dirtyoff = bp->b_dirtyend = 0;
		}
	}
	if (error)
		bp->b_flags |= B_ERROR;
	bp->b_resid = uiop->uio_resid;
	biodone(bp);
	return (error);
}

/*
 * Flush all the blocks associated with a vnode.
 * 	Walk through the buffer pool and push any dirty pages
 *	associated with the vnode.
 */
/* ARGSUSED */
nfs_fsync(vp, fflags, cred, waitfor)
	register struct vnode *vp;
	int fflags;
	struct ucred *cred;
	int waitfor;
{
	register struct nfsnode *np = VTONFS(vp);
	int error = 0;

	if (np->n_flag & NMODIFIED) {
		np->n_flag &= ~NMODIFIED;
		vflushbuf(vp, waitfor == MNT_WAIT ? B_SYNC : 0);
	}
	if (!error && (np->n_flag & NWRITEERR))
		error = np->n_error;
	return (error);
}

/*
 * NFS advisory byte-level locks.
 * Currently unsupported.
 */
nfs_advlock(vp, id, op, fl, flags)
	struct vnode *vp;
	caddr_t id;
	int op;
	struct flock *fl;
	int flags;
{

	return (EOPNOTSUPP);
}

/*
 * Print out the contents of an nfsnode.
 */
nfs_print(vp)
	struct vnode *vp;
{
	register struct nfsnode *np = VTONFS(vp);

	printf("tag VT_NFS, fileid %d fsid 0x%x",
		np->n_vattr.va_fileid, np->n_vattr.va_fsid);
#ifdef FIFO
	if (vp->v_type == VFIFO)
		fifo_printinfo(vp);
#endif /* FIFO */
	printf("%s\n", (np->n_flag & NLOCKED) ? " (LOCKED)" : "");
	if (np->n_lockholder == 0)
		return;
	printf("\towner pid %d", np->n_lockholder);
	if (np->n_lockwaiter)
		printf(" waiting pid %d", np->n_lockwaiter);
	printf("\n");
}
