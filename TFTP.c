
/**********************************************************
Date:       March 26th, 2017
Project :   TFTP Server Implementation in C

Programers: 
Ankur Peshin
Smit Pandit 
***********************************************************************/


#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>





void doFileIO(int socketDescriptor, struct sockaddr_in socketAddress, char fileName[]);
void doWriteIO(int socketDescriptor, struct sockaddr_in socketAddress, char fileName[]);
static void read_sig_alrm(int);
static void write_sig_alrm(int);

int globalExitCounter=0;
int packetCount =0;

int sockfd;
int childSockfd;
struct sockaddr_in mySocketAddress,remoteSocketAddress, myChildSocketAddress;

unsigned char fileBuf[1024];
unsigned char recvbuf[1024];
unsigned char sendbuf[1024];
unsigned char ackBuf[4];

int readLen=0;

int main(int argc, char *argv[]){


    int sockOpt_val = 1;
    char client_buf[1024], opcode;
    char local_buf[1024];

    memset(client_buf, 0, sizeof client_buf);
    memset(local_buf, 0, sizeof client_buf);

    memset(fileBuf, 0, sizeof fileBuf);
    memset(recvbuf, 0, sizeof recvbuf);
    memset(sendbuf, 0, sizeof sendbuf);
    memset(ackBuf, 0, sizeof ackBuf);



    socklen_t socklen;

    ssize_t bytes_client; //Bytes of data received from client

    socklen_t addr_len; //Size of Basic Socket Address Struct

    /*Creating UDP Socket*/

    if((sockfd=socket(AF_INET,SOCK_DGRAM,0))<0){
        perror("Error in Creating UDP Socket");
        exit(1);
    }

    // if (setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&sockOpt_val,sizeof(int)) == -1) {
    //     perror("Socket can't be re-used upon kill.Also,I did not implement SIGKILL. Yet\n");
    // }

    socklen = sizeof(mySocketAddress);
    addr_len = sizeof(struct sockaddr);

    /*Binding UDP Socket to a randomly assigned port*/

    memset(&mySocketAddress,0, sizeof mySocketAddress);
    memset(&myChildSocketAddress,0, sizeof myChildSocketAddress);

    mySocketAddress.sin_addr.s_addr = INADDR_ANY;
    mySocketAddress.sin_family = AF_INET;
    mySocketAddress.sin_port = htons(0);

    myChildSocketAddress.sin_addr.s_addr = INADDR_ANY;
    myChildSocketAddress.sin_family = AF_INET;
    myChildSocketAddress.sin_port = htons(0);

    if(bind(sockfd,(struct sockaddr *)&mySocketAddress,socklen) == -1){
        perror("Bind Unsuccessful\n");
        exit(1);
    }

    if (getsockname(sockfd, (struct sockaddr *)&mySocketAddress, &socklen) == -1){
        perror("Retrieve Port Number Operation Failure");
    }
    else{
        printf("Server connect on Port Number: %d\n", ntohs(mySocketAddress.sin_port));
    }

    /*Listen not needed for UDP Socket*/

    while(1){
        bytes_client = recvfrom(sockfd, client_buf, 1024,0, (struct sockaddr *)&remoteSocketAddress, &addr_len);

        printf("Received Connection From : %s\n ", inet_ntoa(remoteSocketAddress.sin_addr));
        

        client_buf[strlen(client_buf)] = '\0';          //add terminating char at the end of the buffer
        //printf("Opcode is: %d%d\n",client_buf[0],client_buf[1]); 

        if(bytes_client < 0){
            perror("ERROR recvfrom");
        }

        opcode=client_buf[1];
        char filename[20]={0};
        strcpy(filename,&client_buf[2]);
        int modestart=strlen(filename)+2+1;
        char mode[20]={0};
        strcpy(mode,&client_buf[modestart]);
        //printf("Here is the messgae: %d %s %s\n",opcode,filename, mode);    
        fflush(stdout);

        /*Checking whether the incoming command is read or write*/

        if(opcode == 1 || opcode == 2){

        /*Creating child process and also creating a separate socket for the child process*/

            if(!fork()){
                close(sockfd);
                sleep(2);
                if((childSockfd=socket(AF_INET,SOCK_DGRAM,0))<0){
                    perror("Error in Creating Child UDP Socket\n");
                    exit(1);
                }
                else{
                    printf("Child Socket Created\n");
                }
                if (setsockopt(childSockfd,SOL_SOCKET,SO_REUSEADDR,&sockOpt_val,sizeof(int)) == -1) {
                    perror("Error in Setting Child Socket options\n");
                }
                if(bind(childSockfd,(struct sockaddr *)&myChildSocketAddress,socklen) == -1){
                    perror("Bind Unsuccessful for child socket\n");
                    exit(1);
                } else{
                    printf("Child Socket Bound\n");
                }

        /* Calling read or write in the child process based on the opcode received*/

                if(opcode == 1){
                    doFileIO(childSockfd, remoteSocketAddress, filename);
                }
                if(opcode == 2){
                    doWriteIO(childSockfd, remoteSocketAddress, filename);
                }

        /*Closing the child socket after read or write operation is done and then killing the child process*/
                
                close(childSockfd);
                if(globalExitCounter >= 10){
                    exit(1);
                }
                exit(0);


            }

        }

    }

}


