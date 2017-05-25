/************************************************************************

  Demo access to TimeHarp 260 Hardware via TH260LIB v.3.1
  The program performs a measurement based on hardcoded settings.
  The resulting event data is stored in a binary output file.

  Michael Wahl, PicoQuant GmbH, March 2017

  Note: This is a console application (i.e. run in Windows cmd box)

  Note: At the API level the input channel numbers are indexed 0..N-1 
		where N is the number of input channels the device has.

  Note: This demo writes only raw event data to the output file.
		It does not write a file header as regular .ht* files have it. 


  Tested with the following compilers:

  - MinGW 2.0.0-3 (free compiler for Win 32 bit)
  - MS Visual C++ 6.0 (Win 32 bit)
  - MS Visual Studio 2010 (Win 64 bit)
  - Borland C++ 5.3 (Win 32 bit)

************************************************************************/


#ifdef _WIN32
#include <windows.h>
#include <dos.h>
#include <conio.h>

#else
#include <unistd.h>
#define Sleep(msec) usleep(msec*1000)
#endif


#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "th260defin.h"
#include "th260lib.h"
#include "errorcodes.h"



#ifndef __cplusplus
typedef unsigned char bool;
static const bool False = 0;
static const bool True = 1;
#endif

bool IsT2 = 0;
long long RecNum = 0;
int m, c;
double GlobRes = 1e-9;
unsigned int dlen = 0;
char output_filename[3][50];
__int64 time = 0; //__int64 is an interger type with 64 bits
unsigned int cnt_0 = 0, cnt_1 = 0, markers; // markers = external event marker
unsigned __int64 n, truensync = 0, t_index, t_index2, oflcorrection = 0, sync_0 = 0, sync_1 = 0, sync_2 = 0, q;
int result, j, k, g, r, dtime_0, dtime_1, index, index2, x = 0, y = 0, sub_chan, count_in_window = 0,
diff[7], p[7],
count[6] = { 0 }, count_g2[200] = { 0 },// define integer arrays with intialised values of zero for each element
count_result[4][1000] = { 0 },
count_ch2[60000] = { 0 },
count_ch1[4][60000] = { 0 },
coincidences[10][500] = { 0 },
counter[10] = { 0 },
max_ct = 0;//all these are integer type...
double judge, d[7];
FILE *foutput[3];




#define LAST(k,n) ((k) & ((1<<(n))-1))
#define MID(k,m,n) LAST((k)>>(m),((n)-(m)))


