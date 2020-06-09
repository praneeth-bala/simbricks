/*
 * Copyright 2020 Max Planck Institute for Software Systems
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <poll.h>
#include <unistd.h>

#include <nicsim.h>

#include "internal.h"

#define D2H_ELEN (4096 + 64)
#define D2H_ENUM 1024

#define H2D_ELEN (4096 + 64)
#define H2D_ENUM 1024

#define D2N_ELEN (2048 + 64)
#define D2N_ENUM 1024

#define N2D_ELEN (2048 + 64)
#define N2D_ENUM 1024


static uint8_t *d2h_queue;
static size_t d2h_pos;
static size_t d2h_off; /* offset in shm region */

static uint8_t *h2d_queue;
static size_t h2d_pos;
static size_t h2d_off; /* offset in shm region */

static uint8_t *d2n_queue;
static size_t d2n_pos;
static size_t d2n_off; /* offset in shm region */

static uint8_t *n2d_queue;
static size_t n2d_pos;
static size_t n2d_off; /* offset in shm region */


static int shm_fd = -1;
static int pci_cfd = -1;
static int eth_cfd = -1;

static int accept_pci(struct cosim_pcie_proto_dev_intro *di, int pci_lfd)
{
    if ((pci_cfd = accept(pci_lfd, NULL, NULL)) < 0) {
        return -1;
    }
    close(pci_lfd);
    printf("pci connection accepted\n");

    di->d2h_offset = d2h_off;
    di->d2h_elen = D2H_ELEN;
    di->d2h_nentries = D2H_ENUM;

    di->h2d_offset = h2d_off;
    di->h2d_elen = H2D_ELEN;
    di->h2d_nentries = H2D_ENUM;

    if (uxsocket_send(pci_cfd, di, sizeof(*di), shm_fd)) {
        return -1;
    }
    printf("pci intro sent\n");
    return 0;
}

static int accept_eth(int eth_lfd)
{
    struct cosim_eth_proto_dev_intro di;

    if ((eth_cfd = accept(eth_lfd, NULL, NULL)) < 0) {
        return -1;
    }
    close(eth_lfd);
    printf("eth connection accepted\n");

    memset(&di, 0, sizeof(di));
    di.flags = 0;

    di.d2n_offset = d2n_off;
    di.d2n_elen = D2N_ELEN;
    di.d2n_nentries = D2N_ENUM;

    di.n2d_offset = n2d_off;
    di.n2d_elen = N2D_ELEN;
    di.n2d_nentries = N2D_ENUM;

    if (uxsocket_send(eth_cfd, &di, sizeof(di), shm_fd)) {
        return -1;
    }
    printf("eth intro sent\n");
    return 0;
}

static int accept_conns(struct cosim_pcie_proto_dev_intro *di,
        int pci_lfd, int eth_lfd)
{
    struct pollfd pfds[2];
    int await_pci = pci_lfd != -1;
    int await_eth = eth_lfd != -1;
    int ret;

    while (await_pci || await_eth) {
        if (await_pci && await_eth) {
            /* we're waiting on both fds */
            pfds[0].fd = pci_lfd;
            pfds[1].fd = eth_lfd;
            pfds[0].events = pfds[1].events = POLLIN;
            pfds[0].revents = pfds[1].revents = 0;

            ret = poll(pfds, 2, -1);
            if (ret < 0) {
                perror("poll failed");
                return -1;
            }

            if (pfds[0].revents) {
                if (accept_pci(di, pci_lfd) != 0)
                    return -1;
                await_pci = 0;
            }
            if (pfds[1].revents) {
                if (accept_eth(eth_lfd) != 0)
                    return -1;
                await_eth = 0;
            }
        } else if (await_pci) {
            /* waiting just on pci */
            if (accept_pci(di, pci_lfd) != 0)
                return -1;
            await_pci = 0;
        } else {
            /* waiting just on ethernet */
            if (accept_eth(eth_lfd) != 0)
                return -1;
            await_eth = 0;
        }
    }

    return 0;
}

