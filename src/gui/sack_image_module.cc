#define DEFINE_GLOBAL
#include "../global.h"


Persistent<Function> ImageObject::constructor;
Persistent<FunctionTemplate> ImageObject::tpl;
Persistent<Function> FontObject::constructor;
Persistent<Function> ColorObject::constructor;
Persistent<FunctionTemplate> ColorObject::tpl;
Persistent<Function> fontResult;
Persistent<Function> imageResult;
Persistent<Object> priorThis;

static struct imageLocal {
	uv_async_t async; // keep this instance around for as long as we might need to do the periodic callback

}imageLocal;

static Local<Object> makeColor( Isolate *isolate, CDATA c ) {
	Local<Value> argv[1] = { Uint32::New( isolate, c ) };
	Local<Function> cons = Local<Function>::New( isolate, ColorObject::constructor );
	Local<Object> cObject = cons->NewInstance( 1, argv );
	return cObject;
}

static void fontAsyncmsg( uv_async_t* handle ) {
	// Called by UV in main thread after our worker thread calls uv_async_send()
	//    I.e. it's safe to callback to the CB we defined in node!
	v8::Isolate* isolate = v8::Isolate::GetCurrent();
	HandleScope scope( isolate );
	//lprintf( "async message notice. %p", myself );
	{
		Local<Function> cb = Local<Function>::New( isolate, fontResult );
		Local<Object> _this = priorThis.Get( isolate );
		Local<Value> argv[1] = { Number::New( isolate, 0 ) };
		cb->Call( _this, 1, argv );

		uv_close( (uv_handle_t*)&imageLocal.async, NULL );

	}
	//lprintf( "done calling message notice." );
}

static void imageAsyncmsg( uv_async_t* handle ) {
	// Called by UV in main thread after our worker thread calls uv_async_send()
	//    I.e. it's safe to callback to the CB we defined in node!
	v8::Isolate* isolate = v8::Isolate::GetCurrent();
	HandleScope scope( isolate );
	//lprintf( "async message notice. %p", myself );
	{
		Local<Function> cb = Local<Function>::New( isolate, imageResult );
		Local<Object> _this = priorThis.Get( isolate );
		Local<Value> argv[1] = { Number::New( isolate, 0 ) };
		cb->Call( _this, 1, argv );

		uv_close( (uv_handle_t*)&imageLocal.async, NULL );

	}
	//lprintf( "done calling message notice." );
}


static uintptr_t fontPickThread( PTHREAD thread ) {
	MemSet( &imageLocal.async, 0, sizeof( &imageLocal.async ) );
	uv_async_init( uv_default_loop(), &imageLocal.async, imageAsyncmsg );
	SFTFont font = PickFont( 0, 0, NULL, NULL, NULL );
	uv_async_send( &imageLocal.async );

	return 0;
}

static void pickFont( const FunctionCallbackInfo<Value>&  args ) {
	Isolate* isolate = args.GetIsolate();
	priorThis.Reset( isolate, args.This() );
	fontResult.Reset( isolate, Handle<Function>::Cast( args[0] ) );
	ThreadTo( fontPickThread, 0 );
}

static uintptr_t colorPickThread( PTHREAD thread ) {
	MemSet( &imageLocal.async, 0, sizeof( &imageLocal.async ) );
	uv_async_init( uv_default_loop(), &imageLocal.async, imageAsyncmsg );
	CDATA color;
	PickColor( &color, 0, NULL );
	uv_async_send( &imageLocal.async );

	return 0;
}

static void pickColor( const FunctionCallbackInfo<Value>&  args ) {
	Isolate* isolate = args.GetIsolate();
	priorThis.Reset( isolate, args.This() );
	imageResult.Reset( isolate, Handle<Function>::Cast( args[0] ) );
	ThreadTo( colorPickThread, 0 );
}


