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

#define NUM_OF(x) (sizeof (x) / sizeof *(x))

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

int* createArray(const std::vector< int >& v) {
    int* result = new int [v.size()];
    memcpy(result, &v.front(), v.size() * sizeof ( int));
    return result;
}

vector <int> createVector(int* arr) {
    std::vector<int> V(*arr, NUM_OF(arr));
    return V;
}
//for(std::vector<int >::iterator it = v.begin(); it != v.end(); ++it) {
//    /* std::cout << *it; ... */
//}

int main(int argc, char* argv[]) {
    const int VERBOSE = 1;
    const int PIPE_READ = 0;
    const int PIPE_WRITE = 1;
    typedef int pipe_t[2];

    pid_t child_pid;
    std::string word;
    std::ifstream infile;
    int numChildren;

    if (argc != 2) {
        std::cout << "USAGE: uniqify [num_sort_processes]";
        //exit(EXIT_SUCCESS);
    }
    numChildren = atoi(argv[2]);
    numChildren = 3;

    std::vector<pid_t> childPids(numChildren, 0);
    std::vector<std::vector<int> > pipes(numChildren, std::vector<int>(2, 0));
    //childPids = malloc(numChildren * sizeof (pid_t));
    //pipesArr = malloc(numChildren * 2 * sizeof (int));

    /* Fork all the children and save their id's*/
    pipe_t fd;
    for (int i = 0; i < numChildren; ++i) {
        if (pipe(fd) < 0) { // use mkfifo ?
            std::cerr << "Failed to create pipes." << endl;
            return -2;
        }
//        FILE*read = fdopen(fd[PIPE_READ]);
//        FILE*write = fdopen(fd[PIPE_READ]);
        std::vector<int> V(fd, fd + sizeof (fd) / sizeof (int));
        pipes.at(i) = V;
        switch (child_pid = fork()) {
            case -1:
                std::cout << "Failed to fork." << endl;
                exit(EXIT_FAILURE);
            case 0: // is Child
                /* Culose stdin and duplicate PIPE_READ to this position*/
                dup2(STDIN_FILENO, pipes.at(i)[PIPE_READ]);
                dup2(STDOUT_FILENO, pipes.at(i)[PIPE_WRITE]);
                execlp("sort", "sort", NULL);
                //execl("/usr/bin/sort", "sort");
                _exit(127); /* Failed exec */
                break;
            default: // is Parent
                childPids.at(i) = child_pid;
                break;
        }
    }
    if (VERBOSE) {
        cout << "\npids" << endl;
        for (std::vector<int >::iterator it = childPids.begin(); it != childPids.end(); ++it) {
            std::cout << *it << endl;
        }
        cout << "pipes" << endl;
        for (int i = 0; i < pipes.size(); ++i) {
            std::cout << pipes[i][PIPE_READ] << " " << pipes[i][PIPE_WRITE] << endl;
        }
    }

    infile.open("test-loves-labors-lost.txt"); // <-- lots of words

    /* Processing loop */
    int j = 0;
    while (infile >> word) {
        //fputs(word.c_str(), pipes.at(j)[PIPE_READ]);
        (j < numChildren) ? ++j : j = 0;
    }
    //fgets(readbuf, 80, word);

    infile.close();

    /* Wait for children to exit */
    int status;
    do {
        status = 0;
        for (int i = 0; i < numChildren; ++i) {
            if (childPids[i] > 0) {
                if (waitpid(childPids[i], NULL, WNOHANG) == 0) {
                    /* Child is done */
                    childPids[i] = 0;
                    if (VERBOSE) {
                        std::cout << "child " << i << "is done." << endl;
                    }
                } else {
                    /* Still waiting on this child */
                    status = 1;
                }
            }
            /* Give up timeslice and prevent hard loop: this may not work on all flavors of Unix */
            sleep(0);
        }
    } while (status);

    /* Collect pipes, merge and remove duplicates */


    /* Cleanup */
    //free(childPids);
    exit(EXIT_SUCCESS);
}

