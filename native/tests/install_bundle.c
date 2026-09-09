/* Host harness for the exact installer shipped in Kinduino 0.5.2. */
#include "install_slot.h"
#include <stdio.h>
#ifdef __APPLE__
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

/* Darwin denies renaming a non-writable source directory, whereas Linux checks
 * its writable parent. The runtime tightens assets to 0555 before promoting them.
 * Match Linux's rename semantics only in this host harness; shipped code is unchanged. */
int rename(const char* oldpath, const char* newpath) {
    struct stat st;
    int loosened = lstat(oldpath, &st) == 0 && S_ISDIR(st.st_mode) && !(st.st_mode & S_IWUSR);
    if (loosened && chmod(oldpath, (st.st_mode & 0777) | S_IWUSR) != 0) return -1;
    int result = renameat(AT_FDCWD, oldpath, AT_FDCWD, newpath);
    int saved = errno;
    if (loosened && chmod(result == 0 ? newpath : oldpath, st.st_mode & 0777) != 0) return -1;
    errno = saved;
    return result;
}
#endif
int main(int argc, char** argv) {
    if (argc != 4) return 99;
    char id[64] = {0};
    kd_ingest_result result = kd_ingest_install(argv[1], "armv7", argv[2], argv[3], id, sizeof(id));
    printf("Kinduino bundle validation: %d; slot: %s\n", result, id);
    return (int)result;
}
