/*
 * Copyright (c) 1980, 1986, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	@(#)route.c	8.3.1.1 (Berkeley) %G%
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/ioctl.h>

#include <net/if.h>
#include <net/route.h>
#include <net/raw_cb.h>

#include <netinet/in.h>
#include <netinet/in_var.h>

#ifdef NS
#include <netns/ns.h>
#endif

#define	SA(p) ((struct sockaddr *)(p))

int	rttrash;		/* routes not in table but not freed */
struct	sockaddr wildcard;	/* zero valued cookie for wildcard searches */

void
rtable_init(table)
	void **table;
{
	struct domain *dom;
	for (dom = domains; dom; dom = dom->dom_next)
		if (dom->dom_rtattach)
			dom->dom_rtattach(&table[dom->dom_family],
			    dom->dom_rtoffset);
}

void
route_init()
{
	rn_init();	/* initialize all zeroes, all ones, mask table */
	rtable_init((void **)rt_tables);
}

/*
 * Packet routing routines.
 */
void
rtalloc(ro)
	register struct route *ro;
{
	if (ro->ro_rt && ro->ro_rt->rt_ifp && (ro->ro_rt->rt_flags & RTF_UP))
		return;				 /* XXX */
	ro->ro_rt = rtalloc1(&ro->ro_dst, 1);
}

struct rtentry *
rtalloc1(dst, report)
	register struct sockaddr *dst;
	int report;
{
	register struct radix_node_head *rnh = rt_tables[dst->sa_family];
	register struct rtentry *rt;
	register struct radix_node *rn;
	struct rtentry *newrt = 0;
	struct rt_addrinfo info;
	int  s = splnet(), err = 0, msgtype = RTM_MISS;

	bzero((caddr_t)&info, sizeof(info));
	info.rti_info[RTAX_DST] = dst;
	if (rnh && (rn = rnh->rnh_matchaddr((caddr_t)dst, rnh)) &&
	    ((rn->rn_flags & RNF_ROOT) == 0)) {
		newrt = rt = (struct rtentry *)rn;
		if (report && (rt->rt_flags & RTF_CLONING)) {
			info.rti_ifa = rt->rt_ifa;
			info.rti_ifp = rt->rt_ifp;
			info.rti_flags = rt->rt_flags & ~RTF_CLONING;
			info.rti_info[RTAX_GATEWAY] = rt->rt_gateway;
			if ((info.rti_info[RTAX_NETMASK] = rt->rt_genmask) == 0)
				info.rti_flags |= RTF_HOST;
			err = rtrequest1(RTM_RESOLVE, &info, &newrt);
			if (err) {
				newrt = rt;
				rt->rt_refcnt++;
				goto miss;
			}
			newrt->rt_rmx = rt->rt_rmx;
			if (rt->rt_flags & RTF_XRESOLVE) {
				msgtype = RTM_RESOLVE;
				goto miss;
			}
		} else
			rt->rt_refcnt++;
	} else {
		rtstat.rts_unreach++;
	miss:	if (report) {
			rt_missmsg(msgtype, &info, 0, err);
		}
	}
	splx(s);
	return (newrt);
}

void
rtfree(rt)
	register struct rtentry *rt;
{
	register struct ifaddr *ifa;

	if (rt == 0)
		panic("rtfree");
	rt->rt_refcnt--;
	if (rt->rt_refcnt <= 0 && (rt->rt_flags & RTF_UP) == 0) {
		if (rt->rt_nodes->rn_flags & (RNF_ACTIVE | RNF_ROOT))
			panic ("rtfree 2");
		rttrash--;
		if (rt->rt_refcnt < 0) {
			printf("rtfree: %x not freed (neg refs)\n", rt);
			return;
		}
		ifa = rt->rt_ifa;
		IFAFREE(ifa);
		Free(rt_key(rt));
		Free(rt);
	}
}

void
ifafree(ifa)
	register struct ifaddr *ifa;
{
	if (ifa == NULL)
		panic("ifafree");
	if (ifa->ifa_refcnt == 0)
		free(ifa, M_IFADDR);
	else
		ifa->ifa_refcnt--;
}

/*
 * Force a routing table entry to the specified
 * destination to go through the given gateway.
 * Normally called as a result of a routing redirect
 * message from the network layer.
 *
 * N.B.: must be called at splnet
 *
 */
