#include "VideoCaptureWrap.h"
#include "Matrix.h"
#include "OpenCV.h"

#include  <iostream>
using namespace std;


v8::Persistent<FunctionTemplate> VideoCaptureWrap::constructor;

struct videocapture_baton {

	Persistent<Function> cb;
	VideoCaptureWrap *vc;
	Matrix *im;

	uv_work_t request;
};

void
VideoCaptureWrap::Init(Handle<Object> target) {
  NanScope();

	//Class
  Local<FunctionTemplate> ctor = NanNew<FunctionTemplate>(VideoCaptureWrap::New);
  NanAssignPersistent(constructor, ctor);
  ctor->InstanceTemplate()->SetInternalFieldCount(1);
  ctor->SetClassName(NanNew("VideoCapture"));
  
  // Prototype
	//Local<ObjectTemplate> proto = constructor->PrototypeTemplate();

	NODE_SET_PROTOTYPE_METHOD(ctor, "read", Read);
	NODE_SET_PROTOTYPE_METHOD(ctor, "setWidth", SetWidth);
    NODE_SET_PROTOTYPE_METHOD(ctor, "setHeight", SetHeight);
    NODE_SET_PROTOTYPE_METHOD(ctor, "setFrameRate", SetFrameRate);
	NODE_SET_PROTOTYPE_METHOD(ctor, "setPosition", SetPosition);
  NODE_SET_PROTOTYPE_METHOD(ctor, "close", Close);
  NODE_SET_PROTOTYPE_METHOD(ctor, "ReadSync", ReadSync);

	target->Set(NanNew("VideoCapture"), ctor->GetFunction());
};    

NAN_METHOD(VideoCaptureWrap::New) {
	NanScope();

  if (args.This()->InternalFieldCount() == 0)
		return NanThrowTypeError("Cannot Instantiate without new");

	VideoCaptureWrap *v;

	if (args[0]->IsNumber()){
		v = new VideoCaptureWrap(args[0]->NumberValue());
	} else {
    //TODO - assumes that we have string, verify
    v = new VideoCaptureWrap(std::string(*NanAsciiString(args[0]->ToString())));
  }


	v->Wrap(args.This());

	NanReturnValue(args.This());
}

VideoCaptureWrap::VideoCaptureWrap(int device){

	NanScope();
	cap.open(device);

	readImages[0] = new Matrix();
	readImages[1] = new Matrix();
	readCurrentImageIndex = 0;
	isReading = false;

	if(!cap.isOpened()){
    NanThrowError("Camera could not be opened");
	}
}

VideoCaptureWrap::VideoCaptureWrap(const std::string& filename){
  NanScope();
	cap.open(filename);
  // TODO! At the moment this only takes a full path - do relative too.
	if(!cap.isOpened()){
    NanThrowError("Video file could not be opened (opencv reqs. non relative paths)");
	}
}

VideoCaptureWrap::~VideoCaptureWrap() {
    delete readImages[0];
    delete readImages[1];
}

NAN_METHOD(VideoCaptureWrap::SetWidth){

	NanScope();
	VideoCaptureWrap *v = ObjectWrap::Unwrap<VideoCaptureWrap>(args.This());

	if(args.Length() != 1)
		NanReturnUndefined();
	
	int w = args[0]->IntegerValue();

	if(v->cap.isOpened())
		v->cap.set(CV_CAP_PROP_FRAME_WIDTH, w);

	NanReturnUndefined();
}

NAN_METHOD(VideoCaptureWrap::SetHeight){

	NanScope();
	VideoCaptureWrap *v = ObjectWrap::Unwrap<VideoCaptureWrap>(args.This());

	if(args.Length() != 1)
		NanReturnUndefined();
	
	int h = args[0]->IntegerValue();

	v->cap.set(CV_CAP_PROP_FRAME_HEIGHT, h);

	NanReturnUndefined();
}