void doWriteIO(int socketDescriptor, struct sockaddr_in socketAddress, char fileName[]){


    sprintf((char *) ackBuf, "%c%c%c%c", 0x00, 0x04, 0x00, 0x00);

    if (sendto(socketDescriptor, ackBuf, 4, 0, (struct sockaddr *) &socketAddress, sizeof(socketAddress)) != 4 )
        puts("SENDING FAILED!");
    //printf("ACK : %d %d %d %d\n", ackBuf[0], ackBuf[1], ackBuf[2], ackBuf[3]);



    FILE *fp,*fileExistPointer;

    memset(fileBuf,0,1024);
    socklen_t recv_size;

    unsigned int turn_on =1;
    unsigned int turn_off =0;

    unsigned char leadingBlock =0;
    unsigned char trailingBlock =0;

    //To check if the client asks to create a file which already exists

    fileExistPointer = fopen(fileName, "r");
    if(fileExistPointer !=NULL){
        char errorPacket[30] = {"File Already exists"}; 
        int len=strlen(errorPacket);
        errorPacket[len] = '\0';
        sprintf((char *) fileBuf, "%c%c%c%c", 0x00, 0x05, 0x00, 0x06);
        memcpy((char *) fileBuf + 4, errorPacket, len);
        if (sendto(socketDescriptor, fileBuf, 4+len, 0, (struct sockaddr *) &socketAddress, sizeof(socketAddress)) != len )
            puts("SENDING FAILED!");
        return;
    }

    /*Creating new file to write the incoming data into*/ 

    fp = fopen(fileName, "wb");
    signal(SIGALRM,write_sig_alrm);
    while(1)
    {
       
        alarm(turn_on); //Using alarm to ensure that data is received in 1 second otherwise retransmit the ack packet

        int recv=recvfrom(socketDescriptor, recvbuf, 1024, 0,
                          (struct sockaddr *) &socketAddress, &recv_size);
        //printf("Recv : %d %d %d %d\n", recvbuf[0], recvbuf[1], recvbuf[2], recvbuf[3]);

        if(recv <0){
            if(errno == EINTR){
                fprintf(stderr,"Socket Timeout:\n");
            }
            else{
                printf("Recvfrom error\n");
            }
        }
        else{
            alarm(turn_off);        //turning off the alarm and resetting the timeout counter
            globalExitCounter =0;
        }

        /*Check if the received packet is a correct DATA packet then copy the data into the file and send an ACK packet for the said DATA packet*/

        if (recvbuf[1] == 3)
        {
            
            if((leadingBlock < recvbuf[2]) || (leadingBlock == recvbuf[2] && trailingBlock < recvbuf[3])){ //Check to avoid Sorcerer's Apperentice Syndrome
                
                leadingBlock = recvbuf[2];
                trailingBlock = recvbuf[3];
                memcpy(fileBuf, recvbuf + 4, 512);
                //fprintf(fp, "%s" , fileBuf);
                fwrite(fileBuf,1,recv-4,fp);    //Writing data to the file
                memset(fileBuf,0,1024);
                sprintf((char *) ackBuf, "%c%c%c%c", 0x00, 0x04, 0x00, 0x00);
                ackBuf[2] = recvbuf[2];
                ackBuf[3] = recvbuf[3];
                if (sendto(socketDescriptor, ackBuf, 4, 0, (struct sockaddr *) &socketAddress, sizeof(socketAddress)) != 4 )
                    puts("SENDING FAILED!");

                if(recv <516){
                    break;                  //Condition to identify the last packet and thus terminate connection
                }

            }
            memset(recvbuf,0,1024);
        }

    }

}

