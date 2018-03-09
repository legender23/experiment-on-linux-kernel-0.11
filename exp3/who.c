/*
 * linux/lib/who.c
 * 
 * by Jupiter23
 */
#include<errno.h>
#include<asm/segment.h>
#include<linux/kernel.h>

char namezone[24] = {0};
int namelen = 0;

int sys_iam(const char *name) {
    int len_limit = 0;
    while (get_fs_byte(name+len_limit) != '\0' && len_limit < 24) {
        len_limit ++;
    }
    if (len_limit > 23) {
        errno = EINVAL;
        return -1;
    }else {
        for (i = 0;i < len_limit; i++) {
            namezone[i] = get_fs_byte(name+i);
        }
        namelen = len_limit;
        return namelen;
    }
}

int sys_whoami(char *name, unsigned int size) {
    int i;
    if (size < namelen) {
        errno = EINVAL;
        return -1;
    }else {
        for (i = 0; i < namelen; i++) {
           put_fs_byte(namezone[i], (name+i));
        }
        return namelen;
    }
}
