#include <fcntl.h>
#include <stdint.h>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

int exec_getout(std::vector<char> *output, std::string cmd,
                std::vector<const char *> &args) {
  int fd[2];
  if (pipe(fd) != 0)
    return -1;

  pid_t pid = fork();
  if (pid == -1) {
    perror("failed to run command");
    exit(3);
  } else if (pid == 0) {
    std::vector<const char *> full_args;
    full_args.push_back(cmd.c_str());
    for (auto &e : args) {
      full_args.push_back(e);
    }
    full_args.push_back(0);
    dup2(fd[1], 1);
    close(fd[0]);
    int devnull = open("/dev/null", O_WRONLY);
    dup2(devnull, 2);
    execvp(cmd.c_str(), (char *const *)full_args.data());
  }
  close(fd[1]);

  ssize_t total = 0;
  ssize_t n = 0;
  do {
    total += n;
    output->resize(total + BUFSIZ);
  } while ((n = read(fd[0], &(output->at(total)), BUFSIZ)) > 0);
  output->resize(total);
  close(fd[0]);

  int wstatus;
  if (waitpid(pid, &wstatus, 0) == -1) {
    perror("failed to wait");
    exit(4);
  }

  if (WIFEXITED(wstatus) && WEXITSTATUS(wstatus) == 0)
    return 0;
  return 1;
}
