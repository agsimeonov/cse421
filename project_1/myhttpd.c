#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <limits.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>

struct rqst_llist {
    char *remoteip_addr; //the remote ip address
    char *recv_time; //the time the request was received
    char *sched_time; //the time the request was assigned to an execution thread by the scheduler
    char *rqst; //the first line of the request
    int sockfd; //the socket file descriptor
    char *directory; //the current working directory
    char *filename; //the filename needed
    off_t filesize; //the actual size of the requested files
    off_t maxfilesize; //the maximum size of the requested file (may vary in case of error)
    int mode; //0 on GET, 1 on HEAD, else Bad Request
    
    struct rqst_llist *next;
    struct rqst_llist *prev;
};

struct server_args {
    int port_num;
    char *sched;
};

int logging; //0 (no logging) / 1 (regular logging) / 2 (debug logging) / 3 (both regular and debug logging)
char *logfile; //the logfile name
struct rqst_llist *queue; //the queue pointing to it's first element
struct rqst_llist *queue_last; //the last element of the queue
struct rqst_llist *exec_queue; //the first element in the execution queue
struct rqst_llist *exec_queue_last; //the last element in the execution queue
pthread_mutex_t queue_mutex; //the mutex for the queue (need mutex because queuing thread and scheduling thread can't access queue at the same time)
sem_t *scheduling_sem; //a semaphore for scheduling will make thread wait if there is nothing to schedule
pthread_mutex_t exec_queue_mutex; //the mutext for the execution queue
sem_t *exec_sem; //a semaphore for execution will make execution threads wait if there is nothing to execute
pthread_mutex_t logg_mutex;

/* -h */
void h() {
    printf("-d\t\t: enter debugging mode.\n-h\t\t: print usage summary with all options and exit.\n-l file\t\t: log all requests to the given file.\n-p port\t\t: listen on the given port.\n\t\t  if not provided, myhttpd will listen on port 8080.\n-r dir\t\t: set the root directory for the http server to dir.\n\t\t  if not provided, defaults to the directory where\n\t\t  the server is running.\n-t time\t\t: set the queuing time to time seconds.\n\t\t  if not provided, defaults to 60 seconds.\n-n threadnum\t: set number of threads waiting ready in the\n\t\t  execution thread pool to threadnum.\n\t\t  if not provided, defaults to 4 execution threads.\n-s sched\t: set the scheduling policy.\n\t\t  it can be either FCFS or SJF.\n\t\t  if not provided, defaults to FCFS.\n");
}