NAN_METHOD(VideoCaptureWrap::SetFrameRate){

    NanScope();
    VideoCaptureWrap *v = ObjectWrap::Unwrap<VideoCaptureWrap>(args.This());

    if(args.Length() != 1)
        NanReturnUndefined();
    
    int fps = args[0]->IntegerValue();

    if(v->cap.isOpened())
        v->cap.set(CV_CAP_PROP_FPS, fps);

    NanReturnUndefined();
}


NAN_METHOD(VideoCaptureWrap::SetPosition){

	NanScope();
	VideoCaptureWrap *v = ObjectWrap::Unwrap<VideoCaptureWrap>(args.This());

	if(args.Length() != 1)
		NanReturnUndefined();
	
	int pos = args[0]->IntegerValue();

	v->cap.set(CV_CAP_PROP_POS_FRAMES, pos);

	NanReturnUndefined();
}

NAN_METHOD(VideoCaptureWrap::Close){

	NanScope();
	VideoCaptureWrap *v = ObjectWrap::Unwrap<VideoCaptureWrap>(args.This());

	v->cap.release();

	NanReturnUndefined();
}


class AsyncVCWorker : public NanAsyncWorker {
 public:
  AsyncVCWorker(NanCallback *callback, VideoCaptureWrap* vc)
    : NanAsyncWorker(callback), vc(vc) {
        if (vc->isReading) {
          cout << "1A: entering when isReading is true\n";
          readFromCamera = false;
          // return image from previously read slot
          matrix = vc->readImages[1 - vc->readCurrentImageIndex];
        } else {
          cout << "1B: entering when isReading is false\n";
          vc->isReading = true;
          readFromCamera = true;
          // read into current slot
          matrix = vc->readImages[vc->readCurrentImageIndex];
        }
    }
  ~AsyncVCWorker() {}

  // Executed inside the worker-thread.
  // It is not safe to access V8, or V8 data structures
  // here, so everything we need for input and output
  // should go on `this`.
  void Execute () {
      if (readFromCamera)
        this->vc->cap.read(matrix->mat);
  }

  // Executed when the async work is complete
  // this function will be run inside the main event loop
  // so it is safe to use V8 again
  void HandleOKCallback () {
    NanScope();
    
    Local<Object> im_to_return= NanNew(Matrix::constructor)->GetFunction()->NewInstance();
	  Matrix *img = ObjectWrap::Unwrap<Matrix>(im_to_return);
	  cv::Mat mat;
	  mat = this->matrix->mat;
	  img->mat = mat;

    Local<Boolean> didRead = NanNew(readFromCamera);

    // update the slot to read the next image into
    if (readFromCamera) {
      cout << "2B: exiting when isReading was false\n";
      vc->readCurrentImageIndex = 1 - vc->readCurrentImageIndex;    
      vc->isReading = false;
    } else {
      cout << "2A: exiting when isReading was true\n";
    }


    Local<Value> argv[] = {
        NanNull()
      , im_to_return
      , didRead
    };

    TryCatch try_catch;
    callback->Call(3, argv);
    if (try_catch.HasCaught()) {
      FatalException(try_catch);
    }
  }

 private:
  VideoCaptureWrap *vc;
  Matrix* matrix;
  bool readFromCamera;
};



NAN_METHOD(VideoCaptureWrap::Read) {

	NanScope();

  	  VideoCaptureWrap *v = ObjectWrap::Unwrap<VideoCaptureWrap>(args.This());

	  REQ_FUN_ARG(0, cb);

      NanCallback *callback = new NanCallback(cb.As<Function>());
      NanAsyncQueueWorker(new AsyncVCWorker(callback, v));	

	NanReturnUndefined();
}


NAN_METHOD(VideoCaptureWrap::ReadSync) {

	NanScope();
	VideoCaptureWrap *v = ObjectWrap::Unwrap<VideoCaptureWrap>(args.This());

  Local<Object> im_to_return= NanNew(Matrix::constructor)->GetFunction()->NewInstance();
  Matrix *img = ObjectWrap::Unwrap<Matrix>(im_to_return);

  v->cap.read(img->mat);

	NanReturnValue(im_to_return);
}
