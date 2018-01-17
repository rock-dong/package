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
#include <sys/time.h>

#define TEST_DATA_LEN 98

int fd;
int nread,i,nsend;
long totalReadNum = 0;
long errIndex = 0;
char writedata[]="welcome to R16!\n";
char uart1testdata[] = "uart port 1 test data";
char uart2testdata[] = "uart port 2 test data";
int b[1]={0x11};  
char buff[]="Hello\n";
char buffer[1024];
const int READ_THREAD_ID = 0;
const int SEND_THREAD_ID = 1;
pthread_t thread[2];

// 0 is output, 1 is input
int direction = 0;
int bandwidth = 0;
int portnum = 0;
int looptestResult = 0;


const char send_buf[TEST_DATA_LEN]={0xaa,0x55,0x5d,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,0x0f,0x0e,0x0d,0x0c,0x0b,\
		    0x0a,0x09,0x08,0x07,0x06,0x05,0x04,0x03,0x02,0x01,0x00,0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,\
		    0x0c,0x0d,0x0e,0x0f,0x0f,0x0e,0x0d,0x0c,0x0b,0x0a,0x09,0x08,0x07,0x06,0x05,0x04,0x03,0x02,0x01,0x00,0x00,0x01,0x02,\
		    0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,0x0f,0x0e,0x0d,0x0c,0x0b,0x0a,0x09,0x08,0x07,0x06,\
		    0x05,0x04,0x03,0x02,0x01,0xdf};


char comp_buffer[TEST_DATA_LEN]= {0x00};
int headerFlag = 0;
int validLen = 0;
int testloop = 0;


int findFirstPackage(int handle);

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
		case 19200:
			cfsetispeed(&newtio, B19200);
			cfsetospeed(&newtio, B19200);
			break;
		case 38400:
			cfsetispeed(&newtio, B38400);
			cfsetospeed(&newtio, B38400);
			break;
		case 57600:
			cfsetispeed(&newtio, B57600);
			cfsetospeed(&newtio, B57600);
			break;
		case 115200:
			cfsetispeed(&newtio, B115200);
			cfsetospeed(&newtio, B115200);
			break;
		case 1500000:
			cfsetispeed(&newtio, B1500000);
			cfsetospeed(&newtio, B1500000);
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


void* uart_test(void * p)
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


void* looptest_send(void * p)
{
    //char * uartdata = (char*)p;
    char* uartdata = (char *)malloc(TEST_DATA_LEN+1);
    int len = 0;
    int loop = testloop;
    char tmp = 0;
    struct timeval time1;
    memcpy(uartdata, p, TEST_DATA_LEN);
    while(loop > 0){
   
	len = 0;
/*
        if(loop%6 == 0){
	    tmp = uartdata[2];
	    uartdata[2] = loop%128;
	}
*/
	while(len < TEST_DATA_LEN) {    
	    nsend=write(fd,uartdata+len,TEST_DATA_LEN-len);
            gettimeofday(&time1, NULL);
            printf("loop  nsend:%d, %ld:%ld \n",nsend, time1.tv_sec, time1.tv_usec);
	
            if(nsend<0) {
	        printf("send  error\n");
            } else {
                //printf("Send  OK\n"); 
            }  

	    len = nsend + len;
	}
/*	
	if(loop % 6 == 0){
	    uartdata[2] = tmp;
	}
*/
	usleep(20000);
	loop--;
    } 

}	



void* looptest_receive(void * p)
{
    char* expectdata = (char *)p;
    //sleep(1);
    int len = 0;
    int ret = 0;
    int loop =  testloop;
    struct timeval time1;
    
    printf("looptest_receive ... \n");
    
    while(loop > 0) {
        
	len = 0;

        ret = findFirstPackage(fd);
        if(ret == 0){
	    looptestResult++;
	    loop--;
	} else if(ret == -1){
	    loop--;	
	} else {
	    	
	}

	while(len < TEST_DATA_LEN) {
	    nread = read(fd, buffer + len, TEST_DATA_LEN-len);
        //    gettimeofday(&time1, NULL);
            //printf("loop read return %d bytes\n", nread);

            if(nread>0){
               //printf("recv %d bytes, content: %x, %x %x, %ld:%ld\n",nread, buffer[len], buffer[len+1], buffer[len+2], time1.tv_sec, time1.tv_usec);
	        totalReadNum += nread;
            }else{
               //printf("no data\n"); 
            }

	    len = nread + len;

	}

        ret = checkValue(send_buf, buffer, TEST_DATA_LEN);        
        if(ret == 0){
	    looptestResult++;
	}

        gettimeofday(&time1, NULL);
	
	if(ret == 0) {
	    printf("pass %d %ld:%ld\n", loop, time1.tv_sec, time1.tv_usec);
	}else{
	    printf("fail %d %ld:%ld\n", loop, time1.tv_sec, time1.tv_usec);
	    headerFlag = 0;
	    ret = findFirstPackage(fd);	
	    if(ret == 0){
	        looptestResult++;
		loop--;
	    } else if(ret == -1){
	        loop--;
	    } else {
	    
	    }
	}

	loop--;
    }
    
}


