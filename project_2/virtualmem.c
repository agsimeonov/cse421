#include <stdio.h>
#include <stdlib.h>

#define FIFO 6886673
#define SC 926
#define LFU 132183
#define STDIN_FILE ".tmp.stdin.input"

int pgfault(int n, int *array, int size) {
    int i = 0;
    while(i < size) {
        if(array[i] == n) {
            i = 0;
            break;
        }
        i++;
    }
    return i;
}

int scpgfault(int n, int *array, int *array_monitor, int size) {
    int i = 0;
    while(i < size) {
        if(array[i] == n) {
            array_monitor[i] = 1;
            i = 0;
            break;
        }
        i++;
    }
    return i;
}

int lfupgfault(int n, int *array, int *array_monitor, int size) {
    int i = 0;
    while(i < size) {
        if(array[i] == n) {
            array_monitor[i]++;
            i = 0;
            break;
        }
        i++;
    }
    return i;
}

int futureuse(int n, FILE *in_fi) {
    int i = 0;
    int page_ref;
    fpos_t p;
    fgetpos(in_fi, &p);
    while(!feof(in_fi)) {
        if(fscanf(in_fi, "%i,", &page_ref) > 0) {
            if(n == page_ref) {
                break;
            }
        }
        else {
            i++;
            break;
        }
        i++;
    }
    fsetpos(in_fi, &p);
    return i;
}

int maxarrbyindex(int *array, int size) {
    int i = 0;
    int max = 0;
    int max_index = 0;
    while(i < size) {
        if(array[i] > max) {
            max = array[i];
            max_index = i;
        }
        i++;
    }
    return max_index;
}

int minarrbyindex(int *array, int *lfufifo, int size) {
    int i = 0;
    int min = array[0];
    int min_index = 0;
    while(i < size) {
        if(array[i] < min) {
            min = array[i];
            min_index = i;
        }
        else if(array[i] == min) {
            if(lfufifo[i] < lfufifo[min_index]) {
                min_index = i;
            }
        }
        i++;
    }
    return min_index;
}

