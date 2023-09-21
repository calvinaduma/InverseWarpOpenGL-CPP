/*
  Example inverse warps
*/

#include <cmath>

/*
  Routine to inverse map (x, y) output image spatial coordinates
  into (u, v) input image spatial coordinates

  Call routine with (x, y) spatial coordinates in the output
  image. Returns (u, v) spatial coordinates in the input image,
  after applying the inverse map. Note: (u, v) and (x, y) are not 
  rounded to integers, since they are true spatial coordinates.
 
  inwidth and inheight are the input image dimensions
  outwidth and outheight are the output image dimensions
*/

/*
  better the interpolation scheme
*/
#include "matrix.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <OpenImageIO/imageio.h>
#include <fstream>
#include <math.h>
#include <stdio.h>

#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

using namespace std;
OIIO_NAMESPACE_USING;
using std::string;

void performWarp();

struct Pixel{
    unsigned char r,g,b,a;
};

int CHANNELS = 4;
int imWIDTH, imHEIGHT;
int winWIDTH, winHEIGHT;
int vpWIDTH, vpHEIGHT;
int Xoffset, Yoffset;
int pixel_format;
Pixel** IN = NULL;
Pixel** OUT = NULL;
string output_filename = "";
string input_filename = "";
bool readInput = false;
Matrix3D M;

/* 
 Bilinear Interpolation function
 based on Weighted mean from https://en.wikipedia.org/wiki/Bilinear_interpolation
*/
void bilinearInterpolation(int x, int y){
  int blueArea, yellowArea, redArea, greenArea;
  int x1, x2, y1, y2;
  float combinedArea;
  Pixel greenPixel, redPixel, yellowPixel, bluePixel;
  int x_bound = imWIDTH, y_bound = imHEIGHT;

  // the bounds between x2-x1 and y2-y1 equals 6: (x2-x1)=6 & (y2-y1)=6
  x1 = (x - 1)>=0 ? x-1 : 0;
  x2 = (x + 5)<x_bound ? x + 5 : x_bound-1;
  y1 = (y - 4)>=0 ? y - 4 : 0;
  y2 = (y + 2)<y_bound ? y + 2 : y_bound-1;
  cout << "x: " << x << ", y: " << y << endl;
  cout << "GreenPixel (x1,y1): (" << x1 << "," << y1 << ")" << endl;
  cout << "RedPixel (x2,y1): (" << x2 << "," << y1 << ")" << endl;
  cout << "YellowPixel (x1,y2): (" << x1 << "," << y2 << ")" << endl;
  cout << "BluePixel (x2,y2): (" << x2 << "," << y2 << ")" << endl;

  // get the area
  blueArea = (x-x1)*(y-y1);
  yellowArea = (x2-x)*(y-y1);
  redArea = (x-x1)*(y2-y);
  greenArea = (x2-x)*(y2-y);
  cout << "blueArea: " << blueArea << endl;
  cout << "greenArea: " << greenArea << endl;
  cout << "yellowArea: " << yellowArea << endl;
  cout << "redArea: " << redArea << endl << endl;

  // get the points
  greenPixel = IN[y1][x1];
  redPixel = IN[y1][x2];
  yellowPixel = IN[y2][x1];
  bluePixel = IN[y2][x2];

  // combined Area
  combinedArea = blueArea + yellowArea + redArea + greenArea;
  cout << "combinedArea: " << combinedArea << endl;

  IN[y][x].r = ((redPixel.r*redArea)+(greenPixel.r*greenArea)+(yellowPixel.r*yellowArea)+(bluePixel.r*blueArea))/combinedArea;
  IN[y][x].g = ((redPixel.g*redArea)+(greenPixel.g*greenArea)+(yellowPixel.g*yellowArea)+(bluePixel.g*blueArea))/combinedArea;
  IN[y][x].b = ((redPixel.b*redArea)+(greenPixel.b*greenArea)+(yellowPixel.b*yellowArea)+(bluePixel.b*blueArea))/combinedArea;
  IN[y][x].a = ((redPixel.a*redArea)+(greenPixel.a*greenArea)+(yellowPixel.a*yellowArea)+(bluePixel.a*blueArea))/combinedArea;
}