int
rtredirect(dst, gateway, netmask, flags, src, rtp)
	struct sockaddr *dst, *gateway, *netmask, *src;
	int flags;
	struct rtentry **rtp;
{
	register struct rtentry *rt;
	int error = 0;
	short *stat = 0;
	struct rt_addrinfo info;
	struct ifaddr *ifa;

	/* verify the gateway is directly reachable */
	if ((ifa = ifa_ifwithnet(gateway)) == 0) {
		error = ENETUNREACH;
		goto out;
	}
	rt = rtalloc1(dst, 0);
	/*
	 * If the redirect isn't from our current router for this dst,
	 * it's either old or wrong.  If it redirects us to ourselves,
	 * we have a routing loop, perhaps as a result of an interface
	 * going down recently.
	 */
#define	equal(a1, a2) (bcmp((caddr_t)(a1), (caddr_t)(a2), (a1)->sa_len) == 0)
	if (!(flags & RTF_DONE) && rt &&
	     (!equal(src, rt->rt_gateway) || rt->rt_ifa != ifa))
		error = EINVAL;
	else if (ifa_ifwithaddr(gateway))
		error = EHOSTUNREACH;
	if (error)
		goto done;
	/*
	 * Create a new entry if we just got back a wildcard entry
	 * or the the lookup failed.  This is necessary for hosts
	 * which use routing redirects generated by smart gateways
	 * to dynamically build the routing tables.
	 */
	if ((rt == 0) || (rt_mask(rt) && rt_mask(rt)->sa_len < 2))
		goto create;
	/*
	 * Don't listen to the redirect if it's
	 * for a route to an interface. 
	 */
	if (rt->rt_flags & RTF_GATEWAY) {
		if (((rt->rt_flags & RTF_HOST) == 0) && (flags & RTF_HOST)) {
			/*
			 * Changing from route to net => route to host.
			 * Create new route, rather than smashing route to net.
			 */
		create:
			flags |=  RTF_GATEWAY | RTF_DYNAMIC;
			error = rtrequest((int)RTM_ADD, dst, gateway,
				    netmask, flags,
				    (struct rtentry **)0);
			stat = &rtstat.rts_dynamic;
		} else {
			/*
			 * Smash the current notion of the gateway to
			 * this destination.  Should check about netmask!!!
			 */
			rt->rt_flags |= RTF_MODIFIED;
			flags |= RTF_MODIFIED;
			stat = &rtstat.rts_newgateway;
			rt_setgate(rt, rt_key(rt), gateway);
		}
	} else
		error = EHOSTUNREACH;
done:
	if (rt) {
		if (rtp && !error)
			*rtp = rt;
		else
			rtfree(rt);
	}
out:
	if (error)
		rtstat.rts_badredirect++;
	else if (stat != NULL)
		(*stat)++;
	bzero((caddr_t)&info, sizeof(info));
	info.rti_info[RTAX_DST] = dst;
	info.rti_info[RTAX_GATEWAY] = gateway;
	info.rti_info[RTAX_NETMASK] = netmask;
	info.rti_info[RTAX_AUTHOR] = src;
	rt_missmsg(RTM_REDIRECT, &info, flags, error);
}

/*
* Routing table ioctl interface.
*/
int
rtioctl(req, data, p)
	u_long req;
	caddr_t data;
	struct proc *p;
{
	return (EOPNOTSUPP);
}

struct ifaddr *
ifa_ifwithroute(flags, dst, gateway)
	int flags;
	struct sockaddr	*dst, *gateway;
{
	register struct ifaddr *ifa;
	if ((flags & RTF_GATEWAY) == 0) {
		/*
		 * If we are adding a route to an interface,
		 * and the interface is a pt to pt link
		 * we should search for the destination
		 * as our clue to the interface.  Otherwise
		 * we can use the local address.
		 */
		ifa = 0;
		if (flags & RTF_HOST) 
			ifa = ifa_ifwithdstaddr(dst);
		if (ifa == 0)
			ifa = ifa_ifwithaddr(gateway);
	} else {
		/*
		 * If we are adding a route to a remote net
		 * or host, the gateway may still be on the
		 * other end of a pt to pt link.
		 */
		ifa = ifa_ifwithdstaddr(gateway);
	}
	if (ifa == 0)
		ifa = ifa_ifwithnet(gateway);
	return (ifa);
}

