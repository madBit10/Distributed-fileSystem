// s25client.c — upload, download, remove (beginner-friendly)
#include <netinet/in.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 4096

/* ---------- helpers ---------- */
static void error(const char *m){ perror(m); }

static int write_all(int fd, const void *buf, size_t n){
    const char *p = (const char*)buf; size_t sent = 0;
    while(sent < n){
        ssize_t w = write(fd, p + sent, n - sent);
        if(w <= 0) return -1;
        sent += (size_t)w;
    }
    return 0;
}
static int read_all(int fd, void *buf, size_t n){
    char *p = (char*)buf; size_t got = 0;
    while(got < n){
        ssize_t r = read(fd, p + got, n - got);
        if(r <= 0) return -1;
        got += (size_t)r;
    }
    return 0;
}
static int send_cmd(int fd, const char fourcc[4]){
    return write_all(fd, fourcc, 4);
}
static int send_i32(int fd, int32_t v){ return write_all(fd, &v, sizeof(v)); }
static int recv_i32(int fd, int32_t *v){ return read_all(fd, v, sizeof(*v)); }
static int send_i64(int fd, int64_t v){ return write_all(fd, &v, sizeof(v)); }
static int recv_i64(int fd, int64_t *v){ return read_all(fd, v, sizeof(*v)); }

static int send_str(int fd, const char *s){
    int32_t n = (int32_t)strlen(s);
    if(send_i32(fd, n) < 0) return -1;
    return write_all(fd, s, (size_t)n);
}

static const char* base_name(const char *path){
    const char *s = strrchr(path, '/');
    return s ? s+1 : path;
}
static int isAllowedExtension(const char *filename){
    const char *dot = strrchr(filename, '.');
    if(!dot) return 0;
    return (!strcmp(dot, ".txt") || !strcmp(dot, ".pdf") ||
            !strcmp(dot, ".c")   || !strcmp(dot, ".zip"));
}

static long long file_size(const char *fn){
    struct stat st; if(stat(fn,&st)<0) return -1;
    return (long long)st.st_size;
}

static int connectToServer(const char *host, int portno){
    struct hostent *server = gethostbyname(host);
    if(!server){ fprintf(stderr,"no such host: %s\n", host); return -1; }
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0){ error("socket"); return -1; }
    struct sockaddr_in sa; bzero(&sa, sizeof(sa));
    sa.sin_family = AF_INET;
    memcpy(&sa.sin_addr, server->h_addr_list[0], server->h_length);
    sa.sin_port = htons(portno);
    if(connect(sockfd, (struct sockaddr*)&sa, sizeof(sa)) < 0){
        error("connect"); close(sockfd); return -1;
    }
    return sockfd;
}

/* ---------- commands ---------- */

// uploadf file1 [file2] [file3] [~/S1[/subdir]]
static int do_upload(int sockfd, int files, char **fnames, const char *destTag){
    if(send_cmd(sockfd, "UPLD")<0) return -1;
    if(send_i32(sockfd, files)<0) return -1;

    char *buffer = (char*)malloc(BUFFER_SIZE);
    if(!buffer){ fprintf(stderr,"malloc failed\n"); return -1; }

    for(int i=0;i<files;i++){
        const char *fn = fnames[i];
        if(strchr(fn,'/')){ fprintf(stderr,"Please pass basenames only: %s\n", fn); free(buffer); return -1; }
        if(!isAllowedExtension(fn)){ fprintf(stderr,"Blocked ext: %s\n", fn); free(buffer); return -1; }
        long long sz = file_size(fn);
        if(sz < 0){ perror("stat"); free(buffer); return -1; }

        // dest tag
        if(send_str(sockfd, destTag)<0){ free(buffer); return -1; }
        // base name only
        const char *base = base_name(fn);
        if(send_str(sockfd, base)<0){ free(buffer); return -1; }
        // size
        if(send_i64(sockfd, (int64_t)sz)<0){ free(buffer); return -1; }
        // data
        int fd = open(fn, O_RDONLY);
        if(fd<0){ perror("open"); free(buffer); return -1; }
        long long left = sz;
        while(left>0){
            int chunk = (left>BUFFER_SIZE)?BUFFER_SIZE:(int)left;
            ssize_t r = read(fd, buffer, chunk);
            if(r<=0){ perror("read file"); close(fd); free(buffer); return -1; }
            if(write_all(sockfd, buffer, (size_t)r)<0){ perror("send data"); close(fd); free(buffer); return -1; }
            left -= r;
        }
        close(fd);
        printf("Uploaded %s (%lld bytes)\n", base, sz);
    }
    free(buffer);
    return 0;
}

