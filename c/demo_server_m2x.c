/*****************************************************************************
**
**  Name:           brcm_att_m2x_sample_app.c
**
**  Description:    Bluetooth BLE Proximity Monitor application
**
**  Copyright (c) 2013, Broadcom Corp., All Rights Reserved.
**  Broadcom Bluetooth Core. Proprietary and confidential.
**
*****************************************************************************/

/*#include <brcm_att_m2x_sample_app.h*/


#include <sys/socket.h>


#include <stdio.h>
#include <string.h>
#include <sys/un.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>
#include<arpa/inet.h>
#include<netdb.h> /*hostent*/
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "m2x.h"
#include "device.h"



/* DEFINES */
#define MODULE_TEST
#define RUN_M2X


#ifdef MODULE_TEST
#define APPL_TRACE_INFO printf
#define APPL_TRACE_ERROR0 printf
#define APPL_TRACE_ERROR1 printf
#define APPL_TRACE_ERROR2 printf
#define APPL_TRACE_WARNING1 printf
#else
#define APPL_TRACE_INFO(fmt, ...) (0)
#define APPL_TRACE_ERROR0(fmt, ...) (0)
#define APPL_TRACE_ERROR1(fmt, ...) (0)
#define APPL_TRACE_ERROR2(fmt, ...) (0)
#define APPL_TRACE_WARNING1(fmt, ...) (0)
#endif

 
#define MAX_NUM_ENTRIES 25
#define BUF_SIZE 500
#define LISTEN_BACKLOG 50
#define UINT8 unsigned char
#define FALSE 0 
#define TRUE 1

#define UUID_CHARACTERISTIC_FLUX 0x2AA1
#define UUID_CHARACTERISTIC_PRESSURE 0x2A6D
#define UUID_CHARACTERISTIC_TEMPERATURE 0x2A6E
#define UUID_CHARACTERISTIC_HUMIDITY 0x2A6F
#define UUID_CHARACTERISTIC_ACCELEROMETER 0xBBA0
#define UUID_CHARACTERISTIC_GYROSCOPE 0xBBA1

#define STREAM_TO_UINT16(u16, p) {u16 = ((uint16_t)(*(p)) + (((uint16_t)((uint16_t)*((p) + 1))) << 8)); (p) += 2;}


/*
M2X
*/

/*
brcmdemo

curl -i -X PUT http://api-m2x.att.com/v2/devices/e40755c54a392cb8682b52c686dff32c/streams/temperature/value -H "X-M2X-KEY: 55f033a944a2a951abd5a569fcbfad28" -H "Content-Type: application/json" -d "{ \"value\": \"40\" }"

Master Key
13de4d473ccb62875dfe187f6b19045b	GET, POST, PUT, DELETE
const char *M2X_KEY = "d0dd4116c51ae802afe8aab0dd79c718";
const char *M2X_KEY = "88409716c548628dba3e849cec97db96";

*/
const char *M2X_KEY = "13de4d473ccb62875dfe187f6b19045b"; /* Master Key for brcmdemo*/

const char *name = "SamoaHealthMonitor";
const char *description = "Monitoring various sensors";
const char *tags = "ActualMeasurements";
const char *visibility = "private";

#define MAX_STREAM_NAME 50
char stream_name[MAX_STREAM_NAME]; 
int device_count = 0;
int stream_id;


m2x_context *ctx = NULL;
unsigned char buf[2048];
m2x_response response;
const char device_id[33];
int randValue, randValue_2, i;


/*

IPC Communication with Device

*/

#ifdef MODULE_TEST
/*#define WICED_M2X_SOCK_PATH "/projects/mobcom_linuxbld_1_scratch/users/amitb/amitb_mtc_gw_ap_unix/brcm_usrlib/att_m2x/wiced_m2x_sock"*/
#define WICED_M2X_SOCK_PATH "/tmp/wiced_m2x_sock"
#else
#define WICED_M2X_SOCK_PATH "/tmp/wiced_m2x_sock"
#endif

int server_fd = -1;
int read_fd = -1;
int max_fd = -1;
fd_set socks_rd;
int status = 0;
int length = 0;
char errorstring[80];
int errno = 0;

FILE *fp;


/*
BLE Characterstics
*/

