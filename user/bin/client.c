#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int
main(int argc, char *argv[])
{
	int sockfd = 0, n = 0;
	char recvBuff[1024];
	struct sockaddr_in serv_addr;

	if (argc != 2)	{
		printf("Usage: %s <ip of server>\n", argv[0]);
		return 1;
	}

	memset(recvBuff, '0',sizeof(recvBuff));

	/* a socket is created through call to socket() function */
	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		exit(1);
	}

	memset(&serv_addr, '0', sizeof(serv_addr));

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(80);

	if (inet_pton(AF_INET, argv[1], &serv_addr.sin_addr) != 1) {
		perror("inet_pton");
		exit(1);
	}

	if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		perror("connect");
		exit(1);
	}

	while ((n = read(sockfd, recvBuff, sizeof(recvBuff)-1)) > 0) {
		recvBuff[n] = '\0';

		if (fputs(recvBuff, stdout) == EOF) {
			perror("fputs");
			exit(1);
		}
	}

	if (n < 0) {
		perror("read");
		exit(1);
	}

	return 0;
}
