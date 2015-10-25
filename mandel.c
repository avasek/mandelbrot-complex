#include <pthread.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <png.h>
#include <stdint.h>


#define   DEPTH       2000
#define   ESCAPE      49.0
#define   MIN_R       0.0000001
#define   NUM_THREADS 4


//#define EIGHT_BIT 8

//#define BRANCH 1


#ifdef EIGHT_BIT
#define   BIT_DEPTH   8
#endif
#ifndef EIGHT_BIT
#define   BIT_DEPTH   16
#endif
#define   COLOR_TYPE  PNG_COLOR_TYPE_RGB
#define   INTERLACING PNG_INTERLACE_NONE

#define   BLACK   0xFF000000
#define   START   0x0022FF22
#define   END     0x008888FF

#define   MIN_DIM  100   

#define   FOLDER   "./Output"

static uint32_t s_width;
static uint32_t s_height;
static double s_scale;
static double s_center_r;
static double s_center_i;
static double s_power_r;
static double s_power_i;

static png_structp png_ptr;
static double      cornerR;
static double      cornerI;

//static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;


int create_image(uint32_t width, uint32_t height, double scale,
                 double center_r, double center_i, double power_r, double power_i);
void calc_image();
void *handle_pthread(void *ptr_pipe);
png_bytep calculate_row(int i);
double calculate_escape(int x, int y);
void *handle_output(void *row_data_pipe);

void   _abort(const char * s, ...);
double _absolute(double d);

struct row_data{
  int row_number;
  png_bytep vals;
};

struct pipes{
  int pipeRN; // Row Numbers
  int pipeRD; // Row Data
};

int main(int argc, char **argv){
  // For optarg()
  extern char *optarg; 
  extern int optind, opterr, optopt;

  // Command line arguments, with their default values
  // These values determine the location and type of plot to create
  uint32_t width = 1920; // w
  uint32_t height = 1080; // h
  double scale = 0.002; // s
  double center_r = -0.5; // r
  double center_i = 0.0; // i
  double power_r = 2.0; // a
  double power_i = 0.0; // b

  // Collect Command Line arguments
  int opt;
  while((opt=getopt(argc, argv, "w:h:s:r:i:a:b:")) != -1){
    if (optarg == NULL){
      printf("Optarg is null!!");
      return -1;
    }
    switch(opt) {
    case 'w': width=(uint32_t)strtoul(optarg, NULL, 0);
      break;
    case 'h': height=(uint32_t)strtoul(optarg, NULL, 0);
      break;
    case 's': scale=strtod(optarg,(char **) NULL);
      break;
    case 'r': center_r=strtod(optarg,(char **) NULL);
      break;
    case 'i': center_i=strtod(optarg,(char **) NULL);
      break;
    case 'a': power_r=strtod(optarg,(char **) NULL);
      break;
    case 'b': power_i=strtod(optarg,(char **) NULL);
      break;
    default: printf("Bad user argument: %c", (char) opt);
      break;
    }
  }

  if((width < MIN_DIM) || (height < MIN_DIM)){
    printf("Dimensions are too small: %d x %d\nMin: %d\n", width, height, MIN_DIM);
    return -1;
  }


  return create_image(width, height, scale, center_r, center_i, power_r, power_i);
}

int create_image(uint32_t width, uint32_t height, double scale, 
                 double center_r, double center_i, double power_r, double power_i){
  png_FILE_p fp;
  png_infop info_ptr;
  char *name;

  // Set static variables that describe the image that is being created
  s_width = width;
  s_height = height;
  s_scale = scale;
  s_center_r = center_r;
  s_center_i = center_i;
  s_power_r = power_r;
  s_power_i = power_i;


  // Create a filename based on the parameters given by the user
  // Uniquely describes a Mandelbrot image (within the accuracy of the printed values)
  name = (char *)malloc(200*sizeof(char));
  sprintf(name, "%s/Dimension: %dx%d, Center: %.4f%+.4fi, Scale: %.2e, Exp: %0.2e+%0.2ei.png", 
          FOLDER, width, height, center_r, center_i, scale, power_r, power_i);
  printf("Output Filename: %s\n", name);
  fp = (png_FILE_p) fopen(name,"wb");
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
  fclose((FILE *) fp);
  // Return zero on proper exit
  return 0;
}


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


void *handle_pthread(void *ptr_pipe){
  struct pipes pp = *((struct pipes *) ptr_pipe);

  int read_pipe = pp.pipeRN;
  int write_pipe = pp.pipeRD;
  int value=0;
  struct row_data *rowD;

  // Take the row numbers passed in the pipe and calculate the color the entire row
  while(value>=0){
    if(read(read_pipe, &value, sizeof(int))!=sizeof(int)){
      printf("Read Error!!\n");
      exit(-1);
    }
    if (value >= 0){
      if (NULL == (rowD = (struct row_data *) malloc(sizeof(struct row_data)))){
        printf("Error Allocating struct row_data");
        exit(-1);
      }
      rowD->vals = calculate_row(value);
      rowD->row_number = value;
      if(write(write_pipe, rowD, sizeof(struct row_data))!=sizeof(struct row_data)){
        printf("Read Error!!\n");
        exit(-1);
      }
    }
  }
  return NULL;
}


void *handle_output(void *row_data_pipe){
  int next_row;
  int read_pipe = *((int *) row_data_pipe);

  png_bytep *rows;
  struct row_data read_data;

  rows = (png_bytep *)malloc(sizeof(png_bytep)*s_height);

  for (next_row=0; next_row<s_height; next_row++)
    rows[next_row]=NULL;

  next_row = 0;

  while(next_row != s_height){
    if(read(read_pipe, &read_data, sizeof(struct row_data)) != sizeof(struct row_data)){
      printf("Error reading return values\n");
      exit(-1);
    }

    rows[read_data.row_number]=read_data.vals;
    
    while(rows[next_row] != NULL){
      png_write_row(png_ptr,rows[next_row]);
      free(rows[next_row]);
      rows[next_row]=NULL;
      next_row++;
    }
  }
  free(rows);
  return NULL;
}

png_bytep calculate_row(int i){
  png_bytep vals;
  int j;
  double result;

  if ((vals = (png_bytep) malloc(s_width*sizeof(png_byte)*(BIT_DEPTH/8)*3)) == NULL){
    printf("Bad allocaion of row data!\n");
    _exit(-1);
  }

  for(j=0; j < s_width; j++){
    result = calculate_escape(j, i);
    int start = j*3*BIT_DEPTH/8;
    int ii=0;
    if (result > 0.9999999){
      for(;ii<((BIT_DEPTH/8)*3); ii++) 
        vals[start++]=0x00;
    }
#ifdef EIGHT_BIT
    else {
      if (result < 0.1){
        vals[start++]=0x22;
        vals[start++]=0xFF;
        vals[start]=0x22; 
      } else {
        vals[start++]=0x88;
        vals[start++]=0x88;
        vals[start]=0xFF; 
      }
    }
#endif
#ifndef EIGHT_BIT
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
    while (th < (-1.*s_power_i-M_PI))
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
#endif

#ifndef BRANCH
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
      r = 1-2.0*log(0.5*log(rsq))/log(s_power_r*s_power_r+s_power_i*s_power_i);
      r += (double) i;
      r = r/((double) DEPTH);
      return pow(r,0.2);
    }
  }
  return 1.00;
}



double _absolute(double d){
  return d<0 ? -1.0*d : d;
}

void _abort(const char * s, ...) {
  va_list args;
  va_start(args, s);
  vfprintf(stderr, s, args);
  fprintf(stderr, "\n");
  va_end(args);
  abort();
}
