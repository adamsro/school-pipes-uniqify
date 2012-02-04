/*
 *
 * very helpful: http://tldp.org/LDP/lpg/node11.html
 * and http://stackoverflow.com/questions/1381089/multiple-fork-concurrency
 */
#define _POSIX_SOURCE // first line of source?
#include <vector>
#include <iostream>
#include <fstream>

#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>

#define NUM_OF(x) (sizeof (x) / sizeof *(x))

struct pipes_t {
    int read_bychild, write_byparent;
    int read_byparent, write_bychild;
};

//struct pipesfd_t {
//    FILE* read, write;
//};
typedef FILE* pipesfd_t[2];

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

int main(int argc, char* argv[]) {
    const int VERBOSE = 1;
    const int PIPE_READ = 0;
    const int PIPE_WRITE = 1;
    //typedef int pipe_t[2];
    int fd1[2];
    int fd2[2];

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

    std::vector<pid_t> clist(numChildren);
    std::vector<pipes_t> plist(numChildren);
    //std::vector<pipesfd_t> pfdlist(numChildren);

    /* Fork all the children and save their id's*/
    for (int i = 0; i < numChildren; ++i) {
        if ((pipe(fd1) < 0) || (pipe(fd2) < 0)) {
            std::cerr << "PIPE ERROR" << std::endl;
            return -2;
        }
        pipes_t p;
        p.read_bychild = fd1[PIPE_READ];
        p.write_byparent = fd1[PIPE_WRITE];
        p.read_byparent = fd2[PIPE_READ];
        p.write_bychild = fd2[PIPE_WRITE];
        plist.at(i) = p;

        switch (child_pid = fork()) {
            case -1:
                std::cout << "Failed to fork." << std::endl;
                exit(EXIT_FAILURE);
            case 0: // is Child
                /* Culose stdin and duplicate PIPE_READ to this position*/
                close(p.write_byparent);
                close(p.read_byparent);
                if (dup2(p.read_bychild, STDIN_FILENO) != STDIN_FILENO) {
                    std::cerr << "dup2 error to stdin" << std::endl;
                }
                close(p.read_bychild);
                if (dup2(p.write_bychild, STDOUT_FILENO) != STDOUT_FILENO) {
                    std::cerr << "dup2 error to stdout" << std::endl;
                }
                close(p.write_bychild);
                fdopen(plist.at(1).write_byparent, "w");
                /*                pipesfd_t pfd;
                                pfd.read = fdopen(p.read_bychild, "r");
                                pfd.write = fdopen(p.write_bychild, "w");
                                pfdlist.at(i) = pfd;
                 */
                execlp("sort", "sort", NULL); //execl("/usr/bin/sort", "sort");
                _exit(127); /* Failed exec, doesn't flush file desc */
                break;
            default: // is Parent
                clist.at(i) = child_pid;
                break;
        }
    }
    if (VERBOSE) {
        std::cout << "\npids" << std::endl;
        for (std::vector<int >::iterator it = clist.begin();
                it != clist.end(); ++it) {
            std::cout << *it << std::endl;
        }
        std::cout << "pipes" << std::endl;
        for (int i = 0; i < plist.size(); ++i) {
            std::cout << plist.at(i).read_bychild << " " << plist.at(i).write_byparent;
            std::cout << " " << plist.at(i).read_byparent << " ";
            std::cout << plist.at(i).write_bychild << std::endl;
        }
    }

    //pfdlist.at(1).write = fdopen(plist.at(1).write, "r");
    infile.open("test-loves-labors-lost.txt"); // <-- lots of words
    pipesfd_t* pfdlist;
    pfdlist = (pipesfd_t*) malloc(numChildren * sizeof (pipesfd_t));
    for (int i = 0; i < numChildren; ++i) {
        pfdlist[i][PIPE_READ] = fdopen(plist.at(1).read_byparent, "r");
        pfdlist[i][PIPE_WRITE] = fdopen(plist.at(1).write_byparent, "w");
    }
    if (VERBOSE) {
        std::cout << "read      write" << std::endl;
        for (int i = 0; i < numChildren; ++i) {
            std::cout << pfdlist[i][PIPE_READ] << " ";
            std::cout << pfdlist[i][PIPE_WRITE] << std::endl;
        }
    }

    /* Processing loop */
    int j = 0;
    FILE* file;
    //file = fdopen(plist.at(j).write_byparent, "w");
    while (infile >> word) {
        fputs(word.c_str(), file);
        (j < numChildren) ? ++j : j = 0;
    }
    //fgets(readbuf, 80, word);
    /* close to flush buffer*/
    //fclose()

    infile.close();

    /* Wait for children to exit */
    int status;
    do {
        status = 0;
        for (int i = 0; i < numChildren; ++i) {
            if (clist[i] > 0) {
                if (waitpid(clist[i], NULL, WNOHANG) == 0) {
                    /* Child is done */
                    clist[i] = 0;
                    if (VERBOSE) {
                        std::cout << "child " << i << " is done." << std::endl;
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

    return 0;
}

//std::vector<std::vector<int> > pipes(numChildren, std::vector<int> (2));
//childPids = (pid_t*) malloc(numChildren * sizeof (pid_t));
//std::vector<int> V(fd, fd + sizeof (fd) / sizeof (int));
//pipes.at(i) = V;