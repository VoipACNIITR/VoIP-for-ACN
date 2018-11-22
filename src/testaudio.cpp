// testaudio.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "routines.h"
#include <stdlib.h>
#include <conio.h>
#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <winsock2.h>
#include <windows.h> 
#include <iostream>

using namespace std;

#define NUM_OF_BUFS 20

	int sockfd;
	struct sockaddr_in their_addr;	// connector’s address information
	struct sockaddr_in my_addr;		 // my address information
	struct hostent *he;
	char *dest_IP="localhost";//"192.168.7.10";// // "172.28.5.149"; "

#define MYPORT 4950 // the port users will be connecting to
#define MAXBUFLEN 1024+20  // 20 bytes for header
#define ReadDataSize 1024
#define DATAPACKET 0x01
#define REQPACKET  0x00
#define ENDPACKET  0x02
/*---------------------------- for wave in open globals -----------------------------------*/


char data[ReadDataSize];
char rcvdData[ReadDataSize];

WAVEFORMATEX WaveFormat;
HWAVEIN WaveHandle;
HWAVEOUT OutHandle;

char *outData;
BOOL Ext_Thread=FALSE;	
BOOL Buffer_Played=TRUE;
BOOL Terminate_RCVRthread=FALSE;
BOOL Terminate_TxThread=FALSE;	

char* WaveInData[NUM_OF_BUFS];
WAVEHDR WaveInHeader[NUM_OF_BUFS];
WAVEHDR WaveOutHeader[NUM_OF_BUFS];
int recordingCounter=0, playingCounter=0;
int BufferSize;
int size;
int playCounter,recordCounter;


//================ variables for CVSD =============================
char *EncodedData, *DecodedData[NUM_OF_BUFS];
int EncodedLength=0, DecodedLength[NUM_OF_BUFS];

CMutex mutxPlayCounter, mutxBufferPlayed;
CSingleLock PCLock(&mutxPlayCounter),BPLock(&mutxBufferPlayed);


/*---------------------------- for wave in open globals -----------------------------------*/


void Handle_Buffer()
{
		int Res,k;
		int numbytes;	
		

		/*---------------------------- Extract Size of Recorded Data -----------------------------------*/
	 
        size=WaveInHeader[recordingCounter].dwBytesRecorded;
		/*---------------------------- Buffer to Hold Data to Play -----------------------------------*/

		for(k=0;k<size;k++) // 2048
		{
			outData[k]=WaveInData[recordingCounter][k];
			data[k+1]=WaveInData[recordingCounter][k];
		//	DecodedData[recordingCounter][k]=WaveInData[recordingCounter][k];
		}
	
		/*-------------------------------------------- Enqueue This buffer for next recording------------------------------*/

			Res = waveInAddBuffer(WaveHandle, &WaveInHeader[recordingCounter], sizeof(WAVEHDR));
			if(Res!=MMSYSERR_NOERROR)
			{
				LPSTR errtext;
				errtext= new char[150];
				waveInGetErrorText(Res,errtext,150);
				printf("wave in add buffer error:: handle buffer : %d error = %s\n", Res,errtext);
				return;
			}

	//	applyCVSD(outData, size, EncodedData, &EncodedLength);
	//write_file_to_port(outData,size);

		data[0]=DATAPACKET; 

        if((numbytes=sendto(sockfd, data, size+1, 0,(struct sockaddr *)&their_addr, sizeof(struct sockaddr))) == -1) 
		{
				cout<<"sendto error="<<WSAGetLastError()<<endl;
		}
	//	if((numbytes=sendto(sockfd, outData, size, 0,(struct sockaddr *)&their_addr, sizeof(struct sockaddr))) == -1) 
	//	{
	//			cout<<"sendto error="<<WSAGetLastError()<<endl;
	//	}
		//else
			//cout<<"Bytes sent ="<<numbytes<<"   out of ="<<size<<endl;

	//=============================================================//
		//	reverseCVSD(inData, dwLength, DecodedData[playingCounter], &DecodedLength[playingCounter]);

		//  waveOutWrite(OutHandle, &WaveOutHeader[recordingCounter], sizeof(WAVEHDR));

		recordingCounter=recordingCounter+1;
		if(recordingCounter==NUM_OF_BUFS)
		{
		//	cout<<"recording"<<endl;
			recordingCounter=0;
		}
}

static void CALLBACK waveInProc(LPVOID arg)
{
	HWND hWnd = (HWND)arg; 
    MSG     msg; 
 
    while (GetMessage(&msg, (HWND)-1, 0, 0) == 1) 
    { 
		if(Ext_Thread==TRUE)
			break;
	    switch (msg.message) 
        { 
            case MM_WIM_DATA: 
            { 

				Handle_Buffer();
			
				break;
			}
		
		}
	}
	Ext_Thread=FALSE;
	printf("\n\t waveInProc thread exitted");
	ExitThread(0);
}



