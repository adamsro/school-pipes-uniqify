/*
 *
 * very helpful: http://tldp.org/LDP/lpg/node11.html
 * and: http://stackoverflow.com/questions/1381089/multiple-fork-concurrency
 */
#define _POSIX_SOURCE // needed?
#include <vector>
#include <iostream>
#include <fstream>
#include <cerrno>
#include <stdio.h>

#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>

#define NUM_OF(x) (sizeof (x) / sizeof *(x))

#define VERBOSE = 1;

class Exception {
public:

    Exception(std::string message, int line, int errnum = 0) {
        errmsg = message;
        cerrno = errnum;
        errline = line;
    }
    std::string errmsg;
    int cerrno;
    int errline;

};

struct Pipes {
    pid_t child_pid;
    int read_bychild, write_byparent;
    int read_byparent, write_bychild;
};

typedef FILE* pipesfd_t[2];

class Uniqify {
    static const int PIPE_READ = 0;
    static const int PIPE_WRITE = 1;
    std::vector<Pipes> plist;
    int numChildren;
    std::ifstream infile;
    std::ofstream outfile;
    //const Exception e;

public:
    Uniqify(int children);
    void set_input_file(std::string infile);
    void set_output_file(std::string outfile);
    void fork_processes();
    void parser();
    void suppressor();
};

Uniqify::Uniqify(int children) {
    numChildren = children;
    infile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    outfile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
}

void Uniqify::fork_processes() {
    int fd1[2];
    int fd2[2];
    pid_t child_pid;


    /* Fork all the children and save their id's*/
    for (int i = 0; i < numChildren; ++i) {
        if ((pipe(fd1) < 0) || (pipe(fd2) < 0)) {
            throw (Exception("pipe failed", __LINE__, errno));
        }
        Pipes p;
        p.read_bychild = fd1[PIPE_READ];
        p.write_byparent = fd1[PIPE_WRITE];
        p.read_byparent = fd2[PIPE_READ];
        p.write_bychild = fd2[PIPE_WRITE];
        plist.push_back(p);

        switch (child_pid = fork()) {
            case -1:
                throw (Exception("fork failed", __LINE__, errno));
            case 0: // is Child
                /* Culose stdin and duplicate PIPE_READ to this position*/
                close(p.write_byparent);
                close(p.read_byparent);
                if (dup2(p.read_bychild, STDIN_FILENO) != STDIN_FILENO) {
                    throw (Exception("dup2 failed", __LINE__, errno));
                }
                close(p.read_bychild);

                //if (dup2(fd2[PIPE_WRITE], STDOUT_FILENO) != STDOUT_FILENO) {
                if (dup2(p.write_bychild, STDOUT_FILENO) != STDOUT_FILENO) {
                    throw (Exception("dup2 failed", __LINE__, errno));
                }
                close(p.write_bychild);

                execlp("sort", "sort", NULL); //execl("/usr/bin/sort", "sort");
                //_exit(127); /* Failed exec, doesn't flush file desc */
                exit(0);
                break;
            default: // is Parent
                //clist.push_back(child_pid);
                plist.at(i).child_pid = child_pid;
                break;
        }
    }

#ifdef VERBOSE
    std::cout << "pipes" << std::endl;
    for (int i = 0; i < plist.size(); ++i) {
        std::cout << "pid: " << plist.at(i).child_pid << std::endl;
        std::cout << plist.at(i).read_bychild << " " << plist.at(i).write_byparent;
        std::cout << " " << plist.at(i).read_byparent << " ";
        std::cout << plist.at(i).write_bychild << std::endl;
    }
#endif

}

void Uniqify::parser() {
    std::string word;
    pipesfd_t* pfdlist;
    char readbuf [100];
    int i = 0;
    int eofs = 0;

    infile.open("test2.txt"); // <-- lots of words
    pfdlist = (pipesfd_t*) malloc(numChildren * sizeof (pipesfd_t));
    if (pfdlist == NULL) {
        throw (Exception("malloc failed", __LINE__, errno));
    }
    for (int j = 0; j < numChildren; ++j) {
        close(plist.at(j).read_bychild);
        close(plist.at(j).write_bychild);
        pfdlist[j][PIPE_READ] = fdopen(plist.at(j).read_byparent, "r");
        if (pfdlist[j][PIPE_READ] == NULL) {
            throw (Exception("fdopen failed", __LINE__, errno));
        }
        pfdlist[j][PIPE_WRITE] = fdopen(plist.at(j).write_byparent, "w");
        if (pfdlist[j][PIPE_WRITE] == NULL) {
            throw (Exception("fdopen failed", __LINE__, errno));
        }
    }
    /* send input to the sort processes in a round-robin fashion */
    int k = 0;
    while (!infile.eof()) {
        infile >> word;
        word.append("\n");
        if (fputs(word.c_str(), pfdlist[k][PIPE_WRITE]) < 0) {
            throw (Exception("fputs failed", __LINE__, errno));
        }
        (k == numChildren - 1) ? k = 0 : ++k;
    }

    /* Close all the write pipes to flush the buffers */
    for (int l = 0; l < numChildren; ++l) {
        if (fclose(pfdlist[l][PIPE_WRITE]) < 0) {
            throw (Exception("fclose failed", __LINE__, errno));
        }
    }

    /* retreive all words from sort processes in a round-robin fashion */
    outfile.open("output.txt", std::ios::trunc);
    i = 0;
    while (eofs != numChildren) {
        if (fgets(readbuf, sizeof readbuf, pfdlist[i][PIPE_READ]) == NULL && !feof(pfdlist[i][PIPE_READ])) {
            throw (Exception("fgets failed", __LINE__, errno));
        }
        if (feof(pfdlist[i][PIPE_READ])) {
            if (fclose(pfdlist[i][PIPE_READ]) < 0) {
                throw (Exception("fclose failed", __LINE__, errno));
            }
            ++eofs;
        } else {
            outfile << readbuf;
            //suppresser(readbuf);
        }
        (i == numChildren - 1) ? i = 0 : ++i;
    }

    outfile.close();
    infile.close();
    free(pfdlist);
}

void Uniqify::suppressor() {
    static std::string oldword = NULL;
    std::string newword;
    if (newword.compare(oldword) != 0) {
        std::cout << oldword << newword << "\n";
    }


    /* Wait for children to exit */
    int stillwating;
    int i = 0;
    do {
        stillwating = 0;
        if (plist.at(i).child_pid > 0) {
            if (waitpid(plist.at(i).child_pid, NULL, WNOHANG) == 0) {
                plist.at(i).child_pid = 0; /* Child is done */
#ifdef VERBOSE
                std::cout << "child " << i << " is done." << std::endl;
#endif
            } else {
                /* Still waiting on this child */
                stillwating = 1;
#ifdef VERBOSE
                std::cout << "still waiting on child " << i << std::endl;
#endif
            }
        }
        /* Give up timeslice and prevent hard loop: this may not work on all flavors of Unix */
        sleep(0); // 0
        (i == numChildren - 1) ? i = 0 : ++i;
    } while (stillwating);

    /* Collect pipes, merge and remove duplicates */
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cout << "USAGE: uniqify [num_sort_processes]";
        //exit(EXIT_SUCCESS);
    }
    try {
        Uniqify uniq(3);
        uniq.fork_processes();
        uniq.parser();
        uniq.suppressor();
    } catch (const Exception e) {
        std::cout << std::endl << e.errmsg << ", " << strerror(e.cerrno);
        std::cout << ", line " << e.errline << std::endl;
    } catch (...) {
        std::cout << "Error!" <<  std::endl;
    }
}
