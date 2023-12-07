/*
 * tcpclient.c
 *
 *  Created on: 21-Apr-2022
 *      Author: controllerstech
 */


#include "lwip/opt.h"

#include "lwip/api.h"
#include "lwip/sys.h"

#include "tcpclient.h"
#include "string.h"

static struct netconn *conn;
static struct netbuf *buf;
static ip_addr_t *addr, dest_addr;
static unsigned short port, dest_port;

char msgc[100];
char smsgc[200];
int indx = 0;

int motor_left;
int motor_right;
int time_flag = 0;

// Function to send the data to the server
void tcpsend (char *data);
void BEEP(void);
// tcpsem is the binary semaphore to prevent the access to tcpsend
sys_sem_t tcpsem;

static void tcpinit_thread(void *arg)
{
	err_t err, connect_error;

	/* Create a new connection identifier. */
	conn = netconn_new(NETCONN_TCP);

	if (conn!=NULL)
	{
		/* Bind connection to the port number 7 (port of the Client). */
		err = netconn_bind(conn, IP_ADDR_ANY, 7);

		if (err == ERR_OK)
		{
			/* The desination IP adress of the computer */
			IP_ADDR4(&dest_addr, 192, 168, 12, 31);
			dest_port = 10;  // server port

			/* Connect to the TCP Server */
			connect_error = netconn_connect(conn, &dest_addr, dest_port);

			// If the connection to the server is established, the following will continue, else delete the connection
			if (connect_error == ERR_OK)
			{
				// Release the semaphore once the connection is successful
				sys_sem_signal(&tcpsem); // sem = 1
				//sys_mutex_unlock(&tcpmutex);
				while (1)
				{
					/* wait until the data is sent by the server */
					if (netconn_recv(conn, &buf) == ERR_OK)
					{
						/* Extract the address and port in case they are required */
						addr = netbuf_fromaddr(buf);  // get the address of the client
						port = netbuf_fromport(buf);  // get the Port of the client

						/* If there is some data remaining to be sent, the following process will continue */
						do
						{

							strncpy (msgc, buf->p->payload, buf->p->len);   // get the message from the server

							// Or modify the message received, so that we can send it back to the server
							//sprintf (smsgc, "\"%s\" was sent by the Client\n", msgc);

							if(strcmp(msgc,"DRIVE") == 0) {
								motor_right = MOTORFAST;
								motor_left = MOTORFAST;
							}
							else if(strcmp(msgc,"LMSLOW") == 0) {
								motor_right = MOTORSLOW;
							}
							else if(strcmp(msgc,"LMSTOP") == 0){
								motor_right = MOTORSTOP;
							}
							else if(strcmp(msgc,"LMFAST") == 0){
								motor_right = MOTORFAST;
							}
							else if(strcmp(msgc,"RMFAST") == 0){
								motor_left	= MOTORFAST;
							}
							else if(strcmp(msgc,"RMSLOW") == 0) {
								motor_left = MOTORSLOW;
							}
							else if(strcmp(msgc,"RMSTOP") == 0){
								motor_left	= MOTORSTOP;
							}
							else if(strcmp(msgc,"TIME") == 0){
								time_flag = MOTORSLOW;
							}

							if(motor_left == MOTORSTOP && motor_right == MOTORSTOP){
								BEEP();
								osDelay(50);
								BEEP();
							}
							// semaphore must be taken before accessing the tcpsend function
							sys_arch_sem_wait(&tcpsem, 1000); // sem = 0

							// send the data to the TCP Server
							//tcpsend (smsgc);
							memset (msgc, '\0', 100);  // clear the buffer
						}
						while (netbuf_next(buf) >0);

						netbuf_delete(buf);
					}
				}
			}

			else
			{
				/* Close connection and discard connection identifier. */
				netconn_close(conn);
				netconn_delete(conn);
			}
		}
		else
		{
			// if the binding wasn't successful, delete the netconn connection
			netconn_delete(conn);
		}
	}
}

void tcpsend (char *data)
{
	// send the data to the connected connection
	netconn_write(conn, data, strlen(data), NETCONN_COPY);
	// relaese the semaphore
	sys_sem_signal(&tcpsem); // sem = 1

}


static void tcptime_thread (void *arg)
{
	for (;;)
	{
		// semaphore must be taken before accessing the tcpsend function
		sys_arch_sem_wait(&tcpsem, 1000); // sem = 0
		// send the data to the server
		if(time_flag == 1){
			time_flag = 0;
			sprintf (smsgc, "dirve time is %d sec\n", indx);
			tcpsend(smsgc);
		}
		indx++;
		osDelay(1000);
	}
}


void tcpclient_init (void)
{
	sys_sem_new(tcpsem, 2);  // the semaphore would prevent simultaneous access to tcpsend
	sys_thread_new("tcpinit_thread", tcpinit_thread, NULL, DEFAULT_THREAD_STACKSIZE,osPriorityNormal);
	sys_thread_new("tcptime_thread", tcptime_thread, NULL, DEFAULT_THREAD_STACKSIZE,osPriorityNormal);
}

void BEEP(void){
	HAL_GPIO_WritePin(GPIOE, GPIO_PIN_2, GPIO_PIN_SET );
	osDelay(50);
	HAL_GPIO_WritePin(GPIOE, GPIO_PIN_2, GPIO_PIN_RESET );
}