int
rtgateinfo(info)
	register struct rt_addrinfo *info;
{
	struct ifaddr *ifa;
	struct ifnet *ifp;
	int error = 0;
/* Sleazy use of local variables throughout this function and next, XXX!!!! */
#define dst	info->rti_info[RTAX_DST]
#define gateway	info->rti_info[RTAX_GATEWAY]
#define netmask	info->rti_info[RTAX_NETMASK]
#define ifaaddr	info->rti_info[RTAX_IFA]
#define ifpaddr	info->rti_info[RTAX_IFP]
#define flags	info->rti_flags

	/* ifp may be specified by sockaddr_dl
		   when protcol address is ambiguous */
	if (info->rti_ifp == 0 && ifpaddr) {
		ifa = ifa_ifwithnet(ifpaddr);
		info->rti_ifp = ifa ? ifa->ifa_ifp : 0;
	}
	if (info->rti_ifa == 0) {
		struct sockaddr *sa;
		if (ifp = info->rti_ifp) {
			sa = ifaaddr ? ifaaddr : (gateway ? gateway : dst);
			info->rti_ifa = ifaof_ifpforaddr(sa, ifp);
		} else
			info->rti_ifa = ifa_ifwithroute(flags, dst, gateway);
	}
	if (ifa = info->rti_ifa) {
		if (info->rti_ifp == 0)
			info->rti_ifp = ifa->ifa_ifp;
		if (gateway == 0)
		    gateway = ifa->ifa_addr;
	} else
		error = ENETUNREACH;
	return error;
}


int
rtrequest1(req, info, ret_nrt)
	int req;
	register struct rt_addrinfo *info;
	struct rtentry **ret_nrt;
{
	struct rt_addrinfo info;

	bzero((caddr_t)&info, sizeof(info));
	info.rti_flags = flags;
	info.rti_info[RTAX_DST] = dst;
	info.rti_info[RTAX_GATEWAY] = gateway;
	info.rti_info[RTAX_NETMASK] = netmask;
	return rtrequest1(req, &info, ret_nrt);
}

/*
 * These (questionable) definitions of apparent local variables apply
 * to the next two functions.  XXXXXX!!!
 */
#define dst	info->rti_info[RTAX_DST]
#define gateway	info->rti_info[RTAX_GATEWAY]
#define netmask	info->rti_info[RTAX_NETMASK]
#define ifaaddr	info->rti_info[RTAX_IFA]
#define ifpaddr	info->rti_info[RTAX_IFP]
#define flags	info->rti_flags

int
rt_getifa(info)
	register struct rt_addrinfo *info;
{
	struct ifaddr *ifa;
	int error = 0;

	/* ifp may be specified by sockaddr_dl
		   when protocol address is ambiguous */
	if (info->rti_ifp == 0 && ifpaddr && ifpaddr->sa_family == AF_LINK) {
		ifa = ifa_ifwithnet(ifpaddr);
		info->rti_ifp = ifa ? ifa->ifa_ifp : 0;
	}
	if (info->rti_ifa == 0) {
		struct sockaddr *sa;

		sa = ifaaddr ? ifaaddr : (gateway ? gateway : dst);
		if (sa && info->rti_ifp)
			info->rti_ifa = ifaof_ifpforaddr(sa, info->rti_ifp);
		else if (dst && gateway)
			info->rti_ifa = ifa_ifwithroute(flags, dst, gateway);
		else if (sa)
			info->rti_ifa = ifa_ifwithroute(flags, sa, sa);
	}
	if (ifa = info->rti_ifa) {
		if (info->rti_ifp == 0)
			info->rti_ifp = ifa->ifa_ifp;
	} else
		error = ENETUNREACH;
	return error;
}

int
rtrequest1(req, info, ret_nrt)
	int req;
	register struct rt_addrinfo *info;
	struct rtentry **ret_nrt;
{
	int s = splnet(); int error = 0;
	register struct rtentry *rt;
	register struct radix_node *rn;
	register struct radix_node_head *rnh;
	struct sockaddr *ndst;
	struct ifaddr *ifa = info->rti_ifa;
	struct ifnet *ifp;
#define senderr(x) { error = x ; goto bad; }
#define RTE(x) ((struct rtentry *)x)

