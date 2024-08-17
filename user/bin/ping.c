#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

struct ip_hdr {
  /* version / header length */
  uint8_t v_hl;
  /* type of service */
  uint8_t tos;
  /* total length */
  uint16_t len;
  /* identification */
  uint16_t id;
  /* fragment offset field */
  uint16_t offset;
  /* time to live */
  uint8_t ttl;
  /* protocol*/
  uint8_t proto;
  /* checksum */
  uint16_t chksum;
  /* source and destination IP addresses */
  uint32_t src;
  uint32_t dest;
} __attribute__((__packed__));

struct icmp_echo_hdr {
  uint8_t  type;
  uint8_t  code;
  uint16_t chksum;
  uint16_t id;
  uint16_t seqno;
} __attribute__((__packed__));

struct ping_pkt {
  struct icmp_echo_hdr hdr;
  char msg[32];
};

// Calculating the Check Sum
uint16_t
checksum(void* b, int len)
{
  uint16_t* buf = b;
  unsigned int sum = 0;
  uint16_t result;

  for (sum = 0; len > 1; len -= 2)
      sum += *buf++;
  if (len == 1)
      sum += *(uint8_t*) buf;
  sum = (sum >> 16) + (sum & 0xFFFF);
  sum += (sum >> 16);
  result = ~sum;
  return result;
}

double
get_time_in_ms(void)
{
  struct timespec ts;

  clock_gettime(CLOCK_REALTIME, &ts);

  return ts.tv_sec * 100 + (double) ts.tv_nsec / 10000;
}

int
main(int argc, char **argv)
{
  struct sockaddr_in addr;
  int sockfd, i;
  struct timeval timeout;
  uint16_t seqno;

  if (argc < 2) {
    fprintf(stderr, "%s: destination address required\n", argv[0]);
    exit(1);
  }

  if((sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) < 0) {
		perror("socket");
		exit(1);
	}

  memset(&addr, '0', sizeof(addr));

	addr.sin_family = AF_INET;
	addr.sin_port = htons(80);

  fprintf(stderr, "%s\n", argv[1]);

  if (inet_pton(AF_INET, argv[1], &addr.sin_addr.s_addr) != 1) {
		perror("inet_pton");
		exit(1);
	}

  timeout.tv_sec  = 1000;
  timeout.tv_usec = 0;

  if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0) {
    perror("setsockopt");
    exit(1);
  }

  seqno = 0;
  for (i = 0; i < 3; i++) {
    struct ping_pkt packet;
    struct sockaddr_in from;
    socklen_t from_len;
    double start_time, elapsed_time;
    char buf[64];
    int j;

    seqno++;

    packet.hdr.type   = 8;  // echo
    packet.hdr.code   = 0;
    packet.hdr.chksum = 0;
    packet.hdr.id     = 0xAFAF;
    packet.hdr.seqno  = htons(seqno);

    for (j = 0; j < 32; j++)
      packet.msg[j] = (char) j;

    packet.hdr.chksum = checksum(&packet, sizeof packet);

    start_time = get_time_in_ms();

    if (sendto(sockfd, &packet, sizeof packet, 0, (struct sockaddr *) &addr, sizeof addr) < 0) {
      perror("sendto");
      exit(1);
    }

    struct icmp_echo_hdr *icmp;
    ssize_t len;
    struct ip_hdr *ip;

    if ((len = recvfrom(sockfd, buf, sizeof buf, 0, (struct sockaddr *) &from, &from_len)) < 0) {
      perror("recvfrom");
      exit(1);
    }

    elapsed_time = get_time_in_ms() - start_time;

    ip = (struct ip_hdr *) buf;
    icmp = (struct icmp_echo_hdr *) (buf + ((ip->v_hl & 0xF) << 2));

    printf("%d bytes from %s: icmp_seq=%d time=%.1f ms\n", len, argv[1], ntohs(icmp->seqno), elapsed_time);

    sleep(1);
  }

  close(sockfd);

  return 0;
}
