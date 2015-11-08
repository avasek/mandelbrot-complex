#include <pthread.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <png.h>
#include <stdint.h>
#include <errno.h>
#include <sys/stat.h>

/*
  These Values are used to control the recursive fractal funtion. 

  DEPTH:    The maximum number of steps used for testing
  ESCAPE:   The square of the largest absolute value allowed before ending testing
  MIN_R:    This is the square of the smallest absolute value the function will check
              to prevent errors with the log function
*/
#define   DEPTH       2000
#define   ESCAPE      100.0
#define   MIN_R       1E-12




/*
  Use the EIGHT_BIT and BRANCH constants as flags. First clear all flags that have been set inadvertently.
  The default is for both to not be set.

  EIGHT_BIT: If set, the PNG file will be encoded using a 8 bit encoding for pixel color, 
             otherwise the default is 16 bit
  BRANCH:    If set, the branch cut for the arctangent function will be from 
             (theta-pi,theta+pi) where theta is the original argument of the point. Otherwise the branch cut
             will the (b-pi,b+pi), where b is the complex portion of the 
             exponent in the recursive fractal definition
*/
#ifdef EIGHT_BIT
#undef EIGHT_BIT
#endif
#ifdef BRANCH
#undef BRANCH
#endif

//#define EIGHT_BIT 8
#define BRANCH 1


/*
  Use the following definitions to define the IHDR header for the PNG file
  
*/
#ifdef EIGHT_BIT 
#define   BIT_DEPTH   8
#else
#define   BIT_DEPTH   16
#endif

#define   COLOR_TYPE  PNG_COLOR_TYPE_RGB
#define   INTERLACING PNG_INTERLACE_NONE


// Define the minimum dimension allowed for a single side
#define   MIN_DIM  100   
// Use a seperate folder for the 
#define   FOLDER   "./Output"

/*
  Define the static variables which will uniquely describe the Mandelbrot image
  (Note this does not count the number of bits or the branching used)
*/
static uint32_t s_width;
static uint32_t s_height;
static uint32_t NUM_THREADS;
static double   s_scale;
static double   s_center_r;
static double   s_center_i;
static double   s_power_r;
static double   s_power_i;

// The coorinates of the upper left corner of the image
static double   cornerR;
static double   cornerI;

// The pointer for the PNG image
static png_structp png_ptr;



/* 
   This struct is a single row of the image. 
   Use this to keep track of the rows and ensure they are printed in the correct order 
*/
struct row_data{
  int row_number;
  png_bytep vals;
};

/* 
   This struct saves the pipes that will be used by the threads that are calculating the image
   The first pipe is read to find the row numbers and the second is used to write the row_data
*/
struct pipes{
  int pipeRN; // Row Numbers
  int pipeRD; // Row Data
};

int create_image();
void calc_image();
void *handle_pthread(void *ptr_pipe);
png_bytep calculate_row(int i);
double calculate_escape(int x, int y);
void *handle_output(void *row_data_pipe);

void   _abort(const char * s, ...);
double _absolute(double d);

