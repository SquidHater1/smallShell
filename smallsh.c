/* smallsh.c
 * Ryan Smith
 * smithry9@oregonstate.edu
 * CS344 Assignment 3
*/

#define _POSIX_C_SOURCE 200809L
#define DEBUG false //Debug variable to display more detailed outputs if something is not working properly

#define MAX_INPUT_LENGTH 2048//the largest input size allowed from the user
#define MAX_NUM_ARGUMENTS 512//the most command line arguments allowed from the user
#define MAX_BACKGROUND_PROCESSES 100//the max number of background processes allowed at one time


#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>

bool isBackgroundProcess = false;//bool to track if the current process is running in the background
bool foregroundOnly = false;//bool to track if the shell is running in foreground only mode
bool modeChanged = false;//keeps track of if the mode has changed since the last time the command line has returned to the user

int recentStatus = 0;//Keeps track of the most recent exit status from a command

/* promptUser(char [], char [])
 * Takes two char arrays as inputs
 * Returns an integer
 * Prompts the user for an input, using the promptString to represent the prompt
 * Puts the user input in the outputString array and returns the length of the string
 */
int promptUser(char promptString[], char outputString[]){
	//Prints the prompt string to let the user know to type
	printf("%s",promptString);
	//Takes the user input, up to MAX_INPUT_LENGTH characters
	fgets(outputString, MAX_INPUT_LENGTH, stdin);
	
	//Creates variable to track number of characters
	int numCharacters = 0;
	//Loops through up to MAX_INPUT_LENGTH
	for(int i = 0; i < MAX_INPUT_LENGTH; i++){
		//Searches for newline character
		if(outputString[i] == '\n'){
			//replaces newline with null terminator
			outputString[i] = '\0';
			//sets the number of characters
			numCharacters = i;
			break;
		}
	}

	return numCharacters;
}

/* isBlankOrComment(char [], int)
 * Takes a char array and an int as inputs
 * Returns a bool
 * Determines if the input string is blank, nothing but spaces, or has a # in front meaning it is a comment
*/
bool isBlankOrComment(char userInput[], int numCharacters){
	//If the first charcter is a null terminator, return that it is blank
	if(userInput[0] == '\0'){
		return true;
	}
	//Variable to track if there are only spaces in the string
	bool onlySpaces = true;
	//Loops for the number of characters
	for(int i = 0; i < numCharacters; i++){
		//If a # is the first character after any potential spaces, return that it is a comment
		if(userInput[i] == '#'){
			return true;
		}
		//If any other character is found besides '#' and ' ', set that the string is not only spaces
		if(userInput[i] != ' '){
			onlySpaces = false;
			break;
		}
	}
	//return if the string is only spaces
	return onlySpaces;

}

/* convertToWords(char [], int, char [][])
 * Takes a char array, a string array, and an integer as inputs
 * Returns the number of words in the input string
 * Fills the words into an output array
 */
int convertToWords(char userInput[], int numCharacters, char outputWords[MAX_NUM_ARGUMENTS][MAX_INPUT_LENGTH]){
	int wordCount = 0;
	//Loops through each character in userInput
	for(int i = 0; i <= numCharacters; i++){
		//If the character is a space, null terminator, or newline, skip to the next character
		//This means that the commands will still be successfully pulled even with extra spaces
		if(userInput[i] != ' ' && userInput[i] != '\0' && userInput[i] != '\n'){
			int wordLength = 0;
			//set the starting position of the word
			int wordStart = i;
			//loop starting at the word and going to the end of the string
			for(int x = wordStart; x <= numCharacters; x++){
				//if another space is reached, or a newline or null terminator is found, then that is the end of the word
				//Otherwise increase the length of the word by 1
				if(userInput[x] != ' ' && userInput[x] != '\0' && userInput[x] != '\n'){
					wordLength++;
				}else{
					//if the end of the word is found, skip i ahead to the end of the word and break the loop
					i = x;
					break;
				}
			}
			//put the word into the outputWords array
			for(int j = wordStart; j < wordStart+wordLength; j++){
				outputWords[wordCount][j-wordStart] = userInput[j];
			}
			//add a null terminator to the end of the word
			outputWords[wordCount][wordLength] = '\0';
			//increase the total number of words by 1
			wordCount++;
		}
	}
	return wordCount;
}