int nicsim_init(struct cosim_pcie_proto_dev_intro *di,
        const char *pci_socket_path, const char *eth_socket_path,
        const char *shm_path)
{
    int pci_lfd = -1, eth_lfd = -1;
    void *shmptr;

    /* ready in memory queues */
    if ((shm_fd = shm_create(shm_path, 32 * 1024 * 1024, &shmptr)) < 0) {
        return -1;
    }

    d2h_off = 0;
    h2d_off = d2h_off + (uint64_t) D2H_ELEN * D2H_ENUM;
    d2n_off = h2d_off + (uint64_t) H2D_ELEN * H2D_ENUM;
    n2d_off = d2n_off + (uint64_t) D2N_ELEN * D2N_ENUM;

    d2h_queue = (uint8_t *) shmptr + d2h_off;
    h2d_queue = (uint8_t *) shmptr + h2d_off;
    d2n_queue = (uint8_t *) shmptr + d2n_off;
    n2d_queue = (uint8_t *) shmptr + n2d_off;

    d2h_pos = h2d_pos = d2n_pos = n2d_pos = 0;

    /* get listening sockets ready */
    if (pci_socket_path != NULL) {
        if ((pci_lfd = uxsocket_init(pci_socket_path)) < 0) {
            return -1;
        }
    }
    if (eth_socket_path != NULL) {
        if ((eth_lfd = uxsocket_init(eth_socket_path)) < 0) {
            return -1;
        }
    }

    /* accept connection fds */
    if (accept_conns(di, pci_lfd, eth_lfd) != 0) {
        return -1;
    }

    /* receive introductions from other end */
    if (pci_socket_path != NULL) {
        struct cosim_pcie_proto_host_intro hi;
        if (recv(pci_cfd, &hi, sizeof(hi), 0) != sizeof(hi)) {
            return -1;
        }
        printf("pci host info received\n");
    }
    if (eth_socket_path != NULL) {
        struct cosim_eth_proto_net_intro ni;
        if (recv(eth_cfd, &ni, sizeof(ni), 0) != sizeof(ni)) {
            return -1;
        }
        printf("eth net info received\n");
    }

    return 0;
}

void nicsim_cleanup(void)
{
    close(pci_cfd);
    close(eth_cfd);
}

/******************************************************************************/
/* PCI */

volatile union cosim_pcie_proto_h2d *nicif_h2d_poll(void)
{
    volatile union cosim_pcie_proto_h2d *msg =
        (volatile union cosim_pcie_proto_h2d *)
        (h2d_queue + h2d_pos * H2D_ELEN);

    /* message not ready */
    if ((msg->dummy.own_type & COSIM_PCIE_PROTO_H2D_OWN_MASK) !=
            COSIM_PCIE_PROTO_H2D_OWN_DEV)
        return NULL;

    return msg;
}

void nicif_h2d_done(volatile union cosim_pcie_proto_h2d *msg)
{
    msg->dummy.own_type = (msg->dummy.own_type & COSIM_PCIE_PROTO_H2D_MSG_MASK)
        | COSIM_PCIE_PROTO_H2D_OWN_HOST;
}

void nicif_h2d_next(void)
{
    h2d_pos = (h2d_pos + 1) % H2D_ENUM;
}

volatile union cosim_pcie_proto_d2h *nicsim_d2h_alloc(void)
{
    volatile union cosim_pcie_proto_d2h *msg =
        (volatile union cosim_pcie_proto_d2h *)
        (d2h_queue + d2h_pos * D2H_ELEN);

    if ((msg->dummy.own_type & COSIM_PCIE_PROTO_D2H_OWN_MASK) !=
            COSIM_PCIE_PROTO_D2H_OWN_DEV)
    {
        return NULL;
    }

    d2h_pos = (d2h_pos + 1) % D2H_ENUM;
    return msg;
}

/******************************************************************************/
/* Ethernet */

volatile union cosim_eth_proto_n2d *nicif_n2d_poll(void)
{
    volatile union cosim_eth_proto_n2d *msg =
        (volatile union cosim_eth_proto_n2d *)
        (n2d_queue + n2d_pos * N2D_ELEN);

    /* message not ready */
    if ((msg->dummy.own_type & COSIM_ETH_PROTO_N2D_OWN_MASK) !=
            COSIM_ETH_PROTO_N2D_OWN_DEV)
        return NULL;

    return msg;
}

void nicif_n2d_done(volatile union cosim_eth_proto_n2d *msg)
{
    msg->dummy.own_type = (msg->dummy.own_type & COSIM_ETH_PROTO_N2D_MSG_MASK)
        | COSIM_ETH_PROTO_N2D_OWN_NET;
}

void nicif_n2d_next(void)
{
    n2d_pos = (n2d_pos + 1) % N2D_ENUM;
}

volatile union cosim_eth_proto_d2n *nicsim_d2n_alloc(void)
{
    volatile union cosim_eth_proto_d2n *msg =
        (volatile union cosim_eth_proto_d2n *)
        (d2n_queue + d2n_pos * D2N_ELEN);

    if ((msg->dummy.own_type & COSIM_ETH_PROTO_D2N_OWN_MASK) !=
            COSIM_ETH_PROTO_D2N_OWN_DEV)
    {
        return NULL;
    }

    d2n_pos = (d2n_pos + 1) % D2N_ENUM;
    return msg;
}
