/*
 * Original Author: Robert M Adams (adamsro)
 * File: uniqify.cpp
 * Created: 2012 Feb 1, 18:35 by adamsro
 * Last Modified: 2012 Feb 7, 20:00 by adamsro
 *
 * File contains a filter which spawns n sort processses based on a command
 * line argument. The primary purpose of this is project to learn pipes.
 *
 * helpful: http://tldp.org/LDP/lpg/node11.html
 * and: http://stackoverflow.com/questions/1381089/multiple-fork-concurrency
 */
#include <stdio.h>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <vector>
#include <cerrno>
#include <math.h>
#include <algorithm>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <limits.h>

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

/* return true if ! alphapetic char. for a filter */
bool notalpha(char c) {
    if ((int(*)(int)) std::isalpha(c)) {
        return false;
    }
    return true;
}

class Uniqify {
    static const int PIPE_READ = 0;
    static const int PIPE_WRITE = 1;
    int num_children;
    std::vector<pipes_t> plist;
public:
    int num_words_parsed;
    Uniqify(int children);
    void run();
    void print();
    void print_vector(std::vector<std::string> temp);
protected:
    void fork_all_sorts();
    void parser(pipesfd_t* pfdlist);
    void suppressor(pipesfd_t* pfdlist);
    void wait_for_children();

    void fdopen_read_pipes(pipesfd_t* pfdlist);
    void fdopen_write_pipes(pipesfd_t* pfdlist);
    void fclose_read_pipes(pipesfd_t* pfdlist);
    void fclose_write_pipes(pipesfd_t* pfdlist);
    void close_pipes(int i);
};

Uniqify::Uniqify(int children) {
    num_children = children;
}

/* function forks children, parses cin, then when pipes, cout sorted, suppressed words */
void Uniqify::run() {
    pid_t child_pid;
    int status;
    pipesfd_t* pfdlist;

    pfdlist = (pipesfd_t*) std::malloc(num_children * sizeof (pipesfd_t));
    if (pfdlist == NULL) {
        throw (Exception("malloc failed", __LINE__, errno));
    }
    fork_all_sorts();

    fdopen_read_pipes(pfdlist);
    fdopen_write_pipes(pfdlist);
    parser(pfdlist);
    fclose_write_pipes(pfdlist);
    suppressor(pfdlist);
    fclose_read_pipes(pfdlist);
    wait_for_children();
    free(pfdlist);

    //    switch (child_pid = fork()) {
    //        case -1: //fail
    //        case 0: //child
    //
    //            exit(0);
    //        default:
    //
    //    }
}

void Uniqify::fork_all_sorts() {
    int fd1[2];
    int fd2[2];
    pid_t child_pid;

    /* Fork all the children and save their id's*/
    for (int i = 0; i < num_children; ++i) {
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
                /* Close stdin and duplicate PIPE_READ to this position */
                if (p.read_bychild != STDIN_FILENO) {
                    if (dup2(p.read_bychild, STDIN_FILENO) != STDIN_FILENO) {
                        throw (Exception("dup2 failed", __LINE__, errno));
                    }
                }
                /* Close stdout and duplicate PIPE_WRITE to this position */
                if (p.write_bychild != STDOUT_FILENO) {
                    if (dup2(p.write_bychild, STDOUT_FILENO) != STDOUT_FILENO) {
                        throw (Exception("dup2 failed", __LINE__, errno));
                    }
                }
                close_pipes(i);
                execlp("sort", "sort", NULL); //"/usr/bin/sort"
                _exit(127); /* Failed exec, doesn't flush file desc */
                break;
            default: // is Parent
                plist.at(i).child_pid = child_pid;
                break;
        }
    }

#ifdef VERBOSE
    std::cout << "the pipes: " << std::endl;
    for (int i = 0; i < (int) plist.size(); ++i) {
        std::cout << "pid: " << plist.at(i).child_pid << std::endl;
        std::cout << plist.at(i).read_bychild << " " << plist.at(i).write_byparent;
        std::cout << " " << plist.at(i).read_byparent << " ";
        std::cout << plist.at(i).write_bychild << std::endl;
    }
#endif

}

/* send input to the sort processes in a round-robin fashion */
void Uniqify::parser(pipesfd_t* pfdlist) {
    std::string word;
    int i = 0;
    num_words_parsed = 0;
    //    std::ifstream file("test.txt");
    while (std::cin >> word) {
        /* remove all no alpha charactors and transform all uppercase to lowercase */
        word.erase(std::remove_if(word.begin(), word.end(), notalpha), word.end());
        std::transform(word.begin(), word.end(), word.begin(), (int(*)(int)) tolower);
        word.append("\n");
        if (fputs(word.c_str(), pfdlist[i][PIPE_WRITE]) < 0) {

            throw (Exception("fputs failed", __LINE__, errno));
        }
        ++num_words_parsed;
        (i == num_children - 1) ? i = 0 : ++i;
    }
}