const char uuid_flux[] = { 0xa1, 0x2a };
const char uuid_pressure[] = { 0x6d, 0x2a };
const char uuid_temperature[] = { 0x6e, 0x2a };
const char uuid_humidity[] = { 0x6f, 0x2a };
const char uuid_accl[] = { 0xa0, 0x2a };
const char uuid_gyro[] = { 0xa1, 0x2a };

/*
STREAM TO BE UPDATED
*/
    
enum STREAM_TYPES { 
    HUMIDITY = 0, 
    TEMPERATURE, 
    PRESSURE, 
    GYRO, 
    ACCL, 
    MAGNETO_FLUX, 
    MAX_STREAM 
};


/* Stream Names*/

const char *stream_name_list[] = { "HUMIDITY", "TEMPERATURE", "PRESSURE", "GYROSCOPE", "ACCLEROMETER", "MAGNETO", "Null" };


/*
Sample Data for Sensors 

Humidity      org.bluetooth.characteristic.humidity 0x2A6F  Adopted 
Pressure      org.bluetooth.characteristic.pressure 0x2A6D  Adopted
Temperature   org.bluetooth.characteristic.temperature  0x2A6E  Adopted
Magnetic Flux Density - 3D org.bluetooth.characteristic.magnetic_flux_density_3D    0x2AA1  Adopted 

     AABBBBCCCCCCDD(DDDD)
     AA - Length of data (2)
     BBBB - UUID (4)
     CCCCCC - BDA (6)
     DD(DDDD) - Value (2 or 6 bytes)
     interpret_data(length, buf)
e.g.
0a 00 6f 2a 45 20 01 18 10 00 e6 01
0a 00 6d 2a 45 20 01 18 10 00 95 27 
0a 00 6e 2a 45 20 01 18 10 00 e9 00 
0a 00 6f 2a 45 20 01 18 10 00 e6 01 
0e 00 a0 bb 45 20 01 18 10 00 00 00 00 00 00 00
0e 00 a1 bb 45 20 01 18 10 00 c8 00 58 ff 00 00 

*/ 
  

/* 

Function Declarations 

*/
int socket_server_open(char *socket_name);




/*******************************************************************************
 **
 ** Function         create_device
 **
 ** Description     Create  Device, 
 **                 Hardcoded 
 **                  to add reading from configuration file. 
 **
 ** Returns          void
 **
 *******************************************************************************/
static int create_device(void)
{
    char *master_key;
    char fname[50];
    FILE *device_fp; 
    printf("Creating M2X device \n");
 
    sprintf(fname, "/etc/m2x/dev_%s", name);
    device_fp = fopen (fname, "r");
    if (device_fp) 
    {
        fscanf(device_fp, "%s", device_id);
        APPL_TRACE_INFO("Device '%s' Exists. Device ID:%s\n",name, device_id );
        fclose(device_fp);
        return 2;
    } 
    else 
    {
        /* 1.3 Create a Device */
        sprintf(buf, "{\"name\": \"%s\", \"description\": \"%s\", \"tags\": \"%s\", \"visibility\": \"%s\"}",
          name, description, tags, visibility);
        APPL_TRACE_INFO("Create device: %s\n", buf);
        response = m2x_device_create(ctx, buf);
        APPL_TRACE_INFO("Status code: %d\n", response.status);

        if (m2x_is_success(&response) && response.raw) 
        {
            APPL_TRACE_INFO("%s\n", response.raw);
            APPL_TRACE_INFO("Feed ID    : %s\n", json_object_get_string(json_value_get_object(
                response.json), "id"));

            memcpy(device_id, json_object_get_string(json_value_get_object(response.json), "id"),
                        sizeof(device_id));
            APPL_TRACE_INFO("%s\n", &device_id);
            
            /* Store the Device Id in the file */
            /*sprintf(fname, "/etc/m2x/dev_%s", name);*/
            device_fp = fopen (fname, "w+");
            if (device_fp) {  
                fprintf(device_fp, "%s", device_id);
                fclose(device_fp);
            } else
            { 
                printf("create_device: Failed to open file");
            }  
            device_count++;
        } 
       
        /* Release the response so that same struct is used for next response */
        m2x_release_response(&response);
        /* Sleep for about a min, since it takes ~40 sec to create the device */
        sleep(45);
        return 1;
    }
}
    


