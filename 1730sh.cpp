#include <iostream>
#include <cstdio>
#include <string>
#include <vector>
#include <set>
#include <sstream>
#include <algorithm>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <cstring>
#include <stdio.h>
#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>

using namespace std;

string prompt();
int run (const char *, int, char * const[], int = 0, int = 1, int = 2, bool = false, int = 0);
int cd (const string);
void print_help();

int last_status = 0;

/**
 * Sets up signal handling and executes the
 * main shell loop.
 */
int main () {
    //cout.setf(ios::unitbuf);
    vector<string> expr;
    stringstream ss;

    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);

    // main loop
    while (true) {
        expr.clear();
        ss.clear();

        // display prompt
        prompt();
 
        string line, exp;
        //getline(cin, line);

        line = string(readline(prompt().c_str()));
        if (!line.empty())
            add_history(line.c_str());

        // if cin is closed
        if (cin.eof()) {
            cout << endl;
            exit(0);
        }

        ss << line;
        // separate line by pipes and push process into vector
        while (getline(ss, exp, '|')) {
            expr.push_back(exp);
        }

        // if more than one process, setup piping
        if (expr.size() > 1) {
            vector<int> fdins;
            bool error = false;
            int in = 0;
            int out = 1;
            int err = 2;

            int pipefd[2];
            int fdin = 0;
            vector<pid_t> pids;

            ss.clear();
            ss << expr[expr.size()-1];
            string part;
            while (ss >> part) {
                if (part == "<") {
                    string file;
                    ss >> file;

                    int fd;
                    if ((fd = open(file.c_str(), O_RDONLY)) == -1) {
                        perror(string("1730sh: " + file).c_str());
                        error = true;
                    } else
                        in = fd;
                } else if (part == ">") {
                    string file;
                    ss >> file;

                    int fd;
                    if ((fd = open(file.c_str(), O_WRONLY | O_CREAT, 0644)) == -1) {
                    //run(args[0], argc, args, 0, fdin, 2, true, pipeid);
                        perror(string("1730sh: " + file).c_str());
                        error = true;
                    } else
                        out = fd;
                } else if (part == ">>") {
                    string file;
                    ss >> file;

                    int fd;
                    if ((fd = open(file.c_str(), O_WRONLY | O_APPEND | O_CREAT, 0644)) == -1) {
                        perror(string("1730sh: " + file).c_str());
                        error = true;
                    } else
                        out = fd;
                } else if (part == "e>") {
                    string file;
                    ss >> file;

                    int fd;
                    if ((fd = open(file.c_str(), O_WRONLY | O_CREAT, 0644)) == -1) {
                        perror(string("1730sh: " + file).c_str());
                        error = true;
                    } else
                        err = fd;
                } else if (part == "e>>") {
                    string file;
                    ss >> file;

                    int fd;
                    if ((fd = open(file.c_str(), O_WRONLY | O_APPEND | O_CREAT, 0644)) == -1) {
                        perror(string("1730sh: " + file).c_str());
                        error = true;
                    } else
                        err = fd;
                }
            }
           
            // for each process
            for (uint n = 0; n < expr.size(); n++) {
                // setup pipe
                if (pipe(pipefd) == -1)
                    perror("pipe");

                // parse arguments
                ss.clear();
                ss << expr[n];

                string part;

                char ** args = new char*[64];
                for (int i = 0; i < 64; i++) args[i] = '\0';

                int argc;
                for (argc = 0; (ss >> part); argc++) {
                    if (part[0] == '"') {
                        int n = 1;
                        size_t i;
                        while (true) {
                            if ((i = part.find('"', n)) == string::npos) {
                                string npart;
                                ss >> npart;
                                part += " " + npart;
                            } else {
                                n = i+1;
                                if (part[i-1] == '\\') {
                                    part.erase(i-1, 1);
                                    n--;
                                } else {
                                    if (part[i] == '"')
                                        part.erase(i, 1);
                                    if (part[0] == '"')
                                        part.erase(0, 1);
                                    break;
                                }
                            }
                        }
                    }

                    if ((part == "<") || (part == ">") || (part == ">>") || (part == "e>") || (part == "e>>"))
                        ss >> part;
                    else
                        args[argc] = strdup(part.c_str());

                }

                int pipeid;
                if (n == 0)
                    pipeid = 0;
                else if (n == (expr.size()-1))
                    pipeid = 2;
                else
                    pipeid = 1;

                if (!error) {
                    // run
                    pid_t pid;
                    switch (pipeid) {
                        case 0:
                            pid = run(args[0], argc, args, in, pipefd[1], err, true, pipeid);
                            break;
                        case 1:
                            pid = run(args[0], argc, args, fdin, pipefd[1], err, true, pipeid);
                            break;
                        case 2:
                            pid = run(args[0], argc, args, fdin, out, err, true, pipeid);
                            break;
                    }
                    close(pipefd[1]);
                    //run(args[0], argc, args, 0, fdin, 2, true, pipeid);
                    pids.push_back(pid);
                }
                
                for (int i = 0; i < 64; i++) free(args[i]);
                delete[] args;
                
                fdin = pipefd[0];
                fdins.push_back(fdin);
            }
            for (int fd : fdins) close(fd);
            // wait for all processes after last one
            for (int pid : pids) waitpid(pid, NULL, 0);

        }
        // else if one process
        else {
            bool error = false;
            int in = 0;
            int out = 1;
            int err = 2;

            // parse arguments
            ss.clear();
            if (expr.size() > 0)
                ss << expr[0];

            string part;

            char ** args = new char*[64];
            for (int i = 0; i < 64; i++) args[i] = '\0';

            int argc;
            for (argc = 0; (ss >> part); argc++) {
                if (part == "<") {
                    string file;
                    ss >> file;

                    int fd;
                    if ((fd = open(file.c_str(), O_RDONLY)) == -1) {
                        perror(string("1730sh: " + file).c_str());
                        error = true;
                    } else
                        in = fd;
                } else if (part == ">") {
                    string file;
                    ss >> file;

                    int fd;
                    if ((fd = open(file.c_str(), O_WRONLY | O_CREAT, 0644)) == -1) {
                        perror(string("1730sh: " + file).c_str());
                        error = true;
                    } else
                        out = fd;
                } else if (part == ">>") {
                    string file;
                    ss >> file;

                    int fd;
                    if ((fd = open(file.c_str(), O_WRONLY | O_APPEND | O_CREAT, 0644)) == -1) {
                        perror(string("1730sh: " + file).c_str());
                        error = true;
                    } else
                        out = fd;
                } else if (part == "e>") {
                    string file;
                    ss >> file;

                    int fd;
                    if ((fd = open(file.c_str(), O_WRONLY | O_CREAT, 0644)) == -1) {
                        perror(string("1730sh: " + file).c_str());
                        error = true;
                    } else
                        err = fd;
                } else if (part == "e>>") {
                    string file;
                    ss >> file;

                    int fd;
                    if ((fd = open(file.c_str(), O_WRONLY | O_APPEND | O_CREAT, 0644)) == -1) {
                        perror(string("1730sh: " + file).c_str());
                        error = true;
                    } else
                        err = fd;
                }

                if (part[0] == '"') {
                    int n = 1;
                    size_t i;
                    while (true) {
                        if ((i = part.find('"', n)) == string::npos) {
                            string npart;
                            ss >> npart;
                            part += " " + npart;
                        } else {
                            n = i+1;
                            if (part[i-1] == '\\') {
                                part.erase(i-1, 1);
                                n--;
                            } else {
                                if (part[i] == '"')
                                    part.erase(i, 1);
                                if (part[0] == '"')
                                    part.erase(0, 1);
                                break;
                            }
                        }
                    }
                }

                if ((part != "<") && (part != ">") && (part != ">>") && (part != "e>") && (part != "e>>"))
                    args[argc] = strdup(part.c_str());

            }

            if (!error) {
                // run
                run(*args, argc, args, in, out, err);
            }
            
            for (int i = 0; i < 64; i++) free(args[i]);
            delete[] args;
        }
    }
}

