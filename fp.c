/*
 * fp - find free ports
 *
 * Uses the kernel's own port allocator (bind to port 0) for maximum
 * reliability.  The OS guarantees the returned port is not in use and
 * not in TIME_WAIT at the moment of assignment.
 *
 * When multiple ports are requested, all sockets are held open
 * simultaneously so the kernel guarantees every port is unique —
 * no retry loop, no exclusion list.
 *
 * For range-constrained searches we try-bind with a randomised start
 * offset so concurrent invocations are unlikely to collide.
 */

#include <errno.h>
#include <getopt.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define FP_VERSION "1.1.0"
#define MAX_PORTS  1024

static void usage(FILE *out)
{
	fprintf(out,
		"Usage: fp [options]\n"
		"\n"
		"Options:\n"
		"  -n NUM       find NUM free ports (default: 1, max: %d)\n"
		"  -r MIN:MAX   constrain to port range (1-65535)\n"
		"  -u           find UDP ports (default: TCP)\n"
		"  -6           use IPv6  (default: IPv4)\n"
		"  -v           print version\n"
		"  -h           show this help\n",
		MAX_PORTS);
}

/* --------------------------------------------------------------- */
/*  Helpers                                                        */
/* --------------------------------------------------------------- */
static void setup_addr(struct sockaddr_storage *sa, socklen_t *salen,
		       int family, int port)
{
	memset(sa, 0, sizeof(*sa));
	if (family == AF_INET) {
		struct sockaddr_in *a = (struct sockaddr_in *)sa;
		a->sin_family      = AF_INET;
		a->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		a->sin_port        = htons(port);
		*salen = sizeof(*a);
	} else {
		struct sockaddr_in6 *a = (struct sockaddr_in6 *)sa;
		a->sin6_family = AF_INET6;
		a->sin6_addr   = in6addr_loopback;
		a->sin6_port   = htons(port);
		*salen = sizeof(*a);
	}
}

static int read_port(struct sockaddr_storage *sa, int family)
{
	if (family == AF_INET)
		return ntohs(((struct sockaddr_in *)sa)->sin_port);
	return ntohs(((struct sockaddr_in6 *)sa)->sin6_port);
}

/* --------------------------------------------------------------- */
/*  Batch: open N sockets at once — kernel guarantees uniqueness    */
/* --------------------------------------------------------------- */
static int find_free_ports(int *ports, int count, int socktype, int family)
{
	int fds[MAX_PORTS];
	int i;

	/* open and bind all sockets before reading any ports */
	for (i = 0; i < count; i++) {
		fds[i] = socket(family, socktype, 0);
		if (fds[i] < 0) {
			perror("fp: socket");
			goto fail;
		}

		struct sockaddr_storage sa;
		socklen_t salen;
		setup_addr(&sa, &salen, family, 0);

		if (bind(fds[i], (struct sockaddr *)&sa, salen) < 0) {
			perror("fp: bind");
			i++;  /* include this fd in cleanup */
			goto fail;
		}
	}

	/* all bound — now read assigned ports */
	for (i = 0; i < count; i++) {
		struct sockaddr_storage sa;
		socklen_t salen = sizeof(sa);
		if (getsockname(fds[i], (struct sockaddr *)&sa, &salen) < 0) {
			perror("fp: getsockname");
			goto fail_all;
		}
		ports[i] = read_port(&sa, family);
	}

	/* close all */
	for (i = 0; i < count; i++)
		close(fds[i]);
	return 0;

fail_all:
	i = count;
fail:
	for (int j = 0; j < i; j++)
		close(fds[j]);
	return -1;
}

/* --------------------------------------------------------------- */
/*  Range-constrained search: try-bind with random start           */
/* --------------------------------------------------------------- */
static int find_free_ports_range(int *ports, int count, int lo, int hi,
				 int socktype, int family)
{
	int range = hi - lo + 1;
	int start = lo + ((unsigned)rand() % range);
	int found = 0;

	for (int i = 0; i < range && found < count; i++) {
		int port = lo + ((start - lo + i) % range);

		int fd = socket(family, socktype, 0);
		if (fd < 0)
			return -1;

		struct sockaddr_storage sa;
		socklen_t salen;
		setup_addr(&sa, &salen, family, port);

		if (bind(fd, (struct sockaddr *)&sa, salen) == 0)
			ports[found++] = port;

		close(fd);
	}

	return (found == count) ? 0 : -1;
}

/* --------------------------------------------------------------- */
/*  main                                                           */
/* --------------------------------------------------------------- */
int main(int argc, char **argv)
{
	int count    = 1;
	int socktype = SOCK_STREAM;  /* TCP */
	int family   = AF_INET;      /* IPv4 */
	int range_lo = 0, range_hi = 0;
	int use_range = 0;
	int opt;

	srand((unsigned)(time(NULL) ^ getpid()));

	while ((opt = getopt(argc, argv, "n:r:u6vh")) != -1) {
		switch (opt) {
		case 'n':
			count = atoi(optarg);
			if (count < 1 || count > MAX_PORTS) {
				fprintf(stderr,
					"fp: count must be 1-%d\n", MAX_PORTS);
				return 1;
			}
			break;
		case 'r':
			if (sscanf(optarg, "%d:%d", &range_lo, &range_hi) != 2
			    || range_lo < 1 || range_hi > 65535
			    || range_lo > range_hi) {
				fprintf(stderr,
					"fp: bad range (use MIN:MAX, 1-65535)\n");
				return 1;
			}
			use_range = 1;
			break;
		case 'u':
			socktype = SOCK_DGRAM;
			break;
		case '6':
			family = AF_INET6;
			break;
		case 'v':
			puts(FP_VERSION);
			return 0;
		case 'h':
			usage(stdout);
			return 0;
		default:
			usage(stderr);
			return 1;
		}
	}

	if (use_range && count > (range_hi - range_lo + 1)) {
		fprintf(stderr,
			"fp: requested %d ports but range only has %d\n",
			count, range_hi - range_lo + 1);
		return 1;
	}

	int ports[MAX_PORTS];
	int rc;

	if (use_range)
		rc = find_free_ports_range(ports, count, range_lo, range_hi,
					   socktype, family);
	else
		rc = find_free_ports(ports, count, socktype, family);

	if (rc < 0) {
		fprintf(stderr, "fp: could not find free port(s)\n");
		return 1;
	}

	for (int i = 0; i < count; i++)
		printf("%d\n", ports[i]);

	return 0;
}
