/**********************************************************
Date:       May 6th, 2017
Project :   RSYNC Implementation in C

Programers: 
Ankur Peshin
Smit Pandit 
***********************************************************************/

#define __USE_XOPEN
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <openssl/md5.h>
#include <time.h>
#include<features.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/time.h>
#include <netinet/ip.h> 
#include <arpa/inet.h>
#include <sys/param.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include <pthread.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/stat.h>


#define maximumBuffer 1024
#define SERVER_THREAD 1
#define CLIENT_THREAD 2
#define CLIENT 3


int client(int );
int server(int );

int iterateAndCheckMD5(int , int );
char * extractMD5Hash(char *);
int md5Func(char *,FILE *);

void readAndSend(int, FILE * , int);
void createAndStore(int, FILE * , int);



int main(int argc, char *argv[]){
    
	if(argc!=3){
        printf("Invalid Arg Count\n");
        return EXIT_FAILURE;
    }

    int port=atoi(argv[2]);
    
	if(strcmp(argv[1],"client")==0)
        client(port);
    else if(strcmp(argv[1],"server")==0)
        server(port);
    else{
        printf("Wrong Option: %s\n",argv[1] );
        return EXIT_FAILURE;
    }       
    return EXIT_SUCCESS;
}


int client(int port){
   
    char buf_recv[maximumBuffer+1] = {0};
    char buf_send[maximumBuffer+1] = {0};
    
    
	int client_socket = socket( PF_INET, SOCK_STREAM, 0 );

      if ( client_socket < 0 )
      {
        perror( "socket Not Created" );
        exit( EXIT_FAILURE );
      }

      struct hostent * hp = gethostbyname( "127.0.0.1" );

      if ( hp == NULL )
      {
        fprintf( stderr, "gethostbyname() failed\n" );
        return EXIT_FAILURE;
      }

      struct sockaddr_in server_addr;
      server_addr.sin_family = AF_INET;
      memcpy( (void *)&server_addr.sin_addr, (void *)hp->h_addr, hp->h_length );
      
    
      server_addr.sin_port = htons(port);

      if ( connect( client_socket, (struct sockaddr *)&server_addr, sizeof( server_addr ) ) == -1 )
      {
        perror( "connect() failed" );
        return EXIT_FAILURE;
      }
    
    char * buffer=calloc(maximumBuffer+1,sizeof(char));
    memcpy(buffer,"contents\n",9);
	
    send(client_socket,buffer,strlen(buffer),0);
    char file_name[256]={0};
    char md5hash[33]={0};
    
	int bytes_read;
    int first=0;
    
    bytes_read = recv(client_socket, buf_recv, maximumBuffer, 0); // Receiving command from the server
    
    if(bytes_read == 6 && strncmp(buf_recv, "getall", 6) == 0){
        
        iterateAndCheckMD5(client_socket, CLIENT_THREAD); //if getall command received then call iterateAndCheckMD5 to send all files to server
    }
	
    else if(strncmp(buf_recv, "put", 3) == 0){             // put command received from the server
        for(;;){
            
            if(first!=0){
                bytes_read = recv(client_socket, buf_recv, maximumBuffer, 0);    //waiting for the next command from server after all files sent
                if(bytes_read<=0)
                    break;   
            }
			
            if(strncmp(buf_recv, "complete", 8) == 0){          // break after all files are synced
                break;
            }
            
            first=1;
            int fsize = 0;
            
			char tempFileSize[7] = {0};
            bzero(file_name,256);
            bzero(md5hash,33);
            
			strncpy(file_name, &buf_recv[46], bytes_read - 46);
            strncpy(md5hash, &buf_recv[4], 32);
            
			if(access(file_name, F_OK ) == -1){
                                                            //file doesn't exist so get the file from server
                
                bzero(tempFileSize, 7);
                strncpy(tempFileSize, &buf_recv[40], 6);
                
				fsize = atoi(tempFileSize);
                bzero(buf_send,maximumBuffer+1);
                
				sprintf(buf_send,"get %s",file_name);
                send(client_socket,buf_send,strlen(buf_send),0);
                
                FILE *fp = fopen(file_name, "wb");
                
                fflush(NULL);

                createAndStore(client_socket,fp,fsize);

            }

            else{                                           //the file exists so compare the md5hash
                
                char *hash_new = extractMD5Hash(file_name);
                if(strcmp(md5hash,hash_new)==0){
                    bzero(buf_send,maximumBuffer+1);            //if hash is equal skip the file
                    sprintf(buf_send,"skip");
                    send(client_socket,buf_send,strlen(buf_send),0);  

                }
                else{
                    struct stat fileStat;
                    char date[18] = {0};
					
                    bzero(tempFileSize, 7);
                    strncpy(tempFileSize, &buf_recv[40], 6);
					
                    fsize = atoi(tempFileSize);
                    char fname[256]= {0};

                    bzero(buf_recv,maximumBuffer);
                    int bytes_recvd=0;
                    bzero(buf_send,maximumBuffer+1);
					
                    sprintf(buf_send,"query");
                    send(client_socket,buf_send,strlen(buf_send),0);  //if hash is not equal send query
                    bytes_recvd = recv(client_socket,buf_recv,maximumBuffer,0);
					
                    if(strncmp(buf_recv,"query",5) == 0){
						                                                //response to query with filename and stat() information
                        bzero(fname,256);
                        strncpy(fname,&buf_recv[25], bytes_recvd-26);
                        stat(file_name,&fileStat);
						
                        strncpy(date,&buf_recv[6], bytes_recvd-23);
                        
                        struct tm tm;
                        time_t t;
                        int timeLapse = 0;
						
                        strptime(date, "%d-%m-%y %H:%M:%S", &tm);
                        t = mktime(&tm);
						
                        timeLapse = difftime( fileStat.st_mtime,t);         //comparing stat of client and server file
                        if(timeLapse>0){
                            bzero(buf_send,maximumBuffer+1);
                            FILE *fp = fopen(file_name, "rb");
							
                            if(fp==NULL)
                                printf("File %s not found\n",file_name);
							
                            fseek(fp,0,SEEK_END);
                            int fsize=ftell(fp);
                            fseek(fp,0,SEEK_SET);
							
                            bzero(md5hash,33);
                            strcpy(md5hash,extractMD5Hash(file_name));
                            sprintf(buf_send,"put %s    %6d%s",md5hash,fsize,file_name);
                            
                            send(client_socket,buf_send,strlen(buf_send),0);
                            bzero(buf_recv,maximumBuffer+1);
                            recv(client_socket, buf_recv, maximumBuffer, 0); 
                            readAndSend(client_socket,fp,fsize);
                           
                            fclose(fp);
                        }
                        else{
                            bzero(md5hash,33);
                            strcpy(md5hash,extractMD5Hash(file_name));
                            bzero(buf_send,maximumBuffer+1);
                            sprintf(buf_send, "get %s", fname);
                            FILE *fileptr=fopen(file_name,"wb");
                            
                            if(fileptr==NULL){
							printf("Error opening file : %s\n",file_name);
							}
                                
                            send(client_socket,buf_send,strlen(buf_send),0);
                            printf("[client] Detected different & newer file: %s\n",file_name);
                            printf("[client] Downloading %s: %s\n",file_name,md5hash);
                            createAndStore(client_socket,fileptr,fsize);
                        }
                    }
                   
                }
            }
            bzero(buf_recv,maximumBuffer+1);
        }   
        iterateAndCheckMD5(client_socket,CLIENT);

        char complete[10]={0};
        sprintf(complete,"complete");
        send(client_socket,complete,strlen(complete),0);     //send complete command after sync is done
        bzero(buf_recv,maximumBuffer+1);
    }
    return EXIT_SUCCESS;
}


