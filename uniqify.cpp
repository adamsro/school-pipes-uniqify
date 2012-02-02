/*
 *
 * very helpful: http://tldp.org/LDP/lpg/node11.html
 * and http://stackoverflow.com/questions/1381089/multiple-fork-concurrency
 */

#include <vector>
#include <iostream>
#include <fstream>

#include <unistd.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>

using namespace std;

/*
 * Parse words from input stream. convert to lowercase
 * non-alphabetic characters delimit words are discarded
 */
void parser(int numsorts) {
}

void sort_processes() {
}

/*
 * Supress duplicate words, write to output
 */
void suppresser() {
}

int main(int argc, char* argv[]) {
    int pipes[2]; //  [0] is set up for reading, [1] is set up for writing
    pid_t child_pid;
    std::string word;
    std::ifstream infile;
    int status;
    int numChildren;
    pid_t *childPids = NULL;
    int *pipesArr = NULL;
    char readbuf[80];
    int std0;
    int std1;


    if (argc != 2) {
        cout << "USAGE: uniqify [num_sort_processes]";
        //exit(EXIT_SUCCESS);
    }
    numChildren = atoi(argv[2]);
    numChildren = 3;

    //std::vector<pid_t> childPids;
    //std::vector<int> pipesArr;
    childPids = malloc(numChildren * sizeof (pid_t));
    pipesArr = malloc(numChildren * sizeof (pipes));

    std0 = dup(0); // backup stdin
    std1 = dup(1); // backup stdout

    /* Fork all the children and save their id's*/
    for (int i = 0; i < numChildren; ++i) {
        if (pipe(pipes) < 0) { // use mkfifo ?
            std::cerr << "Failed to create pipes." << endl;
            return -2;
        }
        pipesArr[i] = pipes;
        switch (child_pid = fork()) {
            case -1:
                std::cout << "Failed to fork." << endl;
                exit(EXIT_FAILURE);
            case 0: // is Child
                /* Close stdin and duplicate fd[0] to this position*/
                dup2(0, pipes[0]);
                dup2(1, pipes[1]);
                execlp("sort", "sort", NULL);
                //execl("/usr/bin/sort", "sort");
                _exit(127); /* Failed exec */
                break;
            default: // is Parent
                childPids[i] = child_pid;
                break;
        }
    }

    infile.open("test-loves-labors-lost.txt"); // <-- lots of words

    /* Processing loop */
    //while (infile.good()) {
    int j = 0;
    while (infile >> word) {
        //  >> ws; // eats white space
        //cout << word << "\n";
        fgets(readbuf, 80, word);
        //if (infile.eof()) break;
        fputs(readbuf, pipesArr[j]);
        (j < numChildren) ? ++j : j = 0;
    }

    infile.close();

    close(pipes);

    /* Wait for children to exit */
    int stillWaiting;
    do {
        stillWaiting = 0;
        for (int i = 0; i < numChildren; ++i) {
            if (childPids[i] > 0) {
                if (waitpid(childPids[i], NULL, WNOHANG) == 0) {
                    /* Child is done */
                    childPids[i] = 0;
                } else {
                    /* Still waiting on this child */
                    stillWaiting = 1;
                }
            }
            /* Give up timeslice and prevent hard loop: this may not work on all flavors of Unix */
            sleep(0);
        }
    } while (stillWaiting);


    /* Cleanup */
    free(childPids);
    exit(EXIT_SUCCESS);
}