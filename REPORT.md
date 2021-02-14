## Introduction

Our first project for ECS 150 is to implement our own **sshell** which involves
many system calls. Initially we had decided to have a struct with a 2D array, 
however, we thought it was not adequate enough to store the command line once 
we hit the piping stage. One of the listed objectives for this assignment is to
test our data structure knownledge so we decided to go with a struct with a 
linked list of 2D arrays to accomodate for multiple commands. Each head of our
command object will have information of what type of process should be done on 
the command. Then we have some parsing functions to add commands into its 
struct, a piping function, redirection function, sls function, and change 
directory function for the command object to successfully execute the entered
command by the user. Once the command is entered, it creates the command
object, parses the arguments into the struct, then it depending on what type
of operation needed to be perfom on the string, it will call exactly one
function. 

## Functions

After getting the command from user our program will want to know what kind 
of operations need to be done before it gets passed into the appropriate 
function to carry it out. After creating a command object, we will initialize
each node of the object with it's own instruction. For example, if we have
a command like *echo Hello world | grep Hello|wc -l* it will be represented 
as the following: 

       | echo Hello world |-->| grep Hello | --> | wc -l | --> |NULL|	
 	      *node1*            *node2*           *node3*     *node4* 

The above linked list of arguments supplied is achieved by our parsing 
functions. 

/* bool parse_meta(char command[]) */

The command line is first passed into the parse_meta function to check for
errors in the entered user input. A lot of the functions in the string library
is used here to break up the strings to check for potential errors. The original
string is copied into another char array so the orginal string remains untouced.
First, we check to make sure that there are no '>' before '|'. We proceed to 
check and return an error where there are no arguments at the beginning of the 
string. Similarly, we check to make sure that the string has arguments following
the meta characters. This process of checking for commands and output files 
after the meta characters uses a small function called "contains_char". If a 
string passed in does not apply to any of the errors tested then the function 
returns true to indicate that the main function can proceed parsing the command 
line into the command object

/*  bool contains_char(char tok[]) */

The "contains_char" is a helper function to check that there are commands
after the meta characters. A string that is cut to the last occurrence of '<'
or '|' to the end of the orginial string is passed into the function. It
checks character by character to check if the rest of the line contains any
letter. If the rest of the line is all whitespace, then there is a missing 
command and this returns false. Else, if there is a character after it returns
true.

/* bool parse_command(struct command *c, char command[]) */

The "parse_command" takes each string in the node and splits them up by spaces
and have NULL as the last string for execvp function to work later. This will
let us know if a command has more than 16 arguments which could not be done in
the previous parse function. By the end of this function each node has it's own
command, seperated by spaces, stored in an array. 

/* bool parse(struct command *c, char command[]) */

After "parse_meta" returns with no error to the main function, it's time to 
put each argument into the command object as a linked list. It begins by 
splitting the string seperated by '>' or '|' and then stores each of those 
string into our 2D array that we allocated in the beginning. The number of 
arguments are being recorded into the object during this process. Once the 
splitting is finished and we have the array of commands or output files it
continues on to transferring the strings from the allocated array into the
object. Another struct is dynamically allocated to help assemble the linked
arguments. Each string in the 2D array in this current fuction will have its own
node in the order of FIFO. The string will be passed into the "parse_command"
function to reserve its own node. A for loop is used to iterate through the 2D
array of strings and become its own object. In the loop after the string becomes
its own object, it will link to the previous node with the help of the 
"add_command" function. If any string has an invalid number of arguments gets
passed in the add_command function then this parse function returns false to 
main and it displays an error message and gets a new command. Lastly, we use 
mmap to allocate memory for the parent and child to access shared memory to
store the statuses of multiple command execution. This is useful in the piping
function. Commands with an appending has 1 less status printed than normal 
commands so that is adjusted at the end of this function. 


/* struct command* add_command(struct command *head, struct command *cmd) **

The head and the node just created is passed in to get linked to the end of the
head. This function allocates a new node and point to the head. It traverses 
through the linked list until it reaches the last node. It will point the last 
node to the node passes in and with the last node pointing to a null node. The 
result will have an additional node added at the end of the linked list. The 
head is returned to "parse" and the process repeats for the rest of the strings 
in the 2D array.

/* bool execute(struct command *cmd) */

