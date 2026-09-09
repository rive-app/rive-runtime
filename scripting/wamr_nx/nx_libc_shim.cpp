/*
 * Copyright 2026 Rive
 */

// Nintendo's musl derived libc declares these but nnSdk exports no
// definitions, so WAMR's shared posix layer fails to link. The directory
// relative calls back the WASI filesystem, which this platform has no
// equivalent for, so they report ENOSYS; the vectored reads and writes are
// real because they are just loops over the byte oriented calls.

#include <errno.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>

extern "C"
{

    static int notSupported()
    {
        errno = ENOSYS;
        return -1;
    }

    int openat(int, const char*, int, ...) { return notSupported(); }
    int fstatat(int, const char*, struct stat*, int) { return notSupported(); }
    int mkdirat(int, const char*, mode_t) { return notSupported(); }
    int renameat(int, const char*, int, const char*) { return notSupported(); }
    int unlinkat(int, const char*, int) { return notSupported(); }

    // Both report success by errno convention rather than -1, and neither has
    // a meaning here: one preallocates, one advises.
    int posix_fallocate(int, off_t, off_t) { return ENOSYS; }
    int posix_fadvise(int, off_t, off_t, int) { return 0; }

    // Named semaphores. nnSdk has the unnamed sem_* family only.
    sem_t* sem_open(const char*, int, ...)
    {
        notSupported();
        return SEM_FAILED;
    }
    int sem_close(sem_t*) { return notSupported(); }
    int sem_unlink(const char*) { return notSupported(); }

    static ssize_t vectored(int fd,
                            const struct iovec* iov,
                            int count,
                            off_t offset,
                            bool positional,
                            bool writing)
    {
        ssize_t total = 0;
        for (int i = 0; i < count; ++i)
        {
            if (iov[i].iov_len == 0)
            {
                continue;
            }
            ssize_t n;
            if (writing)
            {
                n = positional ? pwrite(fd,
                                        iov[i].iov_base,
                                        iov[i].iov_len,
                                        offset + total)
                               : write(fd, iov[i].iov_base, iov[i].iov_len);
            }
            else
            {
                n = positional ? pread(fd,
                                       iov[i].iov_base,
                                       iov[i].iov_len,
                                       offset + total)
                               : read(fd, iov[i].iov_base, iov[i].iov_len);
            }
            if (n < 0)
            {
                return total > 0 ? total : -1;
            }
            total += n;
            // A short transfer ends the whole call, same as the real thing.
            if ((size_t)n < iov[i].iov_len)
            {
                break;
            }
        }
        return total;
    }

    ssize_t readv(int fd, const struct iovec* iov, int count)
    {
        return vectored(fd, iov, count, 0, false, false);
    }

    ssize_t writev(int fd, const struct iovec* iov, int count)
    {
        return vectored(fd, iov, count, 0, false, true);
    }

    ssize_t preadv(int fd, const struct iovec* iov, int count, off_t offset)
    {
        return vectored(fd, iov, count, offset, true, false);
    }

    ssize_t pwritev(int fd, const struct iovec* iov, int count, off_t offset)
    {
        return vectored(fd, iov, count, offset, true, true);
    }
}