int server(int port){
    
	char dirTemp[] = "/tmp/tempdir.XXXXXX";         //creating temporary directory
    char *temp_directory = mkdtemp (dirTemp);

    if(temp_directory == NULL)
    {
        perror ("Failed to Create Temp Directory\n");
        exit (EXIT_FAILURE);
    }
    
	if (chdir (temp_directory) == -1)
    {
        perror ("Directory Error\n");
        exit (EXIT_FAILURE);
    }
	
	FILE *fp = NULL;
    char msgBuf[maximumBuffer+1];
    char file_name[256];
    int bindSocket, connSocket;
    socklen_t socklen;

    struct sockaddr_in serverAddress;
	struct sockaddr_in clientAddress;
	
    bindSocket = socket(AF_INET, SOCK_STREAM, 0);
    if(bindSocket < 0){
        perror( "Socket not created" );
        exit( EXIT_FAILURE );
    }

    bzero((char *) &serverAddress, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(port);

    if (bind(bindSocket, (struct sockaddr *) &serverAddress, sizeof(serverAddress)) < 0){
				 perror("Binding Error");
				 exit( EXIT_FAILURE );
			 }
        
    listen(bindSocket,5);
	
    socklen = sizeof(clientAddress);
    char *send_Buf = calloc(maximumBuffer+1, sizeof(char));

    while (1) {
        
        connSocket = accept(bindSocket,(struct sockaddr *) &clientAddress, &socklen);
        
        //Clear Buffer
        bzero(msgBuf,maximumBuffer+1);
        
		//Receive a message from client
		int msg_size = recv(connSocket, msgBuf, maximumBuffer, 0); //Waiting for contents command from the client
        
        if(msg_size == 9 && strncmp(msgBuf, "contents",8) == 0){
            if( access( ".4220_file_list.txt", F_OK ) == -1 ) {    // if .4220 file doesn't exist then send get all command to the client
				
                bzero(send_Buf,maximumBuffer+1);
                memcpy(send_Buf, "getall\0", strlen("getall\0"));
                send(connSocket, send_Buf, strlen(send_Buf), 0);
                int msg_size = 0;
                
				while (1) {
                    bzero(msgBuf, maximumBuffer+1);
                    msg_size = recv(connSocket, msgBuf, maximumBuffer, 0); // wait for response from the client
                    if(msg_size <=0){
                        break;
                    }
                    msgBuf[msg_size] = '\0';
                   
                    if(strncmp(msgBuf, "put", 3) == 0) {
						
                        bzero(send_Buf, maximumBuffer+1);
                        bzero(file_name, 256);
                        char num[7]={0};
						
                        strncpy(num,&msgBuf[40],6);
                        strncpy(file_name, &msgBuf[46], msg_size - 46); // sending get command to get the file sent by client with put command
                        sprintf(send_Buf, "get %s", file_name);
                       
                        int fsize=atoi(num);
                        
                        fp = fopen(file_name, "wb");

                        createAndStore(connSocket,fp,fsize);  //creating the file
                    }
                }

            }

            else{
                iterateAndCheckMD5(connSocket, SERVER_THREAD);  //to identify modifications to the server files while the server is running
				
                FILE *fp=fopen(".4220_file_list.txt","r");
                char * line = NULL;
                size_t len = 0;
				
				char file_name[256] = {0};
                char md5hash[33]={0};
				
                ssize_t read;
                if (fp == NULL)
                    exit(EXIT_FAILURE);
                //int read=0;
                while ((read = getline(&line, &len, fp)) != -1) {
                    
                    bzero(msgBuf, maximumBuffer + 1);
                    bzero(file_name, 256);
					
                    strncpy(file_name, &line[36], read - 36-1);
                    strncpy(md5hash, line, 32);
                    FILE *fp = fopen(file_name,"rb");

                    fseek(fp,0,SEEK_END);
                    int fsize = ftell(fp);
                    fseek(fp,0,SEEK_SET);
 
                    sprintf(msgBuf,"put %s    %6d%s",md5hash,fsize,file_name);
                   
                    send(connSocket,msgBuf,strlen(msgBuf),0);               //sending contents of .4220 file
					
                    recv(connSocket, msgBuf, maximumBuffer, 0);             //waiting for response
                    
					if(strncmp(msgBuf,"get", 3) == 0){

                        readAndSend(connSocket,fp, fsize);              //sending the file is get command received
                        fclose(fp);
                    }
					
                    else if(strncmp(msgBuf,"skip", 4) == 0){
                        fclose(fp);
                        continue;
                    }
                    else {
                      
                        struct stat fileStat;
                    
                        bzero(send_Buf,maximumBuffer+1);
                        bzero(msgBuf,maximumBuffer+1);
                        
						stat(file_name,&fileStat);
                        char date[18];
                        
						strftime(date, 20, "%d-%m-%y %H:%M:%S", localtime(&(fileStat.st_mtime)));
                        sprintf(send_Buf,"query %s   %s",date,file_name);
                        
                        send(connSocket,send_Buf,strlen(send_Buf),0);//sending query with stat information in response to  query command from client
                        recv(connSocket, msgBuf, maximumBuffer, 0);
						
                        if(strncmp(msgBuf,"get",3) == 0){
                            readAndSend(connSocket,fp,fsize);  //send file if get command received in response to query
                            fclose(fp);
                        }
						
                        else if(strncmp(msgBuf,"put",3) == 0){
							
                            fclose(fp);                             //create file if put command received in response to query
                            FILE *f = fopen(file_name, "wb");
                            
							fseek(f,0,SEEK_END);
                            int fsize=ftell(f);
                            
							fseek(f,0,SEEK_SET);
                            
							char tempFileSize[7];
                            bzero(tempFileSize, 7);
                            
							strncpy(tempFileSize, &msgBuf[40], 6);
                            fsize = atoi(tempFileSize);
                            
                            bzero(send_Buf,maximumBuffer+1);
                            sprintf(send_Buf, "get %s", file_name);
							
                            send(connSocket,send_Buf,strlen(send_Buf),0);
                            printf("[server] Detected different & newer file: %s\n",file_name);
                            printf("[server] Downloading %s: %s\n",file_name,md5hash);
                            createAndStore(connSocket,f,fsize);               
                        } 
                    }
                }

                if (line)
                    free(line);
                char complete[10]={0};
                sprintf(complete,"complete");           //send complete once sync completed
                send(connSocket,complete,strlen(complete),0);
				
				int bytes_read=0;
                int first=0;
                
                bzero(msgBuf,maximumBuffer+1);
                bytes_read=recv(connSocket,msgBuf,maximumBuffer,0);
                
                if(strncmp(msgBuf, "put", 3) == 0){

                    for(;;){
						
                        if(first!=0){
                            bytes_read = recv(connSocket, msgBuf, maximumBuffer, 0);    
                            if(bytes_read<=0)
                                break;   
                        }
						
                        if(strncmp(msgBuf, "complete", 8) == 0){
                            break;
                        }
						
                        first=1;
                        int fsize = 0;
                        char tempFileSize[7] = {0};
						
                        bzero(file_name,256);
                        bzero(md5hash,33);
                        
                        strncpy(file_name, &msgBuf[46], bytes_read - 46);
                        strncpy(md5hash, &msgBuf[4], 32);
                       
                        if(access(file_name, F_OK ) == -1){

                            bzero(tempFileSize, 7);
                            strncpy(tempFileSize, &msgBuf[40], 6);

                            fsize = atoi(tempFileSize);
                            bzero(send_Buf,maximumBuffer+1);

                            sprintf(send_Buf,"get %s",file_name);
                            send(connSocket,send_Buf,strlen(send_Buf),0);

                            FILE *fp = fopen(file_name, "wb");
                            fflush(NULL);

                            createAndStore(connSocket,fp,fsize);
                        }

                        else{
                           
                            char *hash_new = extractMD5Hash(file_name); //calculate and store hash
							
                            if(strcmp(md5hash,hash_new)==0){
                                bzero(send_Buf,maximumBuffer+1);
                                sprintf(send_Buf,"skip");
                                send(connSocket,send_Buf,strlen(send_Buf),0);   
                            }
                        }
                        bzero(msgBuf,maximumBuffer+1);
                    }   
                }
            }
        }
        iterateAndCheckMD5(connSocket, SERVER_THREAD);
    }
    
    return EXIT_SUCCESS;
}


int iterateAndCheckMD5(int sock,int thread) //communicating md5 hash along with filename to the other party and storing md5 hash in .4220 file for server
{
    DIR *d;
    struct dirent *dir;
	
    char * file_name=".4220_file_list.txt";
    int nsize = strlen(file_name);
	
    FILE *server_list_file; 
   
    if(thread == SERVER_THREAD){
        server_list_file = fopen(file_name, "w");
    }
    
    char * buffer=calloc(maximumBuffer+1,sizeof(char));
    char * buf_my=calloc(maximumBuffer+1,sizeof(char));
	
    d = opendir(".");
	
    if (d)
    {
        while ((dir = readdir(d)) != NULL)
        {
           
            if(strncmp(dir->d_name, file_name,nsize)==0){
                continue;
            }
            if(thread==SERVER_THREAD && dir->d_type == DT_REG ){
                
                md5Func(dir->d_name,server_list_file);
            }
            else if((thread==CLIENT_THREAD || thread==CLIENT)&& dir->d_type == DT_REG ){
				
                char *md5hash=extractMD5Hash(dir->d_name);
				
                if(md5hash==NULL){
                    printf("Error getting MD5 hash for %s\n",dir->d_name );
                    continue;
                }
                bzero(buffer,maximumBuffer+1);
                
                FILE *fp=fopen(dir->d_name,"rb");
                fseek(fp,0,SEEK_END);
                int fsize=ftell(fp);
                fseek(fp,0,SEEK_SET);
				
                if(fsize==0){
                    fclose(fp);
                    continue;
                }
                sprintf( buffer, "put %s    %6d%s", md5hash,fsize ,dir->d_name );
               
                int size=strlen(buffer);
                buffer[size]='\0';
				
                bzero(buf_my,maximumBuffer+1);
                memcpy(buf_my,buffer,size);
				
                send(sock,buf_my,strlen(buf_my),0);
                bzero(buf_my,maximumBuffer+1);
				
                if(thread==CLIENT_THREAD){
					 readAndSend(sock, fp, fsize);
				}
                else{
                    char recvbuff[maximumBuffer+1]={0};
                    bzero(recvbuff,0);
                    recv(sock,recvbuff,maximumBuffer,0);
					
                    if(strncmp(recvbuff,"get",3)==0){
                        readAndSend(sock, fp, fsize);
                        fclose(fp);
                    }
					
                    else if(strncmp(recvbuff,"skip", 4) == 0){
                        continue;
                    }
                }
            }
        }
        closedir(d);
		
        if(thread==SERVER_THREAD)
            fclose(server_list_file);
        
    }
    
    return(0);
}

int md5Func(char *file_name,FILE *server_list_file){ //function to calculate the md5 hash for server

    unsigned char c[MD5_DIGEST_LENGTH];
    MD5_CTX mdContext;
    
	char m[MD5_DIGEST_LENGTH+1]={0};
    int bytes;
    unsigned char data[1024];
	
	int i;

    FILE *file = fopen (file_name, "rb");
    
    if (file == NULL) {
        printf ("%s can't be opened.\n", file_name);
        return 0;
    }

    MD5_Init (&mdContext);
    while ((bytes = fread (data, 1, 1024, file)) != 0)
        MD5_Update (&mdContext, data, bytes);
    MD5_Final (c,&mdContext);

    for(i = 0; i < MD5_DIGEST_LENGTH; i++) {

        fprintf(server_list_file, "%02x", c[i]);
        sprintf(&m[i*2], "%02x", (unsigned int)c[i]);
    }
	
    fprintf(server_list_file, "    %s\n", file_name);

    fclose (file);
    //fclose(server_list_file);
    return 0;
}

char * extractMD5Hash(char * file_name){

    unsigned char c[MD5_DIGEST_LENGTH];

    int i;
    FILE *file = fopen (file_name, "rb");

    MD5_CTX mdContext;
    int bytes;
    unsigned char data[1024];

    if (file == NULL) {
        printf ("%s can't be opened.\n", file_name);
        return NULL;
    }
	
    MD5_Init (&mdContext);
    while ((bytes = fread (data, 1, 1024, file)) != 0)
        MD5_Update (&mdContext, data, bytes);
    MD5_Final (c,&mdContext);

    char* buf_str = (char*) malloc (2*MD5_DIGEST_LENGTH + 1);
    char* buf_ptr = buf_str;

    for(i=0; i<MD5_DIGEST_LENGTH;i++){
        buf_ptr += sprintf(buf_ptr, "%02x", c[i]);
    }
    *(buf_ptr + 1) = '\0';

    return buf_str;
    
}


void readAndSend(int socket, FILE *fp  , int fsize){ //sending file to other party
	
	int sent=0;
    int bread = 0;
    char *buf_my=calloc(maximumBuffer+1,sizeof(char));
    char *ack_buf=calloc(15,sizeof(char));
   
    if(fsize>maximumBuffer){
       
        while(sent<fsize){

            bzero(buf_my,maximumBuffer+1);
            if(fsize-sent>=1024)
                bread = fread(buf_my,1,maximumBuffer,fp);
            else
                bread = fread(buf_my,1,fsize-sent,fp);
           
            sent += send(socket,buf_my,bread,0);
          
            bzero(ack_buf,15);
            recv(socket,ack_buf,15,0);
           
        }
       
    }
    else{
        int nread = 0;
        bzero(buf_my,maximumBuffer+1);
        nread = fread(buf_my,1,fsize,fp);
        
        send(socket,buf_my,nread,0);
        bzero(ack_buf,15);
        recv(socket,ack_buf,15,0);
		
    }
}

void createAndStore(int socket, FILE *fp, int fsize){ //receiving file from other party and storing in directory
    int bytes = 0;
	int received=0;
    int byte_count = 0;
	
    char *buf_recv = calloc(maximumBuffer+1,sizeof(char));
    char *ack_buf=calloc(15,sizeof(char));
    
    if(fsize>maximumBuffer){
        
        char *ack_buf=calloc(15,sizeof(char));

        while(received<fsize){
            bzero(buf_recv,maximumBuffer+1);
            if(fsize-received>=1024){
                byte_count=recv(socket, buf_recv, maximumBuffer, 0);
                received += byte_count;
                fwrite(buf_recv, 1, byte_count, fp);

            }
            else{
                byte_count= recv(socket, buf_recv, fsize-received, 0);
                received += byte_count;
                fwrite(buf_recv, 1, byte_count, fp);
            }
            bzero(ack_buf,15);
            sprintf(ack_buf,"ack %d",byte_count);
            send(socket,ack_buf,strlen(ack_buf),0);
        
        }
    }
    else{
		
        bytes = recv(socket, buf_recv, fsize, 0);
        fwrite(buf_recv, 1, bytes, fp);
        bzero(ack_buf,15);
		
        sprintf(ack_buf,"ack %d",bytes);
        send(socket,ack_buf,strlen(ack_buf),0);
    }
    fclose(fp);
    fp=NULL;

}