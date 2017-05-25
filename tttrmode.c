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


#ifdef _WIN32//compiler
#include <windows.h>
#include <dos.h>
#include <conio.h>
#else
#include <unistd.h>
#define Sleep(msec) usleep(msec*1000)
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h> //added

#include "th260defin.h"
#include "th260lib.h"
#include "errorcodes.h"


#if _MSC_VER >= 1400 //need this with MSVC2005 and higher
#define ctime _ctime32 //clock time (fetch date and time)
#endif


#define DISPCURVES 8	// not really relevant in TT modes but needed in file header definition
#define MAXINPCHANS 8 // maximum input channels = maximum display curves !
#define T3WRAPAROUND 1024 //? Relate to overflow correction


#define INPTYPE_CFD 1 //??
#define INPTYPE_LVLTRG 2
#pragma pack(8) //structure alignment to 8 byte boundaries (32 bits system) only apply to structure access


/*
The following structures are used to hold the file data.
Mostly they directly reflect the file structure.
The data types used here to match the file structure are correct
for the tested compilers.
They may have to be changed for other compilers.
*/



// These are substructures used further below (added)

typedef struct {
	float Start;
	float Step;
	float End;
} tParamStruct;

typedef struct {
	int MapTo;
	int Show;
} tCurveMapping;


typedef union {
	DWORD allbits;
	struct {
		unsigned nsync : 10; 	// number of sync period
		unsigned dtime : 15;    // delay from last sync in units of chosen resolution 
		unsigned channel : 6;
		unsigned special : 1;
	} bits;
} tT3Rec;

// Each T3 mode event record consists of 32 bits. 6 bits for the channel number, 15 bits for the start-stop time, and 10 bits for the sync counter.
//2^15 for the time tag
// The following represents the readable ASCII file header portion. 

struct {
	char Ident[16];				//"HydraHarp"
	char FormatVersion[6];		//file format version
	char CreatorName[18];		//name of creating software
	char CreatorVersion[12];	//version of creating software
	char FileTime[18];
	char CRLF[2];
	char CommentField[256];
} TxtHdr; //p.61/74

		  // The following is binary file header information indentical to that in HHD files.
		  // Note that some items are not meaningful in the time tagging modes.

struct {
	int Curves;              // Number of curves or histograms stored
	int BitsPerRecord;		// number of bits to represent one Histogram bin
	int ActiveCurve;        // the curve number used for last measurement
	int MeasMode;           // interactive mode 0, reserved 1, T2 = 2, T3 = 3 
	int SubMode;            // 0=oscilloscope, 1=integration, 2=TRES
	int Binning;			// 0 = base resolution, 1=x2, 2=x4, 3=x8, 4=x16, 10=x1024
	double Resolution;		// in ps (as resulting from base resolution and binning)
	int Offset;				// offset in ns
	int Tacq;				// acquisition time in ms
	unsigned int StopAt;    // stop at this many counts
	int StopOnOvfl;         // stop if overflow occured =1, otherwise 0
	int Restart;            // restart =1, otherwise 0
	int DispLinLog;         // lin=0, log=1
	unsigned int DispTimeFrom;		// lower time axis bound of display in ns
	unsigned int DispTimeTo;        // upper time axis bound of display in ns
	unsigned int DispCountsFrom;    // lower count axis bound of display
	unsigned int DispCountsTo;      // upper count axis bound of display
	tCurveMapping DispCurves[DISPCURVES];	//declaring a type of structure (e.g. int), int array. 
	tParamStruct Params[3]; // Here you have Array type of param structure ! similar for curve mapping
	int RepeatMode; // Reserve for automated measurements (Automatisation)
	int RepeatsPerCurve;  // Reserve for automated measurements (Automatisation)
	int RepeatTime;  // Reserve for automated measurements (Automatisation)
	int RepeatWaitTime;  // Reserve for automated measurements (Automatisation)
	char ScriptName[20];
} BinHdr;  // Reserve for automated measurements (Automatisation)

		   // The next is a header carrying hardware information

struct {
	char HardwareIdent[16];   // currently TimeHarp 260
	char HardwarePartNo[8];  // PicoQuant part number of the hardware device
	char HardwareVersion[16]; // hardware version string
	int  HardwareSerial; // hardware serial number
	int FeatureSet;      //a bitfield to identify features of the hardware
	double BaseResolution; // base resolution in ps
	int TimingMode; // 0 = hires, 1 = lowres (high response and low response)?
	int InputsEnabled;   //a bitfield to identify the enabled inputs
	int InpChansPresent; //this determines the number of ChannelHeaders below!
	int ExtDevices;      //a bitfield (external devices)
	int MarkerSettings;  //a bitfield (to identify marker enabling and edges)
	int TriggerOutPeriod; // Tigger output pulse period in ns
	int TriggerOutOn; // Trigger Output enabled (0=no, 1=yes)
	int SyncDivider; //divider for Sync Input
	int SyncInputType;   // 0=CFD, 1=Level Trigger (0 is pico version, 1 is for nano)
	int SyncCFDLevel; // Sync input discriminator level in mV
	int SyncCFDZeroCross; //Sync input CFD zero cross level in mV or edge rising / falling
	int SyncOffset;
} MainHardwareHdr; //Sync input timing offset in ps



