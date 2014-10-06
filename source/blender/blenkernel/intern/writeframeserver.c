/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Copyright (c) 2006 Peter Schlaile
 *
 * Contributor(s):
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/writeframeserver.c
 *  \ingroup bke
 *
 * Frameserver
 * Makes Blender accessible from TMPGenc directly using VFAPI (you can
 * use firefox too ;-)
 */

#ifdef WITH_FRAMESERVER

#include <string.h>
#include <stdio.h>
#include <time.h>

#if defined(_WIN32)
#include <winsock2.h>
#include <windows.h>
#include <winbase.h>
#include <direct.h>
#else
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/un.h>
#include <fcntl.h>
#endif

#include <stdlib.h>

#include "DNA_userdef_types.h"

#include "BLI_utildefines.h"

#include "BKE_writeframeserver.h"
#include "BKE_global.h"
#include "BKE_report.h"

#include "DNA_scene_types.h"

#define SOCK_CLOSE(s) closesocket(s); s = -1

#include <stdio.h>


static int sock;
static int connsock;
static int write_ppm;
static int render_width;
static int render_height;

/* Now that the frameserver can render all the ppms at once
 * this flag will say if we need to render just one frame or
 * all at once.
 * The default value it's true since this is the original
 * behaviour. */
static bool only_one_frame = true;
/* current frame for processing. This is only needed if
 * only_one_frame is set to false */
static int currframe;
/* Set currframe to the next frame. */
static int next_frame(RenderData *rd);
/* Set the parameters from the request into a string that is
 * visible from python. */
static int set_changes(char *req);

#if defined(_WIN32)
static int startup_socket_system(void)
{
    WSADATA wsa;
    return (WSAStartup(MAKEWORD(2, 0), &wsa) == 0);
}

static void shutdown_socket_system(void)
{
    WSACleanup();
}
static int select_was_interrupted_by_signal(void)
{
    return (WSAGetLastError() == WSAEINTR);
}
#else
static int startup_socket_system(void)
{
    return 1;
}

static void shutdown_socket_system(void)
{
}

static int select_was_interrupted_by_signal(void)
{
    return (errno == EINTR);
}

static int closesocket(int fd)
{
    return close(fd);
}
#endif

/* Server functions, this will set up the sockets and shut it down on
 * explicits calls. */
int BKE_server_start(struct ReportList *reports)
{
    struct sockaddr_in master_addr;
    int arg = 1;
    char buf[64];

    if (sock != 0) {
        return 1;
    }

    if (!startup_socket_system()) {
        if (reports != NULL)
            BKE_report(reports, RPT_ERROR, "Cannot startup socket system");
        return 0;
    }

    /* Socket creation */
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        shutdown_socket_system();
        if (reports != NULL)
            BKE_report(reports, RPT_ERROR, "Cannot open socket");
        return 0;
    }

    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *) &arg, sizeof(arg));

    master_addr.sin_family = AF_INET;
    master_addr.sin_port = htons(U.frameserverport);
    master_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, (struct sockaddr *)&master_addr, sizeof(master_addr)) < 0) {
        shutdown_socket_system();
        if (reports != NULL)
            BKE_report(reports, RPT_ERROR, "Cannot bind to socket");
        return 0;
    }

    if (listen(sock, SOMAXCONN) < 0) {
        shutdown_socket_system();
        if (reports != NULL)
            BKE_report(reports, RPT_ERROR, "Cannot establish listen backlog");
        return 0;
    }
    if (connsock != -1) {
        SOCK_CLOSE(connsock);
    }

    return 1;
}

void BKE_server_stop(void) {
    if (connsock != -1) {
        SOCK_CLOSE(connsock);
    }
    closesocket(sock);
    sock = 0;
    shutdown_socket_system();
}

int BKE_frameserver_start(struct Scene *scene, RenderData *rd, int rectx, int recty, ReportList *reports)
{
    (void)scene; /* unused */

    if (!BKE_server_start(reports)) {
        return 0;
    }

    render_width = rectx;
    render_height = recty;

    /* set to the first frame of the render! */
    currframe = rd->sfra;

    return 1;
}

