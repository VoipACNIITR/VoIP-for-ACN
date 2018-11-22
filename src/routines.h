
//double cvsdDecode(unsigned short CVSD_Bit);
//unsigned short cvsdEncode(double inputADC);
//void Handle_Buffer();
//void filter(double* input, int length, double* coeff, int Nt, double* output);


#if !defined(routines_cpp)
#define routines_cpp


//double cvsdDecode(unsigned short CVSD_Bit);
//unsigned short cvsdEncode(double inputADC);
//void Handle_Buffer();
//void filter(double* input, int length, double* coeff, int Nt, double* output);


#if !defined(routines_cpp)
#define routines_cpp

//--------------Global Variables -------------------//

const int SYLLAB_COEFF = 32112;
const int RECONS_COEFF = 29491;
const int STEP_HI = 660;
const int STEP_LO = 10;
const int SHIFT_INTEGRATOR_ = 15;
double fill_coeff[49] = {	
   -0.0003,   -0.0012,   -0.0003,    0.0015,    0.0016,   -0.0014,   -0.0037,  
   -0.0002,    0.0059,    0.0043,   -0.0062,   -0.0107,    0.0022,    0.0172,
    0.0080,   -0.0199,   -0.0248,    0.0129,    0.0458,    0.0112,   -0.0665,
   -0.0695,    0.0819,    0.3052,    0.4125,    0.3052,    0.0819,   -0.0695,  
   -0.0665,    0.0112,    0.0458,    0.0129,   -0.0248,   -0.0199,    0.0080,  
    0.0172,    0.0022,   -0.0107,   -0.0062,    0.0043,    0.0059,   -0.0002,  
   -0.0037,   -0.0014,    0.0016,    0.0015,   -0.0003,   -0.0012,   -0.0003 };
   int Nt = 49;
//---------------------------------------------------//
unsigned short cvsdEncode(short inputADC);
short cvsdDecode(unsigned short CVSD_Bit);
void filter ( short* input, int length, double* coeff, int Nt, double* output );



unsigned short cvsdEncode(short inputADC)
{
	static unsigned short cvsd_bits = 0;
	static unsigned short three_bits = 0;
	static long syllab = 0;
	static long predict_val = 0;

	cvsd_bits <<= 1;
    if( (short)inputADC >= (short)predict_val)
		cvsd_bits += 1;
	

	three_bits = cvsd_bits & 0x07;

	if( (three_bits == 0) || (three_bits == 7) )
	{	
		syllab = ((syllab * SYLLAB_COEFF)>>SHIFT_INTEGRATOR_ )+ STEP_HI ; 
	}
	else
	{
		syllab = (syllab * SYLLAB_COEFF)>>SHIFT_INTEGRATOR_ ;
	}

	syllab = syllab + STEP_LO;


	if( three_bits & 1 )
	{
		predict_val = ((predict_val * RECONS_COEFF)>>SHIFT_INTEGRATOR_ ) + syllab ;
	}
	else
	{
		predict_val = ((predict_val * RECONS_COEFF)>>SHIFT_INTEGRATOR_ ) - syllab ;
	}
      return (cvsd_bits);
}

short cvsdDecode(unsigned short CVSD_Bit)
{

	static unsigned short CVSD_three_bits = 0;
	static long CVSD_syllab = 0;
	static long CVSD_predict_val = 0;


	CVSD_three_bits <<= 1;

	if( CVSD_Bit & 1)
	{
		CVSD_three_bits += 1;
	}


	CVSD_three_bits &= 7;


	if( (CVSD_three_bits == 0) || (CVSD_three_bits == 7) )
	{	
		CVSD_syllab = ((CVSD_syllab * SYLLAB_COEFF)>>SHIFT_INTEGRATOR_ ) + STEP_HI ; 
	}
	else
	{
		CVSD_syllab = ((CVSD_syllab * SYLLAB_COEFF)>>SHIFT_INTEGRATOR_ );
	}

	CVSD_syllab = CVSD_syllab + STEP_LO;


	if( CVSD_three_bits & 1 )
	{
		CVSD_predict_val = ((CVSD_predict_val * RECONS_COEFF)>>SHIFT_INTEGRATOR_ ) + CVSD_syllab ;
	}
	else
	{
		CVSD_predict_val = ((CVSD_predict_val * RECONS_COEFF)>>SHIFT_INTEGRATOR_ ) - CVSD_syllab ;
	}
	return ((short)CVSD_predict_val);
}

void filter(short* input, int length, double* coeff, int Nt, double* output)
{
	
	int i,j;
    short tp;

	
 	for(i=0; i<length; i++)
	{
		for(j=0; j<Nt; j++)
		{
			if( (i-j) < 0)
				tp = 0;
			else
				tp = input[i-j];
			
			output[i] += tp*coeff[j];
		}
	}

	return;
}
 




void Apply_CVSD(char *InData, int insize, char *OutData, int *outsize)
{

	short *S;
	int i,j;
	unsigned short bit;
	
	S=(short *)InData;
	
	for(i=0;i<(insize/(2*8));i++)
	{
		OutData[i]=0x00;
		for(j=0;j<8;j++)
		{
			bit=cvsdEncode(S[i*8+j]);
			bit &= 0x01;
			OutData[i]=OutData[i]<<1;
			OutData[i]=(OutData[i] | bit);
		}
	}
	*outsize=(insize/16); // 16= 2*8

}


void Reverse_CVSD(char *encdata, int encsize, char *decdata, int *decsize)
{
	short *O;
	short *outDataArray;
	double *outDataArray_filter;
	int i,j;
	unsigned short bit;
	short val;//,temp;
	char *C;

	 O=new short [8*encsize];
	 outDataArray=new short [8*encsize];
	 outDataArray_filter=new double [8*encsize];
	
	for(i=0;i<encsize;i++)
	{
		for(j=0;j<8;j++)
		{
			bit=(unsigned short)(encdata[i] & 0x80);
			if(bit>0) bit=0x0001;
			else bit=0x0000;
			encdata[i]=encdata[i]<<1;
			val=cvsdDecode(bit);
			outDataArray[i*8+j]=val;
			outDataArray_filter[i*8+j]=0.0;
		}

	}

	filter( outDataArray, (8*encsize), fill_coeff, Nt, outDataArray_filter);  

	for(int index=0; index<(8*encsize); index++)
	{
		O[index]=(short)outDataArray_filter[index];

	}

	C=(char*)O;

	for(i=0;i<(encsize*2*8);i++)
	{
		decdata[i]=C[i];
	}

	*decsize=(2*(8*encsize));
	delete [] O;
	delete [] outDataArray;
	delete [] outDataArray_filter;
}

#endif // !defined(routines_cpp)

#endif // !defined(routines_cpp)
