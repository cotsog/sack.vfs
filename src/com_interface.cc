
#include "global.h"

static struct local {
	int data;
	uv_loop_t* loop;

} l;

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
	{
		struct msgbuf *msg;
		while( msg = (struct msgbuf *)DequeLink( &myself->readQueue ) ) {
			size_t length;
			Local<ArrayBuffer> ab =
				ArrayBuffer::New( isolate,
											  msg->buf,
											  length = msg->buflen );

			PARRAY_BUFFER_HOLDER holder = GetHolder();
			holder->o.Reset( isolate, ab );
			holder->o.SetWeak< ARRAY_BUFFER_HOLDER>( holder, releaseBuffer, WeakCallbackType::kParameter );
			holder->buffer = msg->buf;

			Local<Uint8Array> ui = Uint8Array::New( ab, 0, length );

			Local<Value> argv[] = { ui };
			Local<Function> cb = Local<Function>::New( isolate, myself->readCallback[0] );
			//lprintf( "callback ... %p", myself );
			// using obj->jsThis  fails. here...
			{
				MaybeLocal<Value> result = cb->Call( isolate->GetCurrentContext()->Global(), 1, argv );
				if( result.IsEmpty() ) {
					Deallocate( struct msgbuf *, msg );
					return;
				}
			}
			//lprintf( "called ..." );
			Deallocate( struct msgbuf *, msg );
		}
	}
	//lprintf( "done calling message notice." );
}

void ComObject::New( const v8::FunctionCallbackInfo<Value>& args ) {
		Isolate* isolate = args.GetIsolate();
		if( args.IsConstructCall() ) {
			char *portName;
			int argc = args.Length();
			if( argc > 0 ) {
				String::Utf8Value fName( args[0]->ToString() );
				portName = StrDup( *fName );
			} else {
				isolate->ThrowException( Exception::Error( String::NewFromUtf8( isolate, "Must specify port name to open." ) ) );
				return;
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
            //lprintf( "empty async...." );
				//MemSet( &obj->async, 0, sizeof( obj->async ) );
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
			args.GetReturnValue().Set( cons->NewInstance( isolate->GetCurrentContext(), argc, argv ).ToLocalChecked() );
			delete[] argv;
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


void ComObject::onRead( const v8::FunctionCallbackInfo<Value>& args ) {
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
	com->readCallback = new Persistent<Function,CopyablePersistentTraits<Function>>(isolate,arg0);
}

void ComObject::writeCom( const v8::FunctionCallbackInfo<Value>& args ) {
	int argc = args.Length();
	if( argc < 1 ) {
		//isolate->ThrowException( Exception::Error( String::NewFromUtf8( isolate, "required parameter missing" ) ) );
		return;
	}
	Isolate* isolate = args.GetIsolate();
	ComObject *com = ObjectWrap::Unwrap<ComObject>( args.This() );

	//assert(args[i]->IsFloat32Array());
	if( args[0]->IsUint8Array() ) {
		Local<Uint8Array> myarr = args[0].As<Uint8Array>();
		ArrayBuffer::Contents ab_c = myarr->Buffer()->GetContents();
		char *buf = static_cast<char*>(ab_c.Data()) + myarr->ByteOffset();
		//LogBinary( buf, myarr->Length() );
		SackWriteComm( com->handle, buf, (int)myarr->Length() );
	}

}

void ComObject::closeCom( const v8::FunctionCallbackInfo<Value>& args ) {

	ComObject *com = ObjectWrap::Unwrap<ComObject>( args.This() );
	SackCloseComm( com->handle );
	com->handle = -1;
}
