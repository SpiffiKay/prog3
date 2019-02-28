/**************************************************************************
 * Title: frieseti.smallsh.c						  *
 * Name: Tiffani Auer							  *
 * Due: Mar 4, 2019							  *
 * ***********************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>	
#include <sys/types.h>	
#include <sys/wait.h>
#include <errno.h>	
#include <fcntl.h>
#include <signal.h>	
#include <unistd.h>	

//max amounts for command line
#define MAX_ARGS 512 	  
#define MAX_CHARS 2048 

//struct for flags and handling input/output redirect
struct flags{
    char *input;    //input filename
    char *output;   //output filename
    int redirIn;    //flag input redirect
    int redirOut;   //flag output redirect
    pid_t *bgPIDs;  //track background PIDs
    int numPIDs;    //number of background pids
    int bckgrnd;    //flag if command to run in background
    int allowBckgrnd; //flag if background processes are allowed
};


//prototypes
int CommandLine(char*, char**, char*, struct flags*, int);
void ChangeDir(char**);
int KillProcesses(pid_t[], int);
void fgExitStatus(int);
char *SubPID(char*, int);
void execCommand(char**, struct flags*);





int main(){
    int runShell = 1,
        pCount = 0,
        status = -7,   //-7 indicates no foreground proc have run yet
        shPID = getpid();  //shell pid for $$ expansion
    char *input = malloc(MAX_CHARS * sizeof(char));
    char **args = malloc(MAX_ARGS * sizeof(char));
    char *expanded = malloc(50 * sizeof(char));  //expanded var mem
    memset(input, '\0', MAX_CHARS);
    memset(args, '\0', MAX_ARGS);
    pid_t proc[128];
   
    //instantiate flag struct
    struct flags *flag = malloc(sizeof(struct flags));
    flag->input = calloc(50, sizeof(char));
    flag->output = calloc(50, sizeof(char));
    flag->bgPIDs = calloc(128, sizeof(pid_t));
    flag->allowBckgrnd = 1;    
    flag->numPIDs = 0;

    //ignore ^C
    struct sigaction fg_ign = {0};
    fg_ign.sa_handler = SIG_IGN;
    sigfillset(&sa_sigint.sa_mask);
    fg_ign.sa_flags = 0;
    sigaction(SIGINT, &fg_ign, NULL);

    //catch ^Z to switch bckgrnd mode
    struct sigaction bg_swtch = {0};
//    bg_swtch.sa_handler = 
    sigfullset(&sa_sigstp.sa_mask);
    bg_swtch.sa_flags = 0;
//    sigaction();

    //while shell is running
    do{
      //reset flags struct
      memset(flag->input, '\0', 50);
      memset(flag->output, '\0', 50);
      flag->redirIn = 0;
      flag->redirOut = 0;
      flag->bckgrnd = 0;


      //run command line
      CommandLine(input, args, expanded, flag, shPID);
  
      //blank space
      if(strcmp(args[0], "\0") == 0) 
        continue;
      //comments entered
      else if(strncmp(args[0], "#", 1) == 0)
        continue;    
      //built-in entered: exit
      else if(strcmp(args[0], "exit") == 0)
        runShell = KillProcesses(proc, pCount);
      //built-in entered: status
      else if(strcmp(args[0], "status") == 0)
      { 
         //if no foreground processes have run yet
        if(status == -7)
        {
          write(1, "exit status 0\n", 14);
          fflush(stdout);
        }
        else
          fgExitStatus(status);
      }
      //built in entered: cd
      else if(strcmp(args[0], "cd") == 0)
        ChangeDir(args);
      //all other commands
      else
      {
        execCommand(args, flag);
        printf("all other input\n");

      }
   
    }while(runShell == 1);
  
    //free alloc mem
    free(input);
    free(args);
    free(expanded);  
    free(flag->input);
    free(flag->output);
    free(flag->bgPIDs);
    free(flag);

    return 0;
}

/**************************************************************************
 *Function:
 *Description: 
 *************************************************************************/

/**************************************************************************
 *Function: CommandLine()
 *Description: Prints ": " as the command line, then 
 *************************************************************************/
int CommandLine(char *inputStr, char **args, char *expanded, struct flags *flag, int shPID){
    char *token = NULL;
    char cLine[] = ": ";
    char *input = NULL;  
    char redSym = NULL;
    int i = 0,	
        numChars = -1;
    size_t bufSize = 0;
    memset(args, '\0', MAX_ARGS);  
    memset(inputStr, '\0', MAX_CHARS);
    memset(expanded, '\0', 50);
 
    //print and take command line args
    write(1, cLine, 2);
    fflush(stdout);
    fflush(stdin);
    numChars = getline(&input, &bufSize, stdin);
     //if getline error
    if(numChars == -1)
      clearerr(stdin);
    //copy string from getline to permanent array
    else
      sprintf(inputStr, input);

    //tokenize input
    token = strtok(inputStr, " ");
    //continue to tokenize input, save args to array, check for specific input
    do
    {
      //remove trailing \n from getline
      token[strcspn(token, "\n")] = '\0';
      
      //expand var with $$ with pid
      if(strstr(token, "$$") != 0)
      {  
        sprintf(expanded, token);
        args[i] = SubPID(expanded, shPID); 
      }
      else
        args[i] = token;
    
      //redirect input
      if(strcmp(args[i], "<") == 0)
      {
        //get next arg, save as input file
        token = strtok(NULL, " ");
        token[strcspn(token, "\n")] = '\0';
        args[i] = token;   //add to arg array
        strcpy(flag->input, args[i]);
        printf("flag->input: %s\n", flag->input);
        fflush(stdout);
        flag->redirIn = 1;	//set redir flag
      }
      //redirect output
      else if(strncmp(args[i], ">", 1) == 0)
      {       
        //get next arg, save as output file
        token = strtok(NULL, " ");
        token[strcspn(token, "\n")] = '\0';
        args[i] = token;   //add to arg array
        strcpy(flag->output, args[i]);
        printf("flag->output: %s\n", flag->output);
        fflush(stdout);
        flag->redirOut = 1;  
      }
 
      printf("args[%d]: %s\n", i, args[i]);
      fflush(stdout);

      token = strtok(NULL, " ");
      i++;
    }while(token != NULL);
     
    //do/while increments i one past last arg in loop to add last token
    //to array. decrement i.
    i--;     

    //set flag to run proc in background, take & off arg list
    if(strcmp(args[i], "&") == 0)
    {
      args[i] = '\0';
      flag->bckgrnd = 1;
    }

    //free alloc mem
    free(input);
}