/* GET */
char *rqstget(char *return_value, char *file_value, char *working_dir, char *filename) {
    /* date */
    time_t timer; //argument for time() and gmtime() with current time
    struct tm *tm_timer; //argument for strftime() with current time
    char frmtd_timer[30]; //argument for strftime() for current time - time format as defined by rfc1945
    
    time(&timer); //gets the current time
    tm_timer = gmtime(&timer); //converts timer to a broken-down gmt time
    strftime(frmtd_timer, 30, "%a, %d %b %Y %X GMT", tm_timer); //formats tm_timer
    
    /* server */
    static const char server_version[21] = "Server: myhttpd/1.0\n";
    
    if(strcmp(filename, "400 Bad Request") == 0) {
        goto badrequest;
    }
    
    /* last-modified */
    struct stat file_info; //contains information about a file
    char frmtd_lastmod[30]; //argument for strfime() for last modification time - time format as defined by rfc1945
    
    
dirindexfoundget: //continues here if a directory contains a file names "index.html" or "index.htm"
    if((stat(strcat(working_dir, filename), &file_info)) == -1) {
        if((errno == ENOENT) || 
           (errno == ENOTDIR)) {
            sprintf(return_value, 
                    "HTTP/1.0 404 Not Found\nDate: %s\n%sLast Modified: %s\nContent-Type: text/html\nContent-Length: %lu\n", 
                    frmtd_timer, server_version, frmtd_timer, sizeof("<html><head><title>404 Not Found</title></head><body><h1>404 Not Found</h1></body></html>"));
            strcpy(file_value, "<html><head><title>404 Not Found</title></head><body><h1>404 Not Found</h1></body></html>");
            return return_value;
        } //404 Not Found
        else if(errno == EACCES) {
            sprintf(return_value, 
                    "HTTP/1.0 403 Forbidden\nDate: %s\n%sLast Modified: %s\nContent-Type: text/html\nContent-Length: %lu\n", 
                    frmtd_timer, server_version, frmtd_timer, sizeof("<html><head><title>403 Forbidden</title></head><body><h1>403 Forbidden</h1></body></html>"));
            strcpy(file_value, "<html><head><title>403 Forbidden</title></head><body><h1>403 Forbidden</h1></body></html>");
            return return_value;
        } //403 Forbidden
        else if((errno == ELOOP) || 
                (errno == ENAMETOOLONG)) {
        badrequest:
            sprintf(return_value, 
                    "HTTP/1.0 400 Bad Request\nDate: %s\n%sLast Modified: %s\nContent-Type: text/html\nContent-Length: %lu\n", 
                    frmtd_timer, server_version, frmtd_timer, sizeof("<html><head><title>400 Bad Request</title></head><body><h1>400 Bad Request</h1></body></html>"));
            strcpy(file_value, "<html><head><title>400 Bad Request</title></head><body><h1>400 Bad Request</h1></body></html>");
            return return_value;
        } //400 Bad Request
        else {
            sprintf(return_value, 
                    "HTTP/1.0 500 Internal Server Error\nDate: %s\n%sLast Modified: %s\nContent-Type: text/html\nContent-Length: %lu\n", 
                    frmtd_timer, server_version, frmtd_timer, sizeof("<html><head><title>500 Internal Server Error</title></head><body><h1>500 Internal Server Error</h1></body></html>"));
            strcpy(file_value, "<html><head><title>500 Internal Server Error</title></head><body><h1>500 Internal Server Error</h1></body></html>");
            return return_value;
        } //500 Internal Server Error
    } //aquires information about a file and detects and handles errors on stat()
    else {
        strftime(frmtd_lastmod, 30, "%a, %d %b %Y %X GMT", gmtime(&file_info.st_mtime)); //formats the last modification time
        
        /* Content-Type */
        if(S_ISDIR(file_info.st_mode)) {
            DIR *directory; //the directory stream
            struct dirent *entry; //the current entry in the directory stream
            struct dirent **namelist; //sorted list of file names
            int total_chars = 0; //the total number of characters in the directory entries read
            int total_entries = 0; //the total number of entries read
            int entry_max; //the total entries overall
            int entry_iterator; //iterator for entries
            
            entry_max = strcmp(filename, "/") == 0 ? scandir(".", &namelist, 0, alphasort) : scandir(&filename[1], &namelist, 0, alphasort); //sorts file names in alphanumeric order
            directory = strcmp(filename, "/") == 0 ? opendir(".") : opendir(&filename[1]); //opens the directory stream
            for(entry = readdir(directory); entry != NULL; entry = readdir(directory)) {
                if(strncmp(entry->d_name, ".", 1) == 0) {
                    continue;
                } //files starting with a "." are ignored
                else {
                    struct stat listing_info;
                    char temp_str[PATH_MAX];
                    strcpy(temp_str, working_dir);
                    strcat(temp_str, "/");
                    stat(strcat(temp_str, entry->d_name), &listing_info);
                    
                    total_chars = total_chars + strlen(entry->d_name);
                    if(S_ISDIR(listing_info.st_mode)) {
                        total_chars = total_chars + 1;
                    } //add an character for standard directory distinguishing end "/"
                    total_entries = total_entries + 1;
                } //helps determine the size of the directory listing
            }
            int i = 90 + (2*(strlen(filename))) + (25*total_entries) + (2*total_chars) + 1;
            char dir_listing[i]; //tightly sized directory listing
            strcpy(dir_listing, "<html><head><title>Index of ");
            strcat(dir_listing, filename);
            strcat(dir_listing, "</title></head><body><h1>Index of ");
            strcat(dir_listing, filename);
            strcat(dir_listing, "</h1><ul>");
            closedir(directory); //closes the directory stream
            for(entry_iterator = 0; entry_iterator < entry_max; entry_iterator = entry_iterator + 1) {
                if(strncmp(namelist[entry_iterator]->d_name, ".", 1) == 0) {
                    continue;
                } //files starting with a "." are ignored
                else {
                    struct stat listing_info;
                    char temp_str[PATH_MAX];
                    strcpy(temp_str, working_dir);
                    strcat(temp_str, "/");
                    stat(strcat(temp_str, namelist[entry_iterator]->d_name), &listing_info);
                    
                    strcat(dir_listing, "<li><a href=\"");
                    strcat(dir_listing, namelist[entry_iterator]->d_name);
                    if(S_ISDIR(listing_info.st_mode)) {
                        strcat(dir_listing, "/");
                    } //adds standard directory distinguishing end "/"
                    strcat(dir_listing, "\"> ");
                    strcat(dir_listing, namelist[entry_iterator]->d_name);
                    if(S_ISDIR(listing_info.st_mode)) {
                        strcat(dir_listing, "/");
                    } //adds standard directory distinguishing end "/"
                    strcat(dir_listing, "</a></li>");
                } //generates the directory listing
            }
            strcat(dir_listing, "</ul></body></html>");
            free(namelist);
            
            sprintf(return_value, 
                    "HTTP/1.0 200 OK\nDate: %s\n%sLast Modified: %s\nContent-Type: text/html\nContent-Length: %lu\n", 
                    frmtd_timer, server_version, frmtd_timer, sizeof(dir_listing));
            strcpy(file_value, dir_listing);
            return return_value;
        } //if the request is for a directory
        else {
            if(filename[1] == '.') {
                sprintf(return_value,
                        "HTTP/1.0 404 Not Found\nDate: %s\n%sLast Modified: %s\nContent-Type: text/html\nContent-Length: %lu\n",
                        frmtd_timer, server_version, frmtd_timer, sizeof("<html><head><title>404 Not Found</title></head><body><h1>404 Not Found</h1></body></html>"));
                strcpy(file_value, "<html><head><title>404 Not Found</title></head><body><h1>404 Not Found</h1></body></html>");
                return return_value;
            } //files starting with a "." are ignored
            else {
                char *extension = strrchr(filename, '.'); //aquires the extension starting at '.' if there is one
                char current_dir[PATH_MAX];
                FILE *givenfile = fopen(working_dir, "r"); //opens the file
                if(givenfile == NULL) {
                    sprintf(return_value, 
                            "HTTP/1.0 500 Internal Server Error\nDate: %s\n%sLast Modified: %s\nContent-Type: text/html\nContent-Length: %lu\n", 
                            frmtd_timer, server_version, frmtd_timer, sizeof("<html><head><title>500 Internal Server Error</title></head><body><h1>500 Internal Server Error</h1></body></html>"));
                    strcpy(file_value, "<html><head><title>500 Internal Server Error</title></head><body><h1>500 Internal Server Error</h1></body></html>");
                    return return_value;
                } //could not open file 500 Internal Server Error
                else {
                    fread(file_value, 1, file_info.st_size, givenfile);
                    fclose(givenfile);
                } //assigns the contents of the file to file_value
                
                if((extension != NULL) && ((strcmp(&extension[1], "htm") == 0) || (strcmp(&extension[1], "html") == 0))) {
                    sprintf(return_value, 
                            "HTTP/1.0 200 OK\nDate: %s\n%sLast Modified: %s\nContent-Type: text/html\nContent-Length: %llu\n", 
                            frmtd_timer, server_version, frmtd_lastmod, file_info.st_size);
                    return return_value;
                } //for .htm or .html
                else if((extension != NULL) && (strcmp(&extension[1], "gif") == 0)) {
                    sprintf(return_value, 
                            "HTTP/1.0 200 OK\nDate: %s\n%sLast Modified: %s\nContent-Type: image/gif\nContent-Length: %llu\n", 
                            frmtd_timer, server_version, frmtd_lastmod, file_info.st_size);
                    return return_value;
                } //for .gif
                else {
                    sprintf(return_value, 
                            "HTTP/1.0 200 OK\nDate: %s\n%sLast Modified: %s\nContent-Type: \nContent-Length: %llu\n", 
                            frmtd_timer, server_version, frmtd_lastmod, file_info.st_size);
                    return return_value;
                } //for other undefined file types
            }
        } //if the request is not for a directory
    }
}

