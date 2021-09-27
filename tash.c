#ifdef __STDC_ALLOC_LIB__
#define __STDC_WANT_LIB_EXT2__ 1
#else
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/wait.h>

#define PATH_SIZE 16
#define PATH_2_SIZE 16
#define MAX_LENGTH 1024
#define PATH_SIZE3 16
#define Path_SIZE4 16


void addPath();
void parseAccess();
void parseArgs();
void parseBatchFile();
int builtInSwitch();
int pathChange();
void exitBI();
int splitParallelCommand();
int errMsg();

int main(int argc, char* argv[]){
    
    char cread[PATH_SIZE3];
    char pread[PATH_SIZE3];
    
    int pipegc[2];
    int pipepc[2];

    //Get commands
    char *line;
    size_t len = 0;
    ssize_t line_len;
    
    //For path command
    int numPaths = 0;    
    int nread;  
    char *path[numPaths];
    
    //Buffer for pipes
    char ctoP_buff[PATH_2_SIZE];   //Child to Parent
    int ctoP[2];
    
    char gCtoC_buff[PATH_SIZE];  //Grandchild to Child
    int gCtoC[2];
    
    //Initial Paths
    path[0] = "/bin/";
    path[1] = "/user/bin/";

    if(argc == 1){        //Interactive mode
      printf("tash> ");
      
      while((line_len = getline(&line, &len, stdin)) != -1){  //Main Loop (gets user input)
      
        if(pipe(ctoP) < 0) { errMsg(); } 
        if(pipe(pipepc) < 0) {errMsg(); }
        int rc = fork(); 
        
        //Child
        if(rc == 0){
            
            if(strcmp(line, "\n") != 0){

              char *original[line_len];    //Holds the "original" command. Ex: "/bin/ls -l"
              char *commands[line_len];    //This is the main path of the command. Ex: /bin/ls, /bin/cd, etc
              char *arguments[line_len];   //Explained futher on
              char* redo[line_len];        //Used for testing multiple paths
              
              
              int a = splitParallelCommand(commands, redo, line);

              //Add path to the beginning of the command
              int index = 0;
              int conPath = 0; 
              
              int i = 0;
              numPaths = 0;
              
              while(path[i] != NULL) { numPaths++; i++; }
              
              for(i = 0; i < a; i++){
              
                  if(builtInSwitch(commands[i]) == 0){  //Non-Built in
                    
                    if(conPath < numPaths && numPaths != 0){ addPath(path[conPath], commands, original, i); }
                    
                  }else{
                  
                      addPath("", commands, original, i);  //For built-ins
                  }
                
                  //Need to get everything up till the first space so access() will work
                  char hold[strlen(commands[i])];
                  strcpy(hold, commands[i]);
                  char* space = strchr(hold, ' ');
                  char temp[strlen(commands[i])];
                  
                  if(space != NULL){
                    
                    //Find the index
                    index = (int)(space - hold);
                    parseAccess(hold, index, temp);
                    
                  }else{ strcpy(temp, commands[i]); }
                  
                  commands[a] = NULL;

                  
                  
                  //Can the commmand be accessed?
                  if(access(temp, F_OK) == 0 || builtInSwitch(commands[i]) != 0){ 
                  strcpy(commands[i], temp);
                       
                  } else {
                    //Reset (aka removes the old path so it can try a new one)
                    commands[i] = redo[i]; 
                    i--;
                    conPath++;
                    
                    if(conPath >= numPaths){
                      errMsg();
                      exit(0);
                    }
                  }//else
                  //-----
              }//for



            //Beginning of execution
            
            int count = 0;
            int status;
            int p = 0;
            
            if (pipe(gCtoC) < 0) { errMsg(); }
            if (pipe(pipegc) < 0){ errMsg(); }
            
            for(p = 0; p < a; p++){
            
              int rc1 = fork();
              
              if(rc1 == 0){  //Child
                close(gCtoC[0]);
                close(pipegc[0]);
                
                char* token;
                i = 0;
                bool redirectFile = false;
                   
                //Splits the arguments: [bin/ls, -l]
                //Check here for redirection into a file
                char* file = NULL;
                for(token = strtok(original[p], " "); token != NULL; token = strtok(NULL, " ") ) { 
                  
                  if(redirectFile){
                      file = token;
                      redirectFile = false;
                      i++;
                  }

                  if(strcmp(">", token) == 0){
                      redirectFile = true;
                      i++;

                  }else if(redirectFile == false){
                    
                    arguments[i] = token; 
                    i++;
                  }
                  
                }//for
                
                arguments[i] = NULL; 
                

                int out;
                if(builtInSwitch(commands[p]) == 0){ //Non-Built in
                    
                    if(file != NULL){
                  
                      out = open(file, O_RDWR|O_CREAT|O_TRUNC);
                      if (-1 == out) { perror("Nope"); return 255; }
                      
                      dup2(out, STDOUT_FILENO);
                      
                      execv(commands[p], arguments);
                      close(out);
                      
                    }else{
                    
                      execv(commands[p], arguments);
                    }
                  
                  //Reset arguments
                  i = 0;
                  while(arguments[i] != NULL){
                    arguments[i] = NULL;
                    
                  }
                  file = NULL;
                  
                }else if(builtInSwitch(commands[p]) == 1){  //exit
                
                  exitBI();
                  
                }else if(builtInSwitch(commands[p]) == 2){ //Path
                      
                      int strLength = strlen(redo[p]);
                      char totalPaths[strLength];
                      char final[strLength];
                      
                      strcpy(totalPaths, redo[p]);
                      
                      char removeSpace[strLength];
                      strcpy(removeSpace, redo[p]);
                      
                      
                      int rmv = 0;
                      while(removeSpace[rmv] != '\0'){
                      
                        if(isspace(removeSpace[rmv])){
                          removeSpace[rmv] = '\0';
                        }
                        rmv++;
                      }

                      //4 for 'path'
                      if(strlen(removeSpace) != 4){
                        memcpy(final, totalPaths + 5, strlen(totalPaths));
                      
                      }else{
                        memcpy(final, removeSpace + 4, strlen(totalPaths));  //So if the player wants no path, it leaves a space
                        if(strlen(final) == 0){
                          final[0] = ' ';
                        }
                        
                      }
                      
                      //Write to pipe
                      int amountWritten = write(gCtoC[1], final, strlen(final));
                      if(amountWritten == -1) { 
                        errMsg();
                      }  
                      
                  }else if(builtInSwitch(commands[p]) == 3){  //cd
                     
                    
                      int status = write(pipegc[1], arguments[1], strlen(arguments[1]));
                      if (status == -1){
                          errMsg();
                      }
    
                }

                count++;
                
              } else {  //Parent (Main Child)
                  
                 //Wait for child
                 waitpid(rc1, &status, 0);
                 
                //Path 
                close(gCtoC[1]);
                nread = read(gCtoC[0], gCtoC_buff, PATH_SIZE);
              
                if(nread != -1 && nread != 0){
                
                   int amountWritten = write(ctoP[1], gCtoC_buff, strlen(gCtoC_buff));
                   if(amountWritten == -1)
                   {
                          errMsg();
                          
                   }
                     close(ctoP[1]);
                }
                    
                    
                 //cd     
                    close(pipegc[1]);
                    int rstatus = read(pipegc[0], cread, PATH_SIZE3);
                    
                    if(rstatus != -1 && rstatus != 0){

                         int aw =  write(pipepc[1], cread, strlen(cread));
                        
                         if(aw == -1) {
                            errMsg();
                         }
                         close(pipepc[1]);
                    }
                    
              }//else    
            }//for
  
            exit(0);  //Child exit
                        
          }
          
        } else {  //Parent for Main Child

            wait(NULL);     
            
            close(ctoP[1]);
            close(pipepc[1]);
            
              int rstatus= read(pipepc[0], pread, Path_SIZE4);
              
              if (rstatus >= 0){
                  chdir(pread);
  
              }else{
              
                  errMsg();
              }
            
            nread = read(ctoP[0], ctoP_buff, PATH_2_SIZE);
            close(ctoP[0]);
            close(pipepc[0]);
            
            
            if(nread == -1){
              
              errMsg();
              
            } else {
              
              //Clear path
              int pathCounter = 0;
              while(path[pathCounter] != NULL){
                path[pathCounter] = NULL;
                pathCounter++;
              }
            
              char *splitPaths;//Separates the commands
              
              if(strlen(ctoP_buff) > 1){
                splitPaths = strtok(ctoP_buff, " ");
              
                int a = 0;
                while(splitPaths != NULL){
                              
                  splitPaths[strcspn(splitPaths, "\n")] = '\0';
                  path[a] = splitPaths;
                  a++;
                  splitPaths = strtok(NULL, " ");
                              
                }
                
              } else { path[0] = " "; }
            }
            printf("tash> ");
            
        }
        
      }//While
      
    }else if(argc == 2){  //Batch
          
      
      parseBatchFile(argv[1]);
        
    }
    
    return 0;
  
}//Main