void Receiver(void)
{
    int k;
    int numbytesRcvd,addr_len=sizeof(their_addr);
	
	
	for(; ;)
	  {		
	    if(Terminate_RCVRthread==TRUE) 
		break;

		   // if ((numbytesRcvd=recvfrom(sockfd,DecodedData[playingCounter], MAXBUFLEN, 0,(struct sockaddr *)&their_addr, &addr_len)) == -1) 
		//	{
		//	  perror("recvfrom error");
			  //getchar();
			  //exit(1);
		//	}

			if ((numbytesRcvd=recvfrom(sockfd,rcvdData, ReadDataSize, 0,(struct sockaddr *)&their_addr, &addr_len)) == -1) 
			{
			  perror("recvfrom error");
			}
			for(k=1;k<size;k++) // 2048
			{
				DecodedData[playingCounter][k-1]=rcvdData[k];//WaveInData[recordingCounter][k];
				//	DecodedData[recordingCounter][k]=WaveInData[recordingCounter][k];
			}

			//else
				//cout<<"Bytes Received ="<<numbytesRcvd<<endl;
		//	reverseCVSD(inData, dwLength, DecodedData[playingCounter], &DecodedLength[playingCounter]);
            
			waveOutWrite(OutHandle, &WaveOutHeader[playingCounter], sizeof(WAVEHDR));

			playingCounter=playingCounter+1;
			if(playingCounter==NUM_OF_BUFS) 
			{
//				cout<<"playing"<<endl;
				playingCounter=0;
			}
	  }

	Terminate_RCVRthread=FALSE;
	printf("\n\t Rx thread exitted");

	ExitThread(0);

	return;

}



void main()
{
	int j=0,i;
    WSADATA wsa;

	if(WSAStartup(MAKEWORD(2,2), &wsa)!=0)
	{
		cout<<"WSA startup Error"<<endl;
		getchar();
		exit(0);
	}

    //============================== Initialising Sender Socket =================================//	
	if ((he=gethostbyname(dest_IP)) == NULL)
	{ // get the host info
		cout<<"gethostbyname error"<<endl;
		exit(0);
	}	
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) 
	{
		cout<<"socket Open Erroer"<<endl;
		exit(0);
	}
	their_addr.sin_family = AF_INET; // host byte order
	their_addr.sin_port = htons(MYPORT); // short, network byte order
	their_addr.sin_addr = *((struct in_addr *)he->h_addr);
	memset(&(their_addr.sin_zero), '\0', 8); // zero the rest of the struct

	//===========================================================================================//

    //======================== Initialising Receive Socket ======================================//
	my_addr.sin_family = AF_INET; // host byte order
	my_addr.sin_port = htons(MYPORT); // short, network byte order
	my_addr.sin_addr.s_addr = INADDR_ANY; // automatically fill with my IP
	memset(&(my_addr.sin_zero), '\0', 8); // zero the rest of the struct
	if (bind(sockfd, (struct sockaddr *)&my_addr,sizeof(struct sockaddr)) == -1) 
	{
		perror("bind");
		getchar();
		exit(1);
	}

	DWORD dwTid,dwTid1,dwTid2;
	HANDLE wavThread,wavThread1,wavThread2;

	wavThread=CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)waveInProc,NULL,0,&dwTid);
	CloseHandle(wavThread);  
	
	wavThread1=CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)Receiver,NULL,0,&dwTid1);
	CloseHandle(wavThread1);  
	
	/*---------------------------- Initialize format parameters -----------------------------------*/

	WaveFormat.wFormatTag      = WAVE_FORMAT_PCM;
	WaveFormat.nChannels       = 1; 
	WaveFormat.nSamplesPerSec  = 8000;
	WaveFormat.wBitsPerSample  = 8;
	WaveFormat.nAvgBytesPerSec = 8000; // nsamplespersec *nblockalign
	WaveFormat.nBlockAlign     = 1; // nchannels * wbitspersample/8;
	WaveFormat.cbSize          = 0;

	/*---------------------------- Alocate memory to buffers -----------------------------------*/
	BufferSize =(int)(0.016 * 8000); // recording till size for 16 milisecond

	for(i=0;i<NUM_OF_BUFS;i++)
	{
		WaveInData[i] = new char[BufferSize];
		DecodedData[i] = new char[BufferSize];

		DecodedLength[i]=0;
	}
		outData= new char[BufferSize];	

	/*---------------------------- Five Wave Headers to hold each Buffers -----------------------------------*/		

    for(i=0;i<NUM_OF_BUFS;i++)
	{
		WaveInHeader[i].dwBufferLength =	BufferSize;
		WaveInHeader[i].dwFlags        =	0;
		WaveInHeader[i].lpData         =	WaveInData[i];
	
		
		WaveOutHeader[i].dwBufferLength =	BufferSize;//DecodedLength[i];   should be proper lenght sothat memroy could be reserved for the header
		WaveOutHeader[i].dwFlags        =	0 ;
		WaveOutHeader[i].lpData         =	DecodedData[i];
	}

	/*---------------------------- Query and Then Open Wave Audio Device using waveinopen------------------------------*/
		int Res = waveInOpen(&WaveHandle,WAVE_MAPPER, &WaveFormat,(DWORD)dwTid, 0, WAVE_FORMAT_QUERY);
			if (Res == WAVERR_BADFORMAT){printf("\n\t Wave in Query error"); return;}
		Res = waveInOpen(&WaveHandle, WAVE_MAPPER, &WaveFormat,(DWORD)dwTid, NULL, CALLBACK_THREAD);
			if (Res !=MMSYSERR_NOERROR ) {printf("\n\t Wave in open error"); return;}

	/*---------------------------- Query and Open OutPut Device -----------------------------------*/

		Res = waveOutOpen(&OutHandle,WAVE_MAPPER, &WaveFormat, 0, 0, WAVE_FORMAT_QUERY);   //(DWORD)dwTid2
			if (Res == WAVERR_BADFORMAT){printf("\n\t Wave out Query error\n"); return;}
		Res = waveOutOpen(&OutHandle, WAVE_MAPPER, &WaveFormat,0, 0, CALLBACK_NULL);     //(DWORD)dwTid1
			if (Res !=MMSYSERR_NOERROR ) {printf("\n\t Wave out open error\n"); return;}
	
	/*---------------------------- Prepare Headers and Then Add all five buffers to Audio device---------------------------*/
	for(i=0;i<NUM_OF_BUFS;i++)
	{
		Res = waveInPrepareHeader(WaveHandle, &WaveInHeader[i], sizeof(WAVEHDR));
			if (Res !=MMSYSERR_NOERROR ) {printf("\n\t Wave in prepare header error"); return;}


		Res = waveInAddBuffer(WaveHandle, &WaveInHeader[i], sizeof(WAVEHDR));
			if (Res !=MMSYSERR_NOERROR ) {printf("\n\t Wave in Add header error"); return;}
		
		Res = waveOutPrepareHeader(OutHandle, &WaveOutHeader[i], sizeof(WAVEHDR));
			if (Res !=MMSYSERR_NOERROR ){printf("\n\t Wave out prepare header error\n"); return;}
	}
	
	recordCounter=0;
   