/***********************************/
/*         return value            */
/*  0 is read 98 and value is ok   */
/*  -1 is read 98 and value is fail*/
/*   1 is skip read                */
/***********************************/

int findFirstPackage(int handle)
{
    int firstNum = 0, len = 0, ret = 0;

    //headerFlag = 0;
    if(headerFlag == 0){
        printf("sync %ld +++ \n", totalReadNum);
    }else{
        ret = 1;
    }

    while(headerFlag == 0) {
        nread = read(handle, buffer, TEST_DATA_LEN);

	if(nread > 0){
	    firstNum = findHeader(buffer, nread);    
	    if(firstNum < nread){
                headerFlag = 1;
		memcpy(comp_buffer, buffer+firstNum, nread-firstNum);
		printf("find 0xaa at %ld \n", firstNum + totalReadNum);
            }else{
	  
            }
	    totalReadNum += nread;
        }else{
	    
	}

	if(headerFlag == 1) {
	    len = nread - firstNum;
            while(len < TEST_DATA_LEN){
                nread = read(handle, comp_buffer+len, TEST_DATA_LEN-len);
                if(nread > 0){
		   totalReadNum += nread; 
		}else{
		    
		}

                len = nread + len;
            }

            ret = checkValue(send_buf, comp_buffer, TEST_DATA_LEN);
            printf("first complete package arrive , result %d\n", ret);
		
            printf("sync %ld --- \n", totalReadNum);
	    
        }
	
    }

    return ret;
}


int findHeader(char* buf, int len)
{
     int i = 0;
     while(i < len){
        if(buf[i] == 0xaa){
            break;
        }
	i++;
     }
     
     return i;

}


int checkValue(char* src, char* dest, int length)
{
     int i = 0;
     int ret = 0;
     while(i < length) {
         if(src[i] != dest[i]) {
	    printf("index %d wrong , %x -> %x \n", i, src[i], dest[i]);
	    printf("%x ,%x, %x, %x \n", dest[0],dest[1],dest[2],dest[3]);
            ret = -1;
            break;	    
	 }	 
         i++;
     }

     return ret;
      
}




int parse(char* in, int inputLen, char* tail, int* tailLen)
{
    int i = 0;
    do {
        if((in[i] == 0xaa) && (in[i+1] == 0x55) && (in[i+2] == 0x5d)) {
	    
	}
    }while(i < inputLen);	    

}





int main(int argc, char *argv[])
{
    struct timeval timestart;
    int i = 0;
    gettimeofday(&timestart, NULL);
    printf("receive test start %ld:%ld \n", timestart.tv_sec, timestart.tv_usec);

    if(argc != 5){
        printf("wrong parameter\n");
        printf("argv[1]: 0 => output, 1 => input\n");
        printf("argv[2]: bandwidth\n");
        return -1;
    }

    direction = atoi(argv[1]);
    bandwidth = atoi(argv[2]);
    portnum = atoi(argv[3]);
    testloop = atoi(argv[4]);

    printf("user input direction %d, %d, port %d\n", direction, bandwidth, portnum, testloop);

    if((fd=open_port(fd,portnum))<0){
        perror("open_port error");
	
        return 1;
    }
		
    if((i=set_opt(fd,bandwidth,8,'N',1))<0){
        perror("set_opt error");
        return 1;
    }
		
    
    //printf("fd=%d\n",fd);

    if(direction == 1 || direction == 2){	
        if(pthread_create( &thread[READ_THREAD_ID], NULL, looptest_receive, NULL)== 0){
            printf(" create read thread success!\n");
        } else {
            printf(" create read thread fail!");
        }
    }

    if(direction == 0 || direction == 2){
        if(pthread_create( &thread[SEND_THREAD_ID], NULL, looptest_send, (void *)send_buf)==0){
            printf(" create send thread success!\n");
        } else {
            printf(" create send thread fail!");
        }
    }


    if(testloop < 100){
       sleep(3);
    }else {
       int duration = (testloop/100+1)*2;
       sleep(duration);    
    }

    close(fd);
    gettimeofday(&timestart, NULL);
    
    if(looptestResult == testloop){
        printf("test success\n");
    }else{
        printf("test fail\n");
    }
    
    printf("test end %ld:%ld \n",timestart.tv_sec, timestart.tv_usec);

    return 1;
}
