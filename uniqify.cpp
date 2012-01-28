#include <iostream>
#include <fstream>
// Required by for routine
#include <sys/types.h>
#include <unistd.h>

using namespace std;

/*
 * Parse words from input stream. convert to lowercase
 * non-alphabetic characters delimit words are discarded
 */
void parser(int numsorts) {
    cout << "input: ";
    char input[50];
    //fputs(input, sizeof input, sort);
    fgets(input, sizeof (input), stdin);
    cout << input << "\n";
}

/*
 * Supress duplicate words, write to output
 */
void suppresser() {

}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cout << "usage: uniqify [number of sort processes]";
    }
    //parser(atoi(argv[1]));
    static int idata = 111;
    int istack = 222;
    pid_t childPid;
    switch (childPid = fork()) {
        case -1:
            cout << "Failed to fork.";
            exit(EXIT_FAILURE);
        case 0:
            idata *= 3;
            istack *= 3;
            break;
        default:
            sleep(3);
            break;
    }
    /* Allocated in data segment */
    /* Allocated in stack segment */
    /* Give child a chance to execute */
    /* Both parent and child come here */
    printf("PID=%ld %s idata=%d istack=%d\n",
            (long) getpid(), (childPid == 0) ? "(child) " : "(parent)", idata, istack);
    exit(EXIT_SUCCESS);
}