static char index_page[] =
"HTTP/1.1 200 OK\r\n"
"Content-Type: text/html\r\n"
"\r\n"
"<html><head><title>Blender Frameserver</title></head>\n"
"<body><pre>\n"
"<H2>Blender Frameserver</H2>\n"
"<A HREF=info.txt>Render Info</A><br>\n"
"<A HREF=close.txt>Stop Rendering</A><br>\n"
"\n"
"Images can be found here\n"
"\n"
"images/ppm/%d.ppm\n"
"\n"
"</pre></body></html>\n";

static char good_bye[] =
"HTTP/1.1 200 OK\r\n"
"Content-Type: text/html\r\n"
"\r\n"
"<html><head><title>Blender Frameserver</title></head>\n"
"<body><pre>\n"
"Render stopped. Goodbye</pre></body></html>";

static int safe_write(char *s, int tosend)
{
    int total = tosend;
    do {
        int got = send(connsock, s, tosend, 0);
        if (got < 0) {
            return got;
        }
        tosend -= got;
        s += got;
    } while (tosend > 0);

    return total;
}

static int safe_puts(char *s)
{
    return safe_write(s, strlen(s));
}

static int next_frame(RenderData *rd) {
    int res = currframe;
    currframe += 1;
    if (currframe > rd->efra) {
        G.is_break = TRUE; /* Abort render */
        return -1;
    }
    return res;
}

static int handle_request(RenderData *rd, char *req)
{
    char *p;
    char *path;
    int pathlen;

    /* first of all */
    if (req[0] == '\0') {
        return next_frame(rd);
    }

    if (memcmp(req, "GET ", 4) != 0) {
        return -1;
    }
       
    p = req + 4;
    path = p;

    while (*p != ' ' && *p) p++;

    *p = 0;

    if (strcmp(path, "/index.html") == 0 || strcmp(path, "/") == 0) {
        safe_puts(index_page);
        return -1;
    }

    write_ppm = 0;
    pathlen = strlen(path);

    if (strcmp(path, "/info.txt") == 0) {
        char buf[4096];

        sprintf(buf,
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/html\r\n"
                "\r\n"
                "start %d\n"
                "end %d\n"
                "width %d\n"
                "height %d\n"
                "rate %d\n"
                "ratescale %d\n",
                rd->sfra,
                rd->efra,
                render_width,
                render_height,
                rd->frs_sec,
                1
                );

        safe_puts(buf);
        return -1;
    }

    if (pathlen > 12 && memcmp(path, "/new_render?", 12) == 0) {
        char buf[4096];
        if (set_changes(path+12) != 0) {
            sprintf(buf,
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/html\r\n"
                "\r\n"
                "Error :( \n"
            );
        } else {
            sprintf(buf,
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/html\r\n"
                "\r\n"
                "ok :)\n"
            );
        }
        safe_puts(buf);
        G.is_break = TRUE; /* Abort render */
        return -1;
    }

    if (pathlen > 12 && memcmp(path, "/images/ppm/", 12) == 0) {
        write_ppm = 1;
        return atoi(path + 12);
    }

    if ((pathlen > 15 && memcmp(path, "/images/ppm/all", 15) == 0)) {
        only_one_frame = false;
        write_ppm = 1;
        return next_frame(rd);
    }

    if (strcmp(path, "/close.txt") == 0) {
        safe_puts(good_bye);
        G.is_break = TRUE; /* Abort render */
        return -1;
    }

    return -1;
}

