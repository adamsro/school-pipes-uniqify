/*
 *
 * very helpful: http://tldp.org/LDP/lpg/node11.html
 * and http://stackoverflow.com/questions/1381089/multiple-fork-concurrency
 */
#define _POSIX_SOURCE // first line of source?
#include <vector>
#include <iostream>
#include <fstream>
#include <cerrno>
#include <stdio.h>

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
    std::ofstream outfile;
    int numChildren;

    if (argc != 2) {
        //std::cout << "USAGE: uniqify [num_sort_processes]";
        //exit(EXIT_SUCCESS);
    }
    std::cout << "starting program\n";
    numChildren = atoi(argv[2]);
    numChildren = 2;
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
                    std::cout << "dup2 error: " << strerror(errno) << std::endl;
                }
                close(p.read_bychild);

                //if (dup2(fd2[PIPE_WRITE], STDOUT_FILENO) != STDOUT_FILENO) {
                if (dup2(p.write_bychild, STDOUT_FILENO) != STDOUT_FILENO) {
                    std::cout << "dup2 error: " << strerror(errno) << std::endl;
                }
                close(p.write_bychild);
                //                pipesfd_t* pfdlist;
                //                pfdlist[PIPE_READ] = fdopen(plist.at(1).read_byparent, "r");
                //                pfdlist[PIPE_WRITE] = fdopen(plist.at(1).write_byparent, "w");
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
    if (pfdlist == NULL) {
        std::cout << "malloc error: " << strerror(errno) << std::endl;
    }
    for (int j = 0; j < numChildren; ++j) {
        close(plist.at(j).read_bychild);
        close(plist.at(j).write_bychild);
        pfdlist[j][PIPE_READ] = fdopen(plist.at(j).read_byparent, "r");
        if (pfdlist[j][PIPE_READ] == NULL) {
            std::cout << "fdopen error, read: " << strerror(errno) << std::endl;
        }
        pfdlist[j][PIPE_WRITE] = fdopen(plist.at(j).write_byparent, "w");
        if (pfdlist[j][PIPE_WRITE] == NULL) {
            std::cout << "fdopen error, write: " << strerror(errno) << std::endl;
        }
    }

    /* Processing loop */
    int k = 0;
    while (infile >> word) {
        (k == numChildren - 1) ? k = 0 : ++k;
        word.append("\n");
        if (fputs(word.c_str(), pfdlist[k][PIPE_WRITE]) < 0) {
            std::cout << "fputs error, write: " << strerror(errno) << std::endl;
        }
    }
    for (int l = 0; l < numChildren; ++l) {
        if (fclose(pfdlist[l][PIPE_WRITE]) < 0) {
            std::cout << "fclose error, write: " << strerror(errno) << std::endl;
        }
    }

    char readbuf [100];
    outfile.open("output.txt", std::ios::trunc);
    int i = 0;
    int eofs = 0;
    while (eofs != numChildren) {
        if (fgets(readbuf, 100, pfdlist[i][PIPE_READ]) == NULL && !feof(pfdlist[i][PIPE_READ])) {
            std::cout << "fclose error, write: " << strerror(errno) << std::endl;
        } else if (feof(pfdlist[i][PIPE_READ])) {
            ++eofs;
        } else {
            outfile << readbuf;
        }
        (i == numChildren - 1) ? i = 0 : ++i;
    }

    outfile.close();
    infile.close();

    /* Wait for children to exit */
    int status;
    do {
        for (int i = 0; i < numChildren; ++i) {
            status = 0;
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
/*
    FILE** name;
    name = (FILE**) malloc(numChildren * 2 * sizeof (FILE*));
    for (int i = 0; i < numChildren * 2; i = i + 2) {
        name[i] = fdopen(plist.at(1).read_byparent, "r");
        name[i + 1] = fdopen(plist.at(1).write_byparent, "w");
    }
 */