void ImageObject::Init( Handle<Object> exports ) {
	g.pii = GetImageInterface();
	g.pdi = GetDisplayInterface();


	Isolate* isolate = Isolate::GetCurrent();
	Local<FunctionTemplate> imageTemplate;

	// Prepare constructor template
	imageTemplate = FunctionTemplate::New( isolate, New );
	imageTemplate->SetClassName( String::NewFromUtf8( isolate, "sack.Image" ) );
	imageTemplate->InstanceTemplate()->SetInternalFieldCount( 1 ); // 1 required for wrap

	// Prototype
	NODE_SET_PROTOTYPE_METHOD( imageTemplate, "Image", ImageObject::NewSubImage );

	NODE_SET_PROTOTYPE_METHOD( imageTemplate, "reset", ImageObject::reset );
	NODE_SET_PROTOTYPE_METHOD( imageTemplate, "fill", ImageObject::fill );
	NODE_SET_PROTOTYPE_METHOD( imageTemplate, "fillOver", ImageObject::fillOver );
	NODE_SET_PROTOTYPE_METHOD( imageTemplate, "line", ImageObject::line );
	NODE_SET_PROTOTYPE_METHOD( imageTemplate, "lineOver", ImageObject::lineOver );
	NODE_SET_PROTOTYPE_METHOD( imageTemplate, "plot", ImageObject::plot );
	NODE_SET_PROTOTYPE_METHOD( imageTemplate, "plotOver", ImageObject::plotOver );
	NODE_SET_PROTOTYPE_METHOD( imageTemplate, "drawImage", ImageObject::putImage );
	NODE_SET_PROTOTYPE_METHOD( imageTemplate, "drawImageOver", ImageObject::putImageOver );
	NODE_SET_PROTOTYPE_METHOD( imageTemplate, "imageSurface", ImageObject::imageData );

	imageTemplate->PrototypeTemplate()->SetAccessorProperty( String::NewFromUtf8( isolate, "png" )
		, FunctionTemplate::New( isolate, ImageObject::getPng )
		, Local<FunctionTemplate>(), (PropertyAttribute)(ReadOnly|DontDelete) );
	imageTemplate->PrototypeTemplate()->SetAccessorProperty( String::NewFromUtf8( isolate, "jpg" )
		, FunctionTemplate::New( isolate, ImageObject::getJpeg )
		, Local<FunctionTemplate>(), (PropertyAttribute)(ReadOnly | DontDelete) );
	imageTemplate->PrototypeTemplate()->SetAccessorProperty( String::NewFromUtf8( isolate, "jpgQuality" )
		, FunctionTemplate::New( isolate, ImageObject::getJpegQuality )
		, FunctionTemplate::New( isolate, ImageObject::setJpegQuality ), DontDelete );
	imageTemplate->PrototypeTemplate()->SetAccessorProperty( String::NewFromUtf8( isolate, "width" )
		, FunctionTemplate::New( isolate, ImageObject::getWidth )
		, Local<FunctionTemplate>(), (PropertyAttribute)(ReadOnly | DontDelete) );
	imageTemplate->PrototypeTemplate()->SetAccessorProperty( String::NewFromUtf8( isolate, "height" )
		, FunctionTemplate::New( isolate, ImageObject::getHeight )
		, Local<FunctionTemplate>(), (PropertyAttribute)(ReadOnly | DontDelete) );

	ImageObject::tpl.Reset( isolate, imageTemplate );
	ImageObject::constructor.Reset( isolate, imageTemplate->GetFunction() );
	SET_READONLY( exports, "Image", imageTemplate->GetFunction() );

	Local<FunctionTemplate> fontTemplate;
	fontTemplate = FunctionTemplate::New( isolate, FontObject::New );
	fontTemplate->SetClassName( String::NewFromUtf8( isolate, "sack.Image.Font" ) );
	fontTemplate->InstanceTemplate()->SetInternalFieldCount( 4+1 );

	// Prototype
	NODE_SET_PROTOTYPE_METHOD( fontTemplate, "measure", FontObject::measure );

	Local<FunctionTemplate> colorTemplate;

	// Prepare constructor template
	colorTemplate = FunctionTemplate::New( isolate, ColorObject::New );
	colorTemplate->SetClassName( String::NewFromUtf8( isolate, "sack.Image.Color" ) );
	colorTemplate->InstanceTemplate()->SetInternalFieldCount( 1 ); // 1 required for wrap

	NODE_SET_PROTOTYPE_METHOD( colorTemplate, "toString", ColorObject::toString );
#define SetAccessor(b,c,d) SetAccessorProperty( b, FunctionTemplate::New( isolate, c ), FunctionTemplate::New( isolate, d ), DontDelete )
	colorTemplate->PrototypeTemplate()->SetAccessor( String::NewFromUtf8( isolate, "r" )
		, ColorObject::getRed, ColorObject::setRed );
	colorTemplate->PrototypeTemplate()->SetAccessor( String::NewFromUtf8( isolate, "g" )
		, ColorObject::getGreen, ColorObject::setGreen );
	colorTemplate->PrototypeTemplate()->SetAccessor( String::NewFromUtf8( isolate, "b" )
		, ColorObject::getBlue, ColorObject::setBlue );
	colorTemplate->PrototypeTemplate()->SetAccessor( String::NewFromUtf8( isolate, "a" )
		, ColorObject::getAlpha, ColorObject::setAlpha );
	ColorObject::tpl.Reset( isolate, colorTemplate );
	ColorObject::constructor.Reset( isolate, colorTemplate->GetFunction() );


	FontObject::constructor.Reset( isolate, fontTemplate->GetFunction() );
	SET_READONLY( imageTemplate->GetFunction(), "Font", fontTemplate->GetFunction() );
	SET_READONLY_METHOD( fontTemplate->GetFunction(), "dialog", pickFont );

	Local<Object> colors = Object::New( isolate );
	SET_READONLY( colors, "white", makeColor( isolate, BASE_COLOR_WHITE ) );
	SET_READONLY( colors, "black", makeColor( isolate, BASE_COLOR_BLACK ) );
	SET_READONLY( colors, "green", makeColor( isolate, BASE_COLOR_GREEN ) );
	SET_READONLY( colors, "blue", makeColor( isolate, BASE_COLOR_BLUE ) );
	SET_READONLY( colors, "red", makeColor( isolate, BASE_COLOR_RED ) );
	SET_READONLY( colors, "darkBlue", makeColor( isolate, BASE_COLOR_DARKBLUE ) );
	SET_READONLY( colors, "cyan", makeColor( isolate, BASE_COLOR_CYAN ) );
	SET_READONLY( colors, "brown", makeColor( isolate, BASE_COLOR_BROWN ) );
	SET_READONLY( colors, "lightBrown", makeColor( isolate, BASE_COLOR_LIGHTBROWN ) );
	SET_READONLY( colors, "magenta", makeColor( isolate, BASE_COLOR_MAGENTA ) );
	SET_READONLY( colors, "lightGrey", makeColor( isolate, BASE_COLOR_LIGHTGREY ) );
	SET_READONLY( colors, "darkGrey", makeColor( isolate, BASE_COLOR_DARKGREY ) );
	SET_READONLY( colors, "lightBlue", makeColor( isolate, BASE_COLOR_LIGHTBLUE ) );
	SET_READONLY( colors, "lightGreen", makeColor( isolate, BASE_COLOR_LIGHTGREEN ) );
	SET_READONLY( colors, "lightCyan", makeColor( isolate, BASE_COLOR_LIGHTCYAN ) );
	SET_READONLY( colors, "lightRed", makeColor( isolate, BASE_COLOR_LIGHTRED ) );
	SET_READONLY( colors, "lightMagenta", makeColor( isolate, BASE_COLOR_LIGHTMAGENTA ) );
	SET_READONLY( colors, "yellow", makeColor( isolate, BASE_COLOR_YELLOW ) );
	SET_READONLY( colors, "orange", makeColor( isolate, BASE_COLOR_ORANGE ) );
	SET_READONLY( colors, "niceOrange", makeColor( isolate, BASE_COLOR_NICE_ORANGE ) );
	SET_READONLY( colors, "purple", makeColor( isolate, BASE_COLOR_PURPLE ) );

	Local<Function> i = imageTemplate->GetFunction();
	SET_READONLY( i, "colors", colors );
	SET_READONLY( i, "Color", colorTemplate->GetFunction() );
	SET_READONLY_METHOD( colorTemplate->GetFunction(), "dialog", pickColor );

}
void ImageObject::getPng( const FunctionCallbackInfo<Value>& args ) {
	Isolate* isolate = args.GetIsolate();
	Local<FunctionTemplate> tpl = ImageObject::tpl.Get( isolate );
	if( tpl->HasInstance( args.This() ) ) {
		ImageObject *obj = ObjectWrap::Unwrap<ImageObject>( args.This() );
		uint8_t *buf;
		size_t size;
		if( PngImageFile( obj->image, &buf, &size ) )
		{
			Local<ArrayBuffer> arrayBuffer = ArrayBuffer::New( isolate, buf, size );

			PARRAY_BUFFER_HOLDER holder = GetHolder();
			holder->o.Reset( isolate, arrayBuffer );
			holder->o.SetWeak<ARRAY_BUFFER_HOLDER>( holder, releaseBuffer, WeakCallbackType::kParameter );
			holder->buffer = buf;
			args.GetReturnValue().Set( arrayBuffer );
		}
	}
}
void ImageObject::getJpeg( const FunctionCallbackInfo<Value>&  args ) {
	Isolate* isolate = args.GetIsolate();
	Local<FunctionTemplate> tpl = ImageObject::tpl.Get( isolate );
	if( tpl->HasInstance( args.This() ) ) {

		ImageObject *obj = ObjectWrap::Unwrap<ImageObject>( args.This() );
		uint8_t *buf;
		size_t size;
		if( JpgImageFile( obj->image, &buf, &size, obj->jpegQuality ) )
		{
			Local<ArrayBuffer> arrayBuffer = ArrayBuffer::New( isolate, buf, size );

			PARRAY_BUFFER_HOLDER holder = GetHolder();
			holder->o.Reset( isolate, arrayBuffer );
			holder->o.SetWeak<ARRAY_BUFFER_HOLDER>( holder, releaseBuffer, WeakCallbackType::kParameter );
			holder->buffer = buf;
			args.GetReturnValue().Set( arrayBuffer );
		}
	}
}

