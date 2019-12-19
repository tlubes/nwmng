/*************************************************************************
    > File Name: main.c
    > Author: Kevin
    > Created Time: 2019-12-13
    > Description:
 ************************************************************************/

/* Includes *********************************************************** */
#include <stdio.h>
#include "logging.h"
/* TODO: above includes are used for test */
#include <unistd.h>

#include <sys/wait.h>

#include "projconfig.h"
#include "cli/cli.h"
#include "mng/mng.h"
#include "utils.h"

#ifdef SINGLE_PROC
#include "cfg.h"
#include "cfgdb.h"
#endif
/* Defines  *********************************************************** */

/* Global Variables *************************************************** */
#if 0
typedef int (*proc)(void);

static proc procs[] = {
  cli_proc, mng_proc
};
static const int proc_num = sizeof(procs) / sizeof(proc);
pid_t pids[10] = { 0 };
#endif
/* Static Variables *************************************************** */

/* Static Functions Declaractions ************************************* */
size_t get_executable_path(char* processdir, char* processname, size_t len)
{
  char* path_end;
  if (readlink("/proc/self/exe", processdir, len) <= 0) {
    return -1;
  }
  path_end = strrchr(processdir, '/');
  if (path_end == NULL) {
    return -1;
  }
  ++path_end;
  strcpy(processname, path_end);
  *path_end = '\0';
  return (size_t)(path_end - processdir);
}

int main(int argc, char *argv[])
{
#if 0
  if (argc > 1) {
    logging_init(argv[1], 1, LOG_MINIMAL_LVL(LVL_VER));
  }
  logging_demo();
  set_logging_tostdout(0);
  logging_demo();
  set_logging_tostdout(1);
  set_logging_lvl_threshold(LOG_MINIMAL_LVL(LVL_WRN));
  logging_demo();

  return 0;
#endif
#if 0
  pid_t pid;
  int i = 0;
  for (i = 0; i < proc_num - 1; i++) {
    pid = fork();

    if (pid == 0 || pid == -1) {
      /* If it's child, break, so only the parent process will fork */
      break;
    }
    pids[proc_num - 1 - i] = pid;
  }

  if (i == proc_num - 1) {
    cli_proc_init(proc_num - 1, pids);
  }

  procs[proc_num - 1 - i]();

  if (i == proc_num - 1) {
    wait(NULL);
  }

  switch (pid) {
    case -1:
      break;
    case 0:
      break;
    default:
      break;
  }
#endif
  err_t e;
  char path[PATH_MAX];
  char processname[1024];
  get_executable_path(path, processname, sizeof(path));
  printf("directory:%s\nprocessname:%s\n", path, processname);
  return 0;
  EC(ec_success, logging_init("/home/zhfu/work/projs/nwmng/logs/cli.log",
                              0, /* Not output to stdout */
                              LOG_MINIMAL_LVL(LVL_VER)));
  /* ASSERT_MSG(0, "%s\n", "ssdfasdf"); */
  /* ASSERT(0); */
#ifdef SINGLE_PROC
  EC(ec_success, cfg_init());
  cfg_proc();
  goto out;
#endif

  cli_proc_init(0, NULL);
  cli_proc();

  out:
  /* For free allocated memory if the program returns */
  logging_deinit();
  cfgdb_deinit();
  return 0;
}