/* retreive all words from sort processes in a round-robin fashion */
void Uniqify::suppressor(pipesfd_t* pfdlist) {
    std::string oldword;
    int largest = 0;
    std::vector<std::string> temp(num_children, "");
    int eofs = 0;
    char readbuf [100];
    int i = 0;
    while (eofs != num_children) {
        /* if pipe has never been read, read it for the first time */
        if (temp.at(i) == "") {
            if (fgets(readbuf, sizeof readbuf, pfdlist[i][PIPE_READ]) == NULL) {
                ++eofs;
                temp.at(i) = CHAR_MIN;
            } else {
                temp.at(i) = readbuf;
            }
        }
        /* get the largeset of all n pipes */
        if (i == 0 || temp.at(i).compare(temp.at(largest)) < 0) {
            largest = i;
        }
        /* when one complete loop has been made, print the largest and load a new word into its spot. */
        if (i == num_children - 1) {
            //print_vector(temp);
            // if this word is the same as the word before it, ignore it and move on.
            if (temp.at(largest).compare(oldword) != 0) {
                std::cout << temp.at(largest);
                oldword.assign(temp.at(largest));
            }
            if (fgets(readbuf, sizeof readbuf, pfdlist[largest][PIPE_READ]) == NULL) {
                ++eofs;
                temp.at(largest) = CHAR_MIN;
            } else {
                temp.at(largest) = readbuf;
            }
        }
        (i == num_children - 1) ? i = 0 : ++i;
    }
}

/* used for debugging */
void Uniqify::print_vector(std::vector<std::string> temp) {
    std::cout << "arr: ";
    for (int i = 0; i < (int) temp.size(); ++i) {
        temp.at(i).erase(std::remove(temp.at(i).begin(), temp.at(i).end(), '\n'), temp.at(i).end());
        std::cout << temp.at(i) << " ";
    }
    std::cout << std::endl;
}

/* Wait for children to exit */
void Uniqify::wait_for_children() {
    int numended = 0;
    int i = 0;
    int loops = 0;
    do {
        if (plist.at(i).child_pid > 0) {
            if (waitpid(plist.at(i).child_pid, NULL, WNOHANG) == 0) {
                plist.at(i).child_pid = 0; /* Child is done */
                ++numended;
#ifdef VERBOSE
                std::cout << "child " << i << " is done." << std::endl;
#endif
            } else { /* Still waiting on this child */
#ifdef VERBOSE
                //std::cout << "still waiting on child " << i << " pid: " << plist.at(i).child_pid << std::endl;
#endif
            }
        }
        /* Give up timeslice and prevent hard loop: this may not work on all flavors of Unix */
        sleep(0); // 0
        if (loops == 50) {
            break; /* Zombies!!! */
        }
        ++loops;
        (i == num_children - 1) ? i = 0 : ++i;
    } while (numended != num_children - 1);
}

/* open the right pipes and close everything else */
void Uniqify::fdopen_write_pipes(pipesfd_t* pfdlist) {
    for (int j = 0; j < num_children; ++j) {
        pfdlist[j][PIPE_WRITE] = fdopen(plist.at(j).write_byparent, "w");
        if (pfdlist[j][PIPE_WRITE] == NULL) {
            throw (Exception("fdopen failed", __LINE__, errno));
        }
    }
}

void Uniqify::fdopen_read_pipes(pipesfd_t* pfdlist) {
    for (int i = 0; i < num_children; ++i) {
        close(plist.at(i).read_bychild);
        close(plist.at(i).write_bychild);
        pfdlist[i][PIPE_READ] = fdopen(plist.at(i).read_byparent, "r");
        if (pfdlist[i][PIPE_READ] == NULL) {
            throw (Exception("fdopen failed", __LINE__, errno));
        }
    }
}

/* Close all the write pipes to flush the buffers */
void Uniqify::fclose_write_pipes(pipesfd_t* pfdlist) {
    for (int i = 0; i < num_children; ++i) {
        if (fclose(pfdlist[i][PIPE_WRITE]) < 0) {
            throw (Exception("fclose failed", __LINE__, errno));
        }
    }
}

/* Close pipes so processes will complete */
void Uniqify::fclose_read_pipes(pipesfd_t* pfdlist) {
    for (int i = 0; i < num_children; ++i) {
        if (fclose(pfdlist[i][PIPE_READ]) < 0) {

            throw (Exception("fclose failed", __LINE__, errno));
        }
    }
}

void Uniqify::close_pipes(int i) {
    close(plist.at(i).write_byparent);
    close(plist.at(i).read_byparent);
    close(plist.at(i).write_bychild);
    close(plist.at(i).write_bychild);
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cout << "USAGE: uniqify [num_sort_processes]\n";
        exit(EXIT_SUCCESS);
    }
    clock_t start;
    clock_t theend;
    try {
        start = clock();
        Uniqify uniq(atoi(argv[1]));
        uniq.run();
        theend = clock();
        std::cout << std::endl << uniq.num_words_parsed << "\t";
        /* Measure with clock cycles, then divide by clocks-per-sec for
         * result in sec. more accurate. */
        std::cout << (((double) (theend - start)) / CLOCKS_PER_SEC) << std::endl;
    } catch (const Exception e) {
        /* format error for terminal output */
        std::cout << std::endl << e.errmsg << ", " << strerror(e.cerrno);
        std::cout << ", line " << e.errline << std::endl;
    } catch (...) {
        std::cout << "Error!" << std::endl;
    }
}
