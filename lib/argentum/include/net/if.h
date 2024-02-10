#ifndef _NET_IF_H
#define _NET_IF_H

struct if_nameindex {
  unsigned  if_index;
  char     *if_name;
};

#define IF_NAMESIZE 6
#define IFNAMSIZ    IF_NAMESIZE

#define	IFF_UP		0x0001		/* interface is up */
#define	IFF_BROADCAST	0x0002		/* broadcast address valid */
#define	IFF_DEBUG	0x0004		/* turn on debugging */
#define	IFF_LOOPBACK	0x0008		/* is a loopback net */
#define	IFF_POINTOPOINT	0x0010		/* interface is point-to-point link */
#define	IFF_NOTRAILERS	0x0020		/* avoid use of trailers */
#define	IFF_RUNNING	0x0040		/* resources allocated */
#define	IFF_NOARP	0x0080		/* no address resolution protocol */
#define	IFF_PROMISC	0x0100		/* receive all packets */
#define	IFF_ALLMULTI	0x0200		/* receive all multicast packets */
#define	IFF_OACTIVE	0x0400		/* transmission in progress */
#define	IFF_SIMPLEX	0x0800		/* can't hear own transmissions */
#define	IFF_LINK0	0x1000		/* per link layer defined bit */
#define	IFF_LINK1	0x2000		/* per link layer defined bit */
#define	IFF_LINK2	0x4000		/* per link layer defined bit */
#define	IFF_MULTICAST	0x8000		/* supports multicast */

struct if_nameindex  *if_nameindex(void);

#endif
