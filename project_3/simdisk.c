#include <stdio.h>
#include <stdlib.h>

#define FCFS       7934865
#define SSTF       4585374
#define C_SCAN     6603678
#define LOOK       5875351
#define STDIN_FILE ".tmp.stdin.input"

struct node {
    int data;
    struct node *prev;
    struct node *next;
};

int main(int argc, char *argv[]) {
    int noc = 200;
    long dsp = FCFS;
    int stdin_input = 1;
    FILE *in_fi;
    int argv_iterator;
    char *io_req;
    int int_io_req;
    char c;
    int req_length;
    int last_req = -1;
    struct node *req = NULL;
    int illegal_flagged = 0;
    fpos_t pos;
    int total_cyl = 0;
    
    for(argv_iterator = 1; argv_iterator < argc; argv_iterator++) {
        if((argv[argv_iterator][0] == '-') && 
           (argv[argv_iterator][2] == '\0')) {
            switch(argv[argv_iterator][1]) {
                case 'h':
                    printf("-h\t\t\t: Print a usage summary with all options and exit.\n-n number-of-cylinders\t: Set the number of cylinders on the disk.\n\t\t\t  By default it should be 200.\n\t\t\t  (Be careful that disk cylinder indexes\n\t\t\t  would be between 0-199 in this case.)\n-d disk-shed-policy\t: Set the disk scheduling policy.\n\t\t\t  It can be either FCFC (First-come-first-served),\n\t\t\t  SSTF (Shortest-seek-time-first),\n\t\t\t  C-SCON, or LOOK.\n\t\t\t  The default will be FCFS.\n\t\t\t  Assume at the t=t0, the disk head was moving from 0\n\t\t\t  towards other end of the disk (i.e. cylinder n).\n-i input file\t\t: Read the I/O request sequence\n\t\t\t  from a specified file.\n\t\t\t  If not given, the sequence should be read\n\t\t\t  from STDIN (ended with ENTER).\n");
                    exit(0);
                case 'n':
                    argv_iterator++;
                    if(argv_iterator < argc) {
                        if((noc = atoi(argv[argv_iterator])) < 1) {
                            fprintf(stderr, "Invalid number of cylinders!\n");
                            exit(1);
                        }
                    }
                    else {
                        fprintf(stderr, "\"-n\" must be followed by the number of cylinders on the disk!\n");
                        exit(1);
                    }
                    break;
                case 'd':
                    argv_iterator++;
                    if(argv_iterator < argc) {
                        if((argv[argv_iterator][0] == 'C') && 
                           (argv[argv_iterator][1] == '-') && 
                           (argv[argv_iterator][2] != '\0')) {
                            dsp = a64l(&(argv[argv_iterator][2]));
                        }
                        else {
                            dsp = a64l(argv[argv_iterator]);
                        }
                        if((dsp != FCFS) && 
                           (dsp != SSTF) && 
                           (dsp != C_SCAN) && 
                           (dsp != LOOK)) {
                            fprintf(stderr, "Invalid disk scheduling policy!\n");
                            exit(1);
                        }
                    }
                    else {
                        fprintf(stderr, "\"-d\" must be followed by the disk scheduling policy!\n");
                        exit(1);
                    }
                    break;
                case 'i':
                    argv_iterator++;
                    if(argv_iterator < argc) {
                        stdin_input = 0;
                        in_fi = fopen(argv[argv_iterator], "r");
                        if(in_fi == NULL) {
                            perror("fopen() error");
                            exit(1);
                        }
                    }
                    else {
                        fprintf(stderr, "\"-i\" must be followed by a filename!\n");
                        exit(1);
                    }
                    break;
                default:
                    goto invalid_option;
            }
        }
        else {
        invalid_option:
            fprintf(stderr, "Invalid options specified!\n");
            exit(1);
        }
    }
    
    if(stdin_input == 1) {
        in_fi = fopen(STDIN_FILE, "w");
        char c = getchar();
        while(c != '\n') {
            fputc(c, in_fi);
            c = getchar();
        }
        freopen(STDIN_FILE, "r", in_fi);
    }
    
    switch(dsp) {
        case FCFS:
            printf("FCFS processing order\t\t:");
            while(!feof(in_fi)) {
                fgetpos(in_fi, &pos);
                req_length = 0;
                c = fgetc(in_fi);
                while((c == ' ') ||
                      (c == '\n')) {
                    c = fgetc(in_fi);
                }
                while((c != ' ') &&
                      (c != '\n') &&
                      (c != EOF)) {
                    req_length++;
                    c = fgetc(in_fi);
                }
                req_length = (req_length != 0) ? (req_length + 1) : 0;
                fsetpos(in_fi, &pos);
                io_req = (char *)malloc(req_length);
                
                if(fscanf(in_fi, "%s", io_req) > 0) {
                    int_io_req = atoi(io_req);
                    if(((io_req[0] != '0') && (int_io_req == 0)) ||
                       (int_io_req < 0) ||
                       (int_io_req >= noc)) {
                        switch(illegal_flagged) {
                            case 0:
                                fprintf(stderr, "\nIllegal I/O request\t\t:");
                                illegal_flagged = 1;
                            case 1:
                                fprintf(stderr, " %s", io_req);
                        }
                    }
                    else {
                        printf(" %i", int_io_req);
                        switch(last_req) {
                            case -1:
                                last_req = int_io_req;
                                break;
                            default:
                                total_cyl += abs(last_req - int_io_req);
                                last_req = int_io_req;
                        }
                    }
                }
                else {
                    break;
                }
                
                free(io_req);
            }
            break;
        case SSTF:
            printf("SSTF processing order\t\t:");
            while(!feof(in_fi)) {
                fgetpos(in_fi, &pos);
                req_length = 0;
                c = fgetc(in_fi);
                while((c == ' ') ||
                      (c == '\n')) {
                    c = fgetc(in_fi);
                }
                while((c != ' ') &&
                      (c != '\n') &&
                      (c != EOF)) {
                    req_length++;
                    c = fgetc(in_fi);
                }
                req_length = (req_length != 0) ? (req_length + 1) : 0;
                fsetpos(in_fi, &pos);
                io_req = (char *)malloc(req_length);
                
                if(fscanf(in_fi, "%s", io_req) > 0) {
                    int_io_req = atoi(io_req);
                    if(((io_req[0] != '0') && (int_io_req == 0)) ||
                       (int_io_req < 0) ||
                       (int_io_req >= noc)) {
                        switch(illegal_flagged) {
                            case 0:
                                fprintf(stderr, "\nIllegal I/O request\t\t:");
                                illegal_flagged = 1;
                            case 1:
                                fprintf(stderr, " %s", io_req);
                        }
                    }
                    else {
                        struct node *n = (struct node *)malloc(sizeof(struct node));
                        struct node *temp = req;
                        n->data = int_io_req;
                        if(req == NULL) {
                            n->next = NULL;
                            req = n;
                        }
                        else {
                        sstf_retry:
                            if(temp->next == NULL) {
                                n->next = NULL;
                                temp->next = n;
                                n->prev = temp;
                            }
                            else if(abs(temp->data - int_io_req) < abs(temp->next->data - temp->data)) {
                                temp->next->prev = n;
                                n->next = temp->next;
                                temp->next = n;
                                n->prev = temp;
                            }
                            else {
                                temp = temp->next;
                                goto sstf_retry;
                            }
                        }
                    }
                }
                else {
                    break;
                }
                
                free(io_req);
            }
            while(req != NULL) {
                printf(" %i", req->data);
                switch(last_req) {
                    case -1:
                        last_req = req->data;
                        break;
                    default:
                        total_cyl += abs(last_req - req->data);
                        last_req = req->data;
                }
                struct node *temp = req->next;
                free(req);
                req = temp;
            }
            break;
        case C_SCAN:
            printf("C-SCAN processing order\t\t:");
            int begin_flagged = 0;
            int end_flagged = 0;
            while(!feof(in_fi)) {
                fgetpos(in_fi, &pos);
                req_length = 0;
                c = fgetc(in_fi);
                while((c == ' ') ||
                      (c == '\n')) {
                    c = fgetc(in_fi);
                }
                while((c != ' ') &&
                      (c != '\n') &&
                      (c != EOF)) {
                    req_length++;
                    c = fgetc(in_fi);
                }
                req_length = (req_length != 0) ? (req_length + 1) : 0;
                fsetpos(in_fi, &pos);
                io_req = (char *)malloc(req_length);
                
                if(fscanf(in_fi, "%s", io_req) > 0) {
                    int_io_req = atoi(io_req);
                    if(((io_req[0] != '0') && (int_io_req == 0)) ||
                       (int_io_req < 0) ||
                       (int_io_req >= noc)) {
                        switch(illegal_flagged) {
                            case 0:
                                fprintf(stderr, "\nIllegal I/O request\t\t:");
                                illegal_flagged = 1;
                            case 1:
                                fprintf(stderr, " %s", io_req);
                        }
                    }
                    else {
                        struct node *n = (struct node *)malloc(sizeof(struct node));
                        struct node *temp = req;
                        n->data = int_io_req;
                        if(req == NULL) {
                            struct node *begin = (struct node *)malloc(sizeof(struct node));
                            begin->data = 0;
                            struct node *end = (struct node *)malloc(sizeof(struct node));
                            end->data = (noc - 1);
                            n->next = end;
                            end->prev = n;
                            end->next = begin;
                            begin->prev = end;
                            begin->next = NULL;
                            req = n;
                        }
                        else {                            
                        c_scan_retry:
                            if((int_io_req == 0) && 
                               (begin_flagged != 1)) {
                                begin_flagged = 1;
                            }
                            else if((int_io_req == (noc - 1)) && 
                                    (end_flagged != 1)) {
                                end_flagged = 1;
                            }
                            else {
                                if(temp->next == NULL) {
                                    n->next = NULL;
                                    temp->next = n;
                                    n->prev = temp;
                                }
                                else if(int_io_req < temp->data) {
                                    temp = temp->next;
                                    goto c_scan_retry;
                                }
                                else if((temp->next->data < temp->data) ||
                                        (abs(temp->data - int_io_req) < abs(temp->next->data - temp->data))) {
                                    temp->next->prev = n;
                                    n->next = temp->next;
                                    temp->next = n;
                                    n->prev = temp;
                                }
                                else {
                                    temp = temp->next;
                                    goto c_scan_retry;
                                }
                            }
                        }
                    }
                }
                else {
                    break;
                }
                
                free(io_req);
            }
            while(req != NULL) {
                if((req->data == (noc - 1)) &&
                   (req->next->data == 0) &&
                   (req->next->next == NULL) &&
                   (end_flagged == 0)) {
                }
                else if((req->data == 0) &&
                        (req->next == NULL) &&
                        (begin_flagged == 0)) {
                }
                else  {
                    printf(" %i", req->data);
                    switch(last_req) {
                        case -1:
                            last_req = req->data;
                            break;
                        default:
                            total_cyl += abs(last_req - req->data);
                            last_req = req->data;
                    }
                }
                struct node *temp = req->next;
                free(req);
                req = temp;
            }
            break;
        case LOOK:
            printf("LOOK processing order\t\t:");
            while(!feof(in_fi)) {
                fgetpos(in_fi, &pos);
                req_length = 0;
                c = fgetc(in_fi);
                while((c == ' ') ||
                      (c == '\n')) {
                    c = fgetc(in_fi);
                }
                while((c != ' ') &&
                      (c != '\n') &&
                      (c != EOF)) {
                    req_length++;
                    c = fgetc(in_fi);
                }
                req_length = (req_length != 0) ? (req_length + 1) : 0;
                fsetpos(in_fi, &pos);
                io_req = (char *)malloc(req_length);
                
                if(fscanf(in_fi, "%s", io_req) > 0) {
                    int_io_req = atoi(io_req);
                    if(((io_req[0] != '0') && (int_io_req == 0)) ||
                       (int_io_req < 0) ||
                       (int_io_req >= noc)) {
                        switch(illegal_flagged) {
                            case 0:
                                fprintf(stderr, "\nIllegal I/O request\t\t:");
                                illegal_flagged = 1;
                            case 1:
                                fprintf(stderr, " %s", io_req);
                        }
                    }
                    else {
                        struct node *n = (struct node *)malloc(sizeof(struct node));
                        struct node *temp = req;
                        int mode = 0;
                        n->data = int_io_req;
                        if(req == NULL) {
                            n->next = NULL;
                            req = n;
                        }
                        else {                            
                        look_retry:
                            if(temp->next == NULL) {
                                n->next = NULL;
                                temp->next = n;
                                n->prev = temp;
                            }
                            else if((mode == 0) && 
                                    (int_io_req < temp->data)) {
                                mode = 1;
                                goto look_retry;
                            }
                            else if((mode == 0) &&
                                    ((temp->next->data < temp->data) ||
                                    (abs(temp->data - int_io_req) < abs(temp->next->data - temp->data)))) {
                                temp->next->prev = n;
                                n->next = temp->next;
                                temp->next = n;
                                n->prev = temp;
                            }
                            else if((mode == 1) &&
                                    (temp->next->data < int_io_req)) {
                                        temp->next->prev = n;
                                        n->next = temp->next;
                                        temp->next = n;
                                        n->prev = temp;
                                    }

                            else {
                                temp = temp->next;
                                goto look_retry;
                            }
                        }
                    }
                }
                else {
                    break;
                }
                
                free(io_req);
            }
            while(req != NULL) {
                printf(" %i", req->data);
                switch(last_req) {
                    case -1:
                        last_req = req->data;
                        break;
                    default:
                        total_cyl += abs(last_req - req->data);
                        last_req = req->data;
                }
                struct node *temp = req->next;
                free(req);
                req = temp;
            }
    }
    fprintf(stderr, "\n");
    printf("\n# of cylinders traveled\t\t: %i\n", total_cyl);
    
    fclose(in_fi);
    if(stdin_input == 1) {
        remove(STDIN_FILE);
    }
}
