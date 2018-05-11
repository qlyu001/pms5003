#include <stdio.h>
#include <fcntl.h>   /* File Control Definitions           */
#include <termios.h> /* POSIX Terminal Control Definitions */
#include <unistd.h>  /* UNIX Standard Definitions 	   */ 
#include <errno.h>   /* ERROR Number Definitions  */ 
#include <stdlib.h>
#include <string.h>
#include <byteswap.h>
#include <signal.h>
 

 /*check this website http://codegist.net/code/pms5003/
 
 https://stackoverflow.com/questions/35193613/how-to-read-character-using-serial-port-in-linux
 
 */
int fd;/*File Descriptor*/

void set_interface_attribs()
{
   
		
		printf("\n +----------------------------------+");
		printf("\n |        Serial Port Read          |");
		printf("\n +----------------------------------+");

		/*------------------------------- Opening the Serial Port -------------------------------*/

		/* Change /dev/ttyUSB0 to the one corresponding to your system */

        	fd = open("/dev/ttyUSB0",O_RDWR | O_NOCTTY);	/* ttyUSB0 is the FT232 based USB2SERIAL Converter   */
			   					/* O_RDWR   - Read/Write access to serial port       */
								/* O_NOCTTY - No terminal will control the process   */
								/* Open in blocking mode,read will wait              */
									
									                                        
									
        	if(fd == -1)						/* Error Checking */
            	   printf("\n  Error! in Opening ttyUSB0  ");
        	else
            	   printf("\n  ttyUSB0 Opened Successfully ");


		/*---------- Setting the Attributes of the serial port using termios structure --------- */
		
		struct termios SerialPortSettings;	/* Create the structure                          */

		tcgetattr(fd, &SerialPortSettings);	/* Get the current attributes of the Serial port */

		/* Setting the Baud rate */
		cfsetispeed(&SerialPortSettings,B9600); /* Set Read  Speed as 9600                       */
		cfsetospeed(&SerialPortSettings,B9600); /* Set Write Speed as 9600                       */

		/* 8N1 Mode */
		SerialPortSettings.c_cflag &= ~PARENB;   /* Disables the Parity Enable bit(PARENB),So No Parity   */
		SerialPortSettings.c_cflag &= ~CSTOPB;   /* CSTOPB = 2 Stop bits,here it is cleared so 1 Stop bit */
		SerialPortSettings.c_cflag &= ~CSIZE;	 /* Clears the mask for setting the data size             */
		SerialPortSettings.c_cflag |=  CS8;      /* Set the data bits = 8                                 */
		
		SerialPortSettings.c_cflag &= ~CRTSCTS;       /* No Hardware flow Control                         */
		SerialPortSettings.c_cflag |= CREAD | CLOCAL; /* Enable receiver,Ignore Modem Control lines       */ 
		
		
		SerialPortSettings.c_iflag &= ~(IXON | IXOFF | IXANY);          /* Disable XON/XOFF flow control both i/p and o/p */
		SerialPortSettings.c_iflag &= ~(ICANON | ECHO | ECHOE | ISIG);  /* Non Cannonical mode                            */

		SerialPortSettings.c_oflag &= ~OPOST;/*No Output Processing*/
		
		/* Setting Time outs */
		SerialPortSettings.c_cc[VMIN] = 1; /* Read at least 10 characters */
		SerialPortSettings.c_cc[VTIME] = 1; /* Wait indefinetly   */


		if((tcsetattr(fd,TCSANOW,&SerialPortSettings)) != 0) /* Set the attributes to the termios structure*/
		    printf("\n  ERROR ! in Setting attributes");
		else
                    printf("\n  BaudRate = 9600 \n  StopBits = 1 \n  Parity   = none");
			
	        /*------------------------------- Read data from serial port -----------------------------*/
	    printf("\n +----------------------------------+\n\n\n");

		
}

double avg_sum[12] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

size_t avg_num = 0;
void sigalrm_handler()
{
  if (avg_num > 0) {
    for (size_t i = 0; i < sizeof(avg_sum) / sizeof(avg_sum[0]); i++)
      avg_sum[i] /= avg_num;
    print_data(avg_num, avg_sum);
    exit(0);
  } else {
    fprintf(stderr,
            "warning: no data frames collected in the given time span.\n");
    exit(-1);
  }
}
void forced_read( void *buf, size_t count)
{
    size_t num_read = 0;

  do {

		/*
		int file: The file descriptor of where to read the input. You can either use a file 
		descriptor obtained from the open system call, or you can use 
		0, 1, or 2, to refer to standard input, standard output, or standard error, respectively.

		buf: a character array where the read content will be sotred
		size_t nbytes: number of bytes to read before truncating data.
		return value: returns the number of bytes that were read.
		*/
    int rdlen = read(fd, (char *) buf + num_read, count - num_read);
    if (rdlen < 1) {
      fprintf(stderr,
              "fatal: read() == %d (got %lu of %lu requested): %s\n",
              rdlen, num_read, count, strerror(errno));
      exit(-1);
    }
    //printf("The integer is %d\n", rdlen);
    //printf("%d = %x\n", (char *)buf,(char *)buf);
    num_read += rdlen;
  } while (num_read < count);
 


}

void print_data(size_t num_measurements, double *data)
{
  static const char *labels[] = { "std_pm1", "std_pm2_5", "std_pm10",
    "atm_pm1", "atm_pm2_5", "atm_pm10",
    "Particles > 0.3um / 0.1L air", 
	"Particles > 0.5um / 0.1L air", 
	"Particles > 1um   / 0.1L air",
    "Particles > 2.5um / 0.1L air", 
	"Particles > 5.0um / 0.1L air", 
	"Particles > 50um  / 0.1L air"
  };

  printf("{\"num_measurements\":%lu", num_measurements);

  for (size_t i = 0; i < sizeof(labels) / sizeof(labels[0]); i++){
	printf(",\"%s\":%.02f", labels[i], data[i]);
	printf("\n");
  }
  printf("}\n");

}



int main(){
	 size_t average_over = 0;
  
	set_interface_attribs();


 for (;;) {
    char ch;
    forced_read(&ch, 1);
    if (ch != 0x42)
      continue;
    forced_read(&ch, 1);
    if (ch != 0x4d)
      continue;

    unsigned short data[15];
    forced_read(&data, sizeof(data));
    for (size_t i = 0; i < sizeof(data) / sizeof(short); i++)
      data[i] = __bswap_16(data[i]);

    // Check frame length.
    if (data[0] != 2 * 13 + 2)
      continue;

    // Check sum.
    size_t check_sum = 0x42 + 0x4d;
    for (size_t i = 0; i < sizeof(data) - sizeof(data[0]); i++)
      check_sum += ((unsigned char *) data)[i];
    if (check_sum != data[14])
      continue;

    double data_fp[12];
    for (size_t i = 0; i < sizeof(avg_sum) / sizeof(avg_sum[0]); i++) {
      data_fp[i] = data[i + 1];
      avg_sum[i] += data_fp[i];
    }
    avg_num++;

	//print_data(1, data_fp);

	
    // Print each frame if not averaging over time.
    if (!average_over)
      print_data(1, data_fp);
  }

    close(fd); /* Close the serial port */

}