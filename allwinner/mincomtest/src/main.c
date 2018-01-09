#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <pthread.h>


int fd;
int nread,i,nsend;
char writedata[]="welcome to R16!\n";
char uart1testdata[] = "uart port 1 test data";
char uart2testdata[] = "uart port 2 test data";
int b[1]={0x11};  
char buff[]="Hello\n";
char buffer[1024];
const int READ_THREAD_ID = 0;
const int SEND_THREAD_ID = 1;
pthread_t thread[2];

int set_opt(int fd,int nSpeed, int nBits, char nEvent, int nStop)
{
    struct termios newtio,oldtio;

    if ( tcgetattr( fd,&oldtio) != 0) { 
        perror("SetupSerial 1");

        return -1;
    }

    bzero( &newtio, sizeof( newtio ) );
    newtio.c_cflag |= CLOCAL | CREAD; 
    newtio.c_cflag &= ~CSIZE; 


    switch( nBits ){
        case 7:
            newtio.c_cflag |= CS7;
        break;
        case 8:
            newtio.c_cflag |= CS8;
        break;
    }

    switch( nEvent ){
        case 'O':
            newtio.c_cflag |= PARENB;
            newtio.c_cflag |= PARODD;
            newtio.c_iflag |= (INPCK | ISTRIP);
        break;
        case 'E': 
            newtio.c_iflag |= (INPCK | ISTRIP);
            newtio.c_cflag |= PARENB;
            newtio.c_cflag &= ~PARODD;
        break;
	case 'N': 
            newtio.c_cflag &= ~PARENB;
            break;
	}


    switch( nSpeed ){
        case 2400:
            cfsetispeed(&newtio, B2400);
            cfsetospeed(&newtio, B2400);
        break;
		case 4800:
			cfsetispeed(&newtio, B4800);
			cfsetospeed(&newtio, B4800);
			break;
		case 9600:
			cfsetispeed(&newtio, B9600);
			cfsetospeed(&newtio, B9600);
			break;
		case 115200:
			cfsetispeed(&newtio, B115200);
			cfsetospeed(&newtio, B115200);
			break;
		default:
			cfsetispeed(&newtio, B9600);
			cfsetospeed(&newtio, B9600);
			break;
	}

	if( nStop == 1 )
	{
		newtio.c_cflag &= ~CSTOPB;
	}
	else if ( nStop == 2 )
	{
		newtio.c_cflag |= CSTOPB;
	}

	newtio.c_cc[VTIME] = 0;
	newtio.c_cc[VMIN] = 0;
	tcflush(fd,TCIFLUSH);

	if((tcsetattr(fd,TCSANOW,&newtio))!=0)
	{
		perror("com set error");

		return -1;
	}

	//printf("set done!\n");

	return 0;
}




int open_port(int fd,int comport)
{
    char *dev[]={"/dev/ttyS0","/dev/ttyS1","/dev/ttyS2","/dev/ttyS3"};
    long vdisable;

    if (comport==1) {
        fd = open( "/dev/ttyS0", O_RDWR|O_NOCTTY|O_NDELAY);
        if (-1 == fd){
            perror("Can't Open Serial Port");
            return(-1);
        } else {
            printf("open ttyS0 .....\n");
        }
    } else if (comport==2) {
        fd = open( "/dev/ttyS1", O_RDWR|O_NOCTTY|O_NDELAY);
        if (-1 == fd) {
            perror("Can't Open Serial Port");
            return(-1);
        } else {
            printf("open ttyS1 .....\n");
        }
    } else if (comport==3) {
        fd = open( "/dev/ttyS2", O_RDWR|O_NOCTTY|O_NDELAY);
        if (-1 == fd) {
            perror("Can't Open Serial Port");
            return(-1);
        } else {
            printf("open ttyS2 .....\n");
        }
    } else if (comport==4) {
        fd = open( "/dev/ttyS3", O_RDWR|O_NOCTTY|O_NDELAY);

        if (-1 == fd){
            perror("Can't Open Serial Port");
            return(-1);
        } else {
            printf("open ttyS3 .....\n");
	}
    }


    if(fcntl(fd, F_SETFL, 0)<0) {
        printf("fcntl failed!\n");
    } else {
        //printf("fcntl=%d\n",fcntl(fd, F_SETFL,0));
    }

    if(isatty(STDIN_FILENO)==0)	{
        printf("standard input is not a terminal device\n");
    } else {
        //printf("isatty success!\n");
    }

    //printf("fd-open=%d\n",fd);

    return fd;
}




