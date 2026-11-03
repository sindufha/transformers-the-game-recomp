/* Small, explicit platform services used before/around game initialization.
 * No fabricated registry settings, persistent CryptoAPI keys or external
 * folders. The host supplies an app-private game_dir for save documents. */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>
#include <sys/stat.h>
#include <unistd.h>
#include "win32hle.h"
#include "startup_services.h"

/* Audited pinned runtime uses handle types 1..11; closer table has 32 slots.
 * Type 12 is private to these ephemeral provider contexts. */
enum { TF_H_CRYPTO=12, TF_MAX_PATH=260 };
static char documents_host[1024],documents_guest[TF_MAX_PATH];
static int documents_fd=-1;
static int crypto_context;

static uint32_t file_error(int error) {
    switch(error){case EACCES:case EPERM:case ELOOP:return 5;case ENOENT:case ENOTDIR:return 3;
    case ENAMETOOLONG:return 206;case EEXIST:return 183;case ENOSPC:return 112;default:return 31;}
}
static int prepare_documents(const char *game_dir) {
    char canonical[1024],candidate[1024];struct stat st;
    if(!realpath(game_dir,canonical)||stat(canonical,&st)||!S_ISDIR(st.st_mode))return -1;
    int n=snprintf(candidate,sizeof candidate,"%s/documents",canonical);
    if(n<0||(size_t)n>=sizeof candidate)return -1;
    if(mkdir(candidate,0700)&&errno!=EEXIST)return -1;
    int fd=open(candidate,O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);
    if(fd<0)return -1;
    /* The game's read-side filesystem uppercases absolute Windows paths.
     * Android forbids listing ancestors such as /data, so case-insensitive
     * lookup cannot recover /DATA/USER/... from a Z: host-absolute path.
     * Pin Documents as a drive: only descendants of this readable directory
     * need case matching. Both reads and writes still reach the same files. */
    hle_set_drive('D',candidate);
    char guest[1024];hle_guest_path(candidate,guest,sizeof guest);
    if(strlen(guest)>=TF_MAX_PATH){close(fd);return -1;}
    if(documents_fd>=0)close(documents_fd);
    documents_fd=fd;memcpy(documents_host,candidate,(size_t)n+1);
    memcpy(documents_guest,guest,strlen(guest)+1);
    fprintf(stderr,"[settings] Documents %s -> %s\n",documents_guest,documents_host);
    return 0;
}
/* Match a lexical host path to documents/, then walk every component under
 * a pinned directory fd with O_NOFOLLOW. Prefix matching alone is insufficient
 * because an existing descendant can be a symlink out of the private tree. */
static const char *documents_relative(const char *path) {
    size_t n=strlen(documents_host);
    if(!n||strncmp(path,documents_host,n)||(path[n]&&path[n]!='/'))return NULL;
    path+=n;while(*path=='/')++path;return path;
}
static uint32_t create_private_directory(const char *host_path) {
    if(documents_fd<0)return 3;
    const char *part=documents_relative(host_path);if(!part)return 5;
    int fd=dup(documents_fd);if(fd<0)return file_error(errno);
    if(!*part){close(fd);return 183;}
    uint32_t result=183;
    while(*part){
        char name[256];size_t n=0;while(part[n]&&part[n]!='/')++n;
        if(!n||n>=sizeof name){result=206;break;}
        memcpy(name,part,n);name[n]=0;
        if(!strcmp(name,".")||!strcmp(name,"..")){result=5;break;}
        part+=n;while(*part=='/')++part;
        int next=openat(fd,name,O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);
        if(next<0&&errno==ENOENT){
            if(mkdirat(fd,name,0700)&&errno!=EEXIST){result=file_error(errno);break;}
            next=openat(fd,name,O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);result=0;
        }else if(next>=0)result=183;
        if(next<0){result=file_error(errno);break;}
        close(fd);fd=next;
    }
    close(fd);return result;
}

