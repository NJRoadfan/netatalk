#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/param.h>

#include <atalk/fce_api.h>
#include <atalk/util.h>

#define MAXBUFLEN 1024

static char *fce_ev_names[] = {
    "",
    "FCE_FILE_MODIFY",
    "FCE_FILE_DELETE",
    "FCE_DIR_DELETE",
    "FCE_FILE_CREATE",
    "FCE_DIR_CREATE",
    "FCE_FILE_MOVE",
    "FCE_DIR_MOVE",
    "FCE_LOGIN",
    "FCE_LOGOUT"
};

static int unpack_fce_packet(unsigned char *buf, size_t buflen,
                             struct fce_packet *packet)
{
    const unsigned char *p = buf;
    const unsigned char *end = buf + buflen;

    /* Check if we have enough data for the magic number */
    if (p + sizeof(packet->fcep_magic) > end) {
        return -1;
    }

    memcpy(&packet->fcep_magic[0], p, sizeof(packet->fcep_magic));
    p += sizeof(packet->fcep_magic);

    /* Check if we have enough data for the version */
    if (p + 1 > end) {
        return -1;
    }

    packet->fcep_version = *p++;

    if (packet->fcep_version > 1) {
        /* Check if we have enough data for options */
        if (p + 1 > end) {
            return -1;
        }

        packet->fcep_options = *p++;
    } else {
        packet->fcep_options = 0;
    }

    /* Check if we have enough data for the event */
    if (p + 1 > end) {
        return -1;
    }

    packet->fcep_event = *p++;

    if (packet->fcep_version > 1) {
        /* padding */
        if (p + 1 > end) {
            return -1;
        }

        p++;

        /* reserved */
        if (p + 8 > end) {
            return -1;
        }

        p += 8;
    }

    /* Check if we have enough data for event_id */
    if (p + sizeof(packet->fcep_event_id) > end) {
        return -1;
    }

    memcpy(&packet->fcep_event_id, p, sizeof(packet->fcep_event_id));
    p += sizeof(packet->fcep_event_id);
    packet->fcep_event_id = ntohl(packet->fcep_event_id);

    if (packet->fcep_options & FCE_EV_INFO_PID) {
        /* Check if we have enough data for pid */
        if (p + sizeof(packet->fcep_pid) > end) {
            return -1;
        }

        memcpy(&packet->fcep_pid, p, sizeof(packet->fcep_pid));
        packet->fcep_pid = hton64(packet->fcep_pid);
        p += sizeof(packet->fcep_pid);
    }

    if (packet->fcep_options & FCE_EV_INFO_USER) {
        /* Check if we have enough data for userlen */
        if (p + sizeof(packet->fcep_userlen) > end) {
            return -1;
        }

        memcpy(&packet->fcep_userlen, p, sizeof(packet->fcep_userlen));
        packet->fcep_userlen = ntohs(packet->fcep_userlen);
        p += sizeof(packet->fcep_userlen);

        /* Validate userlen to prevent overflow */
        if (packet->fcep_userlen >= sizeof(packet->fcep_user)) {
            return -1;
        }

        /* Check if we have enough data for user */
        if (p + packet->fcep_userlen > end) {
            return -1;
        }

        memcpy(&packet->fcep_user[0], p, packet->fcep_userlen);
        packet->fcep_user[packet->fcep_userlen] = 0; /* 0 terminate strings */
        p += packet->fcep_userlen;
    }

    /* Check if we have enough data for pathlen1 */
    if (p + sizeof(packet->fcep_pathlen1) > end) {
        return -1;
    }

    memcpy(&packet->fcep_pathlen1, p, sizeof(packet->fcep_pathlen1));
    p += sizeof(packet->fcep_pathlen1);
    packet->fcep_pathlen1 = ntohs(packet->fcep_pathlen1);

    /* Validate pathlen1 to prevent overflow */
    if (packet->fcep_pathlen1 >= sizeof(packet->fcep_path1)) {
        return -1;
    }

    /* Check if we have enough data for path1 */
    if (p + packet->fcep_pathlen1 > end) {
        return -1;
    }

    memcpy(&packet->fcep_path1[0], p, packet->fcep_pathlen1);
    packet->fcep_path1[packet->fcep_pathlen1] = 0; /* 0 terminate strings */
    p += packet->fcep_pathlen1;