void ImageObject::getJpegQuality( const FunctionCallbackInfo<Value>&  args ) {
	Isolate* isolate = args.GetIsolate();
	Local<FunctionTemplate> tpl = ImageObject::tpl.Get( isolate );
	if( tpl->HasInstance( args.This() ) ) {
		ImageObject *obj = ObjectWrap::Unwrap<ImageObject>( args.This() );
		args.GetReturnValue().Set( Integer::New( isolate, obj->jpegQuality ) );
	}
}

void ImageObject::setJpegQuality( const FunctionCallbackInfo<Value>&  args ) {
	Isolate* isolate = args.GetIsolate();
	ImageObject *obj = ObjectWrap::Unwrap<ImageObject>( args.This() );
	obj->jpegQuality = (int)args[0]->IntegerValue();
}

void ImageObject::getWidth( const FunctionCallbackInfo<Value>&  args ) {
	Isolate* isolate = args.GetIsolate();
	Local<FunctionTemplate> tpl = ImageObject::tpl.Get( isolate );
	if( tpl->HasInstance( args.This() ) ) {
		ImageObject *obj = ObjectWrap::Unwrap<ImageObject>( args.This() );
		if( obj->image )
			args.GetReturnValue().Set( Integer::New( args.GetIsolate(), (int)obj->image->width ) );
	}
}
void ImageObject::getHeight( const FunctionCallbackInfo<Value>&  args ) {
	Isolate* isolate = args.GetIsolate();
	Local<FunctionTemplate> tpl = ImageObject::tpl.Get( isolate );
	if( tpl->HasInstance( args.This() ) ) {
		ImageObject *obj = ObjectWrap::Unwrap<ImageObject>( args.This() );
		if( obj->image )
			args.GetReturnValue().Set( Integer::New( args.GetIsolate(), (int)obj->image->height ) );
	}
}