// downlf <~/S1/path1> [~/S1/path2]
static int do_download(const char *host, int port, int count, char **paths){
    int sockfd = connectToServer(host, port);
    if(sockfd<0) return -1;

    if(send_cmd(sockfd, "DOWN")<0){ close(sockfd); return -1; }
    if(send_i32(sockfd, count)<0){ close(sockfd); return -1; }
    for(int i=0;i<count;i++)
        if(send_str(sockfd, paths[i])<0){ close(sockfd); return -1; }

    // For each: name, size, data
    for(int i=0;i<count;i++){
        int32_t nlen;
        if(recv_i32(sockfd, &nlen)<0 || nlen<=0 || nlen>2048){
            fprintf(stderr,"download failed for %s\n", paths[i]);
            close(sockfd); return -1;
        }
        char *name = (char*)malloc(nlen+1);
        read_all(sockfd, name, nlen); name[nlen]='\0';
        int64_t sz;
        if(recv_i64(sockfd,&sz)<0){ fprintf(stderr,"download failed\n"); free(name); close(sockfd); return -1; }

        int fd = open(name, O_WRONLY|O_CREAT|O_TRUNC, 0666);
        if(fd<0){
            perror("open out");
            char dummy[BUFFER_SIZE]; int64_t left=sz;
            while(left>0){ int c=(left>BUFFER_SIZE)?BUFFER_SIZE:(int)left; int r=read(sockfd,dummy,c); if(r<=0)break; left-=r; }
            free(name); continue;
        }
        char *b=(char*)malloc(BUFFER_SIZE);
        int64_t left=sz;
        while(left>0){
            int c=(left>BUFFER_SIZE)?BUFFER_SIZE:(int)left;
            int r=read(sockfd,b,c);
            if(r<=0)break;
            write_all(fd,b,r);
            left-=r;
        }
        free(b); close(fd);
        printf("Downloaded: %s (%lld bytes)\n", name, (long long)sz);
        free(name);
    }

    close(sockfd);
    return 0;
}

// removef <~/S1/path1> [~/S1/path2]
static int do_remove(const char *host, int port, int count, char **paths){
    int sockfd = connectToServer(host, port);
    if(sockfd<0) return -1;

    if(send_cmd(sockfd, "REMF")<0){ close(sockfd); return -1; }
    if(send_i32(sockfd, count)<0){ close(sockfd); return -1; }
    for(int i=0;i<count;i++)
        if(send_str(sockfd, paths[i])<0){ close(sockfd); return -1; }

    for(int i=0;i<count;i++){
        int32_t ok=0;
        if(recv_i32(sockfd,&ok)<0){ fprintf(stderr,"remove failed\n"); close(sockfd); return -1; }
        printf("%s: %s\n", paths[i], ok?"removed":"not found");
    }
    close(sockfd);
    return 0;
}

/* ---------- main ---------- */
int main(int argc, char *argv[]){
    if(argc<3){ fprintf(stderr,"Usage: %s <server_ip> <port>\n", argv[0]); return 1; }
    const char *host = argv[1];
    int port = atoi(argv[2]);

    printf("connected, Enter the commands:\n");
    char line[4096];
    for(;;){
        printf("S25client$ "); fflush(stdout);
        if(!fgets(line,sizeof(line),stdin)) break;
        line[strcspn(line,"\n")] = 0;
        if(!*line) continue;

        // tokenize
        char *args[128]; int ac=0;
        char *tok=strtok(line," ");
        while(tok && ac<128){ args[ac++]=tok; tok=strtok(NULL," "); }
        if(ac==0) continue;

        if(!strcmp(args[0],"exit")||!strcmp(args[0],"quit")) break;

        if(!strcmp(args[0],"uploadf")){
            if(ac<2){ fprintf(stderr,"usage: uploadf <file1> [file2] [file3] [~/S1[/subdir]]\n"); continue; }
            const char *dest="~/S1";
            int last=ac-1;
            if(strncmp(args[last],"~/S1",4)==0){ dest=args[last]; ac--; }
            int files=ac-1;
            if(files<1||files>3){ fprintf(stderr,"upload 1..3 files\n"); continue; }

            int sockfd = connectToServer(host, port);
            if(sockfd<0) continue;
            if(do_upload(sockfd, files, &args[1], dest)<0) fprintf(stderr,"upload failed\n");
            close(sockfd);
            continue;
        }

        if(!strcmp(args[0],"downlf")){
            if(ac<2 || ac>3){ fprintf(stderr,"usage: downlf <~/S1/path1> [~/S1/path2]\n"); continue; }
            if(do_download(host, port, ac-1, &args[1])<0) fprintf(stderr,"downlf failed\n");
            continue;
        }

        if(!strcmp(args[0],"removef")){
            if(ac<2 || ac>3){ fprintf(stderr,"usage: removef <~/S1/path1> [~/S1/path2]\n"); continue; }
            if(do_remove(host, port, ac-1, &args[1])<0) fprintf(stderr,"remove failed\n");
            continue;
        }

        fprintf(stderr,"commands: uploadf, downlf, removef, exit\n");
    }
    return 0;
}