int BKE_frameserver_loop(RenderData *rd, ReportList *reports)
{
    fd_set readfds;
    struct timeval tv;
    struct sockaddr_in addr;
    char buf[4096];
    int len, rval = 0;
#ifdef FREE_WINDOWS
    int socklen;
#else
    unsigned int socklen;
#endif
    int retval = -1;
    bool need_recv = (connsock == -1);

    socklen = sizeof(addr);

    if (connsock == -1) {
        if ((connsock = accept(sock, (struct sockaddr *)&addr, &socklen)) < 0) {
            BKE_report(reports, RPT_ERROR, "accept fail");
            return retval;
        }
    }

    if (need_recv) {
        FD_ZERO(&readfds);
        FD_SET(connsock, &readfds);

        /* wait for Client */
        for (;;) {
            /* give 10 seconds for telnet testing... */
            tv.tv_sec = 10;
            tv.tv_usec = 0;

            rval = select(connsock + 1, &readfds, NULL, NULL, &tv);
            if (rval > 0) {
                break;
            }
            else if (rval == 0) {
                return retval;
            }
            else if (rval < 0) {
                if (!select_was_interrupted_by_signal()) {
                    return retval;
                }
            }
        }

        len = recv(connsock, buf, sizeof(buf) -1, 0);
        if (len < 0) {
            /* error receiving */
            return 0;
        }
        buf[len] = 0;
    } else {
        buf[0] = '\0';
    }

    retval = handle_request(rd, buf);

    if (retval == -1 && connsock != -1) {
        SOCK_CLOSE(connsock);
        retval = -1;
    }

    return retval;
}

static void serve_ppm(int *pixels, int rectx, int recty)
{
    unsigned char *rendered_frame;
    unsigned char *row = (unsigned char *) malloc(render_width * 3);
    int y;
    char header[1024];
    int sended;

    /* Only puts the http header when it's needed. */
    if (currframe == 0 || only_one_frame) {
        sprintf(header,
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: image/ppm\r\n"
                "Connection: close\r\n"
                "\r\n"
                );
        sended = safe_puts(header);
        if (sended < 0) {
            SOCK_CLOSE(connsock);
            G.is_break = TRUE; /* Abort render */
            return;
        }
    }
    /* ppm header */
    sprintf(header,
            "P6\n"
            "# Creator: blender frameserver v0.0.1\n"
            "%d %d\n"
            "255\n",
            rectx, recty);
    sended = safe_puts(header);
    if (sended < 0) {
        SOCK_CLOSE(connsock);
        G.is_break = TRUE; /* Abort render */
        return;
    }

    rendered_frame = (unsigned char *)pixels;

    for (y = recty - 1; y >= 0; y--) {
        unsigned char *target = row;
        unsigned char *src = rendered_frame + rectx * 4 * y;
        unsigned char *end = src + rectx * 4;
        while (src != end) {
            target[2] = src[2];
            target[1] = src[1];
            target[0] = src[0];
            
            target += 3;
            src += 4;
        }
        sended = safe_write((char *)row, 3 * rectx);
        if (sended < 0) {
            SOCK_CLOSE(connsock);
            G.is_break = TRUE; /* Abort render */
            return;
        }
    }
    free(row);
    if (only_one_frame) {
        closesocket(connsock);
        connsock = -1;
    }
    /* Done with this frame */
}

int BKE_frameserver_append(RenderData *UNUSED(rd), int UNUSED(start_frame), int frame, int *pixels,
                           int rectx, int recty, ReportList *UNUSED(reports))
{
    if (write_ppm) {
        serve_ppm(pixels, rectx, recty);
    }
    if (only_one_frame && connsock != -1) {
        closesocket(connsock);
        connsock = -1;
    }

    return 1;
}

void BKE_frameserver_end(void)
{
    /* Only close the client socket. the master server needs to
     * be closed in a explicit way (see BKE_server_stop) */
    if (connsock != -1) {
        SOCK_CLOSE(connsock);
    }
}

/* Python module helpers */
static char _last_request[REQ_MAX_LEN] = "";

static int set_changes(char *req)
{
    time_t timer;
    char time_buffer[25];
    struct tm *tm_info;
    time(&timer);
    tm_info = localtime(&timer);
    strftime(time_buffer, 25, "%Y:%m:%d %H:%M:%S", tm_info);
    printf("[%s] New request with params: %s\n", time_buffer, req);
    strncpy(_last_request, req, REQ_MAX_LEN);
    _last_request[REQ_MAX_LEN-1] = '\0';
    return 0;
}

void BKE_frameserver_get_changes(char *dest)
{
    strncpy(dest, _last_request, REQ_MAX_LEN);
}

void BKE_frameserver_only_one_frame(int param)
{
    only_one_frame = (bool) param;
}

#endif /* WITH_FRAMESERVER */