//Adds a path to the front of the commands. Ex: /bin/ls or /user/bin/ls
void addPath(char* append, char* commands[], char* original[], int pos){

    char* hold;
    hold = commands[pos];
        
    //Since the command after the & symbol will have a " ", this will just remove it.
    if(hold[0] == ' '){
      memmove(hold, hold+1, strlen(hold));
    }
    commands[pos] = hold;
    original[pos] = hold;
                  
    char *str1;
    char *str2;
    str1 = (char*) malloc (1 + strlen(append) + strlen(commands[pos]));
    str2 = (char*) malloc (1 + strlen(append) + strlen(commands[pos]));
    strcpy(str1, append);
    strcpy(str2, append);
    
    strcat(str1, commands[pos]);
    strcat(str2, original[pos]);
    commands[pos] = str1;
    original[pos] = str2;
    

}



int splitParallelCommand(char* commands[], char* redo[], char *line){

    //Split Line 
    //commands example: ["ls -l", "date"]
    char *splitCommands = strtok(line, "&");//Separates the commands
    int a = 0;
    while(splitCommands != NULL){
                  
      splitCommands[strcspn(splitCommands, "\n")] = '\0';
      commands[a] = splitCommands;
      redo[a] = splitCommands;
      a++;
      splitCommands = strtok(NULL, "&");
                  
    }
    return a;
}