/* Empty read-only registry roots. No installer values are invented. */
static int registry_root(uint32_t h){return h>=0x80000000u&&h<=0x80000005u;}
static void reg_open(void) {
    uint32_t root=A32(0),out=A32(4),access=A32(3);if(!out)RET(87,5);MEM32(out)=0;
    if(!registry_root(root))RET(6,5);
    if(A32(2)||(access&0x300u)==0x300u)RET(87,5);
    if(access&~(0x20019u|0x300u|0x80000000u))RET(5,5);
    if(A32(1)&&*ASTR(1))RET(2,5);
    MEM32(out)=root;RET(0,5);
}
static void reg_query(void) {
    if(!registry_root(A32(0)))RET(6,6);
    if(A32(2)||(A32(4)&&!A32(5)))RET(87,6);
    RET(2,6); /* every value is absent; no undefined output fields fabricated */
}
static void reg_close(void){RET(registry_root(A32(0))?0:6,1);}

static void get_folder_path(void) {
    uint32_t out=A32(4),csidl=A32(1);if(!out)RET(0x80070057u,5);MEM8(out)=0;
    if(A32(2)||A32(3)>1||(csidl&~0xc005u)||(csidl&0xfffu)!=5u)RET(0x80070057u,5);
    if(documents_fd<0)RET(0x80070003u,5);
    memcpy(hle_ptr(out),documents_guest,strlen(documents_guest)+1);RET(0,5);
}
static void create_directory(void) {
    if(!A32(1))RET(87,3);if(A32(2))RET(50,3); /* custom SECURITY_ATTRIBUTES unsupported */
    const char *guest=ASTR(1);size_t n=strlen(guest);if(n>=248)RET(206,3);
    if(!(n>2&&guest[1]==':'&&(guest[2]=='\\'||guest[2]=='/')))RET(161,3);
    char host[1024];hle_host_path(guest,host,sizeof host);RET(create_private_directory(host),3);
}
static void shell_execute(void){RET(31,6);} /* no host association/UI adapter */

