#include "OpenCV.h"
#include <vector>

class Matrix;

class VideoCaptureWrap: public node::ObjectWrap {
  public:
      cv::VideoCapture cap;

      static Persistent<FunctionTemplate> constructor;
      static void Init(Handle<Object> target);
      static NAN_METHOD(New);
      
      VideoCaptureWrap(const std::string& filename);
      VideoCaptureWrap(int device); 
      ~VideoCaptureWrap();

      static NAN_METHOD(Read);
      static NAN_METHOD(ReadSync);

      //(Optional) For setting width and height of the input video stream 
      static NAN_METHOD(SetWidth);
      static NAN_METHOD(SetHeight);
      static NAN_METHOD(SetFrameRate);
      
      // to set frame position
      static NAN_METHOD(SetPosition);

      static NAN_METHOD(GetFrameAt);

      //close the stream
      static NAN_METHOD(Close);

      std::vector<Matrix*> readImages;
      unsigned int readCurrentImageIndex = 0;
      bool isReading = false;
};

