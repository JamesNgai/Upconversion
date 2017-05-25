/************************************************************************

  PicoQuant HydraHarp    File Access Demo in C

  Demo access to binary HydraHarp T3 mode data files (*.ht3)
  for file format version 2.0 only
  Read a HydraHarp data file and dump the contents in ASCII
  Michael Wahl, PicoQuant GmbH, May 2012

  Tested with the following compilers:

  - MinGW 2.1 and 3.18 (free compiler for Win 32 bit)
  - MS Visual C++ 6.0 (Win 32 bit)
  - MS Visual C++ 2005 (Win 32/64 bit)
  - MS Visual C++ 2010 (Win 32/64 bit)
  - Borland C++ 5.5 (Win 32 bit)

  It should work with most others.

  This is demo code. Use at your own risk. No warranties.
  
  
  // t_big(0)-t_small(0) maximum 100ns?

************************************************************************/


#include	<windows.h>
#include	<dos.h>
#include	<stdio.h>
#include	<conio.h>
#include	<stddef.h>
#include	<stdlib.h>


#if _MSC_VER >= 1400 //need this with MSVC2005 and higher
#define ctime _ctime32 //clock time (fetch date and time)
#endif


#define DISPCURVES 8	// not really relevant in TT modes but needed in file header definition
#define MAXINPCHANS 8 // maximum input channels = maximum display curves !
#define T3WRAPAROUND 1024 //? Relate to overflow correction


#define INPTYPE_CFD 1 //??
#define INPTYPE_LVLTRG 2

/*
The following structures are used to hold the file data.
Mostly they directly reflect the file structure.
The data types used here to match the file structure are correct
for the tested compilers.
They may have to be changed for other compilers.
*/


#pragma pack(8) //structure alignment to 8 byte boundaries

// These are substructures used further below 

typedef struct{ float Start;
                float Step;
				float End;  } tParamStruct;

typedef struct{ int MapTo;
				int Show; } tCurveMapping;


typedef union {
				DWORD allbits;
				struct	{
					unsigned nsync		:10; 	// number of sync period
					unsigned dtime		:15;    // delay from last sync in units of chosen resolution 
					unsigned channel	:6;
					unsigned special	:1;
						} bits;					} tT3Rec;

// Each T3 mode event record consists of 32 bits. 6 bits for the channel number, 15 bits for the start-stop time, and 10 bits for the sync counter.
//2^15 for the time tag
// The following represents the readable ASCII file header portion. 

struct {		char Ident[16];				//"HydraHarp"
				char FormatVersion[6];		//file format version
				char CreatorName[18];		//name of creating software
				char CreatorVersion[12];	//version of creating software
				char FileTime[18];
				char CRLF[2];
				char CommentField[256]; } TxtHdr; //p.61/74

// The following is binary file header information indentical to that in HHD files.
// Note that some items are not meaningful in the time tagging modes.

struct {		int Curves;              // Number of curves or histograms stored
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
				char ScriptName[20];	} BinHdr;  // Reserve for automated measurements (Automatisation)

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
				int SyncOffset;			} MainHardwareHdr; //Sync input timing offset in ps



//How many of the following array elements are actually present in the file 
//is indicated by InpChansPresent above. Here we allocate the possible maximum.

struct{ 
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
struct{	
				int SyncRate; // sync count rate as displayed by rate meter
				int StopAfter; // stopped after this many ms
				int StopReason; // 0=timeover, 1=manual, 2=overflow, 3=error
				int ImgHdrSize; // size of special header to follow before the records start, Img=Imaging
				unsigned __int64 nRecords;	} TTTRHdr; // number of T2 event records, Hdr = Header

//how many of the following ImgHdr array elements are actually present in the file 
//is indicated by ImgHdrSize above. 
//Storage must be allocated dynamically if ImgHdrSize other than 0 is found.
//				int ImgHdr[ImgHdrSize];

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
       
       quotient = (int)dividend/divisor;
       return dividend-(double)quotient*divisor;
}







void process(char* input_filename)

