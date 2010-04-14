/*
Copyright (c) 2006-2010 Renzo Davoli, Virtual Square Labs, 
University of Bologna. see http://wiki.virtualsquare.org

This license applies to this header file only. The Libvdeplug library
has been released under LGPL.
The source code of the library is part of the VDE project
and is available at http://sourceforge.net/projects/vde/

Permission is hereby granted, free of charge, to any person
obtaining a copy of this software and associated documentation
files (the "Software"), to deal in the Software without
restriction, including without limitation the rights to use,
copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following
conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef _VDEDYNLIB_H
#define _VDEDYNLIB_H
#include <sys/types.h>
#include <dlfcn.h>
#define LIBVDEPLUG_INTERFACE_VERSION 1

struct vdeconn;

typedef struct vdeconn VDECONN;

/* Open a VDE connection.
 * vde_open_options:
 *   port: connect to a specific port of the switch (0=any)
 *   group: change the ownership of the communication port to a specific group
 *        (NULL=no change)
 *   mode: set communication port mode (if 0 standard socket mode applies)
 */
struct vde_open_args {
       int port;
       char *group;
       mode_t mode;
};
       
/* vde_open args:
 *   vde_switch: switch id (path)
 *   descr: description (it will appear in the port description on the switch)
 */
#define vde_open(vde_switch,descr,open_args) \
       vde_open_real((vde_switch),(descr),LIBVDEPLUG_INTERFACE_VERSION,(open_args))

struct vdepluglib {
       void *dl_handle;
       VDECONN * (*vde_open_real)(const char *vde_switch,char *descr,int interface_version, struct vde_open_args *open_args);
       size_t (* vde_recv)(VDECONN *conn,void *buf,size_t len,int flags);
       size_t (* vde_send)(VDECONN *conn,const void *buf,size_t len,int flags);
       int (* vde_datafd)(VDECONN *conn);
       int (* vde_ctlfd)(VDECONN *conn);
       int (* vde_close)(VDECONN *conn);
};

typedef VDECONN * (* VDE_OPEN_REAL_T)(const char *vde_switch,char *descr,int interface_version, struct vde_open_args *open_args);
typedef size_t (* VDE_RECV_T)(VDECONN *conn,void *buf,size_t len,int flags);
typedef size_t (* VDE_SEND_T)(VDECONN *conn,const void *buf,size_t len,int flags);
typedef int (* VDE_INT_FUN)(VDECONN *conn);
#define libvdeplug_dynopen(x) do { \
       (x).dl_handle=dlopen("libvdeplug.so.2",RTLD_NOW); \
       if ((x).dl_handle) { \
               (x).vde_open_real=(VDE_OPEN_REAL_T) dlsym((x).dl_handle,"vde_open_real"); \
               (x).vde_recv=(VDE_RECV_T) dlsym((x).dl_handle,"vde_recv"); \
               (x).vde_send=(VDE_SEND_T) dlsym((x).dl_handle,"vde_send"); \
               (x).vde_datafd=(VDE_INT_FUN) dlsym((x).dl_handle,"vde_datafd"); \
               (x).vde_ctlfd=(VDE_INT_FUN) dlsym((x).dl_handle,"vde_ctlfd"); \
               (x).vde_close=(VDE_INT_FUN) dlsym((x).dl_handle,"vde_close"); \
               } else { \
               (x).vde_open_real=NULL; \
               (x).vde_send= NULL; \
               (x).vde_recv= NULL; \
               (x).vde_datafd= (x).vde_ctlfd= (x).vde_close= NULL; \
               }\
               } while (0)

#define libvdeplug_dynclose(x) do { \
               dlclose((x).dl_handle); \
               } while (0)

#endif