//How many of the following array elements are actually present in the file 
//is indicated by InpChansPresent above. Here we allocate the possible maximum.

struct {
	int InputType;       // CFD or Leveltrigger (0=CFD, 1=Level Trigger)
	int InputCFDLevel; // input CFD discriminator level / trigger level in mV
	int InputCFDZeroCross;  // input CFD zero crossing level in mV or edge rising/falling
	int InputOffset; // input timing offset in ps
} InputChannelSettings[MAXINPCHANS];

// The following block will appear in the file for each i=1 to InputChannelsPresent

// Up to here the header was identical to that of HHD files.
//The following header sections are specific for the TT modes 

//How many of the following array elements are actually present in the file 
//is indicated by InpChansPresent above. Here we allocate the possible maximum.

int InputRate[MAXINPCHANS]; // input count rate as displayed by rate meter
//the following exists only once				
//Following headers are T2,T3 mode specific
struct {
	int SyncRate; // sync count rate as displayed by rate meter
	int StopAfter; // stopped after this many ms
	int StopReason; // 0=timeover, 1=manual, 2=overflow, 3=error
	int ImgHdrSize; // size of special header to follow before the records start, Img=Imaging
	unsigned __int64 nRecords;
} TTTRHdr; // number of T2 event records, Hdr = Header

//how many of the following ImgHdr array elements are actually present in the file 
//is indicated by ImgHdrSize above. 
//Storage must be allocated dynamically if ImgHdrSize other than 0 is found.
//int ImgHdr[ImgHdrSize];

//The headers end after ImgHdr. Following in the file are only event records. 
//How many of them actually are in the file is indicated by nRecords in TTTRHdr above.


//global variable for input_para.txt
int peak, offset, scale, divisor, mfinal[3],
min_avg, max_avg, max_offset, period_int, nb_bins,
clock_period, interval, periods, maxtime, deltat, tmax, nb_bins_time, t_offset;
double t_small[6], t_big[6],
shift[2], shift_int[2],
resol[2], resol_int[2], period;
char output_filename[11][50];
FILE *foutput[11]; // define memory space 11 pointers for files 
long double laser_period, pattern_period;
////////////////////////////

double round(double val) // round up function
{//round function for VS (lack of c99)    
	return floor(val + 0.5);
}
double mod(int dividend, double divisor) // define modulus function
{
	//divident % divisor (modified for floating point values)
	int quotient;

	quotient = (int)dividend / divisor;
	return dividend - (double)quotient*divisor;
}


unsigned int buffer[TTREADMAX];


int main(int argc, char* argv[])
{

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
 int Mode=MODE_T2; //set T2 or T3 here, observe suitable Sync divider and Range!
 int Binning=0; //you can change this, meaningful only in T3 mode
 int Offset=0;  //you can change this, meaningful only in T3 mode
 int Tacq=10000; //Measurement time in millisec, you can change this
 int SyncDivider = 1; //you can change this, observe Mode! READ MANUAL!

 //These settings will apply for TimeHarp 260 P boards
 int SyncCFDZeroCross=-10; //you can change this
 int SyncCFDLevel=-50; //you can change this
 int InputCFDZeroCross=-10; //you can change this
 int InputCFDLevel=-50; //you can change this

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
 Sleep(150);//Set values =  initialisation !

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
 printf("\nProgress:%12u",Progress);

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
			if(fwrite(buffer,4,nRecords,fpout)!=(unsigned)nRecords)
			{
				printf("\nfile write error\n");
				goto stoptttr;
			}               
				Progress += nRecords;
				printf("\b\b\b\b\b\b\b\b\b\b\b\b%12u",Progress);
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
				goto stoptttr; 
			}  
		}

		//within this loop you can also read the count rates if needed.
 }
  
stoptttr:

 retcode = TH260_StopMeas(dev[0]);
 if(retcode<0)
 {
      TH260_GetErrorString(Errorstring, retcode);
      printf("\nTH260_StopMeas error %d (%s). Aborted.\n",retcode,Errorstring);
      goto ex;
 }         
  
ex:

 for(i=0;i<MAXDEVNUM;i++) //no harm to close all
 {
	TH260_CloseDevice(i);
 }
 if(fpout) fclose(fpout);
 printf("\npress RETURN to exit");
 getchar();

 return 0;
}


