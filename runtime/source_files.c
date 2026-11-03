/* SAF paths are lookup tokens for granted descriptors, never reopenable paths.
 * dup preserves the Android grant; pread gives each new open its own offset. */
#define _GNU_SOURCE 1
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
extern int __real_open(const char*,int,...);
extern FILE *__real_fopen(const char*,const char*);
extern int __real_unlink(const char*);
extern int __real_rename(const char*,const char*);
extern ssize_t __real_read(int,void*,size_t);
extern int __real_close(int);
extern off_t __real_lseek(int,off_t,int);
extern off64_t __real_lseek64(int,off64_t,int);
extern int __real_dup(int);
extern int __real_stat(const char*,struct stat*);
extern int __real_stat64(const char*,struct stat64*);
extern int __real_access(const char*,int);
typedef struct position { off64_t offset; unsigned refs; } position;
typedef struct source_handle { int fd; position *pos; struct source_handle *next; } source_handle;
static source_handle *handles;
static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
/* Java owns these original FDs until session teardown. -1 is a normal path. */
static int source_fd(const char *path){
    char target[80];ssize_t n=path?readlink(path,target,sizeof(target)-1):-1;
    if(n<=14)return -1;target[n]=0;
    if(strncmp(target,"/proc/self/fd/",14))return -1;
    unsigned value=0;
    for(const char *p=target+14;*p;p++){
        if(*p<'0'||*p>'9'||value>(INT_MAX-(unsigned)(*p-'0'))/10)return -1;
        value=value*10+(*p-'0');
    }
    return (int)value;
}
static source_handle *find(int fd){for(source_handle *h=handles;h;h=h->next)if(h->fd==fd)return h;return NULL;}
static int attach(int fd,position *shared){
    source_handle *h=calloc(1,sizeof(*h));position *p=shared?shared:calloc(1,sizeof(*p));
    if(!h||!p){free(h);if(!shared)free(p);errno=ENOMEM;return -1;}
    h->fd=fd;h->pos=p;p->refs++;h->next=handles;handles=h;return fd;
}
static int source_open(const char *path,int flags,mode_t mode){
    int original=source_fd(path);if(original<0)return __real_open(path,flags,mode);
    if((flags&O_ACCMODE)!=O_RDONLY||(flags&(O_CREAT|O_TRUNC|O_APPEND))){errno=EACCES;return -1;}
    pthread_mutex_lock(&lock);
    int fd=fcntl(original,(flags&O_CLOEXEC)?F_DUPFD_CLOEXEC:F_DUPFD,0);
    if(fd>=0&&attach(fd,NULL)<0){__real_close(fd);fd=-1;}
    pthread_mutex_unlock(&lock);return fd;
}
int __wrap_open(const char *path,int flags,...){
    mode_t mode=0;if(flags&O_CREAT){va_list args;va_start(args,flags);mode=(mode_t)va_arg(args,int);va_end(args);}
    return source_open(path,flags,mode);
}
int __wrap_open64(const char *path,int flags,...){
    mode_t mode=0;if(flags&O_CREAT){va_list args;va_start(args,flags);mode=(mode_t)va_arg(args,int);va_end(args);}
    return source_open(path,flags,mode);
}
ssize_t __wrap_read(int fd,void *data,size_t bytes){
    pthread_mutex_lock(&lock);source_handle *h=find(fd);
    if(!h){pthread_mutex_unlock(&lock);return __real_read(fd,data,bytes);}
    ssize_t n=pread64(fd,data,bytes,h->pos->offset);if(n>0)h->pos->offset+=n;
    pthread_mutex_unlock(&lock);return n;
}
static off64_t seek_source(source_handle *h,off64_t offset,int whence,off64_t limit){
    off64_t base=0;
    if(whence==SEEK_CUR)base=h->pos->offset;
    else if(whence==SEEK_END){struct stat st;if(fstat(h->fd,&st))return -1;base=st.st_size;}
    else if(whence!=SEEK_SET){errno=EINVAL;return -1;}
    off64_t result;
    if(__builtin_add_overflow(base,offset,&result)||result>limit){errno=EOVERFLOW;return -1;}
    if(result<0){errno=EINVAL;return -1;}
    h->pos->offset=result;return result;
}
off64_t __wrap_lseek64(int fd,off64_t offset,int whence){
    pthread_mutex_lock(&lock);source_handle *h=find(fd);
    if(!h){pthread_mutex_unlock(&lock);return __real_lseek64(fd,offset,whence);}
    off64_t result=seek_source(h,offset,whence,INT64_MAX);pthread_mutex_unlock(&lock);return result;
}
off_t __wrap_lseek(int fd,off_t offset,int whence){
    pthread_mutex_lock(&lock);source_handle *h=find(fd);
    if(!h){pthread_mutex_unlock(&lock);return __real_lseek(fd,offset,whence);}
    off64_t result=seek_source(h,offset,whence,sizeof(off_t)==4?INT32_MAX:INT64_MAX);
    pthread_mutex_unlock(&lock);return (off_t)result;
}
int __wrap_close(int fd){
    pthread_mutex_lock(&lock);source_handle **entry=&handles;
    while(*entry&&(*entry)->fd!=fd)entry=&(*entry)->next;
    if(*entry){source_handle *h=*entry;*entry=h->next;if(!--h->pos->refs)free(h->pos);free(h);}
    int result=__real_close(fd);pthread_mutex_unlock(&lock);return result;
}
int __wrap_dup(int original){
    pthread_mutex_lock(&lock);source_handle *h=find(original);int fd=__real_dup(original);
    if(fd>=0&&h&&attach(fd,h->pos)<0){__real_close(fd);fd=-1;}
    pthread_mutex_unlock(&lock);return fd;
}
/* libc FILE internals bypass linker wrappers: use explicit stdio callbacks. */
static int stream_read(void *cookie,char *data,int bytes){return (int)__wrap_read(*(int*)cookie,data,(size_t)bytes);}
static fpos64_t stream_seek(void *cookie,fpos64_t offset,int whence){return __wrap_lseek64(*(int*)cookie,offset,whence);}
static int stream_close(void *cookie){int result=__wrap_close(*(int*)cookie);free(cookie);return result;}
FILE *__wrap_fopen(const char *path,const char *mode){
    if(source_fd(path)<0)return __real_fopen(path,mode);
    if(!mode||mode[0]!='r'||strchr(mode,'+')){errno=EACCES;return NULL;}
    int *cookie=malloc(sizeof(*cookie));if(!cookie){errno=ENOMEM;return NULL;}
    *cookie=source_open(path,O_RDONLY|(strchr(mode,'e')?O_CLOEXEC:0),0);
    if(*cookie<0){free(cookie);return NULL;}
    FILE *stream=funopen64(cookie,stream_read,NULL,stream_seek,stream_close);
    if(!stream)stream_close(cookie);return stream;
}
FILE *__wrap_fopen64(const char *path,const char *mode){return __wrap_fopen(path,mode);}
int __wrap_stat(const char *path,struct stat *st){int fd=source_fd(path);return fd<0?__real_stat(path,st):fstat(fd,st);}
int __wrap_stat64(const char *path,struct stat64 *st){int fd=source_fd(path);return fd<0?__real_stat64(path,st):fstat64(fd,st);}
int __wrap_access(const char *path,int mode){
    int fd=source_fd(path);if(fd<0)return __real_access(path,mode);
    if(mode&~R_OK){errno=EACCES;return -1;}struct stat st;return fstat(fd,&st);
}
int __wrap_unlink(const char *path){if(source_fd(path)>=0){errno=EACCES;return -1;}return __real_unlink(path);}
int __wrap_rename(const char *from,const char *to){if(source_fd(from)>=0||source_fd(to)>=0){errno=EACCES;return -1;}return __real_rename(from,to);}
int tf_source_files_selftest(void){
    char original[]="tf-source-XXXXXX",link[128]={0},target[80],saved[]="tf-private-XXXXXX";
    int fd=mkstemp(original);if(fd<0)return 210;
    int result=0,a=-1,b=-1,c=-1,private_created=0;FILE *stream=NULL;
    const char expected[]="original-assets";char data[32]={0};
    if(write(fd,expected,sizeof(expected))!=sizeof(expected)){result=211;goto done;}
    snprintf(link,sizeof(link),"%s.link",original);snprintf(target,sizeof(target),"/proc/self/fd/%d",fd);
    if(symlink(target,link)||fchmod(fd,0000)){result=212;goto done;}
    /* Reproduce the real failure: retained FD works, reopening it is denied. */
    int denied=__real_open(link,O_RDONLY);
    if(denied>=0){__real_close(denied);result=221;goto done;}
    if(errno!=EACCES){result=222;goto done;}
    a=__wrap_open(link,O_RDONLY);b=__wrap_open(link,O_RDONLY);
    if(a<0||b<0||__wrap_read(a,data,3)!=3||__wrap_read(b,data,sizeof(expected))!=sizeof(expected)||memcmp(data,expected,sizeof(expected))){result=213;goto done;}
    if(__wrap_lseek(a,4,SEEK_SET)!=4||__wrap_lseek(b,0,SEEK_CUR)!=sizeof(expected)){result=214;goto done;}
    c=__wrap_dup(a);
    if(c<0||__wrap_read(c,data,1)!=1||data[0]!=expected[4]||__wrap_lseek(a,0,SEEK_CUR)!=5){result=223;goto done;}
    if(__wrap_lseek64(a,INT64_C(0x100000007),SEEK_SET)!=INT64_C(0x100000007)||__wrap_read(a,data,1)!=0||
       __wrap_lseek64(a,-1,SEEK_SET)!=-1||__wrap_lseek64(a,0,SEEK_CUR)!=INT64_C(0x100000007)){result=224;goto done;}
    struct stat st;if(__wrap_stat(link,&st)||st.st_size!=sizeof(expected)){result=225;goto done;}
    for(unsigned i=0;i<4;i++){
        int flags[]={O_WRONLY,O_RDWR,O_RDONLY|O_TRUNC,O_WRONLY|O_CREAT|O_TRUNC};
        int bad=__wrap_open(link,flags[i],0600);if(bad>=0){__wrap_close(bad);result=215;goto done;}
    }
    stream=__wrap_fopen(link,"rb");if(!stream||fread(data,1,sizeof(expected),stream)!=sizeof(expected)||memcmp(data,expected,sizeof(expected))){result=216;goto done;}
    if(fseeko64(stream,-3,SEEK_END)||fread(data,1,3,stream)!=3||memcmp(data,expected+sizeof(expected)-3,3)){result=226;goto done;}
    fclose(stream);stream=NULL;
    if(__wrap_fopen(link,"wb")||__wrap_fopen(link,"r+b")||__wrap_unlink(link)==0||__wrap_rename(original,link)==0){result=217;goto done;}
    {int private_fd=mkstemp(saved);if(private_fd<0){result=218;goto done;}private_created=1;__wrap_close(private_fd);stream=__wrap_fopen(saved,"wb");
     if(!stream||fwrite(expected,1,sizeof(expected),stream)!=sizeof(expected)){result=219;goto done;}fclose(stream);stream=NULL;}
    if(pread64(fd,data,sizeof(expected),0)!=sizeof(expected)||memcmp(data,expected,sizeof(expected)))result=220;
done:
    if(stream)fclose(stream);if(a>=0)__wrap_close(a);if(b>=0)__wrap_close(b);if(c>=0)__wrap_close(c);fchmod(fd,0600);__wrap_close(fd);
    __real_unlink(original);if(link[0])__real_unlink(link);if(private_created)__real_unlink(saved);
    if(!result)fprintf(stderr,"[sources-test] PASS: EACCES reopening reproduced; granted FD reads, independent opens, shared dup offset, 64-bit seek, stdio, stat, write/delete denial, private saves and original bytes preserved\n");
    return result;
}
