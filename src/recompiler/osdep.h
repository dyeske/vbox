#ifndef QEMU_OSDEP_H
#define QEMU_OSDEP_H

#if defined(VBOX)

#include <iprt/alloc.h>
#include <iprt/string.h>

#define qemu_vsnprintf(pszBuf, cchBuf, pszFormat, args) \
                            RTStrPrintfV((pszBuf), (cchBuf), (pszFormat), (args))
#define qemu_vprintf(pszFormat, args) \
                            RTLogPrintfV((pszFormat), (args))
#define qemu_printf         RTLogPrintf
#define qemu_malloc(cb)     RTMemAlloc(cb)
#define qemu_mallocz(cb)    RTMemAllocZ(cb)
#define qemu_free(pv)       RTMemFree(pv)
#define qemu_strdup(psz)    RTStrDup(psz)


#else /* !VBOX */

#include <stdarg.h>

int qemu_vsnprintf(char *buf, int buflen, const char *fmt, va_list args);
void qemu_vprintf(const char *fmt, va_list ap);
void qemu_printf(const char *fmt, ...);

void *qemu_malloc(size_t size);
void *qemu_mallocz(size_t size);
void qemu_free(void *ptr);
char *qemu_strdup(const char *str);

void *get_mmap_addr(unsigned long size);

/* specific kludges for OS compatibility (should be moved elsewhere) */
#if defined(__i386__) && !defined(CONFIG_SOFTMMU) && !defined(CONFIG_USER_ONLY)

/* disabled pthread version of longjmp which prevent us from using an
   alternative signal stack */
extern void __longjmp(jmp_buf env, int val);
#define longjmp __longjmp

#include <signal.h>

/* NOTE: it works only because the glibc sigset_t is >= kernel sigset_t */
struct qemu_sigaction {
    union {
        void (*_sa_handler)(int);
        void (*_sa_sigaction)(int, struct siginfo *, void *);
    } _u;
    unsigned long sa_flags;
    void (*sa_restorer)(void);
    sigset_t sa_mask;		/* mask last for extensibility */
};

int qemu_sigaction(int signum, const struct qemu_sigaction *act, 
                   struct qemu_sigaction *oldact);

#undef sigaction
#undef sa_handler
#undef sa_sigaction
#define sigaction qemu_sigaction
#define sa_handler	_u._sa_handler
#define sa_sigaction	_u._sa_sigaction

#endif

#endif /* !VBOX */

#endif