/* HEAD */
char *rqsthead(char *return_value, char *working_dir, char *filename) {
    /* date */
    time_t timer; //argument for time() and gmtime() with current time
    struct tm *tm_timer; //argument for strftime() with current time
    char frmtd_timer[30]; //argument for strftime() for current time - time format as defined by rfc1945
    
    time(&timer); //gets the current time
    tm_timer = gmtime(&timer); //converts timer to a broken-down gmt time
    strftime(frmtd_timer, 30, "%a, %d %b %Y %X GMT", tm_timer); //formats tm_timer
    
    /* server */
    static const char server_version[21] = "Server: myhttpd/1.0\n";
    
    /* last-modified */
    struct stat file_info; //contains information about a file
    char frmtd_lastmod[30]; //argument for strfime() for last modification time - time format as defined by rfc1945
    int dir_changed = 0; //will equal 1 if the working directory was changed

dirindexfound: //continues here if a directory contains a file names "index.html" or "index.htm"
    if((stat(strcat(working_dir, filename), &file_info)) == -1) {
        if((errno == ENOENT) || 
           (errno == ENOTDIR)) {
            sprintf(return_value, 
                    "HTTP/1.0 404 Not Found\nDate: %s\n%sLast Modified: %s\nContent-Type: text/html\nContent-Length: %lu\n", 
                    frmtd_timer, server_version, frmtd_timer, sizeof("<html><head><title>404 Not Found</title></head><body><h1>404 Not Found</h1></body></html>"));
            return return_value;
        } //404 Not Found
        else if(errno == EACCES) {
            sprintf(return_value, 
                    "HTTP/1.0 403 Forbidden\nDate: %s\n%sLast Modified: %s\nContent-Type: text/html\nContent-Length: %lu\n", 
                    frmtd_timer, server_version, frmtd_timer, sizeof("<html><head><title>403 Forbidden</title></head><body><h1>403 Forbidden</h1></body></html>"));
            return return_value;
        } //403 Forbidden
        else if((errno == ELOOP) || 
                (errno == ENAMETOOLONG)) {
            sprintf(return_value, 
                    "HTTP/1.0 400 Bad Request\nDate: %s\n%sLast Modified: %s\nContent-Type: text/html\nContent-Length: %lu\n", 
                    frmtd_timer, server_version, frmtd_timer, sizeof("<html><head><title>400 Bad Request</title></head><body><h1>400 Bad Request</h1></body></html>"));
            return return_value;
        } //400 Bad Request
        else {
            sprintf(return_value, 
                    "HTTP/1.0 500 Internal Server Error\nDate: %s\n%sLast Modified: %s\nContent-Type: text/html\nContent-Length: %lu\n", 
                    frmtd_timer, server_version, frmtd_timer, sizeof("<html><head><title>500 Internal Server Error</title></head><body><h1>500 Internal Server Error</h1></body></html>"));
            return return_value;
        } //500 Internal Server Error
    } //aquires information about a file and detects and handles errors on stat()
    else {
        strftime(frmtd_lastmod, 30, "%a, %d %b %Y %X GMT", gmtime(&file_info.st_mtime)); //formats the last modification time
        
        /* Content-Type */
        if(S_ISDIR(file_info.st_mode)) {
            DIR *directory; //the directory stream
            struct dirent *entry; //the current entry in the directory stream
            struct dirent **namelist; //sorted list of file names
            int total_chars = 0; //the total number of characters in the directory entries read
            int total_entries = 0; //the total number of entries read
            int entry_max; //the total entries overall
            int entry_iterator; //iterator for entries
            
            entry_max = strcmp(filename, "/") == 0 ? scandir(".", &namelist, 0, alphasort) : scandir(&filename[1], &namelist, 0, alphasort); //sorts file names in alphanumeric order
            directory = strcmp(filename, "/") == 0 ? opendir(".") : opendir(&filename[1]); //opens the directory stream
            for(entry = readdir(directory); entry != NULL; entry = readdir(directory)) {
                if(strncmp(entry->d_name, ".", 1) == 0) {
                    continue;
                } //files starting with a "." are ignored
                else {
                    struct stat listing_info;
                    char temp_str[PATH_MAX];
                    strcpy(temp_str, working_dir);
                    strcat(temp_str, "/");
                    stat(strcat(temp_str, entry->d_name), &listing_info);

                    total_chars = total_chars + strlen(entry->d_name);
                    if(S_ISDIR(listing_info.st_mode)) {
                        total_chars = total_chars + 1;
                    } //add an character for standard directory distinguishing end "/"
                    total_entries = total_entries + 1;
                } //helps determine the size of the directory listing
            }
            int i = 90 + (2*(strlen(filename))) + (25*total_entries) + (2*total_chars) + 1;
            char dir_listing[i]; //tightly sized directory listing
            strcpy(dir_listing, "<html><head><title>Index of ");
            strcat(dir_listing, filename);
            strcat(dir_listing, "</title></head><body><h1>Index of ");
            strcat(dir_listing, filename);
            strcat(dir_listing, "</h1><ul>");
            closedir(directory); //closes the directory stream
            for(entry_iterator = 0; entry_iterator < entry_max; entry_iterator = entry_iterator + 1) {
                if(strncmp(namelist[entry_iterator]->d_name, ".", 1) == 0) {
                    continue;
                } //files starting with a "." are ignored
                else {
                    struct stat listing_info;
                    char temp_str[PATH_MAX];
                    strcpy(temp_str, working_dir);
                    strcat(temp_str, "/");
                    stat(strcat(temp_str, namelist[entry_iterator]->d_name), &listing_info);
                    
                    strcat(dir_listing, "<li><a href=\"");
                    strcat(dir_listing, namelist[entry_iterator]->d_name);
                    if(S_ISDIR(listing_info.st_mode)) {
                        strcat(dir_listing, "/");
                    } //adds standard directory distinguishing end "/"
                    strcat(dir_listing, "\"> ");
                    strcat(dir_listing, namelist[entry_iterator]->d_name);
                    if(S_ISDIR(listing_info.st_mode)) {
                        strcat(dir_listing, "/");
                    } //adds standard directory distinguishing end "/"
                    strcat(dir_listing, "</a></li>");
                } //generates the directory listing
            }
            strcat(dir_listing, "</ul></body></html>");
            free(namelist);
            
            sprintf(return_value, 
                    "HTTP/1.0 200 OK\nDate: %s\n%sLast Modified: %s\nContent-Type: text/html\nContent-Length: %lu\n", 
                    frmtd_timer, server_version, frmtd_timer, sizeof(dir_listing));
            return return_value;
        } //if the request is for a directory
        else {
            if(filename[1] == '.') {
                sprintf(return_value,
                        "HTTP/1.0 404 Not Found\nDate: %s\n%sLast Modified: %s\nContent-Type: text/html\nContent-Length: %lu\n",
                        frmtd_timer, server_version, frmtd_timer, sizeof("<html><head><title>404 Not Found</title></head><body><h1>404 Not Found</h1></body></html>"));
                return return_value;
            } //files starting with a "." are ignored
            else {
                char *extension = strrchr(filename, '.'); //aquires the extension starting at '.' if there is one
                
                if((extension != NULL) && ((strcmp(&extension[1], "htm") == 0) || (strcmp(&extension[1], "html") == 0))) {
                    sprintf(return_value, 
                            "HTTP/1.0 200 OK\nDate: %s\n%sLast Modified: %s\nContent-Type: text/html\nContent-Length: %llu\n", 
                            frmtd_timer, server_version, frmtd_lastmod, file_info.st_size);
                    return return_value;
                } //for .htm or .html
                else if((extension != NULL) && (strcmp(&extension[1], "gif") == 0)) {
                    sprintf(return_value, 
                            "HTTP/1.0 200 OK\nDate: %s\n%sLast Modified: %s\nContent-Type: image/gif\nContent-Length: %llu\n", 
                            frmtd_timer, server_version, frmtd_lastmod, file_info.st_size);
                    return return_value;
                } //for .gif
                else {
                    sprintf(return_value, 
                            "HTTP/1.0 200 OK\nDate: %s\n%sLast Modified: %s\nContent-Type: \nContent-Length: %llu\n", 
                            frmtd_timer, server_version, frmtd_lastmod, file_info.st_size);
                    return return_value;
                } //for other undefined file types
            }
        } //if the request is not for a directory
    }
}