void doFileIO(int socketDescriptor, struct sockaddr_in socketAddress, char fileName[]){

    FILE *fp;

    socklen_t recv_size;
    int count = 1;
    unsigned int turn_on =1;
    unsigned int turn_off =0;
    int ssize=0;
    char buffer[512];
    unsigned char leadingBlock=0;
    unsigned char trailingBlock = 0;

    /*Opening file for read*/
    
    fp = fopen(fileName, "r");

    if(fp) {
        while(!feof(fp)){

            /*Sending DATA packets in response to correct ACK packets*/

            signal(SIGALRM,read_sig_alrm);

            if((leadingBlock==recvbuf[2] && trailingBlock==recvbuf[3]) || count ==1){ //Check to avoid Sorcerer's Apperentice Syndrome
                ssize = fread(buffer, 1, 512, fp);
                buffer[strlen(buffer)]= '\0';
                sprintf((char *) sendbuf, "%c%c%c%c", 0x00, 0x03, 0x00, 0x00);
                memcpy((char *) sendbuf + 4, buffer, ssize);
                sendbuf[2] = ((count & 0xFF00) >> 8);
                sendbuf[3] = ((count & 0x00FF));
                //printf("%d : %d : %d : %d : Count : %d\n", sendbuf[0], sendbuf[1],sendbuf[2],sendbuf[3],count);
                readLen= 4 + ssize;
                count++;
                if (sendto(socketDescriptor, sendbuf, readLen, 0, (struct sockaddr *) &socketAddress, sizeof(socketAddress)) != readLen )
                    puts("SENDING FAILED!");
            }



            memset(recvbuf, 0, 1024);


            alarm(turn_on);
            ssize_t recv =recvfrom(socketDescriptor, recvbuf, 1024, 0,
                                   (struct sockaddr *) &socketAddress, &recv_size);
            //printf("Recv %d\n", (int)recv);

            if(recv <0 || recvbuf[1] !=4){
                
                if(errno == EINTR){
                    fprintf(stderr,"Socket Timeout:\n");
                }
                else{
                    printf("Recvfrom error\n");
                }
            }
            else{
                alarm(turn_off);        //turning off the alarm and resetting the timeout counter
                globalExitCounter =0;
            }

            // if (recvbuf[1] == 4){
            //     // puts("Acked");
            // }

            leadingBlock =sendbuf[2];
            trailingBlock = sendbuf[3];

            //printf("%s\n", buffer);
            if(ssize < 512){
                break;                  //Condition to identify the last packet and thus terminate connection
            }
        }
        fclose(fp);

    } else {//If client asks for the file which does not exist

        memset(sendbuf,0,1024);
        char errorPacket[30] = {"File not found."}; 
        int len=strlen(errorPacket);
        errorPacket[len] = '\0';
        sprintf((char *) sendbuf, "%c%c%c%c", 0x00, 0x05, 0x00, 0x01);
        memcpy((char *) sendbuf + 4, errorPacket, len);
        if (sendto(socketDescriptor, sendbuf, 4+len, 0, (struct sockaddr *) &socketAddress, sizeof(socketAddress)) != len )
            puts("SENDING FAILED!");
    }
    


}


static void read_sig_alrm(int signo){
    
    /*Resend same packet if ACK not received till timeout*/

    if (sendto(childSockfd, sendbuf, readLen, 0, (struct sockaddr *) &remoteSocketAddress, sizeof(remoteSocketAddress)) != readLen ){
         puts("SENDING FAILED!");
    }
    // else{
    //     printf("Reminder Packet No %d sent\n",globalExitCounter);
    // }
       
    globalExitCounter++; //timeout check counter incremented
    
    /*Check for timeout condition*/

    if(globalExitCounter >= 10){ 
        close(childSockfd);
        exit(1);
    }

    alarm(1);
    return;
}


static void write_sig_alrm(int signo){
   //printf("%d Second Timeout Alarm\n",globalExitCounter);
    
    /*Resend same packet if ACK not received till timeout*/

    if (sendto(childSockfd, ackBuf, 4, 0, (struct sockaddr *) &remoteSocketAddress, sizeof(remoteSocketAddress)) != 4 ){
        puts("SENDING FAILED!");
    }
    // else{
    //     printf("Reminder Ack No %d sent\n",globalExitCounter);
    // }

    globalExitCounter++;   //timeout check counter incremented

    /*Check for timeout condition*/

    if(globalExitCounter >= 10){
        printf("Timed Out\n");
        close(childSockfd);
        exit(1);
    }

    alarm(1);
    return;
}
