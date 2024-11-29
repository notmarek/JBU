#include "openlipc.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#define SERVICE_NAME "com.koreader.epub"
bool done = false;
pid_t koreader_pid = -1;

LIPCcode stub(LIPC *lipc, const char *property, void *value, void *data) {
  char *id = strtok((char *)value, ":");
  printf("Stub called for %s with value %s -- sending succes\n", property,
         (char *)value);
  char *response = malloc(strlen(id) + 3 + 1);
  snprintf(response, strlen(id) + 3 + 1, "%s:0:", id);
  char *target = malloc(strlen(property) + 6 + 1);
  snprintf(target, strlen(property) + 6 + 1, "%sresult", property);
  LipcSetStringProperty(lipc, "com.lab126.appmgrd", target, response);
  free(response);
  free(target);

  return LIPC_OK;
}

LIPCcode unload(LIPC *lipc, const char *property, void *value, void *data) {
  done = true;
  kill(koreader_pid, SIGINT);
  return stub(lipc, property, value, data);
}

/* Converts a hex character to its integer value */
char from_hex(char ch) {
  return isdigit(ch) ? ch - '0' : tolower(ch) - 'a' + 10;
}

char *url_decode(char *str) {
  char *pstr = str, *buf = malloc(strlen(str) + 1), *pbuf = buf;
  while (*pstr) {
    if (*pstr == '%') {
      if (pstr[1] && pstr[2]) {
        *pbuf++ = from_hex(pstr[1]) << 4 | from_hex(pstr[2]);
        pstr += 2;
      }
    } else if (*pstr == '+') {
      *pbuf++ = ' ';
    } else {
      *pbuf++ = *pstr;
    }
    pstr++;
  }
  *pbuf = '\0';
  return buf;
}


LIPCcode go(LIPC *lipc, const char *property, void *value, void *data) {
  if (koreader_pid != -1) {
    kill(koreader_pid, SIGINT);
  }

  char *id = strtok((char *)value, ":");
  char *uri = strtok(NULL, "");
  char *path = uri + strlen("app://") + strlen(SERVICE_NAME);
  char *decoded = malloc(strlen(path) + 7 + 1);
  sprintf(decoded, "file://%s", path);
  char cmd[1024] = {0};
  sprintf(cmd, "/mnt/us/koreader/koreader.sh --appmgr --asap %s", decoded);


  // execl("/var/local/mkk/su", "su", "-c", &cmd, NULL);

  koreader_pid = fork();
  if (koreader_pid == 0) {
        execl("/var/local/mkk/su", "su", "-c", &cmd, NULL);
  //  char *args[] = {"--appmgr", "--asap", decoded, NULL};
  //  execv("/mnt/us/koreader/koreader.sh", args);
  }
  free(decoded);
  // system(cmd);
  return stub(lipc, property, value, data);
}

int main(void) {
  LIPC *lipc;
  char *status;
  LIPCcode code;
  lipc = LipcOpenEx(SERVICE_NAME, &code);
  if (code != LIPC_OK)
    return 1;

  LipcRegisterStringProperty(lipc, "load", NULL, stub, NULL);
  LipcRegisterStringProperty(lipc, "unload", NULL, unload, NULL);
  LipcRegisterStringProperty(lipc, "pause", NULL, stub, NULL);
  LipcRegisterStringProperty(lipc, "go", NULL, go, NULL);
  LipcSetStringProperty(lipc, "com.lab126.appmgrd", "runresult",
                        "0:com.koreader.epub");

  while (!done) {
    sleep(1);
  }

  LipcClose(lipc);
  return 0;
}