/* executes requests */
void execute_rqst() {
    struct rqst_llist *execution_entry;
    while(1) {
        sem_wait(exec_sem);
        char head_return[200];
        if((exec_queue->rqst != NULL) && (exec_queue->mode != -1)) {
            pthread_mutex_lock(&exec_queue_mutex);
            exec_queue->prev->next = exec_queue->next;
            exec_queue->next->prev = exec_queue->prev;
            execution_entry = exec_queue;
            exec_queue = exec_queue->prev;
            if((exec_queue_last->next->rqst == NULL) && (exec_queue_last->next->mode == -1)) {
                exec_queue_last = exec_queue;
            }
            pthread_mutex_unlock(&exec_queue_mutex);
            if(execution_entry->mode == 0) {
                char *file_value;
                file_value = (char*)calloc(execution_entry->maxfilesize, 1);
                memset(head_return, 0, 200);
                memset(file_value, 0, execution_entry->maxfilesize);
                rqstget(head_return, file_value, execution_entry->directory, execution_entry->filename);
                send(execution_entry->sockfd, head_return, strlen(head_return), 0);
                send(execution_entry->sockfd, "\n", 1, 0);
                send(execution_entry->sockfd, file_value, execution_entry->maxfilesize, 0);
                send(execution_entry->sockfd, "\n", 1, 0);
                free(file_value);
                shutdown(execution_entry->sockfd, SHUT_RDWR);
            } //GET
            else if(execution_entry->mode == 1) {
                memset(head_return, 0, 200);
                rqsthead(head_return, execution_entry->directory, execution_entry->filename);
                send(execution_entry->sockfd, head_return, strlen(head_return), 0);
                shutdown(execution_entry->sockfd, SHUT_RDWR);
            } //HEAD
            else {
                char file_value[114];
                memset(head_return, 0, 200);
                memset(file_value, 0, 114);
                rqstget(head_return, file_value, execution_entry->directory, execution_entry->filename);
                send(execution_entry->sockfd, head_return, strlen(head_return), 0);
                send(execution_entry->sockfd, "\n", 1, 0);
                send(execution_entry->sockfd, file_value, 114, 0);
                send(execution_entry->sockfd, "\n", 1, 0);
                shutdown(execution_entry->sockfd, SHUT_RDWR);
            } //400 Bad Request
            close(execution_entry->sockfd);
        }
        /* logging */
        FILE *flog;
        char reqstatus[4] = {head_return[9], head_return[10], head_return[11], '\0'};
        switch(logging) {
            case 0:
                break;
            case 3:
                //continue
            case 1:
                pthread_mutex_lock(&logg_mutex);
                flog = fopen(logfile, "a");
                if(flog != NULL) {
                    fprintf(flog, "%s - [%s] [%s] \"%s\" %s %llu\n", 
                           execution_entry->remoteip_addr,
                           execution_entry->recv_time,
                           execution_entry->sched_time,
                           execution_entry->rqst,
                           reqstatus,
                           execution_entry->filesize);
                    fclose(flog);
                }
                pthread_mutex_unlock(&logg_mutex);
                if(logging == 3) {
                    //continue
                }
                else {
                    break;
                }
            case 2:
                pthread_mutex_lock(&logg_mutex);
                printf("%s - [%s] [%s] \"%s\" %s %llu\n", 
                       execution_entry->remoteip_addr,
                       execution_entry->recv_time,
                       execution_entry->sched_time,
                       execution_entry->rqst,
                       reqstatus,
                       execution_entry->filesize);
                pthread_mutex_unlock(&logg_mutex);
                break;
            default: 
                break;
        }
        free(execution_entry->remoteip_addr);
        free(execution_entry->recv_time);
        free(execution_entry->sched_time);
        free(execution_entry->rqst);
        free(execution_entry->directory);
        free(execution_entry->filename);
        free(execution_entry);
    }
}