//	printf ("Press any key to start recording...\n");
//   getch();
    printf ("Recording now\n");

	/*---------------------------- Start Recording -----------------------------------*/
	Res = waveInStart(WaveHandle);
		if (Res !=MMSYSERR_NOERROR ) {printf("\n\t Wave in start error"); return;}

	
	printf("\nPress Any Key To Stop\n");
	/*---------------------------- Record Until a key is pressed by user -----------------------------------*/

	while(!kbhit()) 
	{	  
		Sleep(0.1);	
	}
	
		
	/*---------------------------- Free Buffered Memory -----------------------------------*/
	 
	  waveInStop(WaveHandle);
	  waveInReset(WaveHandle);
	 
	  /*------------------- Exit callback thread first via setting the appropriate flag value-------*/
	   
	   Terminate_RCVRthread=TRUE;
	   while(Terminate_RCVRthread==TRUE) Sleep(2);

	   printf("\n\t Going to kill wave in proc");
	   Ext_Thread=TRUE;
	   while(Ext_Thread==TRUE) Sleep(2);
	  /*---------------------------- reset and Close out device -----------------------------------*/
		waveOutReset(OutHandle);
		for (i=0;i<NUM_OF_BUFS;i++)
		{
			waveOutUnprepareHeader(OutHandle,&WaveOutHeader[i], sizeof(WAVEHDR));
		}

		waveOutClose(OutHandle);

	  for (i=0;i<NUM_OF_BUFS;i++)
	  {
		waveInUnprepareHeader(WaveHandle,&WaveInHeader[i],sizeof(WAVEHDR));
		WaveInData[i]=0;
		delete WaveInData[i];
		delete DecodedData[i];
	
	  }
	  	  
	  waveInClose(WaveHandle);
	   delete outData;

	/*---------------------------- Close Wave Input Device -----------------------------------*/
	Sleep(2);
	printf("\n\t terminating main");

	//======================== Closing sender Socket ======================================================
	closesocket(sockfd);
	WSACleanup();

	return;
 }