/**************************************************************************
 *Function: ChangeDir()                                                   *
 *Description: if command "cd" was given with no args change directory to *
 *home. If a directory path is given, change directory to that directory, *
 *if it is valid. If it is invalid, write an error message to stderr.     *
 *************************************************************************/
void ChangeDir(char **args){
    char *home = NULL; 
    char cwd[30];
    memset(cwd, '\0', 30);

    //cd with no args
    if(args[1] == NULL)
    {
      printf("cd no args\n");
      fflush(stdout);
      home = getenv("HOME");
      chdir(home);
    }
    //cd with arg
    else
    {  
      //change dir, print to stderr if dir doesn't exist
      if(chdir(args[1]) != 0)  
        fprintf(stderr, "%s: no such file or directory\n", args[1]); 
    }
}

/**************************************************************************
 *Function: SubPID()							  *
 *Description: Takes a pointer to a string that is an arg that needs 	  *
 *expanded to include the shell's pid, as well as an int holding the shell*
 *pid. It then dissects the arg, and places the pid wherever the 	  *
 *placeholder ($$) is. It then returns a pointer to the new arg with pid. *
 *************************************************************************/
char* SubPID(char *addPID, int pid){
   int frLen = 0;
   char newArg[50],
        wPID[7];
   char *ptr = NULL;
   memset(wPID, '\0', 7);
   memset(newArg, '\0', 50);

   //isolate $$ and separate the parts of the string around it
   ptr = strstr(addPID, "$$");
   frLen = strlen(addPID) - strlen(ptr); //length of string before $$

   //replace $$ with subshell pid
   strncpy(newArg, addPID, frLen); //front of string
   sprintf(wPID, "%d", pid); 
   strcat(newArg, wPID);  //pid

   //if there is string after $$, add to arg
   if(strlen(ptr)> 2)
   {
     ptr += 2;  //cut off $$
     strcat(newArg, ptr);
   }

   //set new arg
   sprintf(addPID, newArg);

   return addPID;   
}

/**************************************************************************
 *Function:
 *Description: 
 *************************************************************************/
void execCommand(char** args, struct flags *flag){
    pid_t spawnPID = -7;
    int input = 0,
	output = 0,
        childExit = 0;
    char temp[100];
  
    spawnPID = fork();
    switch(spawnPID) {
      case -1:
         fprintf(stderr, "Apparently you can't be trusted with forks.\n");
         exit(1);
         break;
      case 0:

         printf("child process\n");
         if(flag->redirIn == 1)
         {
           printf("input redir\n");
           fflush(stdout);
           //open file for input redir
           input = open(flag->input, O_RDONLY);
           if(input == -1)
           {
             fprintf(stderr, "cannot open %s for input\n", flag->input);
             exit(1);
           }
           
           //redirect
           if(dup2(input, STDIN_FILENO) == -1)
           {
             fprintf(stderr, "cannot open %s for input\n", flag->input);
             exit(1);            
           }
           
           //close file after execution
           fcntl(input, F_SETFD, FD_CLOEXEC);
         }

         if(flag->redirOut = 1)
         {
           printf("output redir\n");
           fflush(stdout);
           //open file for input redir
           output = open(flag->output, O_WRONLY | O_CREAT | O_TRUNC, 0666);
           if(output == -1)
           {
             fprintf(stderr, "cannot open %s for output\n", flag->output);
             exit(1);
           }
           
           //redirect
           if(dup2(output, STDOUT_FILENO) == -1)
           {
             fprintf(stderr, "cannot open %s for output\n", flag->output);
             exit(1);            
           }
           
           //close file after execution
           fcntl(output, F_SETFD, FD_CLOEXEC);
         }
         break;
      default:
         break;
    }
}





/**************************************************************************
 *Function: KillProcesses()                                               *
 *Description: Takes the number of running background processes as an     *
 *argument. Then goes through an array of pointers to the processes and   *
 *uses the kill function to send the SIGTERM signal to terminate all the  *
 *currently running background programs. It then returns zero to end the  *
 *program.                                                                *
 *************************************************************************/
int KillProcesses(pid_t proc[], int pCount){
    int i = 0;

    //kills all running background processes
    for(i; i < pCount; i++)
    {
      kill(proc[pCount], SIGTERM);
    }  

    printf("kill program\n");
    return 0;
}

/**************************************************************************
 *Function: fgStatus()
 *Description: 
 *************************************************************************/
void fgExitStatus(int status){
    char exitStatus[100];
    memset(exitStatus, '\0', 100);    

    //exited by status
    if(WIFEXITED(status))
    {
      sprintf(exitStatus,"exit status %d\n", WEXITSTATUS(status));
      write(1, exitStatus, strlen(exitStatus));
    }  
    //terminated by signal 
    else
    {
      sprintf(exitStatus, "terminated by signal %d\n", WTERMSIG(status));
      write(1, exitStatus, strlen(exitStatus));
    }
}