/* server */
int server(struct server_args *args) {
    int port_num = args->port_num;
    char *sched = args->sched;
    
    char orig_dir[PATH_MAX];
    memset(orig_dir, 0, PATH_MAX);
    getcwd(orig_dir, PATH_MAX);
    
    /* gethostname() */
    char hostname[256]; //the host name (cannot exceed 256 characters)
    
    gethostname(hostname, sizeof(hostname)); //get host name for the current machine
    
    /* getaddrinfo() */
    int getaddrinfo_value; //return value of getaddrinfo()
    char servname[port_num < 10000 ? 4 : 5]; //argument for getaddrinfo()
    struct addrinfo hints; //argument for getaddrinfo()
    struct addrinfo *res; //argument for getaddrinfo()
    
    if((sprintf(servname, "%i", port_num)) < 0) {
        perror("sprintf() error");
        exit(1);
    } //error detection for sprintf() sets up servname with the given port
    
    memset(&hints, 0, sizeof(hints)); //makes sure hints is empty
    hints.ai_flags = AI_PASSIVE; //socket address is intended for bind()
    hints.ai_family = AF_UNSPEC; //address family of socket unspecified (IPv4/IPv6)
    hints.ai_socktype = SOCK_STREAM; //socket type is TCP byte-stream socket
    
    if((getaddrinfo_value = getaddrinfo(hostname, servname, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo() error: %s\n", gai_strerror(getaddrinfo_value));
        exit(1);
    } //error detection for getaddrinfo()
    
    /* socket(), setsockopt(), and bind() */
    struct addrinfo *res_iterator; //iterator for the res linked list
    int sockfd; //the socket file descriptor
    int enable_so_reuseaddr = 1; //used to enable SO_REUSEADDR option in setsockopt()
    
    for(res_iterator = res; res_iterator != NULL; res_iterator = res_iterator->ai_next) {
        /* socket() */
        if((sockfd = socket(res_iterator->ai_family, res_iterator->ai_socktype, res_iterator->ai_protocol)) == -1) {
            perror("socket() error");
            continue;
        } //creates a socket and detects errors on socket()
        /* setsockopt() */
        if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable_so_reuseaddr, sizeof(int)) == -1) {
            perror("setsockopt() error");
            exit(1);
        } //allows reuse of local addresses and detects errors on setsockopt()
        /* bind() */
        if(bind(sockfd, res_iterator->ai_addr, res_iterator->ai_addrlen) == -1) {
            close(sockfd);
            perror("bind() error");
            continue;
        } //binds a socket to an address and detects errors on bind()
        break;
    } //iterates through the res linked list
    
    if(res_iterator == NULL) {
        freeaddrinfo(res); //frees the res linked list
        fprintf(stderr, "bind() error: could not bind\n");
        exit(1);
    }
    else {
        freeaddrinfo(res); //frees the res linked list
    } //error detection for bind()
    
    /* listen() */
    if(listen(sockfd, SOMAXCONN) == -1) {
        perror("listen() error");
        exit(1);
    } //listens for socket connections and detects errors on listen()
    
    /* accept() */
    struct sockaddr_storage accept_addr; //connecting socket address
    socklen_t addr_size = sizeof(accept_addr); //argument for accept()
    int new_sockfd; //new socket file descriptor for accept() for each new connection
    
    while(1) {
        if((new_sockfd = accept(sockfd, (struct sockaddr*)&accept_addr, &addr_size)) == -1) {
            perror("accept() error");
            continue;
        } //accepts new connections and detects erros on accept() 
        
        /* inent_ntoa() (for logging)*/
        char *remoteip = inet_ntoa(((struct sockaddr_in*)&accept_addr)->sin_addr);
        
        /* send() and recv() */
        int request_size = 8092;
        char msg_received[request_size]; //the message received
        memset(msg_received, 0, request_size);
        
        recv(new_sockfd, msg_received, request_size, 0); //the message to receive
        /* request received time (for logging) */
        time_t timer; //argument for time() and gmtime() with current time
        struct tm *tm_timer; //argument for strftime() with current time
        char frmtd_timer[27]; //argument for strftime() for current time - formatted as per project description
        
        time(&timer); //gets the current time
        tm_timer = gmtime(&timer); //converts timer to a broken-down gmt time
        strftime(frmtd_timer, 27, "%d/%b/%Y:%X %z", tm_timer); //formats tm_timer
        
        /* first line of request */
        char firstline[strcspn(msg_received, "\n")]; //the first line of the request
        memset(firstline, 0,(strcspn(msg_received, "\n")));
        strncpy(firstline, msg_received, strcspn(msg_received, "\n") - 1);
        
        /* file size and working directory */
        char *msg_tokens; //the tokens of the message dilimited by spaces
        char *extra_tokens;
        char *usr_tokens;
        char *http_tokens;
        char *filename; //to have / in front
        char *orig_msg;
        char *usr_msg;
        char *http_msg;
        char mod_filename[PATH_MAX];
        int dir_changed = 0; //will equal 1 if the working directory was changed 
        
        orig_msg = strdup(msg_received);
        usr_msg = strdup(msg_received);
        http_msg = strdup(msg_received);
        http_tokens = strtok(http_msg, " \n");
        http_tokens = strtok(NULL, " \n");
        http_tokens = strtok(NULL, " \n");
        msg_tokens = strtok(msg_received, " \n");
        int httpbit;
        if((strcmp(msg_received, "GET") == 0) && ((httpbit = ((http_tokens == NULL) || (strlen(http_tokens) < 6)) ? -1 : strncmp(http_tokens, "HTTP/1", 6)) == 0)) {
            char working_dir[PATH_MAX];
            memset(working_dir, 0, PATH_MAX);
            getcwd(working_dir, PATH_MAX);
            
            msg_tokens = strtok(NULL, " ");
            if((msg_tokens[0] == '~') || (msg_tokens[1] == '~')) {
                usr_tokens = strtok(usr_msg, " ~/");
                usr_tokens = strtok(NULL, " ~/");
                extra_tokens = strtok(orig_msg, " ~");
                extra_tokens = strtok(NULL, " ~");
                extra_tokens = strtok(NULL, " ~");
                sprintf(mod_filename, "/home/%s/myhttpd%s", usr_tokens, &extra_tokens[strlen(usr_tokens)]);
                filename = mod_filename;
            }
            else {
                filename = msg_tokens;
            }
            
            struct stat file_info; //contains information about a file
            file_info.st_size = 114; //default size in case of error page
            off_t maxfilesize; //the maximum file size
            char file_dir[PATH_MAX];
            strcpy(file_dir, working_dir);
        dirindexfoundmaxsize:
            if((stat(strcat(file_dir, filename), &file_info) == -1) || (strncmp(&filename[1], ".", 1) == 0)) {
                maxfilesize = 114; //maximum size of an error page
            }
            else if(S_ISDIR(file_info.st_mode)) {
                DIR *directory; //the directory stream
                struct dirent *entry; //the current entry in the directory stream
                int total_chars = 0; //the total number of characters in the directory entries read
                int total_entries = 0; //the total number of entries read
                    
                directory = opendir(file_dir);
                for(entry = readdir(directory); entry != NULL; entry = readdir(directory)) {
                    if(strncmp(entry->d_name, ".", 1) == 0) {
                        continue;
                    } //files starting with a "." are ignored
                    else {
                        struct stat listing_info;
                        char temp_str[PATH_MAX];
                        strcpy(temp_str, file_dir);
                        strcat(temp_str, "/");
                        stat(strcat(temp_str, entry->d_name), &listing_info);
                        
                        if((strncmp(entry->d_name, "index.html", 10) == 0) || (strncmp(entry->d_name, "index.htm", 9) == 0)) {
                            char name_tobe[sizeof(entry->d_name) + 1]; //will hold the new name to be
                            strcpy(name_tobe, "/"); //adds the initial / to the name
                            strcat(name_tobe, entry->d_name); //adds the actual name after the /
                            memset(&file_info, 0, sizeof(file_info)); //clears the file_info structure
                            strcat(working_dir, filename);
                            filename = name_tobe; //sets the new filename
                            closedir(directory); //closes the directory stream
                            chdir(file_dir); //switches the directory to this new one
                            dir_changed = 1;
                            goto dirindexfoundmaxsize; //redo stat() and checks on file
                        } //determines in an the directory contains "index.html
                            
                        total_chars = total_chars + strlen(entry->d_name);
                        if(S_ISDIR(listing_info.st_mode)) {
                            total_chars = total_chars + 1;
                        } //add an character for standard directory distinguishing end "/"
                        total_entries = total_entries + 1;
                    } //helps determine the size of the directory listing
                }
                closedir(directory); //closes the directory stream
                maxfilesize = 90 + (2*(strlen(filename))) + (25*total_entries) + (2*total_chars) + 1;
            } //directory maximum size
            else {
                maxfilesize = file_info.st_size < 114 ? 114 : file_info.st_size; //makes sure the size is at least 114 in case of errors
            } //some file
            
            if(dir_changed == 1) {
                memset(working_dir, 0, PATH_MAX);
                getcwd(working_dir, PATH_MAX);
                chdir(orig_dir);
            }
            
            /* formatted request queuing */
            struct rqst_llist *queue_next;
            queue_next = (struct rqst_llist *)calloc(1, sizeof(struct rqst_llist));
            char *passed_addr;
            passed_addr = (char *)calloc(1, strlen(remoteip));
            sprintf(passed_addr, "%s", remoteip);
            queue_next->remoteip_addr = passed_addr;
            char *passed_time;
            passed_time = (char *)calloc(1, strlen(frmtd_timer));
            sprintf(passed_time, "%s", frmtd_timer);
            queue_next->recv_time = passed_time;
            char *passed_firstline;
            passed_firstline = (char *)calloc(1, strlen(firstline));
            sprintf(passed_firstline, "%s", firstline);
            queue_next->rqst = passed_firstline;
            queue_next->sockfd = new_sockfd;
            char *passed_dir;
            passed_dir = (char *)calloc(1, PATH_MAX);
            sprintf(passed_dir, "%s", working_dir);
            queue_next->directory = passed_dir;
            char *passed_filename;
            passed_filename = (char *)calloc(1, strlen(filename));
            sprintf(passed_filename, "%s", filename);
            queue_next->filename = passed_filename;
            queue_next->filesize = file_info.st_size;
            queue_next->maxfilesize = maxfilesize;
            queue_next->mode = 0;
            
            pthread_mutex_lock(&queue_mutex);
            /* FCFS or SJF placement within the queue linked list */
            if(strcmp(sched, "FCFS") == 0) {     
                queue_next->next = queue_last;
                queue_next->prev = queue_last->prev;
                
                queue_last->prev->next = queue_next;
                queue_last->prev = queue_next;
                
                queue_last = queue_next;
                
                if((queue->rqst == NULL) && (queue->mode == -1)) {
                    queue = queue_last;
                }
            } //FCFS            
            else {
                struct rqst_llist *temp_entry = queue;
                int i = 0;
                while((temp_entry->maxfilesize != -1) && (temp_entry->maxfilesize <= maxfilesize)) {
                    temp_entry = temp_entry->prev;
                    i = i + 1;
                }
                queue_next->next = temp_entry->next;
                queue_next->prev = temp_entry;
                
                temp_entry->next->prev = queue_next;
                temp_entry->next = queue_next;
                
                if(i == 0) {
                    queue = queue_next;
                }
                
                if((temp_entry->rqst == NULL) && (temp_entry->mode == -1)) {
                    queue_last = queue_next;
                }
            } //SJF
            pthread_mutex_unlock(&queue_mutex);
        } //GET
        else if((strcmp(msg_received, "HEAD") == 0) && ((httpbit = ((http_tokens == NULL) || (strlen(http_tokens) < 6)) ? -1 : strncmp(http_tokens, "HTTP/1", 6)) == 0)) {
            char working_dir[PATH_MAX];
            memset(working_dir, 0, PATH_MAX);
            getcwd(working_dir, PATH_MAX);
            
            msg_tokens = strtok(NULL, " ");
            if((msg_tokens[0] == '~') || (msg_tokens[1] == '~')) {
                usr_tokens = strtok(usr_msg, " ~/");
                usr_tokens = strtok(NULL, " ~/");
                extra_tokens = strtok(orig_msg, " ~");
                extra_tokens = strtok(NULL, " ~");
                extra_tokens = strtok(NULL, " ~");
                sprintf(mod_filename, "/home/%s/myhttpd%s", usr_tokens, &extra_tokens[strlen(usr_tokens)]);
                filename = mod_filename;
            }
            else {
                filename = msg_tokens;
            }
            
            struct stat file_info; //contains information about a file
            char file_dir[PATH_MAX];
            strcpy(file_dir, working_dir);
        dirindexfoundmaxsizetwo:
            if((stat(strcat(file_dir, filename), &file_info) == -1) || (strncmp(&filename[1], ".", 1) == 0)) {
                //do nothing
            }
            else if(S_ISDIR(file_info.st_mode)) {
                DIR *directory; //the directory stream
                struct dirent *entry; //the current entry in the directory stream
                
                directory = opendir(file_dir);
                for(entry = readdir(directory); entry != NULL; entry = readdir(directory)) {
                    if(strncmp(entry->d_name, ".", 1) == 0) {
                        continue;
                    } //files starting with a "." are ignored
                    else {
                        struct stat listing_info;
                        char temp_str[PATH_MAX];
                        strcpy(temp_str, file_dir);
                        strcat(temp_str, "/");
                        stat(strcat(temp_str, entry->d_name), &listing_info);
                        
                        if((strncmp(entry->d_name, "index.html", 10) == 0) || (strncmp(entry->d_name, "index.htm", 9) == 0)) {
                            char name_tobe[sizeof(entry->d_name) + 1]; //will hold the new name to be
                            strcpy(name_tobe, "/"); //adds the initial / to the name
                            strcat(name_tobe, entry->d_name); //adds the actual name after the /
                            memset(&file_info, 0, sizeof(file_info)); //clears the file_info structure
                            strcat(working_dir, filename);
                            filename = name_tobe; //sets the new filename
                            closedir(directory); //closes the directory stream
                            chdir(file_dir); //switches the directory to this new one
                            dir_changed = 1;
                            goto dirindexfoundmaxsizetwo; //redo stat() and checks on file
                        } //determines in an the directory contains "index.html
                    } //helps determine the new filename if an index.htm or index.html is in a directory
                }
                closedir(directory); //closes the directory stream
            }
            
            if(dir_changed == 1) {
                memset(working_dir, 0, PATH_MAX);
                getcwd(working_dir, PATH_MAX);
                chdir(orig_dir);
            }
            
            /* formatted request queuing */
            struct rqst_llist *queue_next;
            queue_next = (struct rqst_llist *)calloc(1, sizeof(struct rqst_llist));
            char *passed_addr;
            passed_addr = (char *)calloc(1, strlen(remoteip));
            sprintf(passed_addr, "%s", remoteip);
            queue_next->remoteip_addr = passed_addr;
            char *passed_time;
            passed_time = (char *)calloc(1, strlen(frmtd_timer));
            sprintf(passed_time, "%s", frmtd_timer);
            queue_next->recv_time = passed_time;
            char *passed_firstline;
            passed_firstline = (char *)calloc(1, strlen(firstline));
            sprintf(passed_firstline, "%s", firstline);
            queue_next->rqst = passed_firstline;
            queue_next->sockfd = new_sockfd;
            char *passed_dir;
            passed_dir = (char *)calloc(1, PATH_MAX);
            sprintf(passed_dir, "%s", working_dir);
            queue_next->directory = passed_dir;
            char *passed_filename;
            passed_filename = (char *)calloc(1, strlen(filename));
            sprintf(passed_filename, "%s", filename);
            queue_next->filename = passed_filename;
            queue_next->filesize = 0;
            queue_next->maxfilesize = 0;
            queue_next->mode = 1;
            
            pthread_mutex_lock(&queue_mutex);
            /* FCFS or SJF placement within the queue linked list */
            if(strcmp(sched, "FCFS") == 0) {     
                queue_next->next = queue_last;
                queue_next->prev = queue_last->prev;
                
                queue_last->prev->next = queue_next;
                queue_last->prev = queue_next;
                
                queue_last = queue_next;
                
                if((queue->rqst == NULL) && (queue->mode == -1)) {
                    queue = queue_last;
                }
            } //FCFS
            else {
                struct rqst_llist *temp_entry = queue;
                int i = 0;
                while((temp_entry->maxfilesize != -1) && (temp_entry->maxfilesize <= 0)) {
                    temp_entry = temp_entry->prev;
                    i = i + 1;
                }
                queue_next->next = temp_entry->next;
                queue_next->prev = temp_entry;
                
                temp_entry->next->prev = queue_next;
                temp_entry->next = queue_next;
                
                if(i == 0) {
                    queue = queue_next;
                }
                
                if((temp_entry->rqst == NULL) && (temp_entry->mode == -1)) {
                    queue_last = queue_next;
                }
            } //SJF
            pthread_mutex_unlock(&queue_mutex);
        } //HEAD
        else {
            char working_dir[PATH_MAX];
            memset(working_dir, 0, PATH_MAX);
            getcwd(working_dir, PATH_MAX);
            
            /* formatted request queuing */
            struct rqst_llist *queue_next;
            queue_next = (struct rqst_llist *)calloc(1, sizeof(struct rqst_llist));
            char *passed_addr;
            passed_addr = (char *)calloc(1, strlen(remoteip));
            sprintf(passed_addr, "%s", remoteip);
            queue_next->remoteip_addr = passed_addr;
            char *passed_time;
            passed_time = (char *)calloc(1, strlen(frmtd_timer));
            sprintf(passed_time, "%s", frmtd_timer);
            queue_next->recv_time = passed_time;
            char *passed_firstline;
            passed_firstline = (char *)calloc(1, strlen(firstline));
            sprintf(passed_firstline, "%s", firstline);
            queue_next->rqst = passed_firstline;
            queue_next->sockfd = new_sockfd;
            char *passed_dir;
            passed_dir = (char *)calloc(1, PATH_MAX);
            sprintf(passed_dir, "%s", working_dir);
            queue_next->directory = passed_dir;
            char *passed_filename;
            passed_filename = (char *)calloc(1, 16);
            sprintf(passed_filename, "400 Bad Request");
            queue_next->filename = passed_filename;
            queue_next->filesize = 94;
            queue_next->maxfilesize = 94;
            queue_next->mode = 2;
            
            pthread_mutex_lock(&queue_mutex);
            /* FCFS or SJF placement within the queue linked list */
            if(strcmp(sched, "FCFS") == 0) {     
                queue_next->next = queue_last;
                queue_next->prev = queue_last->prev;
                
                queue_last->prev->next = queue_next;
                queue_last->prev = queue_next;
                
                queue_last = queue_next;
                
                if((queue->rqst == NULL) && (queue->mode == -1)) {
                    queue = queue_last;
                }
            } //FCFS
            else {
                struct rqst_llist *temp_entry = queue;
                int i = 0;
                while((temp_entry->maxfilesize != -1) && (temp_entry->maxfilesize <= 114)) {
                    temp_entry = temp_entry->prev;
                    i = i + 1;
                }
                queue_next->next = temp_entry->next;
                queue_next->prev = temp_entry;
                
                temp_entry->next->prev = queue_next;
                temp_entry->next = queue_next;
                
                if(i == 0) {
                    queue = queue_next;
                }
                
                if((temp_entry->rqst == NULL) && (temp_entry->mode == -1)) {
                    queue_last = queue_next;
                }
            } //SJF
            pthread_mutex_unlock(&queue_mutex);
        } //400 Bad Request
        sem_post(scheduling_sem); //increases the scheduling semaphore value
    }
    return 0;
}