/**
 * Prints prompt
 */
string prompt() {
    string prompt;

    prompt += "1730sh:";

    string pwd = getenv("PWD");
    string home = getenv("HOME");

    if (pwd.find(home, 0) != string::npos)
        prompt += "~" + pwd.substr(home.length(), (pwd.length())-1);
    else
        prompt += pwd; 

    prompt += "$ ";

    return prompt;
}

/**
 * Executes program and sets up piping if required
 */
int run (const char * prog, int argc, char * const args[], int fdin /*=0*/, int fdout /*=1*/, int fderr /*=2*/, bool ispipe /*=false*/, int pipeid /*=0*/) {
    // if expression is empty
    if (argc == 0)
        return -1;

    // exit builtin
    if (strcmp(prog, "exit") == 0) {
        if (argc > 1) {
            exit(stoi(args[1]));
        } else {
            exit(WEXITSTATUS(last_status));
        }
    }

    // cd builtin
    if (strcmp(prog, "cd") == 0) {
        if (argc > 1)
            return cd(args[1]);
        else
            return cd("");
    }

    // create new process
    pid_t pid = fork();

    // if fork error
    if (pid == -1) {
        perror("fork");
    }
    // if child process
    else if (pid == 0) {
        // signal handling
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGTTIN, SIG_DFL);
        signal(SIGTTOU, SIG_DFL);

        if (fdin != 0) {
            if (dup2(fdin, STDIN_FILENO) == -1)
                perror("dup2");
            close(fdin);
        }
        if (fdout != 1) {
            if (dup2(fdout, STDOUT_FILENO) == -1)
                perror("dup2");
            close(fdout);
        }
        if (fderr != 2) {
            if (dup2(fderr, STDERR_FILENO) == -1)
                perror("dup2");
            close(fderr);
        
        }

        // help builtin
        if (strcmp(prog, "help") == 0) {
            cout << "CS1730 shell, version 0" << endl
                << "These shell commands are defined internally. Type 'help' to see this list." << endl
                << endl
                << " cd [PATH]\tAttempts to change the current directory to PATH. If no PATH is specified, attempts to change the current directory to $HOME." << endl
                << " exit [N]\tCauses the shell to exit with a status of N. If N is not specified, the exit status is that o the last job." << endl;
            exit(0);
        }

        // execute
        if (execvp(prog, args) < 0) {
            perror(string("1730sh: " + string(prog)).c_str());
            exit(EXIT_FAILURE);
        }
    }
    // if parent process
    else {
        int status;
        if (!ispipe || pipeid == 2) {
            // wait on single process or last piped job process
            if (waitpid(pid, &status, 0) == -1)
                perror("waitpid");

            last_status = status;
            int estatus = WEXITSTATUS(status);

            // print exit status
            if (WIFEXITED(status))
                cout << "child exited normally with status " << estatus << endl;
            else if (WIFSIGNALED(status))
                cout << "child terminated by signal " << WTERMSIG(status) << " with status " << estatus << endl;
            else
                cout << "child exited abnormally with status " << estatus << endl;

            if (pipeid == 2) {
                if (fdin != 0)
                    close(fdin);
                if (fdout != 1)
                    close(fdout);
            }
        }
    }

    return pid;
}

/**
 * Attempts to change current directory
 */
int cd (const string target) {
    string path = string(getenv("HOME"));
         
    // if target is given
    if (target != "") {
        // if target location isn't "~"
        if (target != "~")
            // set path to target
            path = target;
    }

    // change dir to path
    if (chdir(path.c_str()) == -1) {
        perror("cd");
        return -1;
    } else {
        return 0;
    }
}
