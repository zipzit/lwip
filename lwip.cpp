#include "lwip.h"

using namespace cimg_library;
using namespace v8;

void openImageAsync(uv_work_t * request){
    // here is where we actually open the image file and retrieve the binary
    // data. open image, store data in iob->imgData.
    ImageOpenBaton * iob = static_cast<ImageOpenBaton *>(request->data);
    // TODO: choose decoder according to format. Currently only jpeg.
    try{
        iob->imgData = new CImg<unsigned char>();
        iob->imgData->load_jpeg(iob->imgPath.c_str());
    } catch (CImgException e){
        delete iob->imgData;
        iob->err = true;
        iob->errMsg = "Unable to open file as JPEG";
    }
    return;
}

void openImageAsyncDone(uv_work_t * request, int status){
    // reading image completed. now we call the callback.
    ImageOpenBaton * iob = static_cast<ImageOpenBaton *>(request->data);
    if (iob->err){
        // define the arguments for the callback
        const unsigned int argc = 1;
        Local<Value> argv[argc] = {
            Local<Value>::New(Exception::Error(String::New(iob->errMsg.c_str())))
        };
        // run the callback
        iob->cb->Call(Context::GetCurrent()->Global(), argc, argv);
    } else {
        // build image object
        Handle<Value> imgObj = LwipImage::NewInstance();
        LwipImage * lio = node::ObjectWrap::Unwrap<LwipImage>(Handle<Object>::Cast(imgObj));
        lio->_data = iob->imgData;
        // define the arguments for the callback
        const unsigned int argc = 2;
        Local<Value> argv[argc] = {
            Local<Value>::New(Null()),
            Local<Value>::New(imgObj)
        };
        // run the callback
        iob->cb->Call(Context::GetCurrent()->Global(), argc, argv);
    }
    // dispose of cb, because it's a persistent function
    iob->cb.Dispose();
    // free baton's memory
    delete iob;
}

// create 'open' function template
Handle<Value> open(const Arguments &args){
    // this scope will discard all internal local objects for us.
    HandleScope scope;

    // arguments validation:
    if (args.Length() < 2){
        ThrowException(Exception::TypeError(String::New("'open' takes 2 arguments: (String, Function)")));
        return scope.Close(Undefined());
    }
    if (!args[0]->IsString()){
        ThrowException(Exception::TypeError(String::New("First argument must be a path string")));
        return scope.Close(Undefined());
    }
    if (!args[1]->IsFunction()){
        ThrowException(Exception::TypeError(String::New("Second argument must be a callback function")));
        return scope.Close(Undefined());
    }

    // open the file in 'path' asynchronously
    // the baton is on the heap because it lives in async calls outside the
    // scope of the current function
    ImageOpenBaton * iob = new ImageOpenBaton();
    if (iob == NULL){
        ThrowException(Exception::TypeError(String::New("Out of memory")));
        return scope.Close(Undefined());
    }
    iob->request.data = iob;
    iob->cb = Persistent<Function>::New(Local<Function>::Cast(args[1]));
    iob->imgPath = std::string(*String::Utf8Value(args[0]->ToString()));
    uv_queue_work(uv_default_loop(), &iob->request, openImageAsync, openImageAsyncDone);

    // Close the scope and return 'undefined'
    return scope.Close(Undefined());
}

// create an init function for our node module
void init(Handle<Object> exports, Handle<Object> module){
    LwipImage::Init();
    NODE_SET_METHOD(exports, "open", open);
}

// use NODE_MODULE macro to register our module:
NODE_MODULE(lwip,init)