The execute function creates a fork process in order to keep the shell running
and not exit running the executable when we hit enter. Usually after calling
execvp the current function execute the command and returns immediately. We're
able to continue receiving commands from user because the child process 
executes the command and exits but the parent is still running waiting for the
child status to store into the struct. If the child's success status will be
stored inside the head of the command. 

/* bool pipe_comd(struct command *head) */

We implemented a piping function to take into consideration multiple number of
pipes. Most of the piping is done in the child process where there is another
parent and child process happening. The subchild attaches stdout to the write
end of the pipe, closes the pipes, and executes the command. If the command
fails, an error message is displayed, current node moves to the next, and the
status of 1 is returned to its parent. The parent waits for the child, saves
the status in its array and connects stdin to the read end of the pipe. It
closes its pipes, moves to the next node and the process repeats until we reach
the last node. If the command line ends with an append or redirection, we write
to the file instead of stdout. For this case, the last node is contains the 
file information to write to and the previous node's command is executed. If 
the last operation is piping then the last node's command is executed. If the 
last operation is a redirection then stdout will connect to the file at the end
of the while loop. This whole process happens in the bigger process of the child
and the parent waits for the child with it's own sub child and sub parent to 
exit. The statuses of each piping is stored using mmap because the parent and 
child each have their own memory and the parent cannot use the memory of the 
statuses of the piping done in the child process. Using mmap in the parse 
function to allocate shared memory between parent and process allows us to store
all of the statuses. 

/* bool redir(struct command *comnd) */

The redir function allocates two struct objects, one for the current
instruction and one for the next. We allocated memory for these because we did
not want to mess with the orignal linked list because we want the head as the 
current node. One node will point to the current node, the command, and the 
other will point to the next node containing the supposed file name. We can
check to see if it is an redirect or append operation by consulting the head.
If the function tries to write to a directory, it will exit with error. If not,
it forks and the child process creates a copy of file descriptor to stdout. 
Instead of writing to stdout, it now writes to the file. The parent waits for
the child process to finish and saves its status as one of its properties.

/* bool changedir(struct command *comd) */

Initially we thought that it took one line to change the directory so adding a 
function for this process was not necessary, however, it needed to make sure
throw an error whenever the user tries to cd into a nonexistent directory.
As long as the directory exists then the status 0 is saved in the object, else
a 1 is save and an error message displays.

/* bool  sls(struct command *cmd) */

"sls" opens the current directory and read all of the contents of the directory
. If the content does not begin with "." then the file name and it's size in 
bytes is printed.

/* void print_status(char cmd[], struct command *comd) */

The completed message will be printed along with the statuses which is stored
in an array that will be traversed.

/* main */

The main function receives command from the user and if a new line is entered
the shell keeps running and keeps allowing more commands. The shell will exit
if the user types "those words"exit". After the user enters something, it goes 
through the parsing stages. If there are no errors during parsing, the head of 
the command goes into exactly one function of either piping, redirecting which 
is for appending as well, changing directory, sls, or just execute normal 
commands. Finally it calls the print function to print the complete message with
the corresponding statuses.

## Final Remarks

Overall, through the implementation of this shell we were able to see how 
system calls worked by using the fork+exec+wait process. We encountered many
difficulties during this process, most of them centered around the parsing and
the piping stage. The error cases took a lot of time to figure out a method to
handle it. The piping stage was difficult to implement because there was a lot
of alternating between parent and child and it was difficult to keep track and
on how to exchange status between parent and child. The piping function may 
have some bugs in it and may cause some infinite loops in some cases. Through
the struggle, however, we were able to see how processes work and gained some
insights on how files and directories work. 

## Sources
A lof of the fork+execute+wait process was inspired by Professor Porquest's 
code.

https://stackoverflow.com/questions/8389033/implementation-of-multiple-pipes-in-
c
https://stackoverflow.com/questions/19672778/trying-to-write-to-an-int-in-shared
-memory-using-mmap-with-a-child-process/19673681