int main(int argc, char *argv[]) {
    int av_fr = 5;
    long re_po = FIFO;
    int stdin_input = 1;
    FILE *in_fi;
    
    int argv_iterator;
    for(argv_iterator = 1; argv_iterator < argc; argv_iterator++) {
        if((argv[argv_iterator][0] == '-') && (argv[argv_iterator][2] == '\0')) {
            switch(argv[argv_iterator][1]) {
                case 'h':
                    printf("-h\t\t\t: Print a usage summary with all options and exit.\n-f available-frames\t: Set the number of available frames.\n\t\t\t  By default it should be 5.\n-r replacement-policy\t: Set the page replacement policy.\n\t\t\t  It can be either FIFO (First-in-first- out),\n\t\t\t  SC (second chance/clock),\n\t\t\t  or LFU (Least-frequently-used).\n\t\t\t  The default will be FIFO.\n-i input file\t\t: Read the page reference sequence\n\t\t\t  from a specified file.\n\t\t\t  If not given, the sequence should be read\n\t\t\t  from STDIN (ended with ENTER).\n");
                    exit(0);
                case 'f':
                    argv_iterator++;
                    if(argv_iterator < argc) {
                        av_fr = atoi(argv[argv_iterator]);
                    }
                    else {
                        fprintf(stderr, "\"-f\" must be followed by the number of available frames!\n");
                        exit(1);
                    }
                    break;
                case 'r':
                    argv_iterator++;
                    if(argv_iterator < argc) {
                        re_po = a64l(argv[argv_iterator]);
                        if((re_po != FIFO) && (re_po != SC) && (re_po != LFU)) {
                            fprintf(stderr, "Invalid page replacement policy!\n");
                            exit(1);
                        }
                    }
                    else {
                        fprintf(stderr, "\"-r\" must be followed by the page replacement policy!\n");
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
    
    int frames[av_fr];
    int frames_monitor[av_fr];
    int lfufifo[av_fr];
    int i = 0;
    while(i < av_fr) {
        frames[i] = -1;
        frames_monitor[i] = 0;
        lfufifo[i];
        i++;
    }
    int page_ref;
    int fr_pointer = 0;
    int repl = 0;
    
    switch(re_po) {
        case FIFO:
            while(!feof(in_fi)) {
                if(fscanf(in_fi, "%i,", &page_ref) > 0) {
                    if(pgfault(page_ref, frames, av_fr) != 0) {
                        frames[fr_pointer] = page_ref;
                        fr_pointer = (fr_pointer + 1) % av_fr;
                        repl++;
                    }
                }
                else {
                    break;
                }
            }
            break;
        case SC:
            while(!feof(in_fi)) {
                if(fscanf(in_fi, "%i,", &page_ref) > 0) {
                    if(scpgfault(page_ref, frames, frames_monitor, av_fr) != 0) {
                    secondchance:
                        if(frames_monitor[fr_pointer] == 0) {
                            frames[fr_pointer] = page_ref;
                            frames_monitor[fr_pointer] = 0;
                            fr_pointer = (fr_pointer + 1) % av_fr;
                        }
                        else {
                            frames_monitor[fr_pointer] = 0;
                            fr_pointer = (fr_pointer + 1) % av_fr;
                            goto secondchance;
                        }
                        repl++;
                    }
                }
                else {
                    break;
                }
            }
            break;
        case LFU:
            i = 0;
            while(!feof(in_fi)) {
                if(fscanf(in_fi, "%i,", &page_ref) > 0) {
                    i++;
                }
                else {
                    break;
                }
            }
            int lfurefs[i][2];
            rewind(in_fi);
            int j;
            for(j = 0; j < i; j++) {
                while(!feof(in_fi)) {
                    if(fscanf(in_fi, "%i,", &page_ref) > 0) {
                        int k;
                        for(k = 0; k < j; k++) {
                            if(page_ref == lfurefs[k][0]) {
                                break;
                            }
                        }
                        if(j == k) {
                            lfurefs[j][0] = page_ref;
                            lfurefs[j][1] = 0;
                            j++;
                        }
                    }
                    else {
                        break;
                    }
                }
                if(j == i) {
                    break;
                }
                lfurefs[j][0] = -1;
                lfurefs[j][1] = -1;
            }
            rewind(in_fi);
            int sz = i;
            while(!feof(in_fi)) {
                if(fscanf(in_fi, "%i,", &page_ref) > 0) {
                    int l;
                    for(l = 0; l < sz; l++) {
                        if(lfurefs[l][0] == page_ref) {
                            lfurefs[l][1]++;
                            break;
                        }
                    }
                    int f;
                    int g;
                    if(lfupgfault(page_ref, frames, frames_monitor, av_fr) != 0) {
                        i = minarrbyindex(frames_monitor, lfufifo, av_fr);
                        g = lfufifo[i];
                        lfufifo[i] = (av_fr - 1);
                        f = 0;
                        while(f < av_fr) {
                            if(f != i) {
                                if(g < lfufifo[f]) {
                                    lfufifo[f]--;
                                }
                            }
                            f++;
                        }
                        frames[i] = page_ref;
                        frames_monitor[i] = lfurefs[l][1];
                        repl++;
                    }
                }
                else {
                    break;
                } 
            }
            break;
    }
    
    rewind(in_fi);
    int full = 0;
    i = 0;
    while(i < av_fr) {
        frames[i] = -1;
        frames_monitor[i] = futureuse(frames[i], in_fi);
        i++;
    }
    int repl_opt = 0;
    
    while(!feof(in_fi)) {
        if(fscanf(in_fi, "%i,", &page_ref) > 0) {
            int j = 0;
            if(pgfault(page_ref, frames, av_fr) != 0) {
                if(full != av_fr) {
                    frames[full] = page_ref;
                    frames_monitor[full] = futureuse(frames[full], in_fi);
                    while(j < av_fr) {
                        if(j != full) {
                            frames_monitor[j]--;
                        }
                        j++;
                    }
                    full++;
                }
                else {
                    i = maxarrbyindex(frames_monitor, av_fr);
                    frames[i] = page_ref;
                    frames_monitor[i] = futureuse(frames[i], in_fi);
                    while(j < av_fr) {
                        if(j != i) {
                            frames_monitor[j]--;
                        }
                        j++;
                    }
                }            
                repl_opt++;
            }
            else {
                i = 0;
                while(i < av_fr) {
                    if(frames[i] == page_ref) {
                        break;
                    }
                    i++;
                }
                frames_monitor[i] = futureuse(frames[i], in_fi);
                j = 0;
                while(j < av_fr) {
                    if(j != i) {
                        frames_monitor[j]--;
                    }
                    j++;
                }
            }
        }
	else {
            break;
        }
    }
    
    fclose(in_fi);
    if(stdin_input == 1) {
        remove(STDIN_FILE);
    }
    
    repl = repl < (av_fr + 1) ? 0 : (repl - av_fr);
    repl_opt = repl_opt < (av_fr + 1) ? 0 : (repl_opt - av_fr);
    float penalty = (repl == repl_opt) ? 0 : (((float)repl/(float)repl_opt) - 1) * 100;
    printf("\n# of page replacements with %s  \t: %i\n# of page replacements with Optimal\t: %i\n%% penalty using %s \t\t\t: %.1f%%\n", l64a(re_po), repl, repl_opt, l64a(re_po), penalty);
}