int tf_services_random(void *output,uint32_t count) {
    if(count&&!output)return 0;
    unsigned char *p=output;
    while(count){size_t request=count>256u?256u:count;ssize_t n=getrandom(p,request,0);
        if(n<0&&errno==EINTR)continue;if(n<=0)return 0;p+=(size_t)n;count-=(uint32_t)n;}
    return 1;
}
static void crypt_acquire(void) {
    uint32_t out=A32(0),flags=A32(4);if(!out){hle_set_last_error(87);RET(0,5);}MEM32(out)=0;
    if((flags&0xf0000000u)!=0xf0000000u||(flags&~0xf0000060u)||(A32(1)&&*ASTR(1))){hle_set_last_error(0x80090009u);RET(0,5);}
    if(A32(3)!=1){hle_set_last_error(A32(3)>0&&A32(3)<1000?0x80090017u:0x80090014u);RET(0,5);}
    if(A32(2)&&strcmp(ASTR(2),"Microsoft Base Cryptographic Provider v1.0")){hle_set_last_error(0x80090019u);RET(0,5);}
    uint32_t h=hle_handle_alloc(TF_H_CRYPTO,&crypto_context);if(!h){hle_set_last_error(8);RET(0,5);}MEM32(out)=h;RET(1,5);
}
static void crypt_random(void) {
    if(!hle_handle_obj(A32(0),TF_H_CRYPTO)){hle_set_last_error(6);RET(0,3);}
    if(!tf_services_random(hle_ptr(A32(2)),A32(1))){hle_set_last_error(A32(1)&&!A32(2)?87:31);RET(0,3);}RET(1,3);
}
static void crypt_release(void) {
    if(A32(1)){hle_set_last_error(87);RET(0,2);}
    if(!hle_handle_obj(A32(0),TF_H_CRYPTO)){hle_set_last_error(6);RET(0,2);}
    hle_handle_close(A32(0));RET(1,2);
}
static void create_guid(void) {
    uint32_t out=A32(0);if(!out)RET(0x80070057u,1);uint8_t guid[16];
    if(!tf_services_random(guid,16))RET(0x80004005u,1);
    guid[7]=(uint8_t)((guid[7]&15u)|0x40u);guid[8]=(uint8_t)((guid[8]&63u)|0x80u);
    memcpy(hle_ptr(out),guid,16);RET(0,1);
}
static void uuid_text(const uint8_t *guid,char *text) {
    static const uint8_t order[16]={3,2,1,0,5,4,7,6,8,9,10,11,12,13,14,15};
    static const char hex[]="0123456789abcdef";unsigned j=0;
    for(unsigned i=0;i<16;++i){if(i==4||i==6||i==8||i==10)text[j++]='-';unsigned v=guid[order[i]];text[j++]=hex[v>>4];text[j++]=hex[v&15];}
    text[j]=0;
}
static void uuid_to_string(void) {
    if(!A32(1))RET(87,2);MEM32(A32(1))=0;
    uint8_t nil[16]={0};char *out=hle_alloc(37);if(!out)RET(14,2);
    uuid_text(A32(0)?hle_ptr(A32(0)):nil,out);MEM32(A32(1))=hle_va(out);RET(0,2);
}
static void rpc_string_free(void) {
    uint32_t ptr=A32(0);if(!ptr)RET(87,1);uint32_t va=MEM32(ptr);
    if(va){if(!hle_heap_ok(hle_ptr(va)))RET(87,1);hle_free(hle_ptr(va));}MEM32(ptr)=0;RET(0,1);
}
static void alloc_bstr(void) {
    if(!A32(0))RET(0,1);
    const uint16_t *text=hle_ptr(A32(0));uint32_t n=0;while(n<0x1000000u&&text[n])++n;
    if(n==0x1000000u)RET(0,1);
    uint8_t *base=hle_alloc(6u+2u*n);if(!base)RET(0,1);
    uint32_t length=2*n;memcpy(base,&length,4);memcpy(base+4,text,length);base[4+length]=base[5+length]=0;
    RET(hle_va(base)+4,1);
}
static void free_bstr(void) {
    if(A32(0)){uint32_t va=A32(0);if(va<4||!hle_heap_ok(hle_ptr(va-4)))hle_fatal("SysFreeString received a non-BSTR heap block");hle_free(hle_ptr(va-4));}RETV(1);
}

int tf_services_register(const char *game_dir) {
    static int registered;
    if(game_dir&&prepare_documents(game_dir))return -1;
    if(registered)return 0;
    const win32hle_shim shims[]={
        {"RegOpenKeyExA",reg_open},{"RegQueryValueExA",reg_query},{"RegCloseKey",reg_close},
        {"SHGetFolderPathA",get_folder_path},{"SHCreateDirectoryExA",create_directory},{"ShellExecuteA",shell_execute},
        {"CryptAcquireContextA",crypt_acquire},{"CryptGenRandom",crypt_random},{"CryptReleaseContext",crypt_release},
        {"CoCreateGuid",create_guid},{"UuidToStringA",uuid_to_string},{"RpcStringFreeA",rpc_string_free},
        {"oleaut32.dll#2",alloc_bstr},{"SysAllocString",alloc_bstr},{"oleaut32.dll#6",free_bstr},{"SysFreeString",free_bstr},
        {NULL,NULL}
    };registered=1;return win32hle_register(shims);
}
int tf_services_selftest(void) {
    uint8_t bytes[16];char text[37];if(!tf_services_random(bytes,16))return 71;
    for(unsigned i=0;i<16;++i)bytes[i]=(uint8_t)i;
    uuid_text(bytes,text);if(strcmp(text,"03020100-0504-0706-0809-0a0b0c0d0e0f"))return 72;
    puts("[startup] secure random and UUID formatter self-test passed; no game executed");return 0;
}