ImageObject::ImageObject( uint8_t *buf, size_t len ) {
	image = DecodeMemoryToImage( buf, len );
	jpegQuality = 78;
}

ImageObject::ImageObject( const char *filename )  {
   image = LoadImageFile( filename );
   jpegQuality = 78;
}
ImageObject::ImageObject( Image image ) {
	this->image = image;
	jpegQuality = 78;
}

ImageObject::ImageObject( int x, int y, int w, int h, ImageObject * within )  {
	jpegQuality = 78;
	if( within )
	{
		container = within;
		image = MakeSubImage( within->image, x, y, w, h );
	}
	else {
		container = NULL;
		image = MakeImageFile( w, h );
	}
}

ImageObject::~ImageObject(void) {
	if( !external ) {
		lprintf( "Image has been garbage collected." );
		UnmakeImageFile( image );
	}
}

void ImageObject::New( const FunctionCallbackInfo<Value>& args ) {
	Isolate* isolate = args.GetIsolate();
	if( args.IsConstructCall() ) {

		int x = 0, y = 0, w = 1024, h = 768;
		Local<Object> parentImage;
		ImageObject *parent = NULL;
		char *filename = NULL;
		ImageObject* obj = NULL;
		Local<ArrayBuffer> buffer;
		int argc = args.Length();
		if( argc > 0 ) {
			if( args[0]->IsUint8Array() ) {
				Local<Uint8Array> u8arr = args[0].As<Uint8Array>();
				Local<ArrayBuffer> myarr = u8arr->Buffer();
				obj = new ImageObject( (uint8_t*)myarr->GetContents().Data(), myarr->ByteLength() );
			} else if( args[0]->IsTypedArray() ) {
				Local<TypedArray> u8arr = args[0].As<TypedArray>();
				Local<ArrayBuffer> myarr = u8arr->Buffer();
				obj = new ImageObject( (uint8_t*)myarr->GetContents().Data(), myarr->ByteLength() );
			} else if( args[0]->IsArrayBuffer() ) {
				Local<ArrayBuffer> myarr = args[0].As<ArrayBuffer>();
				obj = new ImageObject( (uint8_t*)myarr->GetContents().Data(), myarr->ByteLength() );
			} else if( args[0]->IsNumber() )
				w = (int)args[0]->NumberValue();
			else {
				String::Utf8Value fName( args[0]->ToString() );
				filename = StrDup( *fName );
			}
		}
		if( !filename && !obj ) {
			if( argc > 1 ) {
				h = (int)args[1]->NumberValue();
			}
			if( argc > 2 ) {
				parentImage = args[2]->ToObject();
				parent = ObjectWrap::Unwrap<ImageObject>( parentImage );
			}
			if( argc > 3 ) {
				x = (int)args[3]->NumberValue();
			}
			if( argc > 4 ) {
				y = (int)args[4]->NumberValue();
			}
		}
		// Invoked as constructor: `new MyObject(...)`
		if( !obj ) {
			if( filename )
				obj = new ImageObject( filename );
			else
				obj = new ImageObject( x, y, w, h, parent );
		}
		obj->_this.Reset( isolate, args.This() );
		obj->Wrap( args.This() );
		args.GetReturnValue().Set( args.This() );

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

void ImageObject::NewSubImage( const FunctionCallbackInfo<Value>& args ) {
	Isolate* isolate = args.GetIsolate();
	if( args.IsConstructCall() ) {

		int x = 0, y = 0, w = 1024, h = 768;
		Local<Object> parentImage;
		ImageObject *parent = ObjectWrap::Unwrap<ImageObject>( args.This() );
		int argc = args.Length();
		int arg_ofs = 0;
		if( argc > 0 ) {
			if( args[0]->IsObject() ) {
				ImageObject *parent = ObjectWrap::Unwrap<ImageObject>( args[0]->ToObject() );
				arg_ofs = 1;
			}
		if( (argc+arg_ofs) > 0 )
			x = (int)args[0+arg_ofs]->NumberValue();
		}
		if( (argc + arg_ofs) > 1 ) {
			y = (int)args[1 + arg_ofs]->NumberValue();
		}
		if( (argc + arg_ofs) > 2 ) {
			w = (int)args[2 + arg_ofs]->NumberValue();
		}
		if( (argc + arg_ofs) > 3 ) {
			h = (int)args[3 + arg_ofs]->NumberValue();
		}

		// Invoked as constructor: `new MyObject(...)`
		ImageObject* obj;
		obj = new ImageObject( x, y, w, h, parent );

		obj->Wrap( args.This() );
		args.GetReturnValue().Set( args.This() );
	}
	else {
		// Invoked as plain function `MyObject(...)`, turn into construct call.
		int argc = args.Length();
		Local<Value> *argv = new Local<Value>[argc+1];
		int n;
		for( n = 0; n < argc; n++ )
			argv[n+1] = args[n];
		argv[0] = args.Holder();

		Local<Function> cons = Local<Function>::New( isolate, constructor );
		args.GetReturnValue().Set( cons->NewInstance( argc, argv ) );
		delete argv;
	}
}


Local<Object> ImageObject::NewImage( Isolate*isolate, Image image, LOGICAL external ) {
	// Invoked as constructor: `new MyObject(...)`
	ImageObject* obj;

	int argc = 0;
	Local<Value> *argv = new Local<Value>[argc];
	Local<Function> cons = Local<Function>::New( isolate, constructor );
	Local<Object> lo = cons->NewInstance( argc, argv );
	obj = ObjectWrap::Unwrap<ImageObject>( lo );
	obj->image = image;
	obj->external = external;
	return lo;
}

ImageObject * ImageObject::MakeNewImage( Isolate*isolate, Image image, LOGICAL external ) {
	// Invoked as constructor: `new MyObject(...)`
	ImageObject* obj;

	int argc = 0;
	Local<Value> *argv = new Local<Value>[argc];
	Local<Function> cons = Local<Function>::New( isolate, constructor );
	Local<Object> lo = cons->NewInstance( argc, argv );
	obj = ObjectWrap::Unwrap<ImageObject>( lo );
	obj->image = image;
	obj->external = external;
	return obj;
}


void ImageObject::reset( const FunctionCallbackInfo<Value>& args ) {
	Isolate* isolate = args.GetIsolate();
	args[3]->ToObject();
	ImageObject *io = ObjectWrap::Unwrap<ImageObject>( args.This() );
	ClearImage( io->image );
}

void ImageObject::fill( const FunctionCallbackInfo<Value>& args ) {
	Isolate* isolate = args.GetIsolate();
	ImageObject *io = ObjectWrap::Unwrap<ImageObject>( args.This() );
	int argc = args.Length();
	int x, y, w, h, c;
	if( argc > 0 ) {
		x = (int)args[0]->NumberValue();
	}
	if( argc > 1 ) {
		y = (int)args[1]->NumberValue();
	}
	if( argc > 2 ) {
		w = (int)args[2]->NumberValue();
	}
	if( argc > 3 ) {
		h = (int)args[3]->NumberValue();
	}
	if( argc > 4 ) {
		if( args[4]->IsObject() ) {
			ColorObject *co = ObjectWrap::Unwrap<ColorObject>( args[4]->ToObject() );
			c = co->color;
		}
		else if( args[4]->IsUint32() )
			c = (int)args[4]->Uint32Value();
		else if( args[4]->IsNumber() )
			c = (int)args[4]->NumberValue();
		else
			c = 0;
	}
	BlatColor( io->image, x, y, w, h, c );
}

void ImageObject::fillOver( const FunctionCallbackInfo<Value>& args ) {
	Isolate* isolate = args.GetIsolate();
	int argc = args.Length();
	ImageObject *io = ObjectWrap::Unwrap<ImageObject>( args.This() );
	int x, y, w, h, c;
	if( argc > 0 ) {
		x = (int)args[0]->NumberValue();
	}
	if( argc > 1 ) {
		y = (int)args[1]->NumberValue();
	}
	if( argc > 2 ) {
		w = (int)args[2]->NumberValue();
	}
	if( argc > 3 ) {
		h = (int)args[3]->NumberValue();
	}
	if( argc > 4 ) {
		if( args[4]->IsObject() ) {
			ColorObject *co = ObjectWrap::Unwrap<ColorObject>( args[4]->ToObject() );
			c = co->color;
		}
		else if( args[4]->IsUint32() )
			c = (int)args[4]->Uint32Value();
		c = (int)args[4]->NumberValue();
	}
	BlatColorAlpha( io->image, x, y, w, h, c );
}

void ImageObject::plot( const FunctionCallbackInfo<Value>& args ) {
	Isolate* isolate = args.GetIsolate();
	int argc = args.Length();
	ImageObject *io = ObjectWrap::Unwrap<ImageObject>( args.This() );
	int x, y, c;
	if( argc > 0 ) {
		x = (int)args[0]->NumberValue();
	}
	if( argc > 1 ) {
		y = (int)args[1]->NumberValue();
	}
	if( argc > 2 ) {
		if( args[2]->IsObject() ) {
			ColorObject *co = ObjectWrap::Unwrap<ColorObject>( args[2]->ToObject() );
			c = co->color;
		}
		else if( args[2]->IsUint32() )
			c = (int)args[2]->Uint32Value();
		else
			c = (int)args[2]->NumberValue();
	}
	g.pii->_plot( io->image, x, y, c );
}

void ImageObject::plotOver( const FunctionCallbackInfo<Value>& args ) {
	Isolate* isolate = args.GetIsolate();
	int argc = args.Length();
	ImageObject *io = ObjectWrap::Unwrap<ImageObject>( args.This() );
	int x, y, c;
	if( argc > 0 ) {
		x = (int)args[0]->NumberValue();
	}
	if( argc > 1 ) {
		y = (int)args[1]->NumberValue();
	}
	if( argc > 2 ) {
		if( args[2]->IsObject() ) {
			ColorObject *co = ObjectWrap::Unwrap<ColorObject>( args[2]->ToObject() );
			c = co->color;
		}
		else if( args[2]->IsUint32() )
			c = (int)args[2]->Uint32Value();
		else
			c = (int)args[2]->NumberValue();
	}
	plotalpha( io->image, x, y, c );
}

void ImageObject::line( const FunctionCallbackInfo<Value>& args ) {
	Isolate* isolate = args.GetIsolate();
	int argc = args.Length();
	ImageObject *io = ObjectWrap::Unwrap<ImageObject>( args.This() );
	int x, y, xTo, yTo, c;
	if( argc > 0 ) {
		x = (int)args[0]->NumberValue();
	}
	if( argc > 1 ) {
		y = (int)args[1]->NumberValue();
	}
	if( argc > 2 ) {
		xTo = (int)args[2]->NumberValue();
	}
	if( argc > 3 ) {
		yTo = (int)args[3]->NumberValue();
	}
	if( argc > 4 ) {
		if( args[2]->IsObject() ) {
			ColorObject *co = ObjectWrap::Unwrap<ColorObject>( args[4]->ToObject() );
			c = co->color;
		}
		else if( args[2]->IsUint32() )
			c = (int)args[4]->Uint32Value();
		else
			c = (int)args[4]->NumberValue();
	}
	if( x == xTo )
		do_vline( io->image, x, y, yTo, c );
	else if( y == yTo )
		do_hline( io->image, y, x, xTo, c );
	else
		do_line( io->image, x, y, xTo, yTo, c );
}

void ImageObject::lineOver( const FunctionCallbackInfo<Value>& args ) {
	Isolate* isolate = args.GetIsolate();
	ImageObject *io = ObjectWrap::Unwrap<ImageObject>( args.This() );
	int argc = args.Length();
	int x, y, xTo, yTo, c;
	if( argc > 0 ) {
		x = (int)args[0]->NumberValue();
	}
	if( argc > 1 ) {
		y = (int)args[1]->NumberValue();
	}
	if( argc > 2 ) {
		xTo = (int)args[2]->NumberValue();
	}
	if( argc > 3 ) {
		yTo = (int)args[3]->NumberValue();
	}
	if( argc > 4 ) {
		if( args[4]->IsObject() ) {
			ColorObject *co = ObjectWrap::Unwrap<ColorObject>( args[4]->ToObject() );
			c = co->color;
		}
		else if( args[2]->IsUint32() )
			c = (int)args[4]->Uint32Value();
		else
			c = (int)args[4]->NumberValue();
	}
	if( x == xTo )
		do_vlineAlpha( io->image, x, y, yTo, c );
	else if( y == yTo )
		do_hlineAlpha( io->image, y, x, xTo, c );
	else
		do_lineAlpha( io->image, x, y, xTo, yTo, c );
}

// {x, y output
// w, h} output
// {x, y input
// w, h} input
void ImageObject::putImage( const FunctionCallbackInfo<Value>& args ) {
	Isolate* isolate = args.GetIsolate();
	ImageObject *io = ObjectWrap::Unwrap<ImageObject>( args.This() );
	int argc = args.Length();
	ImageObject *ii;// = ObjectWrap::Unwrap<ImageObject>( args.This() );
	int x = 0, y= 0, xAt, yAt;
	int w, h;
	if( argc > 0 ) {
		ii = ObjectWrap::Unwrap<ImageObject>( args[0]->ToObject() );
		if( !ii || !ii->image ) {
			lprintf( "Bad First paraemter, must be an image to put?" );
			return;
		}
		else {
			w = ii->image->width;
			h = ii->image->height;
		}
	}
	else {
		// throw error ?
		return;
	}
	if( argc > 2 ) {
		x = (int)args[1]->NumberValue();
		y = (int)args[2]->NumberValue();
	}
	else {
		x = 0;
		y = 0;
		//isolate->ThrowException( Exception::Error( String::NewFromUtf8( isolate, "Required parameters for position missing." ) ) );
		//return;
	}
	if( argc > 3 ) {
		xAt = (int)args[3]->NumberValue();

		if( argc > 4 ) {
			yAt = (int)args[4]->NumberValue();
		}
		else {
			isolate->ThrowException( Exception::Error( String::NewFromUtf8( isolate, "Required parameters for position missing." ) ) );
			return;
		}
		if( argc > 5 ) {
			w = (int)args[5]->NumberValue();
			if( argc > 6 ) {
				h = (int)args[6]->NumberValue();
			}
			if( argc > 7 ) {
				int ow, oh;
		
				ow = xAt;
				oh = yAt;
				xAt = w;
				yAt = h;
				if( argc > 7 ) {
					w = (int)args[7]->NumberValue();
					if( w < 0 ) w = ii->image->width;
				}
				if( argc > 8 ) {
					h = (int)args[8]->NumberValue();
					if( h < 0 ) h = ii->image->height;
				}
				BlotScaledImageSizedEx( io->image, ii->image, x, y, ow, oh, xAt, yAt, w, h, 1, BLOT_COPY );
			}
			else
				BlotImageSizedEx( io->image, ii->image, x, y, xAt, yAt, w, h, 0, BLOT_COPY );
		}
		else  {
			w = xAt; 
			h = yAt;
			BlotImageSizedEx( io->image, ii->image, x, y, 0, 0, w, h, 0, BLOT_COPY );
		}
	}
	else
		BlotImageEx( io->image, ii->image, x, y, 0, BLOT_COPY );
}

void ImageObject::putImageOver( const FunctionCallbackInfo<Value>& args ) {
	Isolate* isolate = args.GetIsolate();
	ImageObject *io = ObjectWrap::Unwrap<ImageObject>( args.This() );
	ImageObject *ii;// = ObjectWrap::Unwrap<ImageObject>( args.This() );
	int argc = args.Length();
	int x, y, xTo, yTo, c;
	if( argc > 0 ) {
		ii = ObjectWrap::Unwrap<ImageObject>( args[0]->ToObject() );
	}
	if( argc > 1 ) {
		x = (int)args[1]->NumberValue();
	}
	if( argc > 2 ) {
		y = (int)args[2]->NumberValue();
	}
	if( argc > 2 ) {
		xTo = (int)args[2]->NumberValue();

		if( argc > 3 ) {
			yTo = (int)args[3]->NumberValue();
		}
		if( argc > 4 ) {
			c = (int)args[4]->NumberValue();
		}
		else {
		}
	}
	else
		BlotImageEx( io->image, ii->image, x, y, ALPHA_TRANSPARENT, BLOT_COPY );
}

void ImageObject::imageData( const FunctionCallbackInfo<Value>& args ) {
	Isolate* isolate = args.GetIsolate();
	
	ImageObject *io = ObjectWrap::Unwrap<ImageObject>( args.This() );

	//Context::Global()
	size_t length;
	Local<SharedArrayBuffer> ab =
		SharedArrayBuffer::New( isolate,
			GetImageSurface( io->image ),
			length = io->image->height * io->image->pwidth );
	Local<Uint8Array> ui = Uint8Array::New( ab, 0, length );

	args.GetReturnValue().Set( ui );
}



FontObject::~FontObject() {
}
FontObject::FontObject( const char *filename, int w, int h, int flags ) {
   font = InternalRenderFontFile( filename, w, h, NULL, NULL, flags );
}

void FontObject::New( const FunctionCallbackInfo<Value>& args ) {
	Isolate* isolate = args.GetIsolate();
	if( args.IsConstructCall() ) {

		int w = 24, h = 24;
		int flags;
		Local<Object> parentFont;
		FontObject *parent = NULL;
		char *filename = NULL;

		int argc = args.Length();

		if( argc > 0 ) {
			String::Utf8Value fName( args[0]->ToString() );
			filename = StrDup( *fName );
		}

		if( argc > 1 ) {
			w = (int)args[1]->NumberValue();
		}
		if( argc > 2 ) {
			h = (int)args[2]->NumberValue();
		}
		if( argc >3 ) {
			flags = (int)args[3]->NumberValue();
		}
		else
			flags = 3;

		// Invoked as constructor: `new MyObject(...)`
		FontObject* obj;
		if( filename )
			obj = new FontObject( filename, w, h, flags );

		obj->Wrap( args.This() );
		args.GetReturnValue().Set( args.This() );

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


void FontObject::measure( const FunctionCallbackInfo<Value>& args ) {
		Isolate* isolate = args.GetIsolate();
		FontObject *fo = ObjectWrap::Unwrap<FontObject>( args.This() );
		int argc = args.Length();
}




ColorObject::~ColorObject() {
}
ColorObject::ColorObject() {
	color = 0xFFCCFFFF;
}
ColorObject::ColorObject( int r,int grn, int b, int a ) {
	color = MakeAlphaColor( r, grn, b, a );
}
ColorObject::ColorObject( CDATA r ) {
	color = r;
}

bool ColorObject::isColor( Isolate *isolate, Local<Object> object ) {
	Local<FunctionTemplate> colorTpl = tpl.Get( isolate );
	return colorTpl->HasInstance( object );
}

CDATA ColorObject::getColor( Local<Object> object ) {
	ColorObject *co = ObjectWrap::Unwrap<ColorObject>( object );
	return co->color;
}

Local<Object> ColorObject::makeColor( Isolate *isolate, CDATA rgba ) {
	Local<Function> cons = Local<Function>::New( isolate, constructor );
	Local<Object> _color = cons->NewInstance( 0, NULL );
	ColorObject *co = ObjectWrap::Unwrap<ColorObject>( _color );
	co->color = rgba;
	return _color;
}

void ColorObject::toString( const FunctionCallbackInfo<Value>& args ) {
	Isolate* isolate = args.GetIsolate();
	ColorObject *co = ObjectWrap::Unwrap<ColorObject>( args.This() );
	char buf[128];
	snprintf( buf, 128, "{r:%d,g:%d,b:%d,a:%d}", RedVal( co->color ), GreenVal( co->color ), BlueVal( co->color ), AlphaVal( co->color ) );
	
	args.GetReturnValue().Set( String::NewFromUtf8( isolate, buf ) );
}

void ColorObject::New( const FunctionCallbackInfo<Value>& args ) {
	Isolate* isolate = args.GetIsolate();
	if( args.IsConstructCall() ) {
		int r, grn, b, a;
		int argc = args.Length();

		ColorObject* obj;
		if( argc == 1 ) {
			if( args[0]->IsObject() ) {
				Local<Object> o = args[0]->ToObject();
				r = (int)o->Get( String::NewFromUtf8( isolate, "r" ) )->NumberValue();
				grn = (int)o->Get( String::NewFromUtf8( isolate, "g" ) )->NumberValue();
				b = (int)o->Get( String::NewFromUtf8( isolate, "b" ) )->NumberValue();
				a = (int)o->Get( String::NewFromUtf8( isolate, "a" ) )->NumberValue();
				obj = new ColorObject( r,grn,b,a );

			}
			else if( args[0]->IsUint32() ) {
				r = (int)args[0]->Uint32Value();
				obj = new ColorObject( r );
			}
			else if( args[0]->IsNumber() ) {
				r = (int)args[0]->NumberValue();
				obj = new ColorObject( SetAlpha( r, 255 ) );
			}
			else if( args[0]->IsString() ) {
				// parse string color....
				//Config
			}

		}
		else if( argc == 4 ) {
			r = (int)args[0]->NumberValue();
			grn = (int)args[1]->NumberValue();
			b = (int)args[2]->NumberValue();
			a = (int)args[3]->NumberValue();
			obj = new ColorObject( r, grn, b, a );
		}
		else {
			obj = new ColorObject();
		}
		obj->Wrap( args.This() );
		args.GetReturnValue().Set( args.This() );

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


void ColorObject::getRed( const FunctionCallbackInfo<Value>& args ) {
	Isolate* isolate = args.GetIsolate();
	if( ColorObject::tpl.Get( isolate )->HasInstance( args.This() ) ) {
		ColorObject *co = ObjectWrap::Unwrap<ColorObject>( args.This() );
		args.GetReturnValue().Set( Integer::New( isolate, RedVal( co->color ) ) );
	}
}
void ColorObject::getGreen( const FunctionCallbackInfo<Value>&  args ) {
	Isolate* isolate = args.GetIsolate();
	if( ColorObject::tpl.Get( isolate )->HasInstance( args.This() ) ) {
		ColorObject *co = ObjectWrap::Unwrap<ColorObject>( args.This() );
		args.GetReturnValue().Set( Integer::New( isolate, GreenVal( co->color ) ) );
	}
}
void ColorObject::getBlue( const FunctionCallbackInfo<Value>&  args ) {
	Isolate* isolate = args.GetIsolate();
	if( ColorObject::tpl.Get( isolate )->HasInstance( args.This() ) ) {
		ColorObject *co = ObjectWrap::Unwrap<ColorObject>( args.This() );
		args.GetReturnValue().Set( Integer::New( isolate, BlueVal( co->color ) ) );
	}
}
void ColorObject::getAlpha( const FunctionCallbackInfo<Value>& args ) {
	Isolate* isolate = args.GetIsolate();
	if( ColorObject::tpl.Get( isolate )->HasInstance( args.This() ) ) {
		ColorObject *co = ObjectWrap::Unwrap<ColorObject>( args.This() );
		args.GetReturnValue().Set( Integer::New( isolate, AlphaVal( co->color ) ) );
	}
}
void ColorObject::setRed( const FunctionCallbackInfo<Value>&  args ) {
	Isolate* isolate = args.GetIsolate();
	ColorObject *co = ObjectWrap::Unwrap<ColorObject>( args.This() );
	double val = args[0]->NumberValue();
	if( val < 0 ) val = 0;
	else if( val > 255 ) val = 255;
	if( val > 0 && val < 1.0 ) val = 255 * val;
	SetRedValue( co->color, (COLOR_CHANNEL)val );
}
void ColorObject::setGreen( const FunctionCallbackInfo<Value>&  args ) {
	Isolate* isolate = args.GetIsolate();
	ColorObject *co = ObjectWrap::Unwrap<ColorObject>( args.This() );
	double val = args[0]->NumberValue();
	if( val < 0 ) val = 0;
	else if( val > 255 ) val = 255;
	if( val > 0 && val < 1.0 ) val = 255 * val;
	SetGreenValue( co->color, (COLOR_CHANNEL)val );
}
void ColorObject::setBlue( const FunctionCallbackInfo<Value>&  args ) {
	Isolate* isolate = args.GetIsolate();
	ColorObject *co = ObjectWrap::Unwrap<ColorObject>( args.This() );
	double val = args[0]->NumberValue();
	if( val < 0 ) val = 0;
	else if( val > 255 ) val = 255;
	if( val > 0 && val < 1.0 ) val = 255 * val;
	SetBlueValue( co->color, (COLOR_CHANNEL)val );
}
void ColorObject::setAlpha( const FunctionCallbackInfo<Value>&  args ) {
	Isolate* isolate = args.GetIsolate();
	ColorObject *co = ObjectWrap::Unwrap<ColorObject>( args.This() );
	double val = args[0]->NumberValue();
	if( val < 0 ) val = 0;
	else if( val > 255 ) val = 255;
	if( val > 0 && val < 1.0 ) val = 255 * val;
	SetAlphaValue( co->color, (COLOR_CHANNEL)val );
}