{
     
     ////////////////////////////////////Local variables for the processing..
    __int64 time=0; //__int64 is an interger type with 64 bits
    unsigned int cnt_0=0, cnt_1=0, markers; // markers = external event marker
    unsigned __int64 n, truensync=0, t_index, t_index2, oflcorrection = 0, sync_0=0,sync_1=0,sync_2=0, q;
    int result,i=0, j, k, g, r, dtime_0, dtime_1, index, index2, x=0, y=0, sub_chan, count_in_window=0,
        diff[7], p[7],
        count[6]={0}, count_g2[200]={0},// define integer arrays with intialised values of zero for each element
        count_result[4][1000]={0},
        count_ch2[60000]={0},
        count_ch1[4][60000]={0},
        coincidences[10][500]={0},
        counter[10]={0},
        max_ct=0;//all these are integer type...
    double judge, d[7];
    tT3Rec T3Rec;
   // /////////Local variables for the processing..
  
   
              
	FILE *fpin;	// file input pointer declaraction
		  
	printf("\nHydraHarp HT3 File Demo");
	printf("\n~~~~~~~~~~~~~~~~~~~~~~~");

/*error checking two*/
 if((fpin=fopen(input_filename,"rb"))==NULL) //Null means failed, and fopen returns pointer address to fpin, rb means read binary
 {
  printf("\ncannot open %s\n",input_filename);
  goto ex;
 }

////////////reading header of .ht3 file 
	result = fread( &TxtHdr, 1, sizeof(TxtHdr) ,fpin); //&  is referencing! why 1??? Reaaad 1 byte of each element? 1 byte = 4 bits.
	
	if (result!= sizeof(TxtHdr))//error handling
	{
	 printf("\nerror reading txt header, aborted.");
	 goto close;
	}

	if(  strncmp(TxtHdr.Ident,"TimeHarp 260",12)  )//error handling
	{
		printf("\nError: File Ident is %s. This program is for 'TimeHarp 260' files only.", TxtHdr.Ident);
		printf("\nError: File Ident is %s. This program is for 'TimeHarp 260' files only.\n", TxtHdr.Ident);
		goto ex;
	}

	if(  strncmp(TxtHdr.FormatVersion,"1.0",3)  )//error handling
	{
		printf("\nError: File format version is %s. This program is for v. 1.0 only.", TxtHdr.FormatVersion);
		printf("\nError: File format version is %s. This program is for v. 1.0 only.\n", TxtHdr.FormatVersion);
		goto ex;
	}
	//we are trying to open, read a file !
	result = fread( &BinHdr, 1, sizeof(BinHdr) ,fpin);
	if (result!= sizeof(BinHdr))
	{
	  printf("\nerror reading bin header, aborted.");
	  goto ex;
	}
	// Note: for formal reasons the BinHdr is identical to that of HHD files.  
	// It therefore contains some settings that are not relevant in the TT modes,
	// e.g. the curve display settings. So we do not write them out here.
	result = fread( &MainHardwareHdr, 1, sizeof(MainHardwareHdr) ,fpin);
	if (result!= sizeof(MainHardwareHdr))
	{
	  printf("\nerror reading MainHardwareHdr, aborted.");
	  goto ex;
	}
	//the following module info is needed for suppoert enquiries only
	
	//the following are important measurement settings
	for(i=0;i<MainHardwareHdr.InpChansPresent;++i)
	{
	 result = fread( &(InputChannelSettings[i]), 1, sizeof(InputChannelSettings[i]) ,fpin);
	 if (result!= sizeof(InputChannelSettings[i]))
	 {
	   printf("\nerror reading InputChannelSettings, aborted.");
	   goto close;
	 }
	}
	for(i=0;i<MainHardwareHdr.InpChansPresent;++i)
	{
	 result = fread( &(InputRate[i]), 1, sizeof(InputRate[i]) ,fpin);
	 if (result!= sizeof(InputRate[i]))
	 {
	   printf("\nerror reading InputRates, aborted.");
	   goto close;
	 }
	}

  result = fread( &TTTRHdr, 1, sizeof(TTTRHdr) ,fpin);
	if (result!= sizeof(TTTRHdr))
	{
	  printf("\nerror reading TTTRHdr, aborted.");
	  goto ex;
	}


    //For this simple demo we skip the imaging header (ImgHdr)
	//You will need to read it if you want to interpret an imaging file.
	fseek(fpin,TTTRHdr.ImgHdrSize*4,SEEK_CUR); //CUR = current position 
	//The C library function int fseek(FILE *stream, long int offset, int whence) sets the file position of the stream to the given offset.


    /* Now read and interpret the TTTR records */           
     printf("\nprocessing..\n");
     printf("\n%u\n",TTTRHdr.nRecords);

	//now read and interpret the event records
    int flag=0;
	for(n=0; n<TTTRHdr.nRecords-100; n++)
	{	if(flag==0) 
		 {result = fread(&T3Rec.allbits,sizeof(T3Rec.allbits),1,fpin);
          } 
         flag=0;
		if(result!=1)// result is originally assigned as 0
		{
			if(feof(fpin)==0)//feof end of file = 0 means file hasnt reached the end (non-zero means closed successfully)
			{
				printf("\nerror in input file! \n");
				goto ex;
			}
		}

		if(T3Rec.bits.special==1) //special bit
		{
			if(T3Rec.bits.channel==0x3F) //overflow
			{
							//number of overflows is stored in nsync
					oflcorrection += (unsigned __int64)T3WRAPAROUND * T3Rec.bits.nsync;
					//The real correction happen here!
			}
		
			if((T3Rec.bits.channel>=1)&&(T3Rec.bits.channel<=15)) //markers //no use here.
			{
				truensync = oflcorrection + T3Rec.bits.nsync; 
				//the time unit depends on sync period which can be obtained from the file header
			}

		}
		else //regular input channel
		 {
             truensync = oflcorrection + T3Rec.bits.nsync;
                          
             if(T3Rec.bits.channel==0) //0 means regular input channel
                { cnt_0++;//then we increase the count by 1, we are still inside for loop
                //x++;
                 //fprintf(foutput[3],"%d\n",truensync);
                index = floor((T3Rec.bits.dtime*BinHdr.Resolution)/resol_int[0]);  //position in period with postdefined resolution bin
                if(index<(period_int/resol_int[0]))  // to avoid wrong addressing
                    count_result[0][index]++; //single counts histogram(int array)
				// if dtime*resolution = delay time and we devide it by post defined resolution bin i.e. resol_int, you get the corresponding position inside the period
//                e.g. compare delay time  = 10 second / 1 second = 10th, then period_int = 20s, you divide by resolution of 1 sec, end index = 20.
//                dtime_0=T3Rec.bits.dtime;  //position in period (int)
//                
                  sync_0 = oflcorrection + T3Rec.bits.nsync;  //absolute number of periods (int64)
                  t_index = (sync_0*1e-6*period_int*1e-6+t_offset);  //corresponding index given step 
				  //count_ch1 is for TTT3 
                  count_ch1[0][t_index]++;   // here we add the event to the time trace
         
//                
//                sub_chan = t_index%4;  //which subset
//                time_trace_subs[sub_chan][t_index/4]++;              // here we add the event to the time trace
//                count_result[1+sub_chan][index]++; //single counts for each subset(int array)

                  count_in_window = 0;   // the detected count fits in one of the two time windows
                if(T3Rec.bits.dtime*BinHdr.Resolution<t_big[0] && T3Rec.bits.dtime*BinHdr.Resolution>t_small[0])    //time range 1
                     {
                     count_ch1[1][t_index]++;
                     count_result[1][index]++; //single counts histogram(int array)
                    // time_trace_subs2[sub_chan][t_index/4]++;    
                     sync_1 = sync_0;
                     count_in_window = 1;
                     }  
  
              
                if(T3Rec.bits.dtime*BinHdr.Resolution<t_big[1] && T3Rec.bits.dtime*BinHdr.Resolution>t_small[1])    //time range 2
                     {
                     count_ch1[2][t_index]++; 
                     count_result[2][index]++; //single counts histogram(int array)
                    // time_trace_subs3[sub_chan][t_index/4]++;   
                     sync_2 = sync_0;
                     count_in_window = 1;
                     }  
                     
                 if(T3Rec.bits.dtime*BinHdr.Resolution<t_big[2] && T3Rec.bits.dtime*BinHdr.Resolution>t_small[2])    //time range 2
                     {
                     count_ch1[3][t_index]++; 
                     count_result[3][index]++; //single counts histogram(int array)
                    // time_trace_subs3[sub_chan][t_index/4]++;   
                     }  
   
//                if(T3Rec.bits.dtime*BinHdr.Resolution<t_big[2] && T3Rec.bits.dtime*BinHdr.Resolution>t_small[2])    //time range 3  --- here only to check the time trace zoom.
//                     {
//                     t_index2 = ((sync_0*1e-6*period_int*1e-6+t_offset)*100)/deltat;  //corresponding index given step 
//                     if(t_index2 < 2000)
//                                 time_trace_zoom[t_index2]++; 
//                     }     
                if(count_in_window == 1)   // coincidences
                      {
                       q = (sync_1-sync_2)-1;//(time delay difference )
                       if(q<10 && q>=0)
                           {
                            coincidences[q][counter[q]]=t_index;
                            counter[q]++;
                            if(counter[q]>max_ct)
                                max_ct++;
                            }
                      }

//
                }  

      
                if(T3Rec.bits.channel==1) 
                { 
                                          
     //           index = floor((T3Rec.bits.dtime*BinHdr.Resolution)/resol_int[0]);  //position in period with postdefined resolution --not used here
//                dtime_0=T3Rec.bits.dtime;  //position in period (int)--not used here
                
                sync_0= oflcorrection + T3Rec.bits.nsync;  //absolute number of periods (int64)
                t_index = (sync_0*1e-6*period_int*1e-6);  //corresponding index given step 
                //t_index += t_offset/deltat;
                count_ch2[t_index]++;              // here we add the event to the time trace
                }                   
                 // printf("\n#%d;%d;\n",x,y);
			 //the nsync time unit depends on sync period which can be obtained from the file header
			 //the dtime unit depends on the resolution and can also be obtained from the file header
		}
if(sync_0*1e-6*period_int*1e-6 >= tmax)
           {
           printf("Maximum measurement time reached");
           break;
           }
    
       
}

for(j=0;j<nb_bins;j++)   // here we write the counts for the histograms
    {
    fprintf(foutput[0],"%d ",count_result[0][j]);
    }
fprintf(foutput[0],"\n");    
for(k=0;k<3;k++)  // here we write the counts for the histograms for each subset
//   // here we write the counts for the time traces subsets
    {
    for(j=0;j<nb_bins;j++)   
        {
        fprintf(foutput[0],"%d ",count_result[1+k][j]);
        }
    fprintf(foutput[0],"\n");
    }
    
for(k=0;k<4;k++)  // here we write the counts for the time traces  
    {for(j=0;j<tmax;j++)
    {
    fprintf(foutput[1],"%d ",count_ch1[k][j]);
    }
    fprintf(foutput[1],"\n");   
    }


//for(k=0;k<4;k++)     // here we write the counts for the time traces subsets
//    {
//    for(j=0;j<nb_bins_time/4;j++)
//        {
//        fprintf(foutput[2],"%lf ",(float)time_trace_subs[k][j]/(float)deltat);
//        }
//    fprintf(foutput[2],"\n");
//    }
//for(k=0;k<4;k++)     // here we write the counts for the time traces subsets in the selected windows (1/2)
//    {
//    for(j=0;j<nb_bins_time/4;j++)
//        {
//        fprintf(foutput[2],"%lf ",(float)time_trace_subs2[k][j]/(float)deltat);
//        }
//    fprintf(foutput[2],"\n");
//    }
//for(k=0;k<4;k++)     // here we write the counts for the time traces subsets in the selected windows (2/2)
//    {
//    for(j=0;j<nb_bins_time/4;j++)
//        {
//        fprintf(foutput[2],"%lf ",(float)time_trace_subs3[k][j]/(float)deltat);
//        }
//    fprintf(foutput[2],"\n");
//    }
//
//
//for(k=0;k<5;k++)  
//   // here we write the coincidences
//    {
    for(j=0;j<10;j++)
          {
          for(k=0;k<=max_ct;k++)
             fprintf(foutput[2],"%d ",coincidences[j][k]);
    fprintf(foutput[2],"\n");
    }
//
//for(j=0;j<2000;j++)
//    {
//    fprintf(foutput[4],"%d ",time_trace_zoom[j]);
//    }
//fprintf(foutput[4],"\n");
//
for(j=0;j<tmax;j++)
    {
    fprintf(foutput[3],"%d ",count_ch2[j]);
    }
fprintf(foutput[3],"\n");


       
       // total time
maxtime = truensync/1e6;
     //  fprintf(foutput[2],"%d ",maxtime);

        
for(i=3;i<4;i++)
    fprintf(foutput[i],"\n");
    
 printf("\n");



close: 
	fclose(fpin);
	printf("Appuyer sur une touche pour continuer...");
    getchar();
ex:
 return;
 excut:
 return;		
}