void repairImage(){
  float area1, area2, area3, area4;
  for (int y=0; y<imHEIGHT; y++)
    for (int x=0; x<imWIDTH; x++){
      bilinearInterpolation(x,y);
    }
}
void inv_map(float x, float y, float &u, float &v,
	int inwidth, int inheight, int outwidth, int outheight){
  
  x /= outwidth;		// normalize (x, y) to (0...1, 0...1)
  y /= outheight;

  u = x/2;
  v = y/2; 
  
  u *= inwidth;			// scale normalized (u, v) to pixel coords
  v *= inheight;
}

void inv_map2(float x, float y, float &u, float &v,
	int inwidth, int inheight, int outwidth, int outheight){
  
  x /= outwidth;		// normalize (x, y) to (0...1, 0...1)
  y /= outheight;

  u = 0.5 * (x * x * x * x + sqrt(sqrt(y)));
  v = 0.5 * (sqrt(sqrt(x)) + y * y * y * y);
  
  u *= inwidth;			// scale normalized (u, v) to pixel coords
  v *= inheight;
}

void readImage( string input_filename ){
  // Create the oiio file handler for the image, and open the file for reading the image.
  // Once open, the file spec will indicate the width, height and number of channels.
  auto infile = ImageInput::open( input_filename );
  if ( !infile ){
      cerr << "Failed to open input file: " << input_filename << ". Exiting... " << endl;
      exit( 1 );
  }
  // Record image width, height and number of channels in global variables
  imWIDTH = infile->spec().width;
  imHEIGHT = infile->spec().height;
  CHANNELS = infile->spec().nchannels;
	winWIDTH = imWIDTH;
	winHEIGHT = imHEIGHT;

  // allocate temporary structure to read the image
  unsigned char temp_pixels[ imWIDTH * imHEIGHT * CHANNELS ];
  // read the image into the tmp_pixels from the input file, flipping it upside down using negative y-stride,
  // since OpenGL pixmaps have the bottom scanline first, and
  // oiio expects the top scanline first in the image file.
  int scanline_size = imWIDTH * CHANNELS * sizeof( unsigned char );
  if( !infile->read_image( TypeDesc::UINT8, &temp_pixels[0] + (imHEIGHT - 1) * scanline_size, AutoStride, -scanline_size)){
      cerr << "Failed to read file " << input_filename << ". Exiting... " << endl;
      exit( 0 );
  }

  // allocates the space necessary for data struct
  IN = new Pixel*[imHEIGHT];
	IN[0] = new Pixel[imHEIGHT*imWIDTH];
	for( int i=1; i<imHEIGHT; i++ ) IN[i] = IN[i-1] + imWIDTH;


  int idx;
  for( int y=0; y<imHEIGHT; ++y ){
    for( int x=0; x<imWIDTH; ++x ){
      idx = ( y * imWIDTH + x ) * CHANNELS;
      if( CHANNELS == 1 ){
        IN[y][x].r = temp_pixels[idx];
        IN[y][x].g = temp_pixels[idx];
        IN[y][x].b = temp_pixels[idx];
        IN[y][x].a = 255;
      } else {
        IN[y][x].r = temp_pixels[idx];
        IN[y][x].g = temp_pixels[idx + 1];
        IN[y][x].b = temp_pixels[idx + 2];
// no alpha value present
        if( CHANNELS < 4 ) IN[y][x].a = 255;
// alpha value present
        else IN[y][x].a = temp_pixels[idx + 3];
      }
    }
  }

  // close the image file after reading, and free up space for the oiio file handler
  infile->close();
  pixel_format = GL_RGBA;
  CHANNELS = 4;
}