//handle_SIGINT
//SIGINT should just exit if passed
void handle_SIGINT(int signo){
	exit(2);
}

//handleSIGTSTP
//SIGTSTP will toggle the foreground only mode for the shell, and let the main function know to print the change
void handle_SIGTSTP(int signo){

	if(foregroundOnly){//toggle foregroundOnly
		foregroundOnly = false;
	}else{
		foregroundOnly = true;
	}
	modeChanged = true;//set modeChanged to true so the main function will print a message.

}

/*handleCommand
 *takes an array of strings, an integer representing the length of the array, a pointer to a bool, an int array of currently running pids, and a length for that array
 *This function will take an array of arguments for a command and determine if the command is valid.
 *If the command is one of the built in commands, it will run it, otherwise it will determine if it is a background process, and if it should redirect the input and/or output.
 *Afterwards, it will pass the command to exec and run it there.
*/
void handleCommand(char command[MAX_NUM_ARGUMENTS][MAX_INPUT_LENGTH], int numArguments, bool *repeat, int currentProcesses[MAX_BACKGROUND_PROCESSES], int *numBackground){
	
	//If there are no arguments, don't do anything
	if(numArguments >= 1){

	if(strcmp(command[0], "exit") == 0){//exit command found as first argument
		if(numArguments > 1){//If there are more than 1 argument, print an error and set success to -1
			printf("Error: too many arguments for command \"exit\"\n");
			fflush(stdout);
		}else{//if there is only 1 command, then the program will exit
			if(DEBUG){
				 printf("Exit command found... Exiting...\n");
				fflush(stdout);
			}
			for(int i = 0; i < *numBackground; i++){
				if(currentProcesses[i] != -1){
					kill(currentProcesses[i], SIGTERM);
				}
			}
			printf("\n");
			fflush(stdout);
			*repeat = false;
		}
	}
	else if(strcmp(command[0],"cd") == 0){//cd command found as first argument
		if(numArguments == 1){//if there are no extra arguments besides just cd, change to the home directory
			if(DEBUG) {
				printf("Changing to home directory...\n");
				fflush(stdout);
			}
			chdir(getenv("HOME"));//change to home directory
		}else if(numArguments == 2){//If there are more 
			if(DEBUG){
				printf("Attempting to change to directory %s...\n",command[1]);
				fflush(stdout);
			}
			if(chdir(command[1]) == -1){//If changing directories fails, print an error
				printf("Error: unable to change to directory %s...\n",command[1]);
				fflush(stdout);
			}
		}else{
			printf("Error: too many arguments for command \"cd\"\n");//if more than 2 arguments are found, print an error
			fflush(stdout);
			
		}
			
	}else if(strcmp(command[0],"status") == 0){//status command found as first argument
		if(numArguments > 1){//if there are more than 1 arguments, print an error
			printf("Error: too many arguments for command \"status\"\n");
			fflush(stdout);
		}else{//otherwise get the exit status of the last run process
			if(DEBUG) {
				printf("Status command found...\n");
				fflush(stdout);
			}
			printf("Exit value %i\n",recentStatus);
			fflush(stdout);
		}
	}else{//If none of the default commands are found, setup code for exec
		
		bool thisIsBackground = false;//bool to keep track of if the command should be a background process
		if(strcmp(command[numArguments-1],"&") == 0){//checks if the last argument in the command is "&"
			if(foregroundOnly == false){//If the shell isn't currently in foreground only mode
				thisIsBackground = true;//set this command to be a background process
			}
			numArguments -= 1;//lower the number of arguments by 1 so "&" isn't included in the exec call
		}

		int childStatus;
		pid_t spawnPid = fork();//create fork
		if(spawnPid == -1){//if the fork failed, print an error
			printf("Error creating child process\n");
			fflush(stdout);
		}else if(spawnPid == 0){
			//This is the child process
			if(DEBUG){
				printf("Child pid: %d, Running command %s\n",getpid(),command[0]);
				fflush(stdout);
			}

			bool changedIn = false;//changedIn and changedOut keep track if the input and/or output have been redirected
			bool changedOut = false;

			for(int x = 0; x < 2; x++){//runs twice, checking the second to last argument each time for redirection symbols
				int i = numArguments-2;
				if(strcmp(command[i],">") == 0 && changedOut == false){//check if the output redirection symbol is found. changedOut makes sure this only happens once
					//redirect output
					char *newFilePath = command[i+1];// get the new file path from the command array, it will be the next argument after the symbol
					int outFile = open(newFilePath,O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);//open the file for writing only

					if(outFile == -1){//if the file doesn't open, print an error and exit
						printf("Error opening file \"%s\"\n",newFilePath);
						fflush(stdout);
						exit(1);
					}
					int result = dup2(outFile,1);//run dup2 to redirect the output to the new file
					if(result == -1){//if there is a problem redirecting, print an error and exit
						printf("Error redirecting output\n");
						fflush(stdout);
						exit(2);
					}
						
					changedOut = true;//set changed out to true to prevent this from running again, even if there are duplicate symbols
					numArguments -= 2;//decrease the number of aruments by 2, preventing the output redirection from passing to exec

				}else if(strcmp(command[i],"<") == 0 && changedIn == false){//check if the input redirection symbol is found. changedIn make sure this only happens once
					//redirect input
					char *newFilePath = command[i+1];//get new file path from command array
					int inFile = open(newFilePath,O_RDONLY, S_IRUSR | S_IWUSR);//open the input file for reasing only
					
					if(inFile == -1){//if the file doesn't open, print an error and exit
						printf("Error opening file \"%s\"\n",newFilePath);
						fflush(stdout);
						exit(1);
					}

					int result = dup2(inFile, 0);//run dup2 to redirect the input to the new file
					if(result == -1){//if there is a problem redirecting, print an error and exit
						printf("Error redirecting input\n");
						fflush(stdout);
						exit(2);
					}
						
					changedIn = true;//set changed in to true to prevent this from running again, even if there are duplicate symbols
					numArguments -= 2;//decrease the number of arguments by 2, preventing the input redirection from passing to exec
				}
			}

			//Set signal handlers for this child process
			struct sigaction ignore_action = {{0}};
			ignore_action.sa_handler = SIG_IGN;//create ignore action
			if(thisIsBackground && *numBackground >= MAX_BACKGROUND_PROCESSES){
				printf("Error: too many background processes.\n");
				fflush(stdout);
				exit(0);
			}
			if(thisIsBackground){//Check if this child is a background process, the signals need to be handled different if they are
				sigaction(SIGTSTP,&ignore_action,NULL);//ignore SIGTSTP(Ctrl+z) and SIGINT(Ctrl+c) if in a background process
				sigaction(SIGINT,&ignore_action,NULL);

				//if the input and output haven't been redirected, redirect them to dev/null/

				if(changedIn == false){//if input wasn't redirected
					int devNull = open("/dev/null",O_RDONLY);//open /dev/null for reading only
					if(devNull == -1){//If there is an error opening /dev/null, print an error and exit
						printf("Error opening file \"/dev/null\"\n");
						fflush(stdout);
						exit(1);
					}
					int result = dup2(devNull, 0);//redirect input to /dev/null
					if(result == -1){//if there is an error redirecting input, print an error and exit
						printf("Error redirecting input\n");
						fflush(stdout);
						exit(2);
					}	
				}
				
				if(changedOut == false){//if output wan't redirected
					int devNull = open("/dev/null",O_WRONLY);//open /dev/null for writing only
					if(devNull == -1){//If there is an error opening /dev/null, print an error and exit
						printf("Error opening file \"/dev/null\"\n");
						fflush(stdout);
						exit(1);
					}
					int result = dup2(devNull, 1);//redirect output to /dev/null
					if(result == -1){//If there is an error redirecting output, print an error and exit
						printf("Errpr redirecting output\n");
						fflush(stdout);
						exit(2);
					}

				}

			}else{
				//if not in a background process
				struct sigaction SIGINT_action = {{0}};//create SIGINT(Ctrl+c) struct

				SIGINT_action.sa_handler = handle_SIGINT;//set the function to the handle_SIGINT function
				sigfillset(&SIGINT_action.sa_mask);//fill the struct
				SIGINT_action.sa_flags = 0;
				sigaction(SIGINT,&SIGINT_action,NULL);

				//Set ignore mask to prevent process fro exiting if (Ctrl+z) signal is recieved
				//https://edstem.org/us/courses/6837/discussion/535386
				sigset_t mask;
				sigfillset(&mask);
				sigdelset(&mask,SIGBUS);
				sigdelset(&mask,SIGFPE);
				sigdelset(&mask,SIGILL);
				sigdelset(&mask,SIGSEGV);

				ignore_action.sa_mask = mask;
				sigaction(SIGTSTP,&ignore_action,NULL);//ignore SIGTSTP(Ctrl+z)
			}
			
			char *execArgs[numArguments+1];//Create new array one longer than the number of arguments that can be passed to exec
			for(int i = 0; i < numArguments; i++){
				execArgs[i] = command[i];//fill the new array with all the arguments from the command array
			}
			if(numArguments > 0){//as long as there are more than 0 arguments
				execArgs[numArguments] = NULL;//set the last spot in the array to null
				execvp(execArgs[0],execArgs);//then pass it into execvp
				//perror("exec()\n");//exec will only run anything past the function call if it failed, so print an error and exit
				//fflush(stdout);
			}
			exit(2);
		}else{
			//This is the parent process

			if(thisIsBackground == false){//if the child process isn't a background process
				spawnPid = waitpid(spawnPid, &childStatus, 0);//wait for the child to finish running
				if(WIFEXITED(childStatus) == false){//If the child didn't exit properly
					printf("terminated by signal %d\n",WTERMSIG(childStatus));//print termination signal
					fflush(stdout);
					recentStatus = WTERMSIG(childStatus);//set the status of the most recent command to the termination signal
				}else{//if the child did exit properly
					recentStatus = WEXITSTATUS(childStatus);//set the status of the most recent command to the exit status
				}
			}else{//if the child process is a background process
				currentProcesses[*numBackground] = spawnPid;//adds the pid of the child to the array of background processes
				*numBackground = *numBackground + 1;//increased the number of background processes by 1
			}
		
		}
	}
	}
}
/* expandCommands(char [][], int)
 * takes an array of strings and an int as inputs
 * returns nothing
 * for each string in the array, replace any instance of "$$" with the process id
*/
void expandCommands(char command[MAX_NUM_ARGUMENTS][MAX_INPUT_LENGTH], int numArguments){
	

	if(DEBUG){
		printf("Process ID is: %d\n",getpid());
		fflush(stdout);
	}
	char temp[500];
	int pidLen = snprintf(temp, 500, "%d\n",getpid());
	if(DEBUG){
		printf("pidLen: %i\n",pidLen);
		fflush(stdout);
	}
	//string version of pid
	char strpid[pidLen];
	//store pid in strpid
	sprintf(strpid, "%d", getpid());

	char buffer[MAX_INPUT_LENGTH];//buffer for parts of word that come after $$
	for(int i = 0; i < numArguments; i++){//repeat for all arguments in the command array
		int pos = 0;//position in the command string
		while(command[i][pos] != '\0'){//While the end of the coommand isn't reached
			if(command[i][pos] == '$' && command[i][pos+1] == '$'){//if there are 2 '$' in a row
				
				int cpos = pos+2;//current position in the command string, after $$
				int bpos = 0;//current position in the buffer string
				while(command[i][cpos] != '\0'){//fill the buffer with the rest of the command string, starting at cpos
					buffer[bpos] = command[i][cpos];
					cpos++;
					bpos++;
				}
				buffer[bpos] = '\0';//put a null terminator at the end of the buffer string
				for(int j = 0; j < pidLen; j++){//starting at the first $, replace the letters in the command string with the pid
					command[i][pos+j] = strpid[j];
				}
				
				bpos = 0;//reset buffer position to 0
				cpos = pos+pidLen-1;//set command position to the end of the pid
				while(buffer[bpos] != '\0'){//add the contents of the buffer to the end of the command string
					command[i][cpos] = buffer[bpos];
					bpos++;
					cpos++;
				}
				command[i][cpos] = '\0';//add a null terminator to the end of the command string
			}
		pos++;//increase the position in the command string by 1
		}
	}
}