void parseAccess(char* hold, int index, char* temp){
                       
  //Removes 1st space and following arguments for access()
  memcpy(temp, &hold[0], index);
  temp[index] = '\0';
  
}

void parseArgs(char* hold, int index, char* args){
  
  memcpy(args, &hold[index-4], strlen(hold));  //-4 to make up for /bin, will have to change later

}


//0 = false & 1+ = true
int builtInSwitch(char* path){
    
    if(strstr(path, "exit") != NULL){ return 1;
    
    }else if(strstr(path, "path") != NULL){ return 2;
    
    }else if(strstr(path, "cd") != NULL){ return 3; }
    
    return 0;
}


//Built-in Commands

int pathChange(char* path[], int numPaths, char* args[]){
    
    int i = 0;
    while(path[i] != NULL){
      path[i] = NULL;
      i++;
    }
    
    i = 1;
    while(args[i] != NULL){
      path[i - 1] = args[i];
      i++;
    }
    return i;
}

int errMsg()
{ 
   char error_message[30] = "An error has occurred\n"; 
   write(STDERR_FILENO, error_message, strlen(error_message));
   return 0;
}

void exitBI(){

  exit(1);
}

//Batch
//Does everything for the batch processes
void parseBatchFile(char* file){

    char gCtoC_buff[PATH_SIZE];  
    int gCtoC[2];
    int nread;  
    
  //Get File variables
   FILE *fp;
   
   //Open File
   fp = fopen(file, "r" );
   
   //Find the length of the file
   fseek(fp, 0L, SEEK_END);
   long res = ftell(fp);
   rewind(fp);

   if ((fp) == NULL){
       errMsg();
       exit(1);
   }
  
  //Set memory, go to the beginning of the file and set the end to NULL
  char *fileContents = malloc(res + 1);
  fread(fileContents, 1, res, fp);
    
  fileContents[res] = '\0';  
  fclose(fp);


   //The amount of commands
   int count = 0;
   int commandCount = 1;
   while(fileContents[count] != '\0'){
     if(fileContents[count] == '&') { commandCount++; }
     count++;
   }

   //Remove \n
   char* commandLine[commandCount];
   char* remove_N = strtok(fileContents, "\n");
   int lineCount = 0;
   while(remove_N != NULL){

     commandLine[lineCount] = remove_N;
     lineCount++;
     remove_N = strtok(NULL, "\n");
   }
    commandLine[lineCount] = NULL;
    
    if(strlen(commandLine[0]) == 1 && commandCount == 2) { 
       commandLine[0] = NULL; 
      errMsg();
    }
    
    char *commands[lineCount];
    
    //Path
    int pathCount = 2;
    char* path[pathCount]; 
    path[0] = "/bin/";
    path[1] = "/user/bin/";  

   //Splits parellel commands
   int a = 0;
   int cv = 0;
   for(cv = 0; cv < lineCount; cv++){

     char *splitCommands = strtok(commandLine[cv], "&");//Separates the commands
      
      while(splitCommands != NULL){          
        splitCommands[strcspn(splitCommands, "\n")] = '\0';
        commands[a] = splitCommands;
        a++;
        splitCommands = strtok(NULL, "&");
                    
      }
    }

    commands[a] = NULL;

    //Start going through each command
    if (pipe(gCtoC) < 0) { errMsg();}
    int p = 0;
    
    //Goes through all the commands
    for(p = 0; p < a; p++) {
        
       char* arguments[a];
        
       int rc1 = fork();
       
       if(rc1 == 0){        //Child
         close(gCtoC[0]);   
         
         char* token;
         int i = 0;
         int pos = 0;
         bool redirectFile = false;

         if(commands[0] == NULL){
           errMsg();
           break;
         }
         
         //Splits the arguments: [bin/ls, -l]
         //Check here for redirection into a file

         char* file = NULL;
         for(token = strtok(commands[p], " "); token != NULL; token = strtok(NULL, " ")) { 

             if(redirectFile && file == NULL){
               file = token;   
                    
             }else if(redirectFile && file != NULL){
               file = NULL;
               break;
             }
      
             if(strcmp(">", token) == 0){
               redirectFile = true;
               if(arguments[pos - 1] == NULL){
                 errMsg();
                 exit(0);
               }
      
             }else if(redirectFile == false){
               arguments[pos] = token; 
               pos++;
               
             }

        }//for
        arguments[pos] = NULL;
          
          
        if(strlen(commands[p]) == 1) { 
          errMsg();
        }
        
       //Add path
        if(builtInSwitch(commands[p]) == 0){
        
          char* hold;
          hold = arguments[0];
              
          //Since the command after the & symbol will have a " ", this will just remove it.
          if(hold[0] == ' '){
            memmove(hold, hold+1, strlen(hold));
          }
          arguments[0] = hold;
                        
          char *str1;
          str1 = (char*) malloc (1 + strlen(path[0]) + strlen(arguments[0]));
          
          strcpy(str1, path[0]);
          strcat(str1, arguments[0]);
          arguments[0] = str1;
          
          
        }
        
        //Begin Execution

                int out;
                if(builtInSwitch(commands[p]) == 0){ //Non-Built in
                
                    if(file != NULL && redirectFile){  
                      out = open(file, O_CREAT|O_RDWR|O_APPEND, S_IRWXU);
                      if (-1 == out) { perror("Nope");}
                      
                      dup2(out, STDOUT_FILENO);
                      
                      execv(arguments[0], arguments);
                      close(out);

                    }else if(redirectFile == false){ 

                        arguments[pos] = NULL;
                        int er = execv(arguments[0], arguments);
                        
                        if(er == -1){
                          errMsg();
                        }      
                    }else{
                      
                      errMsg();
                    }
                  
                  //Reset arguments
                  i = 0;
                  while(arguments[i] != NULL){
                    arguments[i] = NULL;
                    
                  }
                  file = NULL;
                  
                }else if(builtInSwitch(arguments[0]) == 1){  //exit
                                 
                  if(arguments[1] == NULL){
                  
                    exitBI();
                    
                  }else{
                  
                    errMsg();
                  }
                  
                }else if(builtInSwitch(arguments[0]) == 2){ //path
                      
                    write(gCtoC[1], "pathtrue", 8);  

                }else if(builtInSwitch(arguments[0]) == 3){  //cd
                       
                    if(arguments[1] != NULL && arguments[2] == NULL){ 
                      
                       write(gCtoC[1], "cdtrue", 6);  
                     
                     }else{
                     
                        errMsg();
                     }
                      
                }
          
                exit(0);
        
      }else{  //Parent
      
        wait(NULL);   
      
       close(gCtoC[1]);
       
       //Get's data from pipe buffer
        nread = read(gCtoC[0], gCtoC_buff, PATH_SIZE);
        if(nread == -1){
          errMsg();      
        }
        
        if(strcmp(gCtoC_buff, "pathtrue") == 0){
          
          int pathCount = 0;
          while(path[pathCount] != NULL){
          
            path[pathCount] = NULL;
            pathCount++;  
          
          }//while
          
          char* token;
          pathCount = 0;
          for(token = strtok(commands[p], " "); token != NULL; token = strtok(NULL, " ")) {
            if(strcmp(token, "path") != 0){
              path[pathCount] = token;
              pathCount++;
            
            }//if
          }//for
        }//if (For changing path)
        
        int cdCount = 0;
        char* token;
        char* cdCommand[2];
        
        if(strcmp(gCtoC_buff, "cdtrue") == 0){
          for(token = strtok(commands[p], " "); token != NULL; token = strtok(NULL, " ")) {
            if(strcmp(token, "cd") != 0){
              cdCommand[cdCount] = token;
              cdCount++;
            
            }//if
          }//for
        
          chdir(cdCommand[0]);
          
        
        }//if (cd)
        
      }//End of Parent
      
    }//Main For Loop    
                
  
}






