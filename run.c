#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/times.h>

int main(int argc, char **argv){
  // For optarg()
  extern char *optarg;
  extern int optind, opterr, optopt;

  pid_t chPID;
  char * c;
  clock_t start, end;
  struct tms t;
  int i=0, max = 3;
  double step = 0.001;
  double d = 0.0;

  int opt;
  while((opt=getopt(argc, argv, "c:s:")) != -1){
    if (optarg == NULL){
      printf("Optarg is null!!");
      return -1;
    }
    switch(opt) {
    case 'c': max=(int)strtoul(optarg, (char **) NULL, 0);
      break;
    case 's': d=(double)strtod(optarg, (char **) NULL);
      break;
    default: printf("Bad user argument: %c", (char) opt);
      break;
    }
  }


  while(i<max){
    if((start=times(&t))==(clock_t)-1){
      printf("Bad clock!\n");
      return -1;
    }
    switch(chPID=fork()){
    case -1:
      printf("Fork Failed!!\n");
      return -1;
    case 0:
      c = (char *) malloc(10*sizeof(char));
      sprintf(c,"%.5f",d);
      c[9]='\0';
      execlp("./mandel","mandel","-b",(char *const)c, (char *) NULL);
      return -1;
    default:
      wait(NULL);
      if((end=times(&t))==(clock_t)-1){
        printf("Bad clock!\n");
        return -1;
      }
      i++;
      d+=step;
      end = end-start;
      printf("Time: %.2f seconds\n",0.01 * (double)end);
    }
  }

  return 0;
}
