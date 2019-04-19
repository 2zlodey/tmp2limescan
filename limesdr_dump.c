/*9.9.18*/
#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <lime/LimeSuite.h>
#include "wiringPi.h"
#include "wiringPiI2C.h"
#define INIFILENAME "./limesdr_debug.init"
#define i2c_adr 0x22
#define i2c_delay 50000
///////////////////////////////////
int debug=0;
//int dumpcnt=0;
struct inis { //for init-file
  char name[25];
  double value;
};
struct inis iniar[40];
char tmpstr[70];
char *ptmp;
FILE* fd ;
int stringcnt=0;
double lastvalue=0;
int parstr(char instr[],int idx){ //parsing string in two part
  char first[25];
  char second[25];  
  int i1=0;
  int i2=0;
  char valuetmp; 
  char* endstr;
  while (instr[i1]!='=') i1++;
  strncpy (first, instr, i1);
  first[i1]='\0';
  strncpy (second, &instr[i1+1], 25);
  while (second[i2]!='/'  && second[i2]!='\0' && second[i2]!=' ') i2++;
  if (second[i2]!='\0') second[i2]='\0';
  strcpy(iniar[stringcnt].name, first);
  iniar[stringcnt].value = strtod(second,&endstr);
}
int ini2arr(){ //add init-file string 2 array
  int i=0;
  stringcnt =0;
  fd = fopen( INIFILENAME, "r+b" );
  if ( fd == NULL ) perror("fopen()");
  else while (!feof(fd)){
    ptmp=fgets(tmpstr,70,fd);
    if (tmpstr[0]!='#' && tmpstr[0]!=' ' && tmpstr[0]!='\0' && tmpstr[0]!='\n' && tmpstr[0]!='\r') {
      parstr(tmpstr,stringcnt); 
      stringcnt++;
    }
  }
  fclose(fd);
  return stringcnt;
}
int doublret(char instr[]){ //return Value of name
  int i=0;
  while (strcmp(instr,iniar[i].name)!=0 && i<stringcnt) i++;  
  if (strcmp(instr,iniar[i].name)==0) {lastvalue=iniar[i].value; return 0;} else return -1;
}