/*******************************************************************************
 **
 ** Function         initialize_setup
 **
 ** Description     Create  streams and trigger
 **                  Can be hardcoded 
 **                  to add reading from configuration file. 
 **
 ** Returns          void
 **
 *******************************************************************************/
static int create_streams(void)
{

    FILE *streams_fp;
    char fname[10];
    char key_from_file[100];
    int stream_len=0;
    int device_present;
    int stream_present[MAX_STREAM];
    int j;
   
    printf("Creating M2X Streams \n");
    memset(stream_present, 0, MAX_STREAM);
    /* Get the list of streams from the M2X Server and validate that all the Streams are present */
      response = m2x_device_streams(ctx, device_id);
      printf("Response code: %d\n", response.status);
      if (m2x_is_success(&response)) {
        JSON_Array *streams = json_object_get_array(json_value_get_object(
            response.json), "streams");
        size_t i;
        for (i = 0; i < json_array_get_count(streams); i++) {
          JSON_Object *json_stream = json_array_get_object(streams, i);
          const char *stream_name = json_object_get_string(json_stream, "name");
          for (j=0; j< MAX_STREAM; j++)
          {
                if (strncmp(stream_name, stream_name_list[j],strlen(stream_name)))
                {
                  stream_present[j] = 1; /* Stream already present*/
                } 
          }
          printf("Name       : %s\n", stream_name);
          printf("\n");
        }
      }
      m2x_release_response(&response);

    /* Create a Stream */
    for (i=0; i < MAX_STREAM; i++)
    {
        if (stream_present[i])
            continue;   
        printf("Creating %d M2X Stream :\"%s\" of size=%d  \n", i, stream_name_list[i], sizeof(stream_name_list[i]));
        memset(&buf, 0, sizeof(buf));
        stream_len= strlen(stream_name_list[i]);
	    stream_len = (stream_len<MAX_STREAM_NAME)?stream_len:MAX_STREAM_NAME;
        memcpy(stream_name, stream_name_list[i], stream_len+1); 
        /*stream_name[stream_len]='\n';*/
        APPL_TRACE_INFO("\nBefore m2x_device_stream_create device_id: %s    |\n", &device_id);
        APPL_TRACE_INFO("\nBefore m2x_device_stream_create stream_name: %s  |\n", stream_name);
        
        switch(i) {
            case HUMIDITY:
                sprintf(buf, "{ \"unit\": { \"label\": \"percentage\", \"symbol\": \"%%\" }, \"type\": \"numeric\" }");
            break;        
            case TEMPERATURE:
                sprintf(buf, "{ \"unit\": { \"label\": \"celsius\", \"symbol\": \"C\" }, \"type\": \"numeric\" }");
            break;        
            case PRESSURE:
                sprintf(buf, "{ \"unit\": { \"label\": \"pascal\", \"symbol\": \"Pa\" }, \"type\": \"numeric\" }");
            break;        
            
            case GYRO:
                sprintf(buf, "{ \"unit\": { \"label\": \"point\", \"symbol\": \"p\" }, \"type\": \"numeric\" }");
            break;        
            case ACCL:
                sprintf(buf, "{ \"unit\": { \"label\": \"point\", \"symbol\": \"p\" }, \"type\": \"numeric\" }");
            break;        
            case MAGNETO_FLUX:
                sprintf(buf, "{ \"unit\": { \"label\": \"point\", \"symbol\": \"p\" }, \"type\": \"numeric\" }");
            break;
        }
        
        response = m2x_device_stream_create(ctx, &device_id, stream_name, buf);        
        if (m2x_is_success(&response) && response.raw) {
        APPL_TRACE_INFO("%s\n", response.raw);
        }

        /* Store the Device Id in the file */
        sprintf(fname, "/tmp/m2x/stream_%s", stream_name);
        streams_fp = fopen (fname, "w+");
        fprintf(streams_fp, "%s", stream_name);
        fclose(streams_fp);

        /* Release the response so that same struct is used for next response */
        m2x_release_response(&response);
        /* Sleep for about a min, since it takes ~40 sec to create the device */
        sleep(5);
    }
    
    
}



/*******************************************************************************
 **
 ** Function         create_trigger
 **
 ** Description      
 **
 ** Returns          void
 **
 *******************************************************************************/