int main(int argc, char* argv[])
{
char name[50], temp[50] = "start ";
unsigned int buffer[TTREADMAX];
 int dev[MAXDEVNUM]; 
 int found=0;
 FILE *fpout;   
 int retcode;
 int ctcstatus;
 char LIB_Version[8];
 char HW_Model[16];
 char HW_Partno[8];
 char HW_Serial[8];
 char HW_Version[16];
 char Errorstring[40];
 int NumChannels;
 int Mode=MODE_T3; //set T2 or T3 here, observe suitable Sync divider and Range!
 int Binning=0; //you can change this, meaningful only in T3 mode
 int Offset=0;  //you can change this, meaningful only in T3 mode
 int Tacq=1000; //Measurement time in millisec, you can change this
 int SyncDivider = 4; //you can change this, observe Mode! READ MANUAL!

 //These settings will apply for TimeHarp 260 P boards
 int SyncCFDZeroCross=0; //you can change this
 int SyncCFDLevel=-9; //you can change this -9,-17
 int InputCFDZeroCross=-10; //you can change this
 int InputCFDLevel=-500; //you can change this

 //These settings will apply for TimeHarp 260 N boards
 int SyncTiggerEdge=0; //you can change this
 int SyncTriggerLevel=-50; //you can change this
 int InputTriggerEdge=0; //you can change this
 int InputTriggerLevel=-50; //you can change this

 double Resolution; 
 int Syncrate;
 int Countrate;
 int i;
 int flags;
 int warnings;
 char warningstext[16384]; //must have 16384 bytest text buffer
 int nRecords;
 unsigned int Progress;
 printf("\nSeparator");

 const int T3WRAPAROUND = 1024; //2^10 see p.68

 printf("\nTimeHarp 260 TH260Lib.DLL Demo Application   M. Wahl, PicoQuant GmbH, 2017");
 printf("\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
 TH260_GetLibraryVersion(LIB_Version);
 printf("\nLibrary version is %s\n",LIB_Version);
 if(strncmp(LIB_Version,LIB_VERSION,sizeof(LIB_VERSION))!=0)
         printf("\nWarning: The application was built for version %s.",LIB_VERSION);

 if((fpout=fopen("tttrmode.out","wb"))==NULL)
 {
        printf("\ncannot open output file\n"); 
        goto ex;
 }


 printf("\nSearching for TimeHarp 260 devices...");
 printf("\nDevidx     Serial     Status");


 for(i=0;i<MAXDEVNUM;i++)
 {
	retcode = TH260_OpenDevice(i, HW_Serial); 
	if(retcode==0) //Grab any device we can open
	{
		printf("\n  %1d        %7s    open ok", i, HW_Serial);
		dev[found]=i; //keep index to devices we want to use
		found++;
	}
	else
	{
		if(retcode==TH260_ERROR_DEVICE_OPEN_FAIL)
			printf("\n  %1d        %7s    no device", i, HW_Serial);
		else 
		{
			TH260_GetErrorString(Errorstring, retcode);
			printf("\n  %1d        %7s    %s", i, HW_Serial, Errorstring);
		}
	}
 }

 //In this demo we will use the first device we find, i.e. dev[0].
 //You can also use multiple devices in parallel.
 //You can also check for specific serial numbers, so that you always know 
 //which physical device you are talking to.

 if(found<1)
 {
	printf("\nNo device available.");
	goto ex; 
 }
 printf("\nUsing device #%1d",dev[0]);
 printf("\nInitializing the device...");


 retcode = TH260_Initialize(dev[0],Mode);
 if(retcode<0)
 {
        TH260_GetErrorString(Errorstring, retcode);
        printf("\nTH260_Initialize error %d (%s). Aborted.\n",retcode,Errorstring);
        goto ex;
 }

 retcode = TH260_GetHardwareInfo(dev[0],HW_Model,HW_Partno,HW_Version); 
 if(retcode<0)
 {
        TH260_GetErrorString(Errorstring, retcode);
        printf("\nTH260_GetHardwareInfo error %d (%s). Aborted.\n",retcode,Errorstring);
        goto ex;
 }
 else
	printf("\nFound Model %s Part no %s Version %s",HW_Model, HW_Partno, HW_Version);


 retcode = TH260_GetNumOfInputChannels(dev[0],&NumChannels); 
 if(retcode<0)
 {
        TH260_GetErrorString(Errorstring, retcode);
        printf("\nTH260_GetNumOfInputChannels error %d (%s). Aborted.\n",retcode,Errorstring);
        goto ex;
 }
 else
	printf("\nDevice has %i input channels.",NumChannels);



 printf("\n\nUsing the following settings:\n");

 printf("Mode              : %ld\n",Mode);
 printf("Binning           : %ld\n",Binning);
 printf("Offset            : %ld\n",Offset);
 printf("AcquisitionTime   : %ld\n",Tacq);
 printf("SyncDivider       : %ld\n",SyncDivider);

 if(strcmp(HW_Model,"TimeHarp 260 P")==0)
 {
	 printf("SyncCFDZeroCross  : %ld\n",SyncCFDZeroCross);
	 printf("SyncCFDLevel      : %ld\n",SyncCFDLevel);
	 printf("InputCFDZeroCross : %ld\n",InputCFDZeroCross);
	 printf("InputCFDLevel     : %ld\n",InputCFDLevel);
 }
 else if(strcmp(HW_Model,"TimeHarp 260 N")==0)
 {
	 printf("SyncTiggerEdge    : %ld\n",SyncTiggerEdge);
	 printf("SyncTriggerLevel  : %ld\n",SyncTriggerLevel);
	 printf("InputTriggerEdge  : %ld\n",InputTriggerEdge);
	 printf("InputTriggerLevel : %ld\n",InputTriggerLevel);
 }
 else
 {
      printf("\nUnknown hardware model %s. Aborted.\n",HW_Model);
      goto ex;
 }


 retcode = TH260_SetSyncDiv(dev[0],SyncDivider);
 if(retcode<0)
 {
        TH260_GetErrorString(Errorstring, retcode);
        printf("\nPH_SetSyncDiv error %d (%s). Aborted.\n",retcode,Errorstring);
        goto ex;
 }

 if(strcmp(HW_Model,"TimeHarp 260 P")==0)  //Picosecond resolving board
 {
	 retcode=TH260_SetSyncCFD(dev[0],SyncCFDLevel,SyncCFDZeroCross);
	 if(retcode<0)
	 {
			TH260_GetErrorString(Errorstring, retcode);
			printf("\nTH260_SetSyncCFD error %d (%s). Aborted.\n",retcode,Errorstring);
			goto ex;
	 }

	 for(i=0;i<NumChannels;i++) // we use the same input settings for all channels
	 {
		 retcode=TH260_SetInputCFD(dev[0],i,InputCFDLevel,InputCFDZeroCross);
		 if(retcode<0)
		 {
				TH260_GetErrorString(Errorstring, retcode);
				printf("\nTH260_SetInputCFD error %d (%s). Aborted.\n",retcode,Errorstring);
				goto ex;
		 }
	 }
 }

 if(strcmp(HW_Model,"TimeHarp 260 N")==0)  //Nanosecond resolving board
 {
	 retcode=TH260_SetSyncEdgeTrg(dev[0],SyncTriggerLevel,SyncTiggerEdge);
	 if(retcode<0)
	 {
			TH260_GetErrorString(Errorstring, retcode);
			printf("\nTH260_SetSyncEdgeTrg error %d (%s). Aborted.\n",retcode,Errorstring);
			goto ex;
	 }

	 for(i=0;i<NumChannels;i++) // we use the same input settings for all channels
	 {
		 retcode=TH260_SetInputEdgeTrg(dev[0],i,InputTriggerLevel,InputTriggerEdge);
		 if(retcode<0)
		 {
				TH260_GetErrorString(Errorstring, retcode);
				printf("\nTH260_SetInputEdgeTrg error %d (%s). Aborted.\n",retcode,Errorstring);
				goto ex;
		 }
	 }
 }

 retcode = TH260_SetSyncChannelOffset(dev[0],0);
 if(retcode<0)
 {
        TH260_GetErrorString(Errorstring, retcode);
        printf("\nTH260_SetSyncChannelOffset error %d (%s). Aborted.\n",retcode,Errorstring);
        goto ex;
 }

 for(i=0;i<NumChannels;i++) // we use the same input offset for all channels
 {
	 retcode = TH260_SetInputChannelOffset(dev[0],i,0);
	 if(retcode<0)
	 {
			TH260_GetErrorString(Errorstring, retcode);
			printf("\nTH260_SetInputChannelOffset error %d (%s). Aborted.\n",retcode,Errorstring);
			goto ex;
	 }
 }

 retcode = TH260_SetBinning(dev[0],Binning);
 if(retcode<0)
 {
        TH260_GetErrorString(Errorstring, retcode);
        printf("\nTH260_SetBinning error %d (%s). Aborted.\n",retcode,Errorstring);
        goto ex;
 }

 retcode = TH260_SetOffset(dev[0],Offset);
 if(retcode<0)
 {
        TH260_GetErrorString(Errorstring, retcode);
        printf("\nTH260_SetOffset error %d (%s). Aborted.\n",retcode,Errorstring);
        goto ex;
 }
 
 retcode = TH260_GetResolution(dev[0], &Resolution);
 if(retcode<0)
 {
        TH260_GetErrorString(Errorstring, retcode);
        printf("\nTH260_GetResolution error %d (%s). Aborted.\n",retcode,Errorstring);
        goto ex;
 }

 printf("\nResolution is %1.0lfps\n", Resolution);


 printf("\nMeasuring input rates...\n");


 // After Init allow 150 ms for valid  count rate readings
 // Subsequently you get new values after every 100ms
 Sleep(150);

 retcode = TH260_GetSyncRate(dev[0], &Syncrate);
 if(retcode<0)
 {
        TH260_GetErrorString(Errorstring, retcode);
        printf("\nTH260_GetSyncRate error%d (%s). Aborted.\n",retcode,Errorstring);
        goto ex;
 }
 printf("\nSyncrate=%1d/s", Syncrate);


 for(i=0;i<NumChannels;i++) // for all channels
 {
	 retcode = TH260_GetCountRate(dev[0],i,&Countrate);
	 if(retcode<0)
	 {
			TH260_GetErrorString(Errorstring, retcode);
			printf("\nTH260_GetCountRate error %d (%s). Aborted.\n",retcode,Errorstring);
			goto ex;
	 }
	printf("\nCountrate[%1d]=%1d/s", i, Countrate);
 }

 printf("\n");

 //after getting the count rates you can check for warnings
 retcode = TH260_GetWarnings(dev[0],&warnings);
 if(retcode<0)
 {
        TH260_GetErrorString(Errorstring, retcode);
	printf("\nTH260_GetWarnings error %d (%s). Aborted.\n",retcode,Errorstring);
	goto ex;
 }
 if(warnings)
 {
	 TH260_GetWarningsText(dev[0],warningstext, warnings);
	 printf("\n\n%s",warningstext);
 }


 printf("\npress RETURN to start");
 getchar();
 
 printf("\nStarting data collection...\n");

 Progress = 0;
 printf("\nProgress:%12u \n",Progress);

 retcode = TH260_StartMeas(dev[0],Tacq); 
 if(retcode<0)
 {
         TH260_GetErrorString(Errorstring, retcode);
         printf("\nTH260_StartMeas error %d (%s). Aborted.\n",retcode,Errorstring);
         goto ex;
 }

 while(1)
 { 	 
        retcode = TH260_GetFlags(dev[0], &flags);
        if(retcode<0)
        {
		TH260_GetErrorString(Errorstring, retcode);
                printf("\nTH260_GetFlags error %d (%s). Aborted.\n",retcode,Errorstring);
                goto ex;
        }
        
		if (flags&FLAG_FIFOFULL) 
		{
			printf("\nFiFo Overrun!\n"); 
			goto stoptttr;
		}
		
		retcode = TH260_ReadFiFo(dev[0],buffer,TTREADMAX,&nRecords);	//may return less!  
		if(retcode<0) 
		{ 
			TH260_GetErrorString(Errorstring, retcode);
			
			printf("\nTH260_ReadFiFo error %d (%s). Aborted.\n",retcode,Errorstring);
			goto stoptttr; 
		}  

		if(nRecords)
		{
			for (RecNum = 0; RecNum < nRecords; RecNum++){
				if (MID(buffer[RecNum], 31, 32) == 1) //special bit
				{
					

					if ((MID(buffer[RecNum], 25, 31)) == 0x3F) //overflow
					{
					//number of overflows is stored in nsync
						oflcorrection += (unsigned __int64)T3WRAPAROUND * MID(buffer[RecNum], 0, 10);
					//The real correction happen here!
					}

					if ((MID(buffer[RecNum], 25, 31) >= 1) && (MID(buffer[RecNum], 25, 31)) <= 15) //markers //no use here.
					{
						truensync = oflcorrection + MID(buffer[RecNum], 0, 10);
					//the time unit depends on sync period which can be obtained from the file header
					}
					
				}
		
				else //regular input channel
				{
					truensync = oflcorrection + MID(buffer[RecNum], 0, 10);

					if (MID(buffer[RecNum], 25, 31) == 0) // Channel 1: First channel
					{
						cnt_0++;// then we increase the count by 1, we are still inside for loop
						
						index = floor(MID(buffer[RecNum], 10, 25));  // position in period with resolution bin according to our settings
						count_result[0][index]++;
						sync_0 = oflcorrection + MID(buffer[RecNum], 0, 10);  // absolute number of periods (int64), this is number of sync periods, Huge (MHZ) in 1s there is 70 Million periods, 10s = 700 *10^6
						double period=1/(double)Syncrate;
						t_index = (sync_0*1e-6*1e-6*(52e3));  //corresponding index given step 
																			 
						count_ch1[0][t_index]++; //count_ch1 is for TTT3
																		

					}
					if (MID(buffer[RecNum], 25, 31) == 0) // Channel 2: Second Input Channel
					{
						cnt_1++;
						index = floor(MID(buffer[RecNum], 10, 25));
						count_result[1][index]++;
						sync_0 = oflcorrection + MID(buffer[RecNum], 0, 10);  //absolute number of periods (int64)
						t_index = (sync_0*1e-6*52e3*1e-6);  //corresponding index given step 
																  
						count_ch2[t_index]++;              // here we add the event to the time trace
					}

				}
			}
		}

		else
		{
			retcode = TH260_CTCStatus(dev[0], &ctcstatus);
			if(retcode<0)
			{
		TH260_GetErrorString(Errorstring, retcode);
                printf("\nTH260_CTCStatus error %d (%s). Aborted.\n",retcode,Errorstring);
                goto ex;
			}
			if (ctcstatus) 
			{ 
				printf("\nDone\n"); 
				break;
			}  
		}

 }

printf("while loop ended here");
int nb_bins;
for (i = 0; i<3; i++)
	foutput[i] = fopen(output_filename[i], "w");

 nb_bins = (int)floor(1 / Resolution * (double)Syncrate);

 for (j = 0; j<nb_bins; j++)   // here we write the counts for the histograms
 {
	 fprintf(foutput[0], "%d ", count_result[0][j]);
 }

 fprintf(foutput[0], "\n");

 for (k = 0; k<3; k++)  // here we write the counts for the histograms for each subset
						//   // here we write the counts for the time traces subsets
 {
	 for (j = 0; j<nb_bins; j++)
	 {
		 fprintf(foutput[0], "%d ", count_result[k][j]);
	 }
	 fprintf(foutput[0], "\n");
 }

 for (k = 0; k<4; k++)  // here we write the counts for the time traces  
 {
	 for (j = 0; j<Tacq; j++)
	 {
		 fprintf(foutput[1], "%d ", count_ch1[k][j]);
	 }
	 fprintf(foutput[1], "\n");
 }
 for (j = 0; j<Tacq; j++)
 {
	 fprintf(foutput[2], "%d ", count_ch2[j]);
 }
 fprintf(foutput[2], "\n");


 //output filename
 for (i = 0; i<3; i++)
	 strcpy(output_filename[i], name);
 strcat(output_filename[0], "_histogram.txt");
 strcat(output_filename[1], "_ch1.txt");
 strcat(output_filename[2], "_ch2.txt");

printf("CHECKNOW");
 for (i = 0; i<3; i++)
	 foutput[i] = fopen(output_filename[i], "w");
 //output header(x-axis)
 //d=resol[0]/1E-12; n=(int)d;
 // d=resol[1]/1E-12; n=(int)d;
 for (j = 0; j<nb_bins; j++)
	 fprintf(foutput[0], "%d ", j*(int)(Resolution / 1e-12));    // here we write the t axis for the histograms


 for (j = 0; j<Tacq; j++)
	 fprintf(foutput[1], "%d ", j);    // here we write the t axis for the time traces 1
 for (j = 0; j<Tacq; j++)
	 fprintf(foutput[2], "%d ", j);    // here we write the t axis for the time traces 2

 for (i = 0; i<3; i++)
	 fprintf(foutput[i], "\n");

 for (i = 0; i<3; i++)
 {
	 fclose(foutput[i]);
	 //auto-launch output file
	 strcpy(name, temp);
	 strcat(name, output_filename[i]);
	 //system(name);
 }
 printf("\n Task Completed");
 goto stoptttr;

stoptttr:

 retcode = TH260_StopMeas(dev[0]);
 if (retcode<0)
 {
	 TH260_GetErrorString(Errorstring, retcode);
	 printf("\nTH260_StopMeas error %d (%s). Aborted.\n", retcode, Errorstring);
	 goto ex;
 }

ex:

 for (i = 0; i<MAXDEVNUM; i++) //no harm to close all
 {
	 TH260_CloseDevice(i);
 }
 if (fpout) fclose(fpout);
 printf("\npress RETURN to exit");
 getchar();

 return 0;


}