void writeImage( string filename ){
    // make a pixmap that is the size of the window and grab OpenGL framebuffer into it
    // alternatively, you can read the pixmap into a 1d array and export this
    unsigned char temp_pixmap[ winWIDTH * winHEIGHT * CHANNELS ];
    glReadPixels( 0, 0, winWIDTH, winHEIGHT, pixel_format, GL_UNSIGNED_BYTE, temp_pixmap );

    // create the oiio file handler for the image
    auto outfile = ImageOutput::create( filename );
    if( !outfile ){
        cerr << "Failed to create output file: " << filename << ". Exiting... " << endl;
        exit( 1 );
    }

    // Open a file for writing the image. The file header will indicate an image of
    // width WinWidth, height WinHeight, and ImChannels channels per pixel.
    // All channels will be of type unsigned char
    ImageSpec spec( winWIDTH, winHEIGHT, CHANNELS, TypeDesc::UINT8 );
    if (!outfile->open( filename, spec )){
        cerr << "Failed to open output file: " << filename << ". Exiting... " << endl;
        exit( 1 );
    }

    // Write the image to the file. All channel values in the pixmap are taken to be
    // unsigned chars. While writing, flip the image upside down by using negative y stride,
    // since OpenGL pixmaps have the bottom scanline first, and oiio writes the top scanline first in the image file.
    int scanline_size = winWIDTH * CHANNELS * sizeof( unsigned char );
    if( !outfile->write_image( TypeDesc::UINT8, temp_pixmap + (winHEIGHT - 1) * scanline_size, AutoStride, -scanline_size )){
        cerr << "Failed to write to output file: " << filename << ". Exiting... " << endl;
        exit( 1 );
    }

    // close the image file after the image is written and free up space for the
    // ooio file handler
    outfile->close();
}

/*
    Routine to display a pixmap in the current window
*/
void displayImage(){
    // if the window is smaller than the image, scale it down, otherwise do not scale
    if(winWIDTH < imWIDTH  || winHEIGHT < imHEIGHT)
        glPixelZoom(float(vpWIDTH) / imWIDTH, float(vpHEIGHT) / imHEIGHT);
    else
        glPixelZoom(1.0, 1.0);

        // display starting at the lower lefthand corner of the viewport
        glRasterPos2i(0, 0);

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glDrawPixels(imWIDTH, imHEIGHT, pixel_format, GL_UNSIGNED_BYTE, IN[0]);
}

/*
* Displays currrent pixmap
*/
void handleDisplay(){
    // specify window clear (background) color to be opaque black
    glClearColor( 0, 0, 0, 1 );
    // clear window to background color
    glClear( GL_COLOR_BUFFER_BIT );

    // only draw the image if it is of a valid size
    if( imWIDTH > 0 && imHEIGHT > 0) displayImage();

    // flush the OpenGL pipeline to the viewport
    glFlush();
}