static int create_trigger(void)
{    
    /* 3.0 Create a Trigger */
    memset(&buf, 0, sizeof(buf));
    sprintf(buf, "{\"stream\": \"%s\", \"name\": \"High Temperature\", \"condition\": \">\", \"value\": \"75\", \"callback_url\": \"http://requestb.in/1dsaa5u1\", \"status\": \"enabled\",  \"send_location\": \"true\"  }",
          stream_name);
    response = m2x_device_trigger_create(ctx, &device_id, buf);
    if (m2x_is_success(&response) && response.raw) {
      printf("%s\n", response.raw);
    }
    sleep(5);
}


/*******************************************************************************
 **
 ** Function         socket_server_open
 **
 ** Description      
 **
 ** Returns          void
 **
 *******************************************************************************/

int socket_server_open(char *socket_name)
{
    int sock = -1;
    struct sockaddr_un server_address;
    int status;
    char errorstring[80];

    sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) 
    {
        strerror_r(errno, errorstring, sizeof(errorstring));
        APPL_TRACE_ERROR1("socket_server_open: socket failed(%s) \n", errorstring);
        return -1;
    }

    memset((char *) &server_address, 0 , sizeof(server_address));
    server_address.sun_family = AF_UNIX;
    server_address.sun_path[0] = 0;

    strncat(server_address.sun_path, (char*)socket_name, sizeof(server_address.sun_path)-1-strlen(server_address.sun_path));

     /* Remove old socket (if previous server closed without removing it) */
    if (unlink(server_address.sun_path) == 0)
    {
        APPL_TRACE_WARNING1("socket_server_open: removed old file:%s \n", socket_name);
    }

    
    if (bind(sock, (struct sockaddr *) &server_address,
            sizeof(server_address)) < 0)
    {
        strerror_r(errno, errorstring, sizeof(errorstring));
        APPL_TRACE_ERROR1("socket_server_open: bind failed(%s) \n", errorstring);
        close(sock);
        return -1;
    }

    /* Set up queue for incoming connections. */
    status = listen(sock, LISTEN_BACKLOG);
    if (status < 0)
    {
        strerror_r(errno, errorstring, sizeof(errorstring));
        APPL_TRACE_ERROR1("socket_server_open: listen failed(%s) \n", errorstring);
        close(sock);
        return -1;
    }

    return sock;
}



/***************************************************************************

*   Handle an abnormal abort of the app.
*
*
static void HandleChanLogAbort(int sid)
{
    (void)sid;

    LOG_INFO("HandleChanLogAbort\n");

    ATLOG_Shutdown();

    exit(1);
}
*/

/*******************************************************************************
 **
 ** Function         update_value
 **
 ** Description      
 **
 ** Returns          void
 **
 *******************************************************************************/
 

