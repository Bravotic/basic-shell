#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

typedef struct prgline_t {
  const char *command;

  unsigned int line;
  struct prgline_t *next;
  struct prgline_t *prev;
} prgline_t;

prgline_t *programStart;

unsigned int currentLine;

unsigned char canRun;

pid_t shellPID;

static unsigned char isFirstWordNumber (const char *str, unsigned int *i) {
  for ((*i) = 0; str[*i] >= '0' && str[*i] <= '9'; (*i)++);
  return str[*i] <= ' ';
}

#define DELIM_EQUALS 1

static unsigned char hasDelimiter(const char *str, unsigned int *i) {
  for ((*i) = 0; str[*i] != 0; (*i)++) {
    switch(str[*i]) {
    case '=': return DELIM_EQUALS;
    }
  }
  
  return 0;
}

char *readBuffer;
unsigned int bufferSize;

static prgline_t *getForLine(unsigned int line) {
  prgline_t *search;

  search = programStart;
  
  while (search != NULL) {
    if (search->line == line) return search;
      
  }
  return NULL;
}

static void basc_runcommand(char *cmdBuffer) {
  unsigned char dtype;
  unsigned int delimiter;

  if ((dtype = hasDelimiter(cmdBuffer, &delimiter)) != 0) {
    /* This is an equals sign. Set a variable equal to something */
  }
  else {
    /* split into arguments */
    char **args, *cmdPtr;
    unsigned int argsize, argptr;
    pid_t pid, parent;
    
    argsize = 80;
    argptr = 0;
    args = (char**) malloc(argsize * sizeof(char*));

    cmdPtr = strtok(cmdBuffer, " ");
    do {
      args[argptr] = malloc(strlen(cmdPtr) * sizeof(char));
      strcpy(args[argptr], cmdPtr);
      
      argptr++;
      
      if (argptr >= argsize) {
        argsize += 80;
        args = (char**) realloc(args, argsize * sizeof(char*)); 
      }
    } while ((cmdPtr = strtok(NULL, " ")) != NULL);

    if (strcmp(args[0], "list") == 0) {
      for (prgline_t *cur = programStart; cur != NULL; cur = cur->next) {
        printf("%d: %s\n", cur->line, cur->command);
      }
    }

    else if(strcmp(args[0], "goto") ==0) {
      currentLine = atoi(args[1]);
    }

    else if(strcmp(args[0], "run") ==0) {
      prgline_t *cur;
      
      cur = programStart;
      

      canRun = 1;
      
      while(cur != NULL && canRun) {
        char *commandCopy;

        currentLine = cur->line;
        commandCopy = (char*) malloc(sizeof(char) * strlen(cur->command));
        strcpy(commandCopy, cur->command);
        
        basc_runcommand(commandCopy);

        /* We hit a GOTO statement */
        if (currentLine != cur->line) {
          cur = getForLine(currentLine);
        }
        else {
          cur = cur->next;
        }

        /* TODO: Try to reuse command copy */
        free(commandCopy);
      }
    }
    
    else {
      int i;
      pid = fork();
      

      if (pid < 0) {
        printf("There was an error forking\n");
        exit(-1);
      }
      else if (pid) {
        int status;
        do {
          /* Parent must wait for the child process to be done */
          waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
      }
      else {
        /* This is the child, execute the command */
        execvp(args[0], args);
        exit(-1);
      } 
    }

    for (argptr--; argptr != 0; argptr--) {
      free(args[argptr]);
    }
    free(args[0]);
    free(args);
  }
}

static void ll_delete(prgline_t *del) {
  if (del == programStart) {
    if (del->next != NULL) {
      programStart = del->next;
    }
    else {
      programStart = NULL;
    }
  }
  else {
    if (del->next != NULL) {
      del->prev->next = del->next;
    }
    else {
      del->prev->next = NULL;
    }
  }

  free(del);
  
}

static void ll_add(prgline_t *new) {
  prgline_t *current;

  current = programStart;

  while (current != NULL) {
    if (current->line == new->line) {
      if (new->command == NULL) {
        ll_delete(current);
      }
      else {
        current->command = new->command;
        free(new);
      }
      return;
    }

    else if(current->next == NULL) {
      new->prev = current;
      new->next = NULL;
      current->next = new;
      return;
    }

    else if (current->line < new->line
             && current->next->line > new->line) {
      new->next = current->next;
      new->prev = current;
      current->next->prev = new;
      current->next = new;
      
      return;
    }
    else {
      current = current->next;
    }
  }

  programStart = new;
  return;
}



static void basc_createLine(unsigned int line, const char *command) {
  prgline_t *new;

  new = (prgline_t*) malloc(sizeof(prgline_t));

  new->line = line;
  new->command = command;
  new->prev = NULL;
  new->next = NULL;

  
  ll_add(new);
 
}

static void basc_loop() {
  unsigned int delimiter, bufferPtr;
  int c;
  bufferPtr = 0;

  printf("Started BASIC shell with PID %d\n", getpid());
  printf("READY.\n");
  
  while (1) {
    /* Read the command */
    putchar('%');
    putchar(' ');
    while (1)
    {
      c = fgetc(stdin);
      
      if (c == EOF) return;
      else if (c == '\n') break;
      
      readBuffer[bufferPtr++] = (char)c;

      if (bufferPtr >= bufferSize)
      {
        bufferSize += 80;
        readBuffer = (char*) realloc(readBuffer, bufferSize * sizeof(char));
      }
      
    }
    readBuffer[bufferPtr] = 0;

    /* Process the command */

    if (isFirstWordNumber(readBuffer, &delimiter)) {
      char *commandBuffer, *linenumstr, *command;
      unsigned int linenum;

      /* We have no command if this is true */
      if (readBuffer[delimiter] == 0) {
        command = NULL;
      }
      else {
        readBuffer[delimiter] = 0;
        commandBuffer = &readBuffer[delimiter + 1];
        linenumstr = readBuffer;

        command = (char*) malloc(sizeof(char) * strlen(commandBuffer));
        strcpy(command, commandBuffer);
      }
      
      linenum = atoi(linenumstr);
      
      /* We have a line number */
      basc_createLine(linenum, command);
    }
    else {
      /* Just execute it */
      basc_runcommand(readBuffer);
    }

    bufferPtr = 0;
  }

  free(readBuffer);
  
}

static void sigint_handler() {
  fflush(stdout);
  putchar('\n');
  signal(SIGINT, sigint_handler);
  if (getpid() == shellPID) canRun = 0;
  else exit(1);
}

int main () {
  bufferSize = 80;

  shellPID = getpid();
  signal(SIGINT, sigint_handler);
  
  readBuffer = (char*) malloc(bufferSize * sizeof(char));

  programStart = NULL;

  if (readBuffer == NULL)
  {
    printf("Error: Could not allocate buffer for input.\n");
  }


  basc_loop();
}