int main(int argc, char* argv[])
{
    FILE *finput;
    int i,j,n;
    double d;
    char name[50],temp[50]="start ";
    
    
    
    ///////////Error checking one
  if(argc!=2)
    {
        printf( "incorrect number of input\n");
        getchar();
        return 0;
    }    
    if((finput=fopen(argv[1],"r"))==NULL)
    {
        printf( "error 2\n");
        getchar();
        return 0;
    }

    //read input.txt
    fscanf(finput,"%s %lf %lf %lf %lf %lf %lf",name,&t_small[0],&t_small[1],&t_small[2],&t_small[3],&t_small[4],&t_small[5]);
    fscanf(finput,"%s %lf %lf %lf %lf %lf %lf",name,&t_big[0],&t_big[1],&t_big[2],&t_big[3],&t_big[4],&t_big[5]);
    fscanf(finput,"%s %d",name,&interval);
    fscanf(finput,"%s %d",name,&periods);
    fscanf(finput,"%s %lf %lf",name,&resol[0],&resol[1]);
    fscanf(finput,"%s %lf",name,&period);
    fscanf(finput,"%s %d",name,&deltat);
    fscanf(finput,"%s %d",name,&t_offset);
    fscanf(finput,"%s %d",name,&tmax);
    fscanf(finput,"%s %lf %lf",name,&shift[0],&shift[1]);
    fscanf(finput,"%s %d %d",name,&offset,&max_offset);
    fscanf(finput,"%s %d %d",name,&min_avg,&max_avg);
	fscanf(finput,"%s %d",name,&peak);
    fscanf(finput,"%s %s",name,name);
  printf("periods %d period %lf offset %d deltat %d ratio %lf tmax %d;\n",periods,period*1e12,t_offset,deltat,t_offset/deltat,tmax);  

        ///////////setpoint two
     
resol_int[0]=resol[0]/1E-12;
resol_int[1]=resol[1]/1E-12;
shift_int[0]=shift[0]/1E-12;
shift_int[1]=shift[1]/1E-12;
period_int=period/1E-12;
nb_bins = (int) floor(period_int/resol_int[0]);

    //output filename
    for(i=0;i<4;i++)
        strcpy(output_filename[i],name);    
    strcat(output_filename[0],"_histogram.txt");
    strcat(output_filename[1],"_time_trace.txt");
    strcat(output_filename[2],"_coincidences.txt");
    strcat(output_filename[3],"_ch2.txt");

    for(i=0;i<4;i++)
        foutput[i]=fopen(output_filename[i],"w");
    //output header(x-axis)
    //d=resol[0]/1E-12; n=(int)d;
   // d=resol[1]/1E-12; n=(int)d;
    for(j=0;j<nb_bins;j++)
            fprintf(foutput[0],"%d ",j*(int)resol_int[0]);    // here we write the t axis for the histograms


    nb_bins_time = tmax;
    for(j=0;j<nb_bins_time;j++)
            fprintf(foutput[1],"%d ",j);    // here we write the t axis for the time traces
            
/*
    nb_bins_time = tmax/deltat;
    for(j=0;j<nb_bins_time/4;j++)
            fprintf(foutput[2],"%d ",j*deltat*4);    // here we write the t axis for the time traces            
            

    for(j=0;j<periods;j++)       // here we write the period axis for the coincidences
            fprintf(foutput[3],"%d ",j);

    for(j=0;j<2000;j++)
            fprintf(foutput[4],"%d ",j*deltat/100);    // here we write the t axis for the time trace zoom-in
            
    for(j=0;j<tmax;j++)
            fprintf(foutput[5],"%d ",j*deltat/100);    // here we write the t axis for the time trace zoom-in
 */   
/*
    for(i=0;i<max_offset;i++)
        fprintf(foutput[3],"%d ",i);
    for(j=0;j<mfinal[1]*peak*2;j++)
        fprintf(foutput[4],"%d ",(j-mfinal[1]*(peak-2))*n);
    for(i=0;i<max_offset;i++)
        fprintf(foutput[5],"%d ",i);
    for(j=0;j<mfinal[1]*peak*2;j++)
        fprintf(foutput[6],"%d ",(j-mfinal[1]*(peak-2))*n); 
    for(i=3;i<6;i++)
        for(j=0;j<mfinal[0];j++)
            fprintf(foutput[i+4],"%d ",j*n);
*/          
            
            
            
    for(i=0;i<4;i++)
        fprintf(foutput[i],"\n");
  
    //data analysis
	i=0;
	while(fscanf(finput,"%s",name)!= EOF)
    {
        printf("\n#%d:%s\n",i+1,name);
		i++;
        process(name);
    }
    
    fclose(finput);
    for(i=0;i<4;i++)
    {
        fclose(foutput[i]);
        //auto-launch output file
        strcpy(name,temp);  
        strcat(name,output_filename[i]);
        //system(name);
    }         
    return 0;
}