static int post_to_m2x(int length, char *input_val)
{

    int data_length = length - 8; 
    int is_ok;
    unsigned char UUID[2];
    unsigned char BDA[6];
    unsigned char *SENSOR_DATA;
    unsigned char *tmp_val = input_val;
    unsigned int  uuid_is=0;
    short value, x, y, z;
   

/* Receive a message from client */

/*
Sample Data for Sensors 

Humidity      org.bluetooth.characteristic.humidity 0x2A6F  Adopted 
Pressure      org.bluetooth.characteristic.pressure 0x2A6D  Adopted
Temperature   org.bluetooth.characteristic.temperature  0x2A6E  Adopted
Magnetic Flux Density - 3D org.bluetooth.characteristic.magnetic_flux_density_3D    0x2AA1  Adopted 

     AABBBBCCCCCCDD(DDDD)
     AA - Length of data (2)
     BBBB - UUID (2)
     CCCCCC - BDA (6)
     DD(DDDD) - Value (2 or 6 bytes)
     interpret_data(length, buf)


*/ 

/*
     AABBBBCCCCCCDD(DDDD)
     AA - Length of data (2)
     BB BB - UUID (4)
     CC CC CC CC CC CC- BDA (6)
     DD DD (DD DD DD DD) - Value (2 or 6 bytes)

    e.g. 10 or 14
    0a 00 6f 2a 45 20 01 18 10 00 e6 01
    0a 00 6d 2a 45 20 01 18 10 00 95 27 
    0a 00 6e 2a 45 20 01 18 10 00 e9 00 
    0a 00 6f 2a 45 20 01 18 10 00 e6 01 
    0e 00 a0 bb 45 20 01 18 10 00 00 00 00 00 00 00
    0e 00 a1 bb 45 20 01 18 10 00 c8 00 58 ff 00 00 
    
*/  
   memcpy(UUID, tmp_val, 2);
   uuid_is =  UUID[0] | UUID[1]<<8;

   printf("uuid_is = %d, length=%\n", uuid_is, length, data_length);
   
   memcpy(BDA, &tmp_val[2], 6);   
   printf("BDA = %s", BDA);

   if ((data_length == 2) || (data_length ==6)) {
       /*memcpy(SENSOR_DATA, &tmp_val[8], data_length);*/
       SENSOR_DATA=&tmp_val[8];
       /*SENSOR_DATA[data_length]= "\n";*/
      printf("Sensor_data: %x %x", SENSOR_DATA[0], SENSOR_DATA[1]);

     switch(data_length)
     {
        case 2:
	     STREAM_TO_UINT16(value, SENSOR_DATA);
             printf("Sensor_data: %x ",value);
             break;
        case 6:
	     STREAM_TO_UINT16(value, SENSOR_DATA);
	     STREAM_TO_UINT16(x, SENSOR_DATA);
	     STREAM_TO_UINT16(y, SENSOR_DATA);
	     STREAM_TO_UINT16(z, SENSOR_DATA);
      printf("Sensor_data: x=%x y=%x z=%x", x,y,z);
       x = x & 0x000F;
       z = y & 0x00F0;
       z = y & 0x0F00;
        
             break;
     }

   }
   else{
       return;
   }
    /* Reading the first two bytes for the length */ 
    switch(uuid_is) {
        case UUID_CHARACTERISTIC_HUMIDITY: 
            stream_id= HUMIDITY;
            break;
        case UUID_CHARACTERISTIC_TEMPERATURE: 
            stream_id= TEMPERATURE;
            break;
        case UUID_CHARACTERISTIC_PRESSURE: 
            stream_id= PRESSURE;
            break;
        case UUID_CHARACTERISTIC_FLUX: 
            stream_id= MAGNETO_FLUX;
            break;
        case UUID_CHARACTERISTIC_ACCELEROMETER: 
             stream_id= ACCL;
            break;
        case UUID_CHARACTERISTIC_GYROSCOPE: 
             stream_id= GYRO;
            break;
        }
        
    memset(&buf, 0, sizeof(buf));  
    printf("\nReceived Sensor reading from :");
    switch(stream_id) {
        case HUMIDITY:
            printf("HUMIDITY");
            sprintf(buf, "{ \"value\": \"%d.%d\" }", value/10,value%10);        
        break;
        case TEMPERATURE:
            printf("TEMPERATURE");           
            sprintf(buf, "{ \"value\": \"%d\" }", value/10, value%10);        
        break;
        case PRESSURE:
            printf("PRESSURE");
            sprintf(buf, "{ \"value\": \"%d\" }", value);        
        break;

        case GYRO:
            printf("GYRO");
            sprintf(buf, "{ \"value\": \"%x%x%x\" }", x,y,z );        
        break;
        case ACCL:
            printf("ACCL");
            sprintf(buf, "{ \"value\": \"%x%x%x\" }", x,y,z );        
        break;
        case MAGNETO_FLUX:
            printf("MAGNETO");
            sprintf(buf, "{ \"value\": \"%x%x%x\" }", x,y,z );        
        break;
        default:
        
        break;
    }        
    printf("(%d)\n", stream_id);
    
    

    /*{ "value": x }*/

    printf("buf: %s \n", buf);    
#ifdef RUN_M2X
    response = m2x_device_stream_value_update(ctx, &device_id, stream_name_list[stream_id], buf);
    if (m2x_is_success(&response) && response.raw) {
        printf("%s\n", response.raw);
    }
    else {
        /* Error in updating value */
    }
        
    /* Release the response so that same struct is used for next response */
    m2x_release_response(&response);
  /*  sleep(5);*/
#endif
    
}

/* 
* An App to create a new device and streams then update the stream 
* from various clients
*
*/