///////////////////////////////////
int main(int argc, char** argv)
{
  int alg = 1;	
  int i=0;
  int isAlexIni = 1;
  int nowgain= 0;
  int prevgain= 0;
  int xmblock = 0;
  int oldblock = 0;
  int xskip = 0;
  int ii= 0;
  int sbl=0;//,xsbl=0;
  int nb_samples=0;
  double gain = 0;
  unsigned int freq = 1940000000;
  double bandwidth_calibrating = 8e6;
  double sample_rate = 5000000;
  lms_device_t* device = NULL;
  double host_sample_rate;
  unsigned int buffer_size = 1000*50;
  unsigned int device_i = 0;
  unsigned int channel = 0;
  size_t antenna = 3; //  1-RX LNA_H port   2-RX LNA_L port  3-RX LNA_W port
  char* output_filename = NULL;
  int signalthreshold_m1=12; //   M1   
  int blocksize=1024;     //   M1    size of block;
  long int curpwr=0;      //   M1
  int maxpwr=0;           //   M1
  int blockcnt=0;         //   M1
  double sr2=480000;       //   M2
  int signalthreshold=12;  //   M2   
  int skrblock=35;         //   M2  size of block 
  int oursize=3;           //   M2    
  int deltablock=2;        //   M2
  long int tmpiq; //       //   M2    
  int jj=0;                //   M2    
// new m3
  int m3signalthreshold=12;  //   M2   
  int m3skrblock=35;         //   M2  size of block 
  int m3oursize=3;           //   M2    
  int m3deltablock=2;        //   M2
  double m3dif=0.1;
  double m2dif=0.25;
  
//  double sr3=5000000;           //  M3  
  int debugcnt=0;               //  M3  
  long long int maxnumber = 0;       //  M3 
  long int maxcount = 0;        //  M3 
  int zoom = 1;                 //  M3 
  int periodsize = round(sample_rate/10000*25);//2.5milesec
  int subperiod=1250;           //  M3 250microsec=1250smpl
/////////////////////////////////////// from    ini
  int maxvar=0;
  maxvar=ini2arr(); 
  if (maxvar>0){ //if init-file present:
//   if (debug) printf("vsego=%d\n",maxvar);
//    while (i<stringcnt) {printf("name='%s' value='%f'\n",iniar[i].name,iniar[i].value);i++;}  
//anywhere
    if (doublret("isAlexIni")==0) isAlexIni=lastvalue;
    if (doublret("sample_rate")==0) sample_rate=lastvalue;
    if (doublret("buffer_size")==0) buffer_size=lastvalue;
//met.1
    if (doublret("signalthreshold_m1")==0) signalthreshold_m1=lastvalue;
    if (doublret("blocksize")==0) blocksize=lastvalue;
//met.2    
    if (doublret("sr2")==0) sr2=lastvalue;
    if (doublret("signalthreshold")==0) signalthreshold_m1=lastvalue;
    if (doublret("skrblock")==0)  skrblock=lastvalue;
    if (doublret("oursize")==0)   oursize=lastvalue;
    if (doublret("deltablock")==0) deltablock=lastvalue;
    if (doublret("m2dif")==0) m2dif=lastvalue;
//met.3
    if (doublret("m3signalthreshold")==0) m3signalthreshold=lastvalue;
    if (doublret("m3skrblock")==0)  m3skrblock=lastvalue;
    if (doublret("m3oursize")==0)   m3oursize=lastvalue;
    if (doublret("m3deltablock")==0) m3deltablock=lastvalue;
    if (doublret("m3dif")==0) m3dif=lastvalue;
    if (doublret("overalg")==0)  alg=lastvalue;
  }
//////////////////////////////////////// from command prompt:
  if ( argc < 2 ) {
   printf("--usage: %s <OPTIONS>\n", argv[0]);
   printf("  -f <FREQUENCY>\n"
      "  -b <BANDWIDTH_CALIBRATING> (default: 8e6)\n"
      "  -s <SAMPLE_RATE> (default: 5e6)\n"
      "  -g <GAIN_dB> range [0, 73] (default: 0)\n"
      "  -l <BUFFER_SIZE>  (default: 1000*1000)\n"
      "  -d <DEVICE_INDEX> (default: 0)\n"
      "  -c <CHANNEL_INDEX> (default: 0)\n"
      "  -a <ANTENNA> (1=LNAL | 2=LNAH | 3=LNAW) (default: LNAW)\n"
      "  -o <OUTPUT_FILENAME> (default: stdout)\n"
      "  -n NUMBER of sampes\n"
      "  -m method (0-16bit IQ-stream,1,2,3-alg1,2,3)\n"
      "  -z zoom\n"
      "  -p 1 out struct lte tdd\n"
      "  -x signalthreshold\n");
    return 1;
  }
  for ( i = 1; i < argc-1; i += 2 ) {
    if      (strcmp(argv[i], "-f") == 0) { freq = atof( argv[i+1] ); }
    else if (strcmp(argv[i], "-b") == 0) { bandwidth_calibrating = atof( argv[i+1] ); }
    else if (strcmp(argv[i], "-s") == 0) { sample_rate = atof( argv[i+1] ); }
    else if (strcmp(argv[i], "-g") == 0) { gain = atof( argv[i+1] ); }
    else if (strcmp(argv[i], "-l") == 0) { buffer_size = atoi( argv[i+1] ); }
    else if (strcmp(argv[i], "-d") == 0) { device_i = atoi( argv[i+1] ); }
    else if (strcmp(argv[i], "-c") == 0) { channel = atoi( argv[i+1] ); }
    else if (strcmp(argv[i], "-a") == 0) { antenna = atoi( argv[i+1]); }
    else if (strcmp(argv[i], "-o") == 0) { output_filename = argv[i+1]; }
    else if (strcmp(argv[i], "-m") == 0) { alg = atof( argv[i+1] ); }
    else if (strcmp(argv[i], "-n") == 0) { maxnumber = atof( argv[i+1] ); }
    else if (strcmp(argv[i], "-z") == 0) { zoom = atof( argv[i+1] ); }
    else if (strcmp(argv[i], "-p") == 0) { debugcnt = atof( argv[i+1] ); }
    else if (strcmp(argv[i], "-x") == 0) { signalthreshold = atof( argv[i+1] ); }
  }
  if ( freq == 0 ) {
    fprintf( stderr, "ERROR: invalid frequency : %d\n", freq );
    return 1;
  }
  
  alg=1;


  if (alg==1) {buffer_size=50*1000;}
  else if (alg==2 || alg==0) {
    buffer_size=18000;
    sample_rate = sr2;
  }
/////////////////////////////////////////////////////////////////////////////////////////////////
  struct s16iq_sample_s {
    short i;
    short q;
  } __attribute__((packed));
  struct s16iq_sample_s *buff = (struct s16iq_sample_s*)malloc(sizeof(struct s16iq_sample_s) * buffer_size);
  if ( buff == NULL ) {
    perror("malloc()");
    return 1;
  }
  FILE* fd = stdout;
  if ( output_filename != NULL ) {
    fd = fopen( output_filename, "w+b" );
    if ( fd == NULL ) {
      perror("fopen()");
      return 1;
    }
  } else fd = stderr;

/////////////////////////////// function of limeutils
  lms_range_t curbwr;
  float_type hosthz;
  float_type rfhz;
  float_type curbw;

  int getlna(){
    uint16_t lnag;
    if ( LMS_ReadParam(device,LMS7param(G_LNA_RFE),&lnag) < 0 ) {
      fprintf(stderr, "LMS_ReadParam() lna: %s\n", LMS_GetLastErrorMessage());
      return -1;
    } else return lnag;
  }
  int gettia(){
    uint16_t tiag;
    if ( LMS_ReadParam(device,LMS7param(G_TIA_RFE),&tiag) < 0 ) {
      fprintf(stderr, "LMS_ReadParam() tia : %s\n", LMS_GetLastErrorMessage());
      return -1;
    } else return tiag;
  }
  int getpga(){
    uint16_t pgag;
    if ( LMS_ReadParam(device,LMS7param(G_PGA_RBB),&pgag) < 0 ) {
      fprintf(stderr, "LMS_ReadParam() pga : %s\n", LMS_GetLastErrorMessage());
      return -1;
    } else return pgag;
  }
  long int getbwr(){ //return min bandwidth & out 2 term min & max 
    if ( LMS_GetLPFBWRange( device, 0, &curbwr ) < 0 ) {
      fprintf(stderr, "LMS_GetLPFBWRange() : %s\n", LMS_GetLastErrorMessage());
      return -1;
    } else {
      printf ("Curent Bandwidth range  min=%.1lfMHz max=%.1lfMHz\n",curbwr.min/1000000,curbwr.max/1000000);
      return round(curbwr.min);
    }
  }
  int getsr(){ // return SampleRate KHz & out 2 term Host&RF-rate
    if ( LMS_GetSampleRate( device, 0 , 0, &hosthz, &rfhz ) < 0 ) {
      fprintf(stderr, "LMS_GetSampleRate() : %s\n", LMS_GetLastErrorMessage());
      return -1;
    } else {
     if (debug) printf("Host=%2.1fMHz (%lf) RF=%2.1fMHz(%lf)\n",round(hosthz/1000)/1000,hosthz,round(rfhz/1000)/1000,rfhz);
      return round(hosthz/1000);
    }
  }
  int setsr(double srate){ //set samplerate in Hz & out 2 term Host&RF-rate
    if ( LMS_SetSampleRate( device, srate, 0 ) < 0 ) {
      fprintf(stderr, "LMS_SetSampleRate() : %s\n", LMS_GetLastErrorMessage());
      return -1;
    } else {
      if ( LMS_GetSampleRate( device, 0 , 0, &hosthz, &rfhz ) < 0 ) {
        fprintf(stderr, "LMS_GetSampleRate() : %s\n", LMS_GetLastErrorMessage());
        return -1;
      } else {
       if (debug) printf("Host=%5.3fMHz RF=%lf\n",round(hosthz/1000)/1000,rfhz);
        return 0; 
      }
    }
  }
  int setgain(int db){ //set gain in DB & return gain
    if ( db >= 0 ) {
    if (debug) printf("set gain@%ddB",db);
      if ( LMS_SetGaindB( device, 0, 0, db ) < 0 ) {
        fprintf(stderr, "LMS_SetGaindB() : %s\n", LMS_GetLastErrorMessage());
        return -1;
      } else  return db;  
    } else return -1;
  }
  int getgain(){ //return gain in dB
    unsigned curgain;
    if ( LMS_GetGaindB( device, 0, 0, &curgain ) < 0 ) {
      fprintf(stderr, "LMS_GetGaindB() : %s\n", LMS_GetLastErrorMessage());
      return -1;
    } else  return round(curgain); 
  }
  long int getbw(){ //get bedwidth
    if ( LMS_GetLPFBW( device, 0, 0, &curbw ) < 0 ) {
      fprintf(stderr, "LMS_GetLPFBW() : %s\n", LMS_GetLastErrorMessage());
      return -1;
    } else {
      printf ("Curent Bandwidth = %2.3lfMHz\n",curbw/1000000);
      return round(curbw);
    } 
  }
  int setbw(float_type newbw){ //set bandwith rate
    if ( LMS_SetLPFBW( device, 0, 0, newbw ) < 0 ) {
      fprintf(stderr, "LMS_SetLPFBW() : %s\n", LMS_GetLastErrorMessage());
      return -1;
    } else {
      printf ("New Bandwidth = %2.3lfMHz\n",newbw/1000000);
      return 0;
    } 
  }
  int setminbw(){ //set minbandwith
    if ( LMS_SetLPFBW( device, 0, 0, 1400000 ) < 0 ) {
      fprintf(stderr, "LMS_SetLPFBW() : %s\n", LMS_GetLastErrorMessage());
       return -1;
    } else {
      getbw();
      return 0;
    }
  }
////////////////////////////////        INIT LIME       /////////////////
  int device_count = LMS_GetDeviceList(NULL);
  void zerror(int retcode){if (retcode<0) fprintf(stderr, "LMS ERROR:%s\n", LMS_GetLastErrorMessage());}

  lms_info_str_t device_list[ device_count ];
  zerror(LMS_GetDeviceList(device_list));

 if (debug) printf(">device list recived?\n");
  zerror( LMS_Open(&device, device_list[ device_i ], NULL));

 if (debug) printf(">device pointer returned?\n>RESET!..\n");
  zerror(LMS_Reset(device)); //   reset

 if (debug) printf(">Seting DEFAULT...\n");
  zerror(LMS_Init(device));  //  default

  zerror(LMS_EnableChannel(device, LMS_CH_RX, 0, true)); //enable RX-0 channel
 if (debug) printf(">Seting channel to RX-0\n");

  if (alg==2) sample_rate=sr2;
  zerror(LMS_SetSampleRate(device, sample_rate, 0));
 if (debug) printf(">Seting samplerate =%9.0f done!\n",sample_rate);
//  LMS_GetSampleRate(device, LMS_CH_RX, 0, &host_sample_rate, NULL );

  zerror(LMS_SetAntenna( device, LMS_CH_RX, 0, 3 ));//1=RX LNA_H 2=RX LNA_L 3=RX LNA_W 
 if (debug) printf(">Seting antenna. done!\n");//


  zerror(LMS_Calibrate( device, LMS_CH_RX, 0, bandwidth_calibrating, 0 ));
 if (debug) printf(">Calibrating. done!\n");

  zerror(LMS_SetLOFrequency( device, LMS_CH_RX, 0, freq));
 if (debug) printf(">Seting freq =  %d done!\n",freq);

  zerror(LMS_SetGaindB( device, LMS_CH_RX, 0, gain ));
 if (debug) printf(">Set gain to %2.0f done!\n",gain);

///////////// init & start stream
	lms_stream_t rx_stream = {
		.channel = channel,
		.fifoSize = buffer_size * sizeof(*buff),
		.throughputVsLatency = 0.5,
		.isTx = LMS_CH_RX,
		.dataFmt = LMS_FMT_I16
	};
 if (debug) printf("setup stream:");

	if ( LMS_SetupStream(device, &rx_stream) < 0 ) {
		fprintf(stderr, "LMS_SetupStream() : %s\n", LMS_GetLastErrorMessage());
		return 1;
	}	
 if (debug) printf("ok\nstart stream:");
	LMS_StartStream(&rx_stream);
 if (debug) printf("ok.\n");
	
////////////////////////////// our functional ///////////////////////////////
  int dif(int a, int b){
    if (abs(a-b) > m2dif*a) return 1;
    else return 0; 
  }
  long int readpwr(){
    tmpiq=(buff[ii].i*buff[ii].i+buff[ii].q*buff[ii].q)>>8;
    ii++;
    return tmpiq;
  }
  long int skrpwr(int size_skrblock){ // middle powwer
    long long int retvalue=0;
    for (jj = 0; ((jj < size_skrblock) && (ii<buffer_size-size_skrblock)); jj++) retvalue+=readpwr();
    retvalue=round(retvalue/(jj+1));
    return retvalue; 
  }                  

  int minexmblock(){ //// find XMBLOCK from alg(0-3)
    double maxpwr=0;
    while (ii<buffer_size){
      if (readpwr()>signalthreshold_m1) {
        curpwr=skrpwr(blocksize);
        if (maxpwr<curpwr) maxpwr=curpwr;
      }
    }
    xmblock=maxpwr; // max from average PWR (sq. of AMP )
   printf("---->>>>>>>>>>\n");
    return maxpwr;
  }
//////// i2c
  union i2c_smbus_data{
    uint8_t  byte ;
    uint16_t word ;
    uint8_t  block [34] ;// block [0] is used for length + one more for PEC
  };
  int devlna=wiringPiI2CSetup(i2c_adr);
  usleep(i2c_delay);
  if (debug) printf("main.I2C init device ok.\n"); 
  
  wiringPiI2CWriteReg8 (devlna, 0, 0 );usleep(i2c_delay);
  wiringPiI2CWriteReg8 (devlna, 9, 255 );usleep(i2c_delay);
  if (debug) printf("main.I2C registr init ok.\n");

  ///// LNA on/off 
  int extlnaon(){
      int reg=wiringPiI2CReadReg8(devlna,9);usleep(i2c_delay);
      if (reg>127) wiringPiI2CWriteReg8 (devlna, 9, (reg-128));usleep(i2c_delay);
      if (debug) printf("    LNA  on.\n");
  }  // set @channal lna=ON +16dB
  int extlnaoff(){
      int reg=wiringPiI2CReadReg8(devlna,9);usleep(i2c_delay);
      if (reg<=127) wiringPiI2CWriteReg8 (devlna, 9, (reg+128));usleep(i2c_delay);
      if (debug) printf("    LNA  off.\n");
  } // set  @channal lna=OFF +0dB

//////   ATT on/off
  int extatton(){
      int reg=wiringPiI2CReadReg8(devlna,9);usleep(i2c_delay);
      wiringPiI2CWriteReg8 (devlna, 9, (reg&135));usleep(i2c_delay);
      if (debug) printf("    ATT on. -16dB.\n");
  } // set  @channal att -16dB
  int extattoff(){
      int reg=wiringPiI2CReadReg8(devlna,9);usleep(i2c_delay);
      wiringPiI2CWriteReg8 (devlna, 9, (reg&135+120));usleep(i2c_delay);
  if (debug) printf("    ATT off  0dB.\n");
  } // set  @channal att -0dB

////////  BPF/BAND
  int setfilter(int n){
    if (n<1 || n>8) n=8;
    int reg=wiringPiI2CReadReg8(devlna,9);usleep(i2c_delay);
    int bpf[9]={0,3,1,2,0,4,6,5,7};  //array of bytes for i2c reg
    wiringPiI2CWriteReg8 (devlna, 9, (reg&248 + bpf[n] ) );usleep(i2c_delay);
    return (reg&248 + bpf[n] ); // 
  }
  int setband(int band){
    int reg=wiringPiI2CReadReg8(devlna,9);usleep(i2c_delay);
    int bands[7]={3,5,20,28,8,7,1}; // array of availeble bands
    int bpf[8] = {3,1,2,0,4,6,5,7}; // array of bytes for i2c reg
    int i=0;
    while ( i<7 && bands[i]!=band) i++;
    wiringPiI2CWriteReg8 (devlna, 9, (reg&248 + bpf[i] ) );usleep(i2c_delay);
    return i+1; //number of filter BPFn 1-8
  }
/////////////////////// autogain
  int curzone=0;
  struct zone_t { //
    double min;
    double max;
    int dbshift;
  };
  struct zone_t zone[6];
//zone 0;
  zone[0].min=100*100;
  zone[0].max=2000*2000;
  zone[0].dbshift=-16;
//zone 1;
  zone[1].min=100*100;
  zone[1].max=2000*2000;
  zone[1].dbshift=0;
//zone 2;
  zone[2].min=100*100;
  zone[2].max=2000*2000;
  zone[2].dbshift=24;
//zone 3;
  zone[3].min=100*100;
  zone[3].max=2000*2000;
  zone[3].dbshift=48;
//zone 4;
  zone[4].min=200*200;
  zone[4].max=2000*2000;
  zone[4].dbshift=72;
//zone 5;
  zone[5].min=200*200;
  zone[5].max=2000*2000;
  zone[5].dbshift=88;
//////////////////////////////////////////////////////////////////////////////////
  int setzone(int z){
    printf("sz.curzone=%d newzone=%d",curzone,z);
    if (z<0 ||z>5) {printf("!!!!!!!!!!!error!!!!!!!!!!!!!");return -1;}
    if (z==0) {setgain(0); extlnaoff(); extatton();}
    else if (z==5) {setgain(72); extlnaon(); extattoff();} 
    else {setgain(zone[z].dbshift); extlnaoff(); extattoff();}
    usleep(i2c_delay);
    return z;
  }
    double lmine(){
      LMS_RecvStream( &rx_stream, buff, buffer_size, NULL, 1000 );
      double max,skr;
      for (int ii=0; ii<buffer_size;ii++){
        skr+=buff[ii].i*buff[ii].i+buff[ii].q*buff[ii].q;
        if ((15&(ii+1))==0) {
          skr=skr/16; 
          if (max<skr) max=skr;
          skr=0;
        }  
      } 
      return max;     
    }

  int autosetlevel(){
printf("------------------------\n");
    double qmine(){
      LMS_RecvStream( &rx_stream, buff, 1024, NULL, 1000 );
      double max,skr;
      for (int ii=0; ii<1024;ii++){
        skr+=buff[ii].i*buff[ii].i+buff[ii].q*buff[ii].q;
        if ((15&(ii+1))==0) {
          skr=skr/16; 
          if (max<skr) max=skr;
          skr=0;
        }  
      } 
      return max;     
    }

    double tmpxbl=qmine();         
    while (tmpxbl < zone[curzone].min || tmpxbl > zone[curzone].max  ){
      tmpxbl=qmine();  //mine level
printf("---->>>>>>>>>>\n");

      if (tmpxbl < zone[curzone].min ) { //nead ampl.
        if (curzone<5)  curzone=setzone(curzone+1); //we can? we do!
        else {tmpxbl=zone[curzone].min;return -1;} //we lost him! 
      } 
//-------------------
      if (tmpxbl > zone[curzone].max ) { //nead att
        if (curzone>0) curzone=setzone(curzone-1); //we can? we do!
        else {tmpxbl=zone[curzone].max; return -2;};  //we find him!
      }

    } //end while
    return 0;
  } //end auto
///////////////////////////////////// main loop
  short delay=0, delaylimit=3;  //
  long int prevxmblock=0;
  curzone=0;
  setzone(curzone);

  if (maxnumber>0) if (debug) printf("main.alg=%d. Iterations counter : %lld\n",alg,maxnumber);
  else if (maxnumber<0)if (debug) printf("main.alg=%d. start infinity loop. \n",alg);

////////////////////////////                ///////////
  while( maxnumber--!=0 ) { //start main loop
    xmblock=lmine();
    if (xmblock > zone[curzone].min && xmblock < zone[curzone].max ) delay=0; 
    else {
      delay++;
      xmblock=prevxmblock;
      if (delay >= delaylimit){delay=0; autosetlevel();}
    } 
    if (xmblock > 0) sbl=round(10*log10(round(xmblock)/1000000));else sbl=0;
//    printf("%lld, %.0fue, %ddB, (shift=%ddB,orig=%d) zone=%d delay=%d\n",maxnumber, round(sqrt(xmblock)), sbl-zone[curzone].dbshift, zone[curzone].dbshift, sbl,curzone,delay);               
    printf(" %.0f,%d,%d,%d,%d,%d\n",round(sqrt(xmblock)), sbl-zone[curzone].dbshift, zone[curzone].dbshift, sbl,curzone,delay);               

    prevxmblock=xmblock;
   if (curzone<0 || curzone>5) {return -1; }
 } /////////////////////// end mainloop	/////////////////
  LMS_StopStream(&rx_stream);
  LMS_DestroyStream(device, &rx_stream);
  free( buff );
  fclose( fd );
  LMS_Close(device);
  return 0;
}