int main(int argc, char **argv){
  // For optarg()
  extern char *optarg; 
  extern int optind, opterr, optopt;

  // Command line arguments, with their default values
  // These values determine the location and type of plot to create
  s_width = 1920; // w
  s_height = 1080; // h
  s_scale = 0.002; // s
  s_center_r = -0.5; // r
  s_center_i = 0.0; // i
  s_power_r = 2.0; // a
  s_power_i = 0.0; // b

  NUM_THREADS = 4; // t

  // Collect Command Line arguments
  int opt;
  while((opt=getopt(argc, argv, "w:h:s:r:i:a:b:t:")) != -1){
    if (optarg == NULL){
      printf("Optarg is null!!");
      return -1;
    }
    switch(opt) {
    case 'w': s_width=(uint32_t)strtoul(optarg, NULL, 0);
      break;
    case 'h': s_height=(uint32_t)strtoul(optarg, NULL, 0);
      break;
    case 't': NUM_THREADS=(uint32_t)strtoul(optarg, NULL, 0);
      break;
    case 's': s_scale=strtod(optarg,(char **) NULL);
      break;
    case 'r': s_center_r=strtod(optarg,(char **) NULL);
      break;
    case 'i': s_center_i=strtod(optarg,(char **) NULL);
      break;
    case 'a': s_power_r=strtod(optarg,(char **) NULL);
      break;
    case 'b': s_power_i=strtod(optarg,(char **) NULL);
      break;
    default: printf("Bad user argument: %c", (char) opt);
      break;
    }
  }

  // Test the parameters passed through the command line to confirm 
  // that the height and width are within the desired range
  if((s_width < MIN_DIM) || (s_height < MIN_DIM)){
    printf("Dimensions are too small: %d x %d\nMin: %d\n", s_width, s_height, MIN_DIM);
    return -1;
  }

  // Now that the parameters of the set have been determined, create the fractal
  return create_image();
}


/* 
   This function will create the PNG image which is used to store the Mandelbrot set.  
   After creating the empty PNG image, it will call the function calc_image to calculate the 
   fractal. After the image has been calculated the PNG image is ended.

   Input:
              None
   Output:    
              Returns 0 on success and -1 on failure
*/