int main(int argc, char *argv[])
{
    char *master_key=0;
    
    fp = 0;     
    APPL_TRACE_INFO("Starting with M2x \n");
 
    if (argc < 2) {
        printf("Correct Usage brcm_m2x_demo <<filename>>\n");
        /* exit(1);*/
    } else {
        APPL_TRACE_INFO("Opening File:  %s\n", argv[1]);	
        fp = fopen(argv[1], "r");  
    } 
    if (argc < 2) {
        APPL_TRACE_INFO("Opening File: /etc/m2x/m2x_key \n" );
        fp = fopen("/etc/m2x/m2x_key", "r");   
    }  
    if (fp) {
        /* 1.1 Read the key from the file. */        
        master_key = (char *) malloc(strlen(M2X_KEY) * sizeof (char));
        fscanf(fp,"%s",master_key );        
        APPL_TRACE_INFO("Master Key from file: %s\n", master_key);
        /*memcpy(master_key,, strlen(M2X_KEY));*/
        ctx = m2x_open(master_key);        
        free(master_key);
        fclose(fp);
    }
    else {
        /* 1.1 Open the Account using Master Key. */
        APPL_TRACE_INFO("Error in opening file %s\n", argv[1]);
        APPL_TRACE_INFO("reading default master key: %s\n", M2X_KEY);
        ctx = m2x_open(M2X_KEY);
    }


    for (i=0; i < MAX_STREAM; i++) {
        printf("Need to create %d M2X Stream :\"%s\" of size=%d  \n", i, stream_name_list[i], strlen(stream_name_list[i]));

    }

    /* 1.2 Enable Verbose */
    m2x_set_verbose(ctx, 1);    

    
    /* Create Device and Stream*/
#ifdef RUN_M2X 
    create_device();
    create_streams();
#endif
    /* Open Server Socket */
    server_fd = socket_server_open(WICED_M2X_SOCK_PATH);
   	APPL_TRACE_ERROR1("socket_local_client: fd = %d \n", server_fd);
    
	if(server_fd > 0) {
       APPL_TRACE_INFO("socket_local_server: opening server socket successful \n");
    }
    else {
        APPL_TRACE_ERROR1("socket_local_server: returned error %d \n", errno );
        exit(0);
    }

 	while (1) {
		FD_ZERO(&socks_rd);
		FD_SET(server_fd, &socks_rd);
		max_fd  = server_fd;

		if(read_fd > 0) {
			FD_SET(read_fd, &socks_rd);
			if (read_fd > max_fd)
				max_fd = read_fd;
		}

		status = select (max_fd + 1, &socks_rd, (fd_set *) NULL,(fd_set *) NULL,NULL);
		if(status < 0) {	
			strerror_r(errno, errorstring, sizeof(errorstring));
			APPL_TRACE_ERROR1("server: select failed(%s)", errorstring);
			exit(0);
		}
		else {
			length  = 0;

			if(FD_ISSET(server_fd, &socks_rd)) {
				read_fd = accept(server_fd, NULL, NULL);
				if ( read_fd < 0) {
					strerror_r(errno, errorstring, sizeof(errorstring));
					APPL_TRACE_ERROR1("server: accept error %s", errorstring);
				}
				else {
					APPL_TRACE_INFO("server: client connected %d", read_fd);
                }
			}
            /* If there is something to read from the socket */
            if (FD_ISSET(read_fd, &socks_rd)) {
                /* Read length */
                status = read(read_fd, (void *)&length, 2);
				if(status <= 0 )
				{
					strerror_r(errno, errorstring, sizeof(errorstring));
					APPL_TRACE_ERROR1("server: read failed(%s)", errorstring);
					close(read_fd);
					read_fd = -1;
				}
                else {
                    APPL_TRACE_INFO("received length:%d", length);
                    
                }
				
                status = read(read_fd, (UINT8 *) &buf[0], length);
                
				if(status <= 0 ) {
					strerror_r(errno, errorstring, sizeof(errorstring));
					APPL_TRACE_ERROR1("server: read failed(%s)", errorstring);
					close(read_fd);
					read_fd = -1;
				}
                else {
                    printf("Received data: ");
                    for (i=0;i<length;i++) {
                        printf("%c", buf[i]);                        
                    }
                    printf("\n");
                    post_to_m2x(length, buf);
                }                
			}
		}
	}

	close(read_fd);
    close(server_fd);   
}



