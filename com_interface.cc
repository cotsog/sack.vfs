#include <node.h>
#include <node_object_wrap.h>
#include <v8.h>
#include <uv.h>
//#include <nan.h>

#include "src/sack.h"
#undef New

using namespace v8;

static struct local {
	int data;
	uv_loop_t* loop;

} l;


#include "global.h"

Persistent<Function> ComObject::constructor;

ComObject::ComObject( char *name ) {
   this->name = name;
	handle = SackOpenComm( name, 0, 0 );
}

ComObject::~ComObject() {
	if( handle >=0 )
      SackCloseComm( handle );
  	Deallocate( char*, name );
}


void ComObject::Init( Handle<Object> exports ) {
		Isolate* isolate = Isolate::GetCurrent();
		Local<FunctionTemplate> comTemplate;

		comTemplate = FunctionTemplate::New( isolate, New );
		comTemplate->SetClassName( String::NewFromUtf8( isolate, "sack.vfs.Volume" ) );
		comTemplate->InstanceTemplate()->SetInternalFieldCount( 1 ); // 1 required for wrap

		// Prototype
		NODE_SET_PROTOTYPE_METHOD( comTemplate, "onRead", onRead );
		NODE_SET_PROTOTYPE_METHOD( comTemplate, "write", writeCom );
		NODE_SET_PROTOTYPE_METHOD( comTemplate, "close", closeCom );

		
		constructor.Reset( isolate, comTemplate->GetFunction() );
		exports->Set( String::NewFromUtf8( isolate, "ComPort" ),
			comTemplate->GetFunction() );
}


struct msgbuf {
	size_t buflen;
   uint8_t buf[1];
};


static void asyncmsg( uv_async_t* handle ) {
	// Called by UV in main thread after our worker thread calls uv_async_send()
	//    I.e. it's safe to callback to the CB we defined in node!
	v8::Isolate* isolate = v8::Isolate::GetCurrent();
	
	ComObject* myself = (ComObject*)handle->data;

	HandleScope scope(isolate);
	//lprintf( "async message notice. %p", myself );
	{
		struct msgbuf *msg;
		while( msg = (struct msgbuf *)DequeLink( &myself->readQueue ) ) {
			size_t length;
			Local<SharedArrayBuffer> ab =
				SharedArrayBuffer::New( isolate,
											  msg->buf,
											  length = msg->buflen );
			Local<Uint8Array> ui = Uint8Array::New( ab, 0, length );

			Local<Value> argv[] = { ui };
			Local<Function> cb = Local<Function>::New( isolate, myself->readCallback );
			//lprintf( "callback ... %p", myself );
			// using obj->jsThis  fails. here...
			cb->Call( isolate->GetCurrentContext()->Global(), 1, argv );
			//lprintf( "called ..." );
			Deallocate( struct msgbuf *, msg );
		}
	}
	//lprintf( "done calling message notice." );
}

void ComObject::New( const FunctionCallbackInfo<Value>& args ) {
		Isolate* isolate = args.GetIsolate();
		if( args.IsConstructCall() ) {
			char *portName;
			int argc = args.Length();
			if( argc > 0 ) {
				String::Utf8Value fName( args[0]->ToString() );
				portName = StrDup( *fName );
			}

			// Invoked as constructor: `new MyObject(...)`
			ComObject* obj = new ComObject( portName );
			if( obj->handle < 0 )
			{
				char msg[256];
				snprintf( msg, 256, "Failed to open %s", obj->name );
				isolate->ThrowException( Exception::Error( String::NewFromUtf8(isolate, msg) ) );
			}
			else {

				MemSet( &obj->async, 0, sizeof( obj->async ) );
				//Environment* env = Environment::GetCurrent(args);
				if( !l.loop )
					l.loop = uv_default_loop();
				uv_async_init( l.loop, &obj->async, asyncmsg );
				obj->async.data = obj;

				obj->Wrap( args.This() );
				args.GetReturnValue().Set( args.This() );
			}

		}
		else {
			// Invoked as plain function `MyObject(...)`, turn into construct call.
			int argc = args.Length();
			Local<Value> *argv = new Local<Value>[argc];
			for( int n = 0; n < argc; n++ )
				argv[n] = args[n];

			Local<Function> cons = Local<Function>::New( isolate, constructor );
			args.GetReturnValue().Set( cons->NewInstance( argc, argv ) );
			delete argv;
		}
}



static void CPROC dispatchRead( uintptr_t psv, int nCommId, POINTER buffer, int len ) {
	struct msgbuf *msgbuf = NewPlus( struct msgbuf, len );

	MemCpy( msgbuf->buf, buffer, len );
	msgbuf->buflen = len;
	ComObject *com = (ComObject*)psv;
	EnqueLink( &com->readQueue, msgbuf );
  	uv_async_send( &com->async );
}


void ComObject::onRead( const FunctionCallbackInfo<Value>& args ) {
	Isolate* isolate = args.GetIsolate();
	int argc = args.Length();
	if( argc < 1 ) {
		isolate->ThrowException( Exception::Error( String::NewFromUtf8( isolate, "Must pass callback to onRead handler" ) ) );
		return;
	}
	
	ComObject *com = ObjectWrap::Unwrap<ComObject>( args.This() );

	if( com->handle >= 0 ) {
		SackSetReadCallback( com->handle, dispatchRead, (uintptr_t)com );
	}

	Handle<Function> arg0 = Handle<Function>::Cast( args[0] );
	Persistent<Function> cb( isolate, arg0 );

	com->readCallback = cb;
}

void ComObject::writeCom( const FunctionCallbackInfo<Value>& args ) {

}

void ComObject::closeCom( const FunctionCallbackInfo<Value>& args ) {

}
