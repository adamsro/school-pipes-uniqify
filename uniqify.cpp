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

#include <iostream>
#include <vector>
#include <cerrno>
#include <math.h>

#define NUM_OF(x) (sizeof (x) / sizeof *(x))

//#define VERBOSE = 0;

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
    std::vector<pipes_t> plist;
    int numChildren;
    pipesfd_t* pfdlist;
public:
    int num_words_parsed;
    std::vector<std::string> words;
    Uniqify(int children);
    void run();
    void print();
protected:
    void pipe_and_fork();
    void open_pipes();
    void send_input();
    void close_write_pipes();
    void receive_input();
    void close_read_pipes();
    void sort_unique();
    void wait_for_children();
};

Uniqify::Uniqify(int children) {
    numChildren = children;
}

/* get this mess in motion */
void Uniqify::run() {
    pipe_and_fork();
    pfdlist = (pipesfd_t*) malloc(numChildren * sizeof (pipesfd_t));
    if (pfdlist == NULL) {
        throw (Exception("malloc failed", __LINE__, errno));
    }
    open_pipes();
    send_input();
    close_write_pipes();
    receive_input();
    close_read_pipes();
    free(pfdlist);
    sort_unique();
    wait_for_children();
}

void Uniqify::pipe_and_fork() {
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
                /* Close stdin and duplicate PIPE_READ to this position */
                close(p.write_byparent);
                close(p.read_byparent);
                if (p.read_bychild != STDIN_FILENO) {
                    if (dup2(p.read_bychild, STDIN_FILENO) != STDIN_FILENO) {
                        throw (Exception("dup2 failed", __LINE__, errno));
                    }
                    close(p.read_bychild);
                }
                /* Close stdout and duplicate PIPE_WRITE to this position */
                if (p.write_bychild != STDOUT_FILENO) {
                    if (dup2(p.write_bychild, STDOUT_FILENO) != STDOUT_FILENO) {
                        throw (Exception("dup2 failed", __LINE__, errno));
                    }
                    close(p.write_bychild);
                }
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

/* open the right pipes and close everything else */
void Uniqify::open_pipes() {
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
}

/* send input to the sort processes in a round-robin fashion */
void Uniqify::send_input() {
    std::string word;
    int i = 0;
    num_words_parsed = 0;
    while (std::cin >> word) {
        /* remove all no alpha charactors and transform all uppercase to lowercase */
        word.erase(std::remove_if(word.begin(), word.end(), notalpha), word.end());
        std::transform(word.begin(), word.end(), word.begin(), (int(*)(int)) tolower);
        word.append("\n");
        if (fputs(word.c_str(), pfdlist[i][PIPE_WRITE]) < 0) {

            throw (Exception("fputs failed", __LINE__, errno));
        }
        ++num_words_parsed;
        (i == numChildren - 1) ? i = 0 : ++i;
    }
}

/* Close all the write pipes to flush the buffers */
void Uniqify::close_write_pipes() {
    for (int l = 0; l < numChildren; ++l) {
        if (fclose(pfdlist[l][PIPE_WRITE]) < 0) {

            throw (Exception("fclose failed", __LINE__, errno));
        }
    }
}

/* retreive all words from sort processes in a round-robin fashion */
void Uniqify::receive_input() {
    static const int WORD_RESIZE = 50;
    words.resize(WORD_RESIZE);
    char readbuf [100];
    int i = 0;
    int eofs = 0;
    while (eofs != numChildren) {
        if (fgets(readbuf, sizeof readbuf, pfdlist[i][PIPE_READ]) == NULL) {
            ++eofs;
        } else {
            words.push_back(readbuf);
        }
        (i == numChildren - 1) ? i = 0 : ++i;
    }
}
/* Close pipes so processes will complete */
void Uniqify::close_read_pipes() {
    for (int i = 0; i < numChildren; ++i) {
        if (fclose(pfdlist[i][PIPE_READ]) < 0) {
            throw (Exception("fclose failed", __LINE__, errno));
        }
    }
}

void Uniqify::sort_unique() {
    std::sort(words.begin(), words.end());
    words.erase(std::unique(words.begin(), words.end()), words.end());
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
        (i == numChildren - 1) ? i = 0 : ++i;
    } while (numended != numChildren - 1);
}
/* Get the output. */
void Uniqify::print() {
    for (std::vector<std::string>::iterator it = words.begin(); it != words.end(); ++it) {
        std::cout << *it;
    }
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
        uniq.print();
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