	if ((rnh = rt_tables[dst->sa_family]) == 0)
		senderr(EAFNOSUPPORT);
	if (flags & RTF_HOST)
		netmask = 0;
	switch (req) {
	case RTM_DELPKT:
		if (rnh->rnh_deladdr == 0)
			senderr(EOPNOTSUPP);
		rn = rnh->rnh_delpkt(info->rti_pkthdr, (caddr_t)info, rnh);
		goto delete;
	case RTM_DELETE:
		if ((ifaaddr || ifpaddr) && rnh->rnh_delete1) {
			extern struct radix_node_head *mask_rnhead;
			if (error = rtgateinfo(info))
				senderr(error);
			ifa = info->rti_ifa;
			ifp = info->rti_ifp;
			if (netmask && (rn = rn_search(netmask,
                                            mask_rnhead->rnh_treetop)))
                                netmask = SA(rn->rn_key);
			for (rn = rnh->rnh_matchaddr(dst, rnh); rn;
						    rn = rn->rn_dupedkey) {
				/* following test includes case netmask == 0 */
				if (SA(rn->rn_mask) == netmask &&
				    ((ifaaddr && ifa == RTE(rn)->rt_ifa) ||
				     (!ifaaddr && ifp == RTE(rn)->rt_ifp)))
					break;
			if (rn == 0 || (rn = rnh->rnh_delete1(rn, rnh)) == 0)
				senderr(ESRCH);
			}
		} else if ((rn = rnh->rnh_deladdr(dst, netmask, rnh)) == 0)
			senderr(ESRCH);
		if (rn->rn_flags & (RNF_ACTIVE | RNF_ROOT))
			panic ("rtrequest delete");
		rt = RTE(rn);
		rt->rt_flags &= ~RTF_UP;
		if (rt->rt_gwroute) {
			rt = rt->rt_gwroute; RTFREE(rt);
			(rt = RTE(rn))->rt_gwroute = 0;
		}
		if ((ifa = rt->rt_ifa) && ifa->ifa_rtrequest)
			ifa->ifa_rtrequest(RTM_DELETE, rt, info);
		rttrash++;
		if (ret_nrt)
			*ret_nrt = rt;
		else if (rt->rt_refcnt <= 0) {
			rt->rt_refcnt++;
			rtfree(rt);
		}
		break;


	case RTM_ADDPKT:
		if (rnh->rnh_addpkt == 0)
			senderr(EOPNOTSUPP);
		/*FALLTHROUGH*/
	case RTM_ADD:
		if (ifa == 0) {
			if (error = rtgateinfo(info))
				senderr(error);
			ifa = info->rti_ifa;
		}
	case RTM_RESOLVE:
		R_Malloc(rt, struct rtentry *, sizeof(*rt));
		if (rt == 0)
			senderr(ENOBUFS);
		Bzero(rt, sizeof(*rt));
		rt->rt_flags = RTF_UP | flags;
		if (rt_setgate(rt, dst, gateway)) {
			Free(rt);
			senderr(ENOBUFS);
		}
		if (req == RTM_ADDPKT) {
			rn = rnh->rnh_addpkt(info->rti_pkthdr, (caddr_t)info,
						rnh, rt->rt_nodes);
			goto add; /* addpkt() must allocate space */
		}
		ndst = rt_key(rt);
		if (netmask) {
			rt_maskedcopy(dst, ndst, netmask);
		} else
			Bcopy(dst, ndst, dst->sa_len);
		rn = rnh->rnh_addaddr((caddr_t)ndst, (caddr_t)netmask,
					rnh, rt->rt_nodes);
	add:
		if (rn == 0) {
			if (rt->rt_gwroute)
				rtfree(rt->rt_gwroute);
			Free(rt_key(rt));
			Free(rt);
			senderr(EEXIST);
		}
		ifa->ifa_refcnt++;
		rt->rt_ifa = ifa;
		rt->rt_ifp = ifa->ifa_ifp;
		if (ifa->ifa_rtrequest)
			ifa->ifa_rtrequest(req, rt, info);
		if (ret_nrt) {
			*ret_nrt = rt;
			rt->rt_refcnt++;
		}
		break;
	default:
		error = EOPNOTSUPP;
	}
bad:
	splx(s);
	return (error);
#undef dst
#undef gateway
#undef netmask
#undef ifaaddr
#undef ifpaddr
#undef flags
#undef dst
#undef gateway
#undef netmask
#undef ifaaddr
#undef ifpaddr
#undef flags
}

int
rtrequest(req, dst, gateway, netmask, flags, ret_nrt)
	int req, flags;
	struct sockaddr *dst, *gateway, *netmask;
	struct rtentry **ret_nrt;
{
	struct rt_addrinfo info;