int main(int argc, char *argv[]){
	
	struct sigaction ignore_action = {{0}}, SIGTSTP_action = {{0}};//create handlers for ignore action and SIGTSTP (Ctrl+z)
	ignore_action.sa_handler = SIG_IGN;
	sigaction(SIGINT,&ignore_action,NULL);//Ignore SIGINT (Ctrl+c)

	SIGTSTP_action.sa_handler = handle_SIGTSTP;//set SIGTSTP (Ctrl+z) handler function to handle_SIGTSTP
	sigfillset(&SIGTSTP_action.sa_mask);
	SIGTSTP_action.sa_flags = 0;
	sigaction(SIGTSTP,&SIGTSTP_action,NULL);

	bool repeat = true;//Code will continue to prompt user for inputs while repeat is true
	int currentProcesses[MAX_BACKGROUND_PROCESSES];//array to store the pids of each background processes running
	int numBackground = 0;//the current number of background processes running

	while(repeat){
		//Before user input is prompted, print out changed to foreground mode and clear any zombie processes
		if(modeChanged == true){//If the foreground only mode has been changed since the last time the user was prompted, print out the new mode
			modeChanged = false;
			if(foregroundOnly){
				printf("\nEntering foreground-only mode (& is now ignored)\n");
				fflush(stdout);
			}else{
				printf("\nExiting foreground-only mode\n");
				fflush(stdout);
			}
		}

		//loop through the background processes and wait for each one using WNOHANG
		for(int i = 0; i < numBackground; i++){
			int childStatus;
			pid_t childPid = currentProcesses[i];//get process pid from array
			childPid = waitpid(childPid, &childStatus, WNOHANG);//wait for process, immediatly returning 0 if the process is still running

			if(childPid != 0){//if the process isn't still running
				currentProcesses[i] = -1;//set the pid to -1 in the process array
				printf("background pid %i is done: exit value %i\n",childPid, WEXITSTATUS(childStatus));
				fflush(stdout);
			}
		}

		for(int i = 0; i < numBackground; i++){//loop through the array again
			if(currentProcesses[i] == -1){//if the pid of the process was set to -1
				for(int x = i+1; x < numBackground; x++){//move all the other pids after this one back one spot in the array, replacing the -1 spot
					currentProcesses[x-1] = currentProcesses[x];
				}
				numBackground -= 1;//decrease the number of background processes by 1
			}
		}

		char userInput[MAX_INPUT_LENGTH];//create array to read user input
		int numChars = promptUser(": ", userInput);//runs prompt user function and stores the number of characters in numChars

		if(isBlankOrComment(userInput, numChars)){//If the command is just a blank line or a comment line, do nothing
			if(DEBUG){
				printf("Blank line or comment! No commands...\n");
				fflush(stdout);
			}
		}else{
			char arguments[MAX_NUM_ARGUMENTS][MAX_INPUT_LENGTH];//create an array for the seperate arguments in the command
			int numArguments = convertToWords(userInput, numChars, arguments);//convert the input to seperate arguments and store the number of arguments in numArguments
			if(DEBUG){
				printf("Arguments found: \n");
				for(int i = 0; i < numArguments; i++){
					printf("%s\n",arguments[i]);
				}
				fflush(stdout);	
			}
			expandCommands(arguments, numArguments);//Expand any instance of $$ into the process id of the shell
			if(DEBUG){
				printf("Expanded arguments: \n");
				for(int i = 0; i < numArguments; i++){
					printf("%s\n",arguments[i]);
				}
				fflush(stdout);
			}
			handleCommand(arguments, numArguments, &repeat, currentProcesses, &numBackground);//pass the command array into the handleCommand function
		}


	}

	return 0;
}
