/*
 *
 * very helpful: http://tldp.org/LDP/lpg/node11.html
 * and: http://stackoverflow.com/questions/1381089/multiple-fork-concurrency
 */

#include <iostream>
#include <vector>
#include <cerrno>
//#include <fstream>
//#include <locale>

#define NUM_OF(x) (sizeof (x) / sizeof *(x))

#define VERBOSE = 1;

class Exception {
public:
    std::string errmsg;
    int cerrno;
    int errline;

    Exception(std::string message, int line, int errnum = 0) {
        errmsg = message;
        cerrno = errnum;
        errline = line;
    }
};

struct pipes_t {
    pid_t child_pid;
    int read_bychild, write_byparent;
    int read_byparent, write_bychild;
};

typedef FILE* pipesfd_t[2];

class Uniqify {
    static const int PIPE_READ = 0;
    static const int PIPE_WRITE = 1;
    std::vector<pipes_t> plist;
    int numChildren;

public:
    Uniqify(int children);
    void run();
protected:
    void fork_processes();
    void parse();
    void wait_for_children();
    std::string normalize_str(std::string input);
};

Uniqify::Uniqify(int children) {
    numChildren = children;
}

void Uniqify::run() {
    fork_processes();
    parse();
    wait_for_children();
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
        pipes_t p;
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

                if (dup2(p.write_bychild, STDOUT_FILENO) != STDOUT_FILENO) {
                    throw (Exception("dup2 failed", __LINE__, errno));
                }
                close(p.write_bychild);

                execlp("sort", "sort", NULL); //"/usr/bin/sort"
                _exit(127); /* Failed exec, doesn't flush file desc */
                break;
            default: // is Parent
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

void Uniqify::parse() {
    std::string word;
    pipesfd_t* pfdlist;
    char readbuf [100];
    int i = 0;
    int eofs = 0;
    std::string oldword;

    pfdlist = (pipesfd_t*) malloc(numChildren * sizeof (pipesfd_t));
    if (pfdlist == NULL) {
        throw (Exception("malloc failed", __LINE__, errno));
    }
    /* open the right pipes and close everything else */
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
    while (std::cin >> word) {
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
    i = 0;
    word = "";
    while (eofs != numChildren) {
        if (fgets(readbuf, sizeof readbuf, pfdlist[i][PIPE_READ]) == NULL) {
            if (fclose(pfdlist[i][PIPE_READ]) < 0) {
                throw (Exception("fclose failed", __LINE__, errno));
            }
            ++eofs;
        } else {
            /* Suppress duplicates if they are sequential */
            oldword.assign(word);
            word = readbuf;
            if (oldword.compare(word) != 0) {
                std::cout << word;
            }
        }
        (i == numChildren - 1) ? i = 0 : ++i;
    }
    free(pfdlist);
}

void Uniqify::wait_for_children() {

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
                //        std::cout << "still waiting on child " << i << std::endl;
#endif
            }
        }
        /* Give up timeslice and prevent hard loop: this may not work on all flavors of Unix */
        sleep(0); // 0
        (i == numChildren - 1) ? i = 0 : ++i;
    } while (stillwating);

    /* Collect pipes, merge and remove duplicates */
}

std::string Uniqify::normalize_str(std::string input) {
    std::string output;
    std::locale loc;
    for (size_t i = 0; i < input.length(); ++i)
        // output << tolower(input[i], loc);
        return output;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cout << "USAGE: uniqify [num_sort_processes]\n";
        exit(EXIT_SUCCESS);
    }
    try {
        Uniqify uniq(atoi(argv[1]));
        uniq.run();
    } catch (const Exception e) {
        std::cout << std::endl << e.errmsg << ", " << strerror(e.cerrno);
        std::cout << ", line " << e.errline << std::endl;
    } catch (...) {
        std::cout << "Error!" << std::endl;
    }
}