/*
    Reshape Callback Routine: If the window is too small to fit the image,
    make a viewport of the maximum size that maintains the image proportions.
    Otherwise, size the viewport to match the image size. In either case, the
    viewport is centered in the window.
*/
void handleReshape(int w, int h){
    float imageaspect = ( float )imWIDTH / (float )imHEIGHT;	// aspect ratio of image
    float newaspect = ( float  )w / ( float )h; // new aspect ratio of window

    // record the new window size in global variables for easy access
    winWIDTH = w;
    winHEIGHT = h;

    // if the image fits in the window, viewport is the same size as the image
    if( w >= imWIDTH && h >= imHEIGHT ){
    Xoffset = ( w - imWIDTH ) / 2;
    Yoffset = ( h - imHEIGHT ) / 2;
    vpWIDTH = imWIDTH;
    vpHEIGHT = imHEIGHT;
    }
    // if the window is wider than the image, use the full window height
    // and size the width to match the image aspect ratio
    else if( newaspect > imageaspect ){
    vpHEIGHT = h;
    vpWIDTH = int( imageaspect * vpHEIGHT );
    Xoffset = int(( w - vpWIDTH) / 2 );
    Yoffset = 0;
    }
    // if the window is narrower than the image, use the full window width
    // and size the height to match the image aspect ratio
    else{
    vpWIDTH = w;
    vpHEIGHT = int( vpWIDTH / imageaspect );
    Yoffset = int(( h - vpHEIGHT) / 2 );
    Xoffset = 0;
    }

    // center the viewport in the window
    glViewport( Xoffset, Yoffset, vpWIDTH, vpHEIGHT );

    // viewport coordinates are simply pixel coordinates
    glMatrixMode( GL_PROJECTION );
    glLoadIdentity();
    gluOrtho2D( 0, vpWIDTH, 0, vpHEIGHT );
    glMatrixMode( GL_MODELVIEW );
}

/*
	Handles Keyboard click
	Any key press exits program
*/
void handleKeyboard( unsigned char key, int x, int y ){
	switch ( key ){
    case 'W':  case 'w':
      if (output_filename != "")
        writeImage( output_filename );
      else{
        cout << "Enter name for output file: ";
        cin >> output_filename;
        writeImage( output_filename );
      }
      break;
    case 'm': case 'M':
      performWarp();
      glutPostRedisplay();
      break;
    case 'r': case 'R':
      repairImage();
      glutPostRedisplay();
      break;
    case 'Q': case 'q':
      exit(0);
		default:
			return;
	}
}

void performWarp(){
  OUT = new Pixel*[imHEIGHT];
	OUT[0] = new Pixel[imHEIGHT*imWIDTH];
	for( int i=1; i<imHEIGHT; i++ ) OUT[i] = OUT[i-1] + imWIDTH;

  for (int x=0; x<imWIDTH; x++)
    for (int y=0; y<imHEIGHT; y++){
      float u=0, v=0;
      inv_map(x, y, u, v, imWIDTH, imHEIGHT, imWIDTH, imHEIGHT);
      OUT[y][x] = IN[(int)v][(int)u];
    }
  
  // place OUT data back into IN for display
	delete IN[0];
	delete IN;
	IN = new Pixel*[imHEIGHT];
	IN[0] = new Pixel[imWIDTH*imHEIGHT];
	for (int i=1; i<imHEIGHT; i++) IN[i] = IN[i-1] + imWIDTH;

	for (int y=0; y<imHEIGHT; y++)
		for (int x=0; x<imWIDTH; x++)
			IN[y][x] = OUT[y][x];

  delete OUT[0];
	delete OUT;
}

/*
   Main program to read an image file, then ask the user
   for transform information, transform the image and display
   it using the appropriate warp.  Optionally save the transformed
   images in  files.
*/
int main(int argc, char *argv[]){
	if( argc < 2 ){
		cout << "ERROR! Not enough arguments input to command line! Exiting..." << endl;
		exit(1);
	} else if( argc > 4 ){
		cout << "ERROR! Too many arguments input to command line! Exiting..." << endl;
		exit(1);
	} else if( argc == 3 ){
		output_filename = argv[2];
	}

	//your code to read in the input image
	input_filename = argv[1];
	readImage( input_filename );

	// completes the affine transformation with forward mapping
	// ForwardMap(M);

	//your code to display the warped image
	glutInit( &argc, argv );
    glutInitDisplayMode( GLUT_RGBA );
    glutInitWindowSize( winWIDTH, winHEIGHT );
    glutCreateWindow( "WARPED IMAGE" );

	glutDisplayFunc( handleDisplay );
	glutKeyboardFunc( handleKeyboard );
    glutReshapeFunc( handleReshape );	
	//glutMouseFunc( handleMouseClick );
	
	glutMainLoop();
   	return 0;
}