/* Main (main thread is scheduling thread) */
int main(int argc, char *argv[]) {
    struct rqst_llist initial_queue; //the blank delimiter of our circular double linked list queue
    initial_queue.remoteip_addr = NULL;
    initial_queue.recv_time = NULL;
    initial_queue.sched_time = NULL;
    initial_queue.rqst = NULL;
    initial_queue.sockfd = -1;
    initial_queue.directory = NULL;
    initial_queue.filename = NULL;
    initial_queue.filesize = -1;
    initial_queue.maxfilesize = -1;
    initial_queue.mode = -1;
    initial_queue.next = &initial_queue;
    initial_queue.prev = &initial_queue;
    queue_last = &initial_queue;
    queue = &initial_queue;
    
    struct rqst_llist initial_exec_queue; //the blank delimiter of our circular double linked list execution queue
    initial_exec_queue.remoteip_addr = NULL;
    initial_exec_queue.recv_time = NULL;
    initial_exec_queue.sched_time = NULL;
    initial_exec_queue.rqst = NULL;
    initial_exec_queue.sockfd = -1;
    initial_exec_queue.directory = NULL;
    initial_exec_queue.filename = NULL;
    initial_exec_queue.filesize = -1;
    initial_exec_queue.maxfilesize = -1;
    initial_exec_queue.mode = -1;
    initial_exec_queue.next = &initial_exec_queue;
    initial_exec_queue.prev = &initial_exec_queue;
    exec_queue_last = &initial_exec_queue;
    exec_queue = &initial_exec_queue;
    
    if(pthread_mutex_init(&queue_mutex, NULL) != 0) {
        perror("pthread_mutex_init() error (queue_mutex)");
        exit(1);
    } //initializes the mutex and detects errors
    
    if((scheduling_sem = sem_open("myhttpd_scheduling_sem", O_CREAT, S_IRWXU, 0)) == SEM_FAILED) { //using sem_open for OS X compliance (sem_init not implemented in OS X)
        perror("sem_open() error (scheduling_sem)");
        exit(1);
    } //initializes the scheduling semaphore
    
    if(pthread_mutex_init(&exec_queue_mutex, NULL) != 0) {
        perror("pthread_mutex_init() error (exec_queue_mutex)");
        exit(1);
    } //initializes the execution mutex and detects errors
    
    if((exec_sem = sem_open("myhttpd_exec_sem", O_CREAT, S_IRWXU, 0)) == SEM_FAILED) { //using sem_open for OS X compliance (sem_init not implemented in OS X)
        perror("sem_open() error (exec_sem)");
        exit(1);
    } //initializes the scheduling semaphore
    
    if(pthread_mutex_init(&logg_mutex, NULL) != 0) {
        perror("pthread_mutex_init() error (logg_mutex)");
        exit(1);
    } //initializes the logg mutex and detects errors
    
    int port = 8080; //default port
    int argv_iterator; //iterator for argv
    int daemon = 0; //equals 1 when debugging is set and 0 otherwise
    int qtime = 60; //default queuing time
    int threadnum = 4; //default number of execution threads
    logging = 0; //default is no logging
    logfile = NULL; //null pointer when not set otherwise holds the name of the log file
    char *sched = "FCFS"; //default scheduling policy
    
    for(argv_iterator = 1; argv_iterator < argc; argv_iterator++) {
        if(strcmp(argv[argv_iterator], "-d") == 0) {
            daemon = 1;
            if(logging == 1) {
                logging = 3;
            }
            else {
                logging = 2;
            }
        }
        else if(strcmp(argv[argv_iterator], "-h") == 0) {
            h();
            exit(0);
        }
        else if(strcmp(argv[argv_iterator], "-l") == 0) {
            argv_iterator = argv_iterator + 1; //the actual log file name
            logfile = argv[argv_iterator];
            if(logging == 2) {
                logging = 3;
            }
            else {
                logging = 1;
            }
        }
        else if(strcmp(argv[argv_iterator], "-p") == 0) {
            argv_iterator = argv_iterator + 1; //the actual port
            int port_value = atoi(argv[argv_iterator]); //iteger port_value

            if((port_value >= 1024) && (port_value <= 65535)) {
                port = port_value;
            } //changes port to the given port
            else {
                fprintf(stderr, "invalid port number: %s\n", argv[argv_iterator]);
                exit(1);
            } //error detection for port number assignment
        }
        else if(strcmp(argv[argv_iterator], "-r") == 0) {
            argv_iterator = argv_iterator + 1; //the actual directory
            
            if((chdir(argv[argv_iterator])) != 0) {
                perror("chdir() error");
                exit(1);
            } //changes the working directory and detects erros on chrdir()
        }
        else if(strcmp(argv[argv_iterator], "-t") == 0) {
            argv_iterator = argv_iterator + 1; //the actual queuing time
            if(atoi(argv[argv_iterator]) < 0) {
                fprintf(stderr, "invalid time (needs >= 0) given: %s\n", argv[argv_iterator]);
                exit(1);
            }
            else {
                qtime = atoi(argv[argv_iterator]);
            } 
        }
        else if(strcmp(argv[argv_iterator], "-n") == 0) {
            argv_iterator = argv_iterator + 1; //the actual number of threads
            if(atoi(argv[argv_iterator]) > 0) {
                threadnum = atoi(argv[argv_iterator]);
            }
            else {
                fprintf(stderr, "invalid number of threads (needs > 0) given: %s\n", argv[argv_iterator]);
                exit(1);
            }
        }
        else if(strcmp(argv[argv_iterator], "-s") == 0) {
            argv_iterator = argv_iterator + 1; //the actual scheduling algorithm
            char *upper_sched = argv[argv_iterator]; //uppercase schedule
            
            int i = 0;
            char c[4] = {'\0', '\0', '\0', '\0'};
            while(upper_sched[i] != '\0') {
                c[i] = toupper(upper_sched[i]);
                i = i + 1;
            }
            upper_sched = c;
            
            if((strcmp(upper_sched, "FCFS") == 0) || (strcmp(upper_sched, "SJF") == 0)) {
                sched = upper_sched;
            }
            else {
                fprintf(stderr, "invalid scheduling algorithm: %s\n", upper_sched);
                exit(1);
            }
        }
        else {
            fprintf(stderr, "invalid option: %s\n", argv[argv_iterator]);
            exit(1);
        } //error detection for argv options
    } //iterates though argv assigning the given options
    
    struct server_args servargs;
    servargs.port_num = port;
    servargs.sched = sched;
    
    if(daemon == 0) {
        pid_t pid;
        if((pid = fork()) < 0) {
            perror("fork() error");
            exit(1);
        }
        else if(pid != 0) {
            exit(0);
        }
        setsid();
        umask(0);
    } //regular mode
    else {
        //do nothing
    } //debugging mode
    
    pthread_t *exec_thread;
    while(threadnum > 0) {
        exec_thread = (pthread_t *)malloc(sizeof(pthread_t));
        pthread_create(exec_thread, NULL, (void *)&execute_rqst, NULL);
        threadnum = threadnum - 1;
    }
    
    pthread_t server_thread; //queuing thread
    pthread_create(&server_thread, NULL, (void *)&server, &servargs);
    
    sleep(qtime);
    
    struct rqst_llist *to_exec;
    while(1) {
        sem_wait(scheduling_sem); //decreases the scheduling semaphore value
        if((queue->rqst != NULL) && (queue->mode != -1)) {
            /* choose from queue */
            pthread_mutex_lock(&queue_mutex);
            queue->prev->next = queue->next;
            queue->next->prev = queue->prev;
            to_exec = queue;
            queue = queue->prev;
            if((queue_last->next->rqst == NULL) && (queue_last->next->mode == -1)) {
                queue_last = queue;
            }
            pthread_mutex_unlock(&queue_mutex);
            
            /* request assign time (for logging) */
            time_t timer; //argument for time() and gmtime() with current time
            struct tm *tm_timer; //argument for strftime() with current time
            char *frmtd_timer; //argument for strftime() for current time - formatted as per project description
            frmtd_timer = (char *)calloc(1, 27);
            time(&timer); //gets the current time
            tm_timer = gmtime(&timer); //converts timer to a broken-down gmt time
            strftime(frmtd_timer, 27, "%d/%b/%Y:%X %z", tm_timer); //formats tm_timer
            to_exec->sched_time = frmtd_timer;

            /* assign to execute */
            pthread_mutex_lock(&exec_queue_mutex);
            to_exec->next = exec_queue_last;
            to_exec->prev = exec_queue_last->prev;
            exec_queue_last->prev->next = to_exec;
            exec_queue_last->prev = to_exec;
            exec_queue_last = to_exec;
            if((exec_queue->rqst == NULL) && (exec_queue->mode == -1)) {
                exec_queue = exec_queue_last;
            }
            pthread_mutex_unlock(&exec_queue_mutex);
        }
        sem_post(exec_sem);
    } //always pics the first element in the queue because the queue was already ordered by scheduling algorithm selected
    
    pthread_join(server_thread, NULL);
    
    return 0;
}