    if (packet->fcep_options & FCE_EV_INFO_SRCPATH) {
        /* Check if we have enough data for pathlen2 */
        if (p + sizeof(packet->fcep_pathlen2) > end) {
            return -1;
        }

        memcpy(&packet->fcep_pathlen2, p, sizeof(packet->fcep_pathlen2));
        p += sizeof(packet->fcep_pathlen2);
        packet->fcep_pathlen2 = ntohs(packet->fcep_pathlen2);

        /* Validate pathlen2 to prevent overflow */
        if (packet->fcep_pathlen2 >= sizeof(packet->fcep_path2)) {
            return -1;
        }

        /* Check if we have enough data for path2 */
        if (p + packet->fcep_pathlen2 > end) {
            return -1;
        }

        memcpy(&packet->fcep_path2[0], p, packet->fcep_pathlen2);
        packet->fcep_path2[packet->fcep_pathlen2] = 0; /* 0 terminate strings */
    }

    return 0;
}

int main(int argc, char **argv)
{
    int sockfd;
    int rv;
    int c;
    struct addrinfo hints;
    struct addrinfo *servinfo;
    struct addrinfo *p;
    ssize_t numbytes;
    struct sockaddr_storage their_addr;
    unsigned char buf[MAXBUFLEN];
    socklen_t addr_len;
    char *host = strdup("localhost");
    char *port = strdup(FCE_DEFAULT_PORT_STRING);

    while ((c = getopt(argc, argv, "h:p:")) != -1) {
        switch (c) {
        case 'h':
            if (host) {
                free((void *)host);
            }

            host = strdup(optarg);
            break;

        case 'p':
            if (port) {
                free((void *)port);
            }

            port = strdup(optarg);
            break;

        case '?':
        default:
            fprintf(stderr, "Usage: %s [-h hostname] [-p port]\n", argv[0]);

            if (host) {
                free((void *)host);
            }

            if (port) {
                free((void *)port);
            }

            return 1;
        }
    }

    memset(&hints, 0, sizeof(hints));
    /* set to AF_INET to force IPv4 */
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    if ((rv = getaddrinfo(host, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        free((void *)host);
        free((void *)port);
        return 1;
    }

    /* loop through all the results and bind to the first we can */
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1) {
            perror("listener: socket");
            continue;
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("listener: bind");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "listener: failed to bind socket\n");
        free((void *)host);
        free((void *)port);
        return 2;
    }

    printf("Listening for Netatalk FCE datagrams on %s:%s...\n", host, port);
    freeaddrinfo(servinfo);
    free((void *)host);
    free((void *)port);
    addr_len = sizeof(their_addr);
    struct fce_packet packet;

    while (1) {
        if ((numbytes = recvfrom(sockfd,
                                 buf,
                                 MAXBUFLEN - 1,
                                 0,
                                 (struct sockaddr *)&their_addr,
                                 &addr_len)) == -1) {
            perror("recvfrom");
            exit(1);
        }

        if (unpack_fce_packet((unsigned char *)buf, numbytes, &packet) < 0) {
            fprintf(stderr, "Invalid FCE packet received\n");
            continue;
        }

        if (memcmp(packet.fcep_magic, FCE_PACKET_MAGIC,
                   sizeof(packet.fcep_magic)) == 0) {
            switch (packet.fcep_event) {
            case FCE_CONN_START:
                printf("FCE Start\n");
                break;

            case FCE_CONN_BROKEN:
                printf("Broken FCE connection\n");
                break;

            default:
                printf("ID: %" PRIu32 ", Event: %s", packet.fcep_event_id,
                       fce_ev_names[packet.fcep_event]);

                if (packet.fcep_options & FCE_EV_INFO_PID) {
                    printf(", pid: %" PRId64, packet.fcep_pid);
                }

                if (packet.fcep_options & FCE_EV_INFO_USER) {
                    printf(", user: %s", packet.fcep_user);
                }

                if (packet.fcep_options & FCE_EV_INFO_SRCPATH) {
                    printf(", source: %s", packet.fcep_path2);
                }

                printf(", Path: %s\n", packet.fcep_path1);
                break;
            }
        }
    }

    close(sockfd);
    return 0;
}