int create_image(){
  png_FILE_p fp;
  png_infop info_ptr;
  char *name;
  int saveError;


  saveError = errno;
  mkdir(FOLDER,
        S_IRUSR|S_IWUSR|S_IXUSR|
        S_IRGRP|S_IWGRP|S_IXGRP|
        S_IROTH|S_IWOTH|S_IXOTH  );

  errno = saveError;


  // Create a filename based on the parameters given by the user
  // Uniquely describes a Mandelbrot image (within the accuracy of the printed values)
  if ((name = (char *)malloc(200*sizeof(char))) == NULL){
    printf("Error allocating memory for image name!\n");
    return -1;
  }
#ifdef BRANCH
  sprintf(name, "%s/Dimension: %dx%d, Center: %.4f%+.4fi, Scale: %.2e, Exp: %0.2e+%0.2ei, Branch set.png", 
          FOLDER, s_width, s_height, s_center_r, s_center_i, s_scale, s_power_r, s_power_i);
#else
  sprintf(name, "%s/Dimension: %dx%d, Center: %.4f%+.4fi, Scale: %.2e, Exp: %0.2e+%0.2ei, Branch not set.png", 
          FOLDER, s_width, s_height, s_center_r, s_center_i, s_scale, s_power_r, s_power_i);
#endif

  printf("Output Filename: %s\n", name);

  if((fp = (png_FILE_p) fopen(name,"wb"))==NULL){
    printf("File error creating file: %s\n", name);
    return -1;
  }

  free(name);

  // Initialize the PNG file which will hold the Mandelbrot image
  if (!(png_ptr=png_create_write_struct(PNG_LIBPNG_VER_STRING, (png_voidp) NULL, (png_error_ptr) NULL, (png_error_ptr) NULL))) {
    printf("Oh No!!! Bad pointer png_ptr\n");
    return -1;
  }
  if (!(info_ptr=png_create_info_struct(png_ptr))) {
    printf("Oh No!!! Bad pointer png_infop\n");
    return -1;
  }
  if (setjmp(png_jmpbuf(png_ptr)))
    _abort("[write_png_file] Error during init_io");
  png_init_io(png_ptr,fp);
  if (setjmp(png_jmpbuf(png_ptr)))
    _abort("[write_png_file] Error during set IHDR");
  // Set the type of png file based on the defaults, the specified size, and bit depth
  png_set_IHDR(png_ptr, info_ptr, s_width, s_height,
               BIT_DEPTH, COLOR_TYPE, INTERLACING,
               PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
  if (setjmp(png_jmpbuf(png_ptr)))
    _abort("[write_png_file] Error during write info");
  png_write_info(png_ptr,info_ptr);

  // Capture the data required for the image
  calc_image();

  // Write the end of file for the PNG image
  if (setjmp(png_jmpbuf(png_ptr)))
    _abort("[write_png_file] Error during ending");
  png_write_end(png_ptr,info_ptr);

  // Close the file
  fclose((FILE *) fp);
  // Return zero on proper exit
  return 0;
}


/*
  Function: calc_image

  Creates the threads which will calculate the fractal for each row 
  and the thread which uses those values to write a row to the PNG

  Input:
          None
  Output:
          None (PNG data is computed during this time)
*/
void calc_image(){
  pthread_t threads[NUM_THREADS+1];
  int i, j;
  int pipeRows[2], pipeRet[2];

  struct pipes pipefd;

  // Find the upper lefthand corner of the image
  cornerR = s_center_r-s_scale*s_width/2;
  cornerI = s_center_i+s_scale*s_height/2;

  // Create a pipe to pass information to the pthreads
  if(pipe(pipeRows) != 0) {
    printf("Error Making Pipes!\n");
    exit(-1);
  }
  // Create a pipe to return the row data from the pthreads
  if(pipe(pipeRet) != 0) {
    printf("Error Making Pipes!\n");
    exit(-1);
  }
  // Create the thread which will print the row data to the image
  if(pthread_create(&threads[NUM_THREADS],NULL,handle_output,&(pipeRet[0])) != 0){
    printf("thread Error!\n");
    _exit(-1);
  }
  // Create the pthreads and start them waiting for the rows to calculate
  for (i=0; i < NUM_THREADS; i++){
    pipefd.pipeRN = pipeRows[0];
    pipefd.pipeRD = pipeRet[1];
    if(pthread_create(&threads[i],NULL,handle_pthread,&pipefd) != 0){
      printf("thread Error!\n");
      _exit(-1);
    }
  }

  // Pass the row numbers to the pthreads
  for(j=0; j < s_height; j++){
    if(write(pipeRows[1],&j,sizeof(int))!=sizeof(int)){
      printf("Error writing: %d\n",j);
      exit(-1);
    }
  }

  // Write the end of row flag for each of the pthreads
  for (i=0; i<NUM_THREADS; i++){
    j=-1;
    if(write(pipeRows[1],&j,sizeof(int))!=sizeof(int)){
      printf("Error writing stop\n");
      exit(-1);
    }
  }
  
  // Await the termination of the pthreads in order to finish the PNG image
  for (i=0; i<NUM_THREADS+1; i++){
    if(pthread_join(threads[i],NULL)!=0){
      printf("Error during thread join!\n");
      _exit(-1);
    }
  }
}


/*
  Function: handle_pthread

  This function will read row numbers from an input pipe. From these row numbers,
  the function will use the calculate_row function to find the row data. After finding
  the row data, the function will pass the result to the 
  image writing thread using the output pipe.

  Input: 
        void *ptr_pipe: pointer to a struct ptr_pipe, 
                        which hold the pointer to the input and output pipe file descriptors
  Output:
        NULL
*/
void *handle_pthread(void *ptr_pipe){
  struct pipes pp;
  struct row_data rowD;
  int value;
  
  // Initialize pipes and value
  pp = *((struct pipes *) ptr_pipe);
  value=0;

  // Take the row numbers passed in the pipe and calculate the color the entire row
  while(value>=0){
    if(read(pp.pipeRN, &value, sizeof(int))!=sizeof(int)){
      printf("Read Error!!\n");
      exit(-1);
    }
    // If the value is less than zero, the image is done, otherwise continue calculating the row data
    if (value >= 0){
      // Calculate the values in the row and keep them with the row number
      rowD.vals = calculate_row(value);
      rowD.row_number = value;
      // Pass the row data to the thread which is writing the row
      if(write(pp.pipeRD, &rowD, sizeof(struct row_data))!=sizeof(struct row_data)){
        printf("Error writing the row data!!\n");
        exit(-1);
      }
    }
  }
  return NULL;
}


/*
  Function: handle_output

  This function will read row data from the fractal calculating pthreads. As the pthreads
  calculate the row data, they will pass the data using the row_data struct, allowing the 
  data for the row, and the row number, to be passed to the thread using this function.
  At such time, this thread will save the row data in an array. After saving the PNG image
  will be written, keeping track of proper row numbers. As the image is printed, the memory
  will be released, allowing the overall program a smaller overhead.

  Input: 
        void *row_data_pipe: pointer to the file descriptor of the pipe for the row data
  Output:
        NULL
*/
void *handle_output(void *row_data_pipe){
  int next_row;
  int read_pipe;
  png_bytep *rows;
  struct row_data read_data;

  read_pipe = *((int *) row_data_pipe);

  // Make an array to hold the data for the individual rows 
  //  and initilize all of the pointers to NULL
  rows = (png_bytep *)malloc(sizeof(png_bytep)*s_height);
  for (next_row=0; next_row<s_height; next_row++)
    rows[next_row]=NULL;

  /*
    Read the row data from one of the thread that are calculating the image.
    Once this data has been read, add the row data to the image, using the given row number. 
    Next, use the array to write the next row in the image, until the first break in 
    the image is found. At this point, start again with reading data from the from the 
    calculation threads.
   */
  next_row = 0;
  while(next_row != s_height){
    if(read(read_pipe, &read_data, sizeof(struct row_data)) != sizeof(struct row_data)){
      printf("Error reading return values\n");
      exit(-1);
    }
    // Save row data
    rows[read_data.row_number]=read_data.vals;
    
    // Write as many rows as is possible, with the current information
    while(rows[next_row] != NULL){
      png_write_row(png_ptr,rows[next_row]); // Write image row
      free(rows[next_row]);
      rows[next_row]=NULL;
      next_row++;
    }
  }
  free(rows);
  return NULL; // Required from pthread
}


/*
  Function: calculate_row

  This function takes a row number and calculates the value of the fractal at each pixel in the row.
  With this value, the proper color is found and converted using the proper bit depth.

  Preprocessor Flags:
        EIGHT_BIT: If this flag is set, the function will calculate the pixels using an 
                   eight bit color scheme. If the flag is not set, the pixels will be calculated
                   using a sixteen bit scheme
  Input: 
        int i: row number for the row that this function is computing
  Output:
        png_bytep: a pointer to the bytes which will be used to write a single row of the PNG image
*/
png_bytep calculate_row(int i){
  png_bytep vals;
  double result;
  int j, start, ii;

  // Allocate space to store the row data
  if ((vals = (png_bytep) malloc(s_width*sizeof(png_byte)*(BIT_DEPTH/8)*3)) == NULL){
    printf("Bad allocaion of row data!\n");
    _exit(-1);
  }


  for(j=0; j < s_width; j++){
    // Calculate the result
    result = calculate_escape(j, i);

    // Find the start of the correct pixel
    start = j*3*BIT_DEPTH/8;

    // If the pixel is in the set (recurses to the limit), set the pixel to black
    if (result >= 1.0){
      for(ii=0;ii<((BIT_DEPTH/8)*3); ii++) 
        vals[start++]=0x00;
    }

    /* 
       Here we use the value returned by the calculat_escape function to determine
       the color of the pixel. For the eight bit encoding, there are three png_bytes of 
       color data, whereas the sixteen bit encoding has 6 png_bytes
    */ 
#ifdef EIGHT_BIT
    else {
      vals[start++]=0xFF-(png_byte)(result*0xFF);
      vals[start++]=0xFF-(png_byte)(result*0x77);
      vals[start]=0xFF-(png_byte)(result*0xFF);
    }
#else
    else{
      vals[start++]=0xDD-(png_byte)(result*0xAA);
      vals[start++]=0xFF-(png_byte)(result*0xFF);
      vals[start++]=0xFF-(png_byte)(result*0xFF);
      vals[start++]=0xFF-(png_byte)(result*0xFF);
      vals[start++]=0xFF-(png_byte)(result*0x77);
      vals[start]=0xFF-(png_byte)(result*0xFF);
        
    }
#endif

  }
  return vals;
}

/*
  Function: calculate_escape

  This is the function which tests each point to find if it is in the fractal and if not, the escape
  from that set. 

  ------------------------------------------------------------------------
  Background:
  ------------------------------------------------------------------------
  For this, the following basic recursive definition of the fractal is used, assuming
  the complex value c is the point being tested. The Mandelbrot set is traditionally defined for (a+bi) = 2.

  Base Case:
  Z(0) = c
  Recursive Case:
  Z(N+1) = Z(N)^(a+bi)+c

  The fractal is defined to be the set of points C for which the following limit is held.

  lim N->Inf |Z(N)| < bound, bound > 0, bound element of the Reals.

  For the Mandelbrot set [(a+bi) = 2] is independant of the value of the bound, given that b >= 2.

  Traditionally, the Mandelbrot fractal is displayed as a colorful image. This is obtained by noting the 
  value of N and |Z(N)| for the first value that is larger than the bound. The following formula is used 
  to generate a value in the range (0,1), based on the Taylor expansion shown at
  http://linas.org/art-gallery/escape/escape.html.

  modN = N + 1 - log(log(|Z(N)|)) / log(r^a * e^(-b * theta))
  
  The justification of the final term is shown below. The returned value is modN divided by the 
  maximum number of iterations allowed by the program.

  ------------------------------------------------------------------------
  Implementation:
  ------------------------------------------------------------------------
  The recursive case can be solved using the following expressions:

  Define The Following:
                Z(N)        = r e^(i * theta)       -> Polar form of a complex number
                Coefficient = r^a * e^(-b * theta)  = Coe 
                Angle       = a * theta + b * ln(r) = Ang

  From these definitions, we can write the recursive form in an easily calculable form, 
         where Re{Z} is the real component of Z and Im{Z} is the imaginary component of Z:
                Re{Z(N+1)} = Coe * cos ( Ang ) + Re{c}
                Im{Z(N+1)} = Coe * sin ( Ang ) + Im{c}

  Now, we convert the value Z(N+1) from Standard Form to Polar Form:
                Z(N+1) = r e^(i * theta)
                r      = Sqrt(Re{Z(N+1)}*Re{Z(N+1)}+Im{Z(N+1)}*Im{Z(N+1)})
                theta  = arctan2(Re{Z(N+1)}, Im{Z(N+1)})

  In order to save us from using the square root function, we can use the following modifications
                Z(N+1) = Sqrt(rsq) * e^(i * theta)
                rsq    = Re{Z(N+1)}*Re{Z(N+1)}+Im{Z(N+1)}*Im{Z(N+1)}
                theta  = arctan2(Re{Z(N+1)}, Im{Z(N+1)})

  For this to b used, we also must modify the original definitions:
                Z(N)        = Sqrt(rsq) * e^(i * theta)
                Coefficient = rsq^(a/2) * e^(-b * theta)    = Coe 
                Angle       = a * theta + 1/2 * b * ln(rsq) = Ang

  ------------------------------------------------------------------------
  Branch Cuts:
  ------------------------------------------------------------------------
  One more discussion must be had before generating using these formulas, and that involves branch cuts.
  Branch cuts are important when discussing the value returned by the arctangent function. The function is
  defined such that:

       tan(x) = y <==> arctan(y) = x

  However, the following is true of the tangent function:
       tan(x) = tan(x + 2*pi) = tan(x + 4*pi) = tan(x - 2*pi) = .... = tan(x+2*pi*i), i-> integer

  The arctangent function traditionally returns a value in the range (-pi, pi]. However, due to the property
  of the tangent function, the value returned by the arctangent function can be in any range (x-pi, x+pi]
  for any value of x. The range returned is known as the branch, and the extreme values of the range are
  known as the branch cut (Note that in polar form, the angles x+pi and x-pi are coincident in the polar plane).

  Unfortunately for complex exponentiation, the choice of which value of x to use to center the range
  will affect the result that is obtained in a very meaningful way (Consider the formula above for Coe)

  For the generation of fractals, there are many ways to pick a meaningful branch. Below are two methods

  1. x = -b
           Use the imaginary component of the exponent to set the branch 

  2. x = Arg(c)
           Use the argument of the original point to determine the branch


  General Statements on branch cut:
  Generally I have used this modified exponent "Multibrot" set generator for three general cases
  and have the following generalizations about the "best" branch cut mode

  1. Integer values of a and b=0 (ie. Z(N+1) = Z(N)^4 + C)
      Here the branch cut method makes little to no difference

  2. Small values for b and a=2 ((ie. Z(N+1) = Z(N)^(2+0.01i) + C))
      I find the images look best with the first method of branch cut (flag not set)

  3. Non-integer values of a and b=0
     I have liked the images with the second method of the branch cut (flag is set)
  
  ------------------------------------------------------------------------
  Function:
  ------------------------------------------------------------------------
  Preprocessor Flags:
        BRANCH: If this flag is set, the function will calculate the branch cut for the arctangent
                function using the argument of the initial point (r e ^ (i theta)). If not, the 
                branch cut will be based on the complex component of the exponent in the recursive
                definition of the fractal.
  Input: 
        int x,y: The coordinates of the pixel being calculated in the PNG image
  Output:
        double: a pointer to the bytes which will be used to write a single row of the PNG image
*/
double calculate_escape(int x, int y){
  double reV, imV, a, b;
  double rsq, r, th, coe, ang;
  int i;

#ifdef BRANCH
  double br;
#endif

  reV = cornerR+s_scale*x;
  imV = cornerI-s_scale*y;

  a = reV, b = imV;
  rsq = a*a + b*b;

  if (rsq < MIN_R)
    i=DEPTH;
  else{
    i=0;
    th = atan2(b,a);

#ifdef BRANCH
    br = th;
    while (br > (M_PI-s_power_i))
      br -= 2*M_PI;
    while (br < (-1.*s_power_i-M_PI))
      br += 2*M_PI;
#endif

  }

  for(; i < DEPTH; i++){
// Perform a branch cut for the complex exponential    
#ifdef BRANCH
    while (th > (br+M_PI))
      th -= 2.0*M_PI;
    while (th < (br-M_PI))
      th += 2.0*M_PI;
#else
    while (th > (M_PI-s_power_i))
      th -= 2*M_PI;
    while (th < (-1.*s_power_i-M_PI))
      th += 2*M_PI;
#endif

    coe = pow(rsq, s_power_r/2.)*exp(-1.0*s_power_i*th);
    ang = s_power_r*th+0.5*s_power_i*log(rsq);

    a = coe*cos(ang)+reV;
    b = coe*sin(ang)+imV;

    rsq = a*a + b*b;
    th = atan2(b,a);

    if (rsq < MIN_R)
      return 1.0;

    if (rsq >= ESCAPE) {
      coe = pow(rsq, s_power_r/2.)*exp(-1.0*s_power_i*th);
      coe = 2.0*log(coe)/log(rsq);
      r = 1.0 - log(0.5*log(rsq)) / log(coe);
      r += (double) i;
      r = log(r)/log((double) DEPTH);
      return pow(r,1.0);
    }
  }
  return 1.00;
}


void _abort(const char * s, ...) {
  va_list args;
  va_start(args, s);
  vfprintf(stderr, s, args);
  fprintf(stderr, "\n");
  va_end(args);
  abort();
}
