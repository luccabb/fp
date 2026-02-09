/*
 * fp - find free ports
 *
 * Uses the kernel's own port allocator (bind to port 0) for maximum
 * reliability.  The OS guarantees the returned port is not in use and
 * not in TIME_WAIT at the moment of assignment.
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

#define FP_VERSION "1.0.0"
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
/*  Core: let the kernel pick a free port (bind-to-0 method)       */
/* --------------------------------------------------------------- */
static int find_free_port(int socktype, int family, const int *exclude,
			  int nexclude)
{
	int attempts = 64; /* retry if we hit a duplicate */

	while (attempts-- > 0) {
		int fd = socket(family, socktype, 0);
		if (fd < 0) {
			perror("fp: socket");
			return -1;
		}

		struct sockaddr_storage sa;
		socklen_t salen;
		memset(&sa, 0, sizeof(sa));

		if (family == AF_INET) {
			struct sockaddr_in *a = (struct sockaddr_in *)&sa;
			a->sin_family      = AF_INET;
			a->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
			a->sin_port        = 0;
			salen = sizeof(*a);
		} else {
			struct sockaddr_in6 *a = (struct sockaddr_in6 *)&sa;
			a->sin6_family = AF_INET6;
			a->sin6_addr   = in6addr_loopback;
			a->sin6_port   = 0;
			salen = sizeof(*a);
		}

		if (bind(fd, (struct sockaddr *)&sa, salen) < 0) {
			close(fd);
			perror("fp: bind");
			return -1;
		}

		if (getsockname(fd, (struct sockaddr *)&sa, &salen) < 0) {
			close(fd);
			perror("fp: getsockname");
			return -1;
		}

		int port;
		if (family == AF_INET)
			port = ntohs(((struct sockaddr_in *)&sa)->sin_port);
		else
			port = ntohs(((struct sockaddr_in6 *)&sa)->sin6_port);

		close(fd);

		/* check exclusion list (already-returned ports) */
		int dup = 0;
		for (int i = 0; i < nexclude; i++) {
			if (exclude[i] == port) { dup = 1; break; }
		}
		if (!dup)
			return port;
	}
	return -1;
}

/* --------------------------------------------------------------- */
/*  Range-constrained search: try-bind with random start           */
/* --------------------------------------------------------------- */
static int try_bind_port(int port, int socktype, int family)
{
	int fd = socket(family, socktype, 0);
	if (fd < 0)
		return 0;

	struct sockaddr_storage sa;
	socklen_t salen;
	memset(&sa, 0, sizeof(sa));

	if (family == AF_INET) {
		struct sockaddr_in *a = (struct sockaddr_in *)&sa;
		a->sin_family      = AF_INET;
		a->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		a->sin_port        = htons(port);
		salen = sizeof(*a);
	} else {
		struct sockaddr_in6 *a = (struct sockaddr_in6 *)&sa;
		a->sin6_family = AF_INET6;
		a->sin6_addr   = in6addr_loopback;
		a->sin6_port   = htons(port);
		salen = sizeof(*a);
	}

	int ok = (bind(fd, (struct sockaddr *)&sa, salen) == 0);
	close(fd);
	return ok;
}

static int find_free_port_in_range(int lo, int hi, int socktype, int family,
				   const int *exclude, int nexclude)
{
	int range = hi - lo + 1;
	int start = lo + ((unsigned)rand() % range);

	for (int i = 0; i < range; i++) {
		int port = lo + ((start - lo + i) % range);

		/* skip already-returned ports */
		int skip = 0;
		for (int j = 0; j < nexclude; j++) {
			if (exclude[j] == port) { skip = 1; break; }
		}
		if (skip)
			continue;

		if (try_bind_port(port, socktype, family))
			return port;
	}
	return -1;
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

	/* seed PRNG for range-mode random start offset */
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

	int found[MAX_PORTS];

	for (int i = 0; i < count; i++) {
		int port;
		if (use_range)
			port = find_free_port_in_range(range_lo, range_hi,
						       socktype, family,
						       found, i);
		else
			port = find_free_port(socktype, family, found, i);

		if (port < 0) {
			fprintf(stderr, "fp: could not find a free port\n");
			return 1;
		}
		found[i] = port;
		printf("%d\n", port);
	}

	return 0;
}