void* com_read(void * p)
{
    while(1){
        nread = read(fd,buffer,8);

        if(nread>0){
            printf("\nrecv:%d\n",nread);
            printf("%s",buffer);
        }
    }
}


void* uart1_test(void * p)
{
    char* expectdata = (char *)p;
    //printf("uart1_test...");
    sleep(1);
    nread = read(fd,buffer,strlen(expectdata));
    //printf("read return %d bytes\n", nread);

    if(nread>0){
        //printf("recvcontent: %s\n",buffer);
	if(strncmp(buffer, expectdata, strlen(expectdata)) == 0){
            printf("uart 1 test success\n");
	}else{
	    printf("data corrupt, uart 1 test fail\n");
	}
    }else{
        printf("no data, uart 1 test fail\n"); 
    }
    
}

void* uart2_test(void * p)
{
    char* expectdata = (char *)p;
    //printf("uart2_test...");
    sleep(1);
    nread = read(fd,buffer,strlen(expectdata)+1);
    //printf("read return %d bytes\n", nread);

    if(nread>0){
        //printf("recvcontent: %s\n",buffer);
	if(0 == strncmp(buffer, expectdata, strlen(expectdata))){
            printf("uart 2 test success\n");
	}else{
	    printf("data corrupt, uart 2 test fail\n");
	}
    }else{
        printf("no data, uart 2 test fail\n"); 
    }
    
}


void* com_send(void * p)
{
    while(1){
        //printf("send data");
        nsend=write(fd,writedata,strlen(writedata));

        //printf("  nsend:%d  \n",nread);
	
        if(nsend<0) {
	    //printf("send  error\n");
        } else {
            printf("Send  OK\n"); 
        }
	
        sleep(2);
    }
}

void* com_send_once(void * p)
{
        //printf("send data once \n");
	char * uartdata = (char*)p;
        nsend=write(fd,uartdata,strlen(uartdata));

        //printf("  nsend:%d  \n",nsend);
	
        if(nsend<0) {
	    printf("send  error\n");
        } else {
            printf("Send  OK\n"); 
        } 
}	

int main(void)
{
    printf("test start...\n");
    if((fd=open_port(fd,2))<0){
        perror("open_port error");
	
        return 1;
    }
		
    if((i=set_opt(fd,115200,8,'N',1))<0){
        perror("set_opt error");
        return 1;
    }
		
    //printf("fd=%d\n",fd);
		
    if(pthread_create( &thread[READ_THREAD_ID], NULL, uart1_test, (void *)uart1testdata)== 0){
        //printf(" create read thread success!\n");
    } else {
        printf(" create read thread fail!");
    }


    if(pthread_create( &thread[SEND_THREAD_ID], NULL, com_send_once, (void *)uart1testdata)==0){
        //printf(" create send thread success!\n");
    } else {
        printf(" create send thread fail!");
    }
		
    sleep(2);
	
    close(fd);


    if((fd=open_port(fd,3))<0){
        perror("open_port error");	
        return 1;
    }
		
    if((i=set_opt(fd,115200,8,'N',1))<0){
        perror("set_opt error");
        return 1;
    }
		
    //printf("fd=%d\n",fd);
		
    if(pthread_create( &thread[READ_THREAD_ID], NULL, uart2_test,(void *)uart2testdata)== 0){
        //printf(" create read thread success!\n");
    } else {
        printf(" create read thread fail!");
    }


    if(pthread_create( &thread[SEND_THREAD_ID], NULL, com_send_once,(void *)uart2testdata)==0){
        //printf(" create send thread success!\n");
    } else {
        printf(" create send thread fail!");
    }
		

    sleep(2);

    close(fd);


    printf("test end");

    return 1;
}
