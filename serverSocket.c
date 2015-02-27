/*
   serverSocket.c
   Jordan Humphries
*/
 
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <utmp.h>
#include <err.h>
#include <wordexp.h>

#define PORT "<ENTER PORT NUMBER>"  // the port users will be connecting to

#define BACKLOG 10     // how many pending connections queue will hold

void sigchld_handler(int s){
   while(waitpid(-1, NULL, WNOHANG) > 0);
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa){
   if (sa->sa_family == AF_INET) {
      return &(((struct sockaddr_in*)sa)->sin_addr);
   }

   return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char * argv[], char * envp[]){
   int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
   struct addrinfo hints, *servinfo, *p;
   struct sockaddr_storage their_addr; // connector's address information
   socklen_t sin_size;
   struct sigaction sa;
   int yes=1;
   char s[INET6_ADDRSTRLEN];
   int rv;
   memset(&hints, 0, sizeof hints);
   hints.ai_family = AF_UNSPEC;
   hints.ai_socktype = SOCK_STREAM;
   hints.ai_flags = AI_PASSIVE; // use my IP

   if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
      fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
      return 1;
   }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
       if ((sockfd = socket(p->ai_family, p->ai_socktype,
          p->ai_protocol)) == -1) {
          perror("server: socket");
          continue;
       }

       /* if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        } */

       if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
          close(sockfd);
          perror("server: bind");
          continue;
       }

       break;
   }

   if (p == NULL)  {
      fprintf(stderr, "server: failed to bind\n");
      return 2;
   }

   freeaddrinfo(servinfo); // all done with this structure

   if (listen(sockfd, BACKLOG) == -1) {
      perror("listen");
      exit(1);
   }

   sa.sa_handler = sigchld_handler; // reap all dead processes
   sigemptyset(&sa.sa_mask);
   sa.sa_flags = SA_RESTART;
   if (sigaction(SIGCHLD, &sa, NULL) == -1) {
      perror("sigaction");
      exit(1);
   }

   printf("server: waiting for connections...\n");

   while(1) {  // main accept() loop
      sin_size = sizeof their_addr;
      new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        
      if (new_fd == -1) {
         perror("accept");
         continue;
      }

      inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
      printf("server: got connection from %s\n", s);

      if (!fork()) { // this is the child process         
         char buffer[1000],buffer2[1000];
	 char againMsg[]="Enter another message: \t";
       	 do {
	    size_t nBytes;
	    close(sockfd); // child doesn't need the listener
	    nBytes=recv(new_fd,buffer,sizeof buffer,0);  //receive a message from the client
	    if(nBytes<0){ 
	       perror("Problem in receive"); 
	       exit(1); 
	    }
					
	    buffer[nBytes-2]='\0';  //remove the newline and null terminate
	    fprintf(stdout,"I got a message: %s\n",buffer);
		
	     char buffer3[100], cmd[50];
	     char *dummy;
		
	     dummy = strtok(buffer, " ");
	     while (dummy != NULL){
		sprintf(buffer3, "%s", dummy);
		fprintf(stdout,"%s\n", dummy);
		dummy = strtok(NULL, " ");
	    }
		
            if(strcasecmp(buffer, "Quit")==0) 
               break; //If the user types quit, close the connection
            
            if(strcasecmp(buffer, "TheTime")==0){ //Prints out the current time
               struct timeval tv;
               struct tm* ptm;
               char time_string[40];

               gettimeofday (&tv, NULL);
               ptm = localtime (&tv.tv_sec);
               strftime (time_string, sizeof(time_string), "%Y-%m-%d %H:%M:%S", ptm);
               //fprintf (stdout, "%s\n", time_string);
	       sprintf(buffer2, "The time is %s\n", time_string);
	       fprintf(stdout, "Sending this to client: %s\n", buffer2);
	       if (send(new_fd,buffer2,strlen(buffer2), 0) == -1)
	          perror("send");
            }
            
            if(strcasecmp(buffer, "Users")==0){ //Checks how many users are currently logged in
                char numberOfUsers[30];
		snprintf(cmd, sizeof(cmd), "who | wc -w");
                FILE *command = popen(cmd, "r");
                fgets (numberOfUsers, 100, command);
                
   		sprintf(buffer2, "The number of users logged in is %s", numberOfUsers);
            	fprintf(stdout, "Sending this to client: %s\n", buffer2);
            	if (send(new_fd,buffer2,strlen(buffer2), 0) == -1)
	           perror("send");
            }
            
            if(strcasecmp(buffer, "Logged")==0){ //Checks if a particular user is logged in
               snprintf(cmd, sizeof(cmd), "who | cut -d ' ' -f 1 | grep %s", buffer3);
               FILE *command = popen(cmd, "r");
               
               fgets (buffer2, 100, command);
               
               if (strcasecmp(buffer2, buffer3) ==0){
               	sprintf(buffer2, "%s is logged in\n", buffer3);
               	fprintf(stdout, "Sending this to client: %s\n", buffer2);
               	if (send(new_fd,buffer2,strlen(buffer2), 0) == -1)
	           perror("send");
               }else{
               	sprintf(buffer2, "%s is not logged in\n", buffer3);
               	fprintf(stdout, "Sending this to client: %s\n", buffer2);
               	if (send(new_fd,buffer2,strlen(buffer2), 0) == -1)
	           perror("send");
               }
            }
            
            if(strcasecmp(buffer, "Sessions")==0){ //Checks how many terminal sessions are connected
               snprintf(cmd, sizeof(cmd), "netstat | grep 400060 | wc -l");
	       FILE *command = popen(cmd, "r");
	       int numberOfSessions = atoi(buffer2);
	       
	       fgets(buffer2, 100, command);
//	       numberOfSessions = numberOfSessions/2;

	       sprintf(buffer2, "There are %d number of connections\n", numberOfSessions);
	       fprintf(stdout, "Sending this to client: %s\n", buffer2);
	       if (send(new_fd,buffer2,strlen(buffer2), 0) == -1)
	          perror("send");
            }
            
            if(strcasecmp(buffer, "Stat")==0){ //Lists files in current directory
               snprintf(cmd, sizeof(cmd), "ls --file-type | grep -v /");
	       FILE *command = popen(cmd, "r");
	         
               while (fgets(buffer2, 100, command) !=NULL){ //Prints out directory contents
		  sprintf(buffer3,"%s\n", buffer2);
		  fprintf(stdout, "Sending this to client: %s\n", buffer2);
		  if (send(new_fd,buffer2,strlen(buffer2), 0) == -1)
	             perror("send");
		  
	       }
            }
            
            if(strcasecmp(buffer, "chanDir")==0){ //Change directory and list contents of new directory
              if (chdir(buffer3) == 0){
	         sprintf(buffer2, "Changed directories!\n"); 
	         fprintf(stdout, "Sending this to client: %s\n", buffer2);
	         if (send(new_fd, buffer2, strlen(buffer2), 0) == -1)
	            perror("send");
	      
	      //And for proof of that
/*	      snprintf(cmd, sizeof(cmd), "ls --file-type | grep -v /");
	      FILE *command = popen(cmd, "r");
		 
	      while ((fgets, buffer2, 100, command) != NULL){
	        sprintf(buffer3, "%s\n", buffer2);
	        fprintf(stdout, "Sending this to client: %s\n", buffer2);
	        if (send(new_fd, buffer2, strlen(buffer2), 0) == -1)
	      	   perror("send");
*/	      }else{
	         sprintf(buffer2, "Directory change failed\n");
		 fprintf(stdout, "Sending this to client: %s\n", buffer2);
		 if (send(new_fd, buffer2, strlen(buffer2), 0) == -1)
		    perror("send");
	      }
	   }
            
	   //2 MORE COMMANDS FOR BONUS MARKS	
           if(strcasecmp(buffer, "remove") == 0){ //Remove a file 
	      snprintf(cmd, sizeof(cmd), "rm %s", buffer3);
	      FILE *command = popen(cmd, "r");

	      remove(buffer3);
	   }

	   if(strcasecmp(buffer, "create") == 0){ //Create a file
	      snprintf(cmd, sizeof(cmd), "touch %s", buffer3);
	      system (cmd);
	   }
					
	 send(new_fd,againMsg,strlen(againMsg),0);
      }while(true); 
      close(new_fd);
      exit(0);
   }
   close(new_fd);  // parent doesn't need this
  }

  return 0;
}