	bzero((caddr_t)&info, sizeof(info));
	info.rti_flags = flags;
	info.rti_info[RTAX_DST] = dst;
	info.rti_info[RTAX_GATEWAY] = gateway;
	info.rti_info[RTAX_NETMASK] = netmask;
	return rtrequest1(req, info, ret_nrt);
}
#define ROUNDUP(a) (a>0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))

int
rt_setgate(rt0, dst, gate)
	struct rtentry *rt0;
	struct sockaddr *dst, *gate;
{
	caddr_t new, old;
	int dlen = ROUNDUP(dst->sa_len), glen = ROUNDUP(gate->sa_len);
	register struct rtentry *rt = rt0;

	if (rt->rt_gateway == 0 || glen > ROUNDUP(rt->rt_gateway->sa_len)) {
		old = (caddr_t)rt_key(rt);
		R_Malloc(new, caddr_t, dlen + glen);
		if (new == 0)
			return 1;
		rt->rt_nodes->rn_key = new;
	} else {
		new = rt->rt_nodes->rn_key;
		old = 0;
	}
	Bcopy(gate, (rt->rt_gateway = (struct sockaddr *)(new + dlen)), glen);
	if (old) {
		Bcopy(dst, new, dlen);
		Free(old);
	}
	if (rt->rt_gwroute) {
		rt = rt->rt_gwroute; RTFREE(rt);
		rt = rt0; rt->rt_gwroute = 0;
	}
	return 0;
}

void
rt_maskedcopy(src, dst, netmask)
	struct sockaddr *src, *dst, *netmask;
{
	register u_char *cp1 = (u_char *)src;
	register u_char *cp2 = (u_char *)dst;
	register u_char *cp3 = (u_char *)netmask;
	u_char *cplim = cp2 + *cp3;
	u_char *cplim2 = cp2 + *cp1;

	*cp2++ = *cp1++; *cp2++ = *cp1++; /* copies sa_len & sa_family */
	cp3 += 2;
	if (cplim > cplim2)
		cplim = cplim2;
	while (cp2 < cplim)
		*cp2++ = *cp1++ & *cp3++;
	if (cp2 < cplim2)
		bzero((caddr_t)cp2, (unsigned)(cplim2 - cp2));
}

/*
 * Set up a routing table entry, normally
 * for an interface.
 */
int
rtinit(ifa, cmd, flags)
	register struct ifaddr *ifa;
	int cmd, flags;
{
	register struct rtentry *rt;
	register struct sockaddr *dst;
	register struct sockaddr *deldst;
	struct mbuf *m = 0;
	struct rtentry *nrt = 0;
	struct radix_node_head *rnh;
	struct radix_node *rn;
	int error;
	struct rt_addrinfo info;

	dst = flags & RTF_HOST ? ifa->ifa_dstaddr : ifa->ifa_addr;
	if (cmd == RTM_DELETE) {
		if ((flags & RTF_HOST) == 0 && ifa->ifa_netmask) {
			m = m_get(M_WAIT, MT_SONAME);
			deldst = mtod(m, struct sockaddr *);
			rt_maskedcopy(dst, deldst, ifa->ifa_netmask);
			dst = deldst;
		}
		if ((rnh = rt_tables[dst->sa_family]) == 0 ||
		    (rn = rnh->rnh_lookup(dst, ifa->ifa_netmask, rnh)) == 0 ||
		    (rn->rn_flags & RNF_ROOT) != 0 ||
		    ((struct rtentry *)rn)->rt_ifa != ifa ||
		    !equal(rt_key(rt), dst))
		     {
			if (m)
				(void) m_free(m);
			return (flags & RTF_HOST ? EHOSTUNREACH : ENETUNREACH);
		}
	}
	bzero((caddr_t)&info, sizeof(info));
	info.rti_ifa = ifa;
	info.rti_flags = flags | ifa->ifa_flags;
	info.rti_info[RTAX_DST] = dst;
	info.rti_info[RTAX_GATEWAY] = ifa->ifa_addr;
	info.rti_info[RTAX_NETMASK] = ifa->ifa_netmask;
	error = rtrequest1(cmd, &info, &nrt);

	if (error == 0 && (rt = nrt)) {
		rt_newaddrmsg(cmd, ifa, error, rt);
		if (cmd == RTM_DELETE) {
			if (rt->rt_refcnt <= 0) {
				rt->rt_refcnt++;
				rtfree(rt);
			}
		} else if (cmd == RTM_ADD)
			rt->rt_refcnt--;
	}
	if (m)
		(void) m_free(m);
	return (error);
}
