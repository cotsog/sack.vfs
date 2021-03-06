
#include "global.h"
#include <math.h>

static void buildObject( PDATALIST msg_data, Local<Object> o, struct reviver_data *revive );
static Local<Value> makeValue( struct json_value_container *val, struct reviver_data *revive );

static struct timings {
	uint64_t start;
	uint64_t deltas[10];
}timings;

static void makeJSON( const v8::FunctionCallbackInfo<Value>& args );
static void escapeJSON( const v8::FunctionCallbackInfo<Value>& args );
static void parseJSON( const v8::FunctionCallbackInfo<Value>& args );
static void makeJSON6( const v8::FunctionCallbackInfo<Value>& args );
static void escapeJSON6( const v8::FunctionCallbackInfo<Value>& args );
static void parseJSON6( const v8::FunctionCallbackInfo<Value>& args );
static void beginJSON6( const v8::FunctionCallbackInfo<Value>& args );
static void writeJSON6( const v8::FunctionCallbackInfo<Value>& args );
static void endJSON6( const v8::FunctionCallbackInfo<Value>& args );
static void showTimings( const v8::FunctionCallbackInfo<Value>& args );

class parseObject : public node::ObjectWrap {
	struct json_parse_state *state;
public:
	static Persistent<Function> constructor;
	static Persistent<Function> constructor6;
	Persistent<Function, CopyablePersistentTraits<Function>> readCallback; //

public:

	static void Init( Handle<Object> exports );
	parseObject();

	static void New( const v8::FunctionCallbackInfo<Value>& args );
	static void write( const v8::FunctionCallbackInfo<Value>& args );
	static void New6( const v8::FunctionCallbackInfo<Value>& args );
	static void write6( const v8::FunctionCallbackInfo<Value>& args );

	~parseObject();
};

Persistent<Function> parseObject::constructor;
Persistent<Function> parseObject::constructor6;

void InitJSON( Isolate *isolate, Handle<Object> exports ){
	Local<Object> o = Object::New( isolate );
	SET_READONLY_METHOD( o, "parse", parseJSON );
	NODE_SET_METHOD( o, "stringify", makeJSON );
	NODE_SET_METHOD( o, "escape", escapeJSON );
	NODE_SET_METHOD( o, "timing", showTimings );
	SET_READONLY( exports, "JSON", o );

	{
		Local<FunctionTemplate> parseTemplate;
		parseTemplate = FunctionTemplate::New( isolate, parseObject::New );
		parseTemplate->SetClassName( String::NewFromUtf8( isolate, "sack.core.json6_parser" ) );
		parseTemplate->InstanceTemplate()->SetInternalFieldCount( 1 );  // need 1 implicit constructor for wrap
		NODE_SET_PROTOTYPE_METHOD( parseTemplate, "write", parseObject::write );

		parseObject::constructor.Reset( isolate, parseTemplate->GetFunction() );

		SET_READONLY( o, "begin", parseTemplate->GetFunction() );
	}

	Local<Object> o2 = Object::New( isolate );
	SET_READONLY_METHOD( o2, "parse", parseJSON6 );
	NODE_SET_METHOD( o2, "stringify", makeJSON6 );
	NODE_SET_METHOD( o2, "escape", escapeJSON6 );
	NODE_SET_METHOD( o2, "timing", showTimings );
	SET_READONLY( exports, "JSON6", o2 );

	{
		Local<FunctionTemplate> parseTemplate;
		parseTemplate = FunctionTemplate::New( isolate, parseObject::New6 );
		parseTemplate->SetClassName( String::NewFromUtf8( isolate, "sack.core.json6_parser" ) );
		parseTemplate->InstanceTemplate()->SetInternalFieldCount( 1 );  // need 1 implicit constructor for wrap
		NODE_SET_PROTOTYPE_METHOD( parseTemplate, "write", parseObject::write6 );

		parseObject::constructor6.Reset( isolate, parseTemplate->GetFunction() );

		SET_READONLY( o2, "begin", parseTemplate->GetFunction() );
	}
}

parseObject::parseObject() {
	state = json_begin_parse();
}

parseObject::~parseObject() {
	json_parse_dispose_state( &state );
}

#define logTick(n) do { uint64_t tick = GetCPUTick(); if( n >= 0 ) timings.deltas[n] += tick-timings.start; timings.start = tick; } while(0)

void parseObject::write( const v8::FunctionCallbackInfo<Value>& args ) {
	Isolate* isolate = args.GetIsolate();
	parseObject *parser = ObjectWrap::Unwrap<parseObject>( args.Holder() );
	int argc = args.Length();
	if( argc == 0 ) {
		isolate->ThrowException( Exception::Error( String::NewFromUtf8( isolate, "Missing data parameter." ) ) );
		return;
	}

	String::Utf8Value data( args[0]->ToString() );
	int result;
	for( result = json6_parse_add_data( parser->state, *data, data.length() );
		result > 0;
		result = json6_parse_add_data( parser->state, NULL, 0 )
		) {
		struct json_value_container * val;
		PDATALIST elements = json_parse_get_data( parser->state );
		Local<Object> o;
		Local<Value> v;// = Object::New( isolate );

		Local<Value> argv[1];
		val = (struct json_value_container *)GetDataItem( &elements, 0 );
		if( val ) {
			struct reviver_data r;
			r.revive = FALSE;
			r.isolate = isolate;
			r.context = r.isolate->GetCurrentContext();
			argv[0] = convertMessageToJS( elements, &r );
			Local<Function> cb = Local<Function>::New( isolate, parser->readCallback );
			{
				MaybeLocal<Value> result = cb->Call( isolate->GetCurrentContext()->Global(), 1, argv );
				if( result.IsEmpty() ) // if an exception occurred stop, and return it.
					return;
			}
		}
		json_dispose_message( &elements );
		if( result < 2 )
			break;
	}
	if( result < 0 ) {
		PTEXT error = json_parse_get_error( parser->state );
		if( error ) {
			isolate->ThrowException( Exception::Error( String::NewFromUtf8( isolate, GetText( error ) ) ) );
			LineRelease( error );
		} else
			isolate->ThrowException( Exception::Error( String::NewFromUtf8( isolate, "No Error Text" STRSYM(__LINE__)) ) );
		json_parse_clear_state( parser->state );
		return;
	}

}

void parseObject::New( const v8::FunctionCallbackInfo<Value>& args ) {
	Isolate* isolate = args.GetIsolate();
	int argc = args.Length();
	if( argc == 0 ) {
		isolate->ThrowException( Exception::Error( String::NewFromUtf8( isolate, "Must callback to read into." ) ) );
		return;
	}

	if( args.IsConstructCall() ) {
		// Invoked as constructor: `new MyObject(...)`
		parseObject* obj = new parseObject();
		Handle<Function> arg0 = Handle<Function>::Cast( args[0] );
		Persistent<Function> cb( isolate, arg0 );
		obj->readCallback = cb;

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
		args.GetReturnValue().Set( cons->NewInstance( isolate->GetCurrentContext(), argc, argv ).ToLocalChecked() );
		delete[] argv;
	}

}



void parseObject::write6(const v8::FunctionCallbackInfo<Value>& args) {
	Isolate* isolate = args.GetIsolate();
	parseObject *parser = ObjectWrap::Unwrap<parseObject>( args.Holder() );
	int argc = args.Length();
	if( argc == 0 ) {
		isolate->ThrowException( Exception::Error( String::NewFromUtf8( isolate, "Missing data parameter." ) ) );
		return;
	}

	String::Utf8Value data( args[0]->ToString() );
	int result;
	for( result = json6_parse_add_data( parser->state, *data, data.length() );
		result > 0;
		result = json6_parse_add_data( parser->state, NULL, 0 )
		) {
		struct json_value_container * val;
		PDATALIST elements = json_parse_get_data( parser->state );
		val = (struct json_value_container *)GetDataItem( &elements, 0 );
		if( val ) {
			Local<Value> argv[1];
			struct reviver_data r;
			r.revive = FALSE;
			r.isolate = isolate;
			r.context = isolate->GetCurrentContext();
			argv[0] = convertMessageToJS( elements, &r );
			Local<Function> cb = Local<Function>::New( isolate, parser->readCallback );
			{
				MaybeLocal<Value> result = cb->Call( isolate->GetCurrentContext()->Global(), 1, argv );
				if( result.IsEmpty() ) // if an exception occurred stop, and return it.
					return;
			}
			json_dispose_message( &elements );
		}
		if( result < 2 )
			break;
	}
	if( result < 0 ) {
		PTEXT error = json_parse_get_error( parser->state );
		if( error ) {
			isolate->ThrowException( Exception::Error( String::NewFromUtf8( isolate, GetText( error ) ) ) );
			LineRelease( error );
		} else
			isolate->ThrowException( Exception::Error( String::NewFromUtf8( isolate, "No Error Text" STRSYM(__LINE__) ) ) );
		json_parse_clear_state( parser->state );
		return;
	}

}

void parseObject::New6( const v8::FunctionCallbackInfo<Value>& args ) {
	Isolate* isolate = args.GetIsolate();
	int argc = args.Length();
	if( argc == 0 ) {
		isolate->ThrowException( Exception::Error( String::NewFromUtf8( isolate, "Must callback function to get values." ) ) );
		return;
	}

	if( args.IsConstructCall() ) {
		// Invoked as constructor: `new MyObject(...)`
		parseObject* obj = new parseObject();
		Handle<Function> arg0 = Handle<Function>::Cast( args[0] );
		Persistent<Function> cb( isolate, arg0 );
		obj->readCallback = cb;

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
		args.GetReturnValue().Set( cons->NewInstance( isolate->GetCurrentContext(), argc, argv ).ToLocalChecked() );
		delete[] argv;
	}
}

#define MODE NewStringType::kNormal
//#define MODE NewStringType::kInternalized

static inline Local<Value> makeValue( struct json_value_container *val, struct reviver_data *revive ) {

	Local<Value> result;
	switch( val->value_type ) {
	case VALUE_UNDEFINED:
		result = Undefined( revive->isolate );
		break;
	default:
		lprintf( "Parser faulted; should never have a uninitilized value." );
		result = Undefined( revive->isolate );
		break;
	case VALUE_NULL:
		result = Null( revive->isolate );
		break;
	case VALUE_TRUE:
		result = True( revive->isolate );
		break;
	case VALUE_FALSE:
		result = False( revive->isolate );
		break;
	case VALUE_EMPTY:
		result = Undefined(revive->isolate);
		break;
	case VALUE_STRING:
		result = String::NewFromUtf8( revive->isolate, val->string, MODE, (int)val->stringLen ).ToLocalChecked();
		break;
	case VALUE_NUMBER:
		if( val->float_result )
			result = Number::New( revive->isolate, val->result_d );
		else
			result = Number::New( revive->isolate, (double)val->result_n );
		break;
	case VALUE_ARRAY:
		result = Array::New( revive->isolate );
		break;
	case VALUE_OBJECT:
		result = Object::New( revive->isolate );
		break;
	case VALUE_NEG_NAN:
		result = Number::New(revive->isolate, -NAN);
		break;
	case VALUE_NAN:
		result = Number::New(revive->isolate, NAN);
		break;
	case VALUE_NEG_INFINITY:
		result = Number::New(revive->isolate, -INFINITY);
		break;
	case VALUE_INFINITY:
		result = Number::New(revive->isolate, INFINITY);
		break;
	}
	if( revive->revive ) {
		Local<Value> args[2] = { revive->value, result };
		Local<Value> r = revive->reviver->Call( revive->_this, 2, args );
	}
	return result;
}

static void buildObject( PDATALIST msg_data, Local<Object> o, struct reviver_data *revive ) {
	Local<Value> thisVal;
	Local<String> stringKey;
	Local<Value> thisKey;
	LOGICAL saveRevive = revive->revive;
	struct json_value_container *val;
	Local<Object> sub_o;
	INDEX idx;
	int index = 0;
	DATA_FORALL( msg_data, idx, struct json_value_container*, val )
	{
		//lprintf( "value name is : %d %s", val->value_type, val->name ? val->name : "(NULL)" );
		switch( val->value_type ) {
		default:
			if( val->name ) {
				stringKey = String::NewFromUtf8( revive->isolate, val->name, MODE, (int)val->nameLen ).ToLocalChecked();
				revive->value = stringKey;
				o->CreateDataProperty( revive->context, stringKey
						, makeValue( val, revive ) );
			}
			else {
				if( val->value_type == VALUE_EMPTY )
					revive->revive = FALSE;
				if( revive->revive )
					revive->value = Integer::New( revive->isolate, index );
				o->Set( index++, thisVal = makeValue( val, revive ) );
				if( val->value_type == VALUE_EMPTY )
					o->Delete( revive->context, index - 1 );
			}
			revive->revive = saveRevive;
			break;
		case VALUE_ARRAY:
			if( val->name ) {
				o->CreateDataProperty( revive->context,
					stringKey = String::NewFromUtf8( revive->isolate, val->name, MODE, (int)val->nameLen ).ToLocalChecked()
					, sub_o = Array::New( revive->isolate ) );
				thisKey = stringKey;
			}
			else {
				if( revive->revive )
					thisKey = Integer::New( revive->isolate, index );
				o->Set( index++, sub_o = Array::New( revive->isolate ) );
			}
			buildObject( val->contains, sub_o, revive );
			if( revive->revive ) {
				Local<Value> args[2] = { thisKey, sub_o };
				revive->reviver->Call( revive->_this, 2, args );
			}
			break;
		case VALUE_OBJECT:
			if( val->name ) {
				stringKey = String::NewFromUtf8( revive->isolate, val->name, MODE, (int)val->nameLen ).ToLocalChecked();
				o->CreateDataProperty( revive->context, stringKey
							, sub_o = Object::New( revive->isolate ) );
				thisKey = stringKey;
			}
			else {
				if( revive->revive )
					thisKey = Integer::New( revive->isolate, index );
				o->Set( index++, sub_o = Object::New( revive->isolate ) );
			}

			buildObject( val->contains, sub_o, revive );
			if( revive->revive ) {
				Local<Value> args[2] = { thisKey, sub_o };
				revive->reviver->Call( revive->_this, 2, args );
			}
			break;
		}
	}
}

Local<Value> convertMessageToJS( PDATALIST msg, struct reviver_data *revive ) {
	Local<Object> o;
	Local<Value> v;// = Object::New( revive->isolate );

	struct json_value_container *val = (struct json_value_container *)GetDataItem( &msg, 0 );
	if( val && val->contains ) {
		if( val->value_type == VALUE_OBJECT )
			o = Object::New( revive->isolate );
		else if( val->value_type == VALUE_ARRAY )
			o = Array::New( revive->isolate );
		else
			lprintf( "Value has contents, but is not a container type?!" );
		buildObject( val->contains, o, revive );
		return o;
	}
	return makeValue( val, revive );
}


Local<Value> ParseJSON(  const char *utf8String, size_t len, struct reviver_data *revive ) {
	PDATALIST parsed = NULL;
	Local<Object> o;// = Object::New( isolate );
	Local<Value> v;// = Object::New( isolate );
	if( !json_parse_message( (char*)utf8String, len, &parsed ) )
	{
		//lprintf( "Failed to parse data..." );
		PTEXT error = json_parse_get_error( NULL );
		if( error )
			revive->isolate->ThrowException( Exception::Error( String::NewFromUtf8( revive->isolate, GetText( error ) ) ) );
		else
			revive->isolate->ThrowException( Exception::Error( String::NewFromUtf8( revive->isolate, "JSON Error not posted." ) ) );
		LineRelease( error );
		return Undefined( revive->isolate );
	}
	if( parsed->Cnt > 1 ) {
		lprintf( "Multiple values would result, invalid parse." );
		return Undefined(revive->isolate);
		// outside should always be a single value
	}

	Local<Value> value = convertMessageToJS( parsed, revive );
	json_dispose_message( &parsed );
	return value;
}

void parseJSON( const v8::FunctionCallbackInfo<Value>& args )
{
	struct reviver_data r;
	const char *msg;
	String::Utf8Value tmp( args[0] );
	r.isolate = Isolate::GetCurrent();
	msg = *tmp;
	Handle<Function> reviver;

	if( args.Length() > 1 ) {
		if( args[1]->IsFunction() ) {
			r._this = args.Holder();
			r.value = String::NewFromUtf8( r.isolate, "" );
			r.reviver = Handle<Function>::Cast( args[1] );
			r.revive = TRUE;
		}
		else {
			r.isolate->ThrowException( Exception::TypeError(
				String::NewFromUtf8( r.isolate, TranslateText( "Reviver parameter is not a function." ) ) ) );

			return;
		}
	}
	else
		r.revive = FALSE;
	r.context = r.isolate->GetCurrentContext();
	args.GetReturnValue().Set( ParseJSON( msg, tmp.length(), &r ) );
}


void makeJSON( const v8::FunctionCallbackInfo<Value>& args ) {
	args.GetReturnValue().Set( String::NewFromUtf8( args.GetIsolate(), "undefined :) Stringify is not completed" ) );
}

void showTimings( const v8::FunctionCallbackInfo<Value>& args ) {
     uint32_t val;
#define LOGVAL(n) val = ConvertTickToMicrosecond( timings.deltas[n] ); printf( #n " : %d.%03d\n", val/1000, val%1000 );
LOGVAL(0);
LOGVAL(1);
LOGVAL(2);
LOGVAL(3);
LOGVAL(4);
LOGVAL(5);
LOGVAL(6);
LOGVAL(7);
	{
		int n;for(n=0;n<10;n++) timings.deltas[n] = 0;
	}
	logTick(-1);
}

void escapeJSON( const v8::FunctionCallbackInfo<Value>& args ) {
	Isolate* isolate = Isolate::GetCurrent();
	if( args.Length() == 0 ) {
		isolate->ThrowException( Exception::TypeError(
			String::NewFromUtf8( isolate, TranslateText( "Missing parameter, string to escape" ) ) ) );
		return;
	}
	char *msg;
	String::Utf8Value tmp( args[0] );
	size_t outlen;
	msg = json_escape_string_length( *tmp, tmp.length(), &outlen );
	args.GetReturnValue().Set( String::NewFromUtf8( isolate, msg, NewStringType::kNormal, (int)outlen ).ToLocalChecked() );
	Release( msg );
}


Local<Value> ParseJSON6(  const char *utf8String, size_t len, struct reviver_data *revive ) {
	PDATALIST parsed = NULL;
        //logTick(2);
	if( !json6_parse_message( (char*)utf8String, len, &parsed ) ) {
		//PTEXT error = json_parse_get_error( parser->state );
		//lprintf( "Failed to parse data..." );
		PTEXT error = json_parse_get_error( NULL );
		if( error )
			revive->isolate->ThrowException( Exception::Error( String::NewFromUtf8( revive->isolate, GetText( error ) ) ) );
		else
			revive->isolate->ThrowException( Exception::Error( String::NewFromUtf8( revive->isolate, "No Error Text" STRSYM(__LINE__) ) ) );
		LineRelease( error );
		return Undefined(revive->isolate);
	}
	if( parsed && parsed->Cnt > 1 ) {
		lprintf( "Multiple values would result, invalid parse." );
		return Undefined(revive->isolate);
		// outside should always be a single value
	}
        //logTick(3);
	Local<Value> value = convertMessageToJS( parsed, revive );
        //logTick(4);

	json_dispose_message( &parsed );
        //logTick(5);

	return value;
}

void parseJSON6( const v8::FunctionCallbackInfo<Value>& args )
{
	//logTick(0);
	struct reviver_data r;
	r.isolate = Isolate::GetCurrent();
	if( args.Length() == 0 ) {
		r.isolate->ThrowException( Exception::TypeError(
			String::NewFromUtf8( r.isolate, TranslateText( "Missing parameter, data to parse" ) ) ) );
		return;
	}
	const char *msg;
	String::Utf8Value tmp( args[0] );
	Handle<Function> reviver;
	msg = *tmp;
	if( args.Length() > 1 ) {
		if( args[1]->IsFunction() ) {
			r._this = args.Holder();
			r.value = String::NewFromUtf8( r.isolate, "" );
			r.revive = TRUE;
			r.reviver = Handle<Function>::Cast( args[1] );
		}
		else {
			r.isolate->ThrowException( Exception::TypeError(
				String::NewFromUtf8( r.isolate, TranslateText( "Reviver parameter is not a function." ) ) ) );
			return;
		}
	}
	else
		r.revive = FALSE;

        //logTick(1);
	r.context = r.isolate->GetCurrentContext();

	args.GetReturnValue().Set( ParseJSON6( msg, tmp.length(), &r ) );

}


void makeJSON6( const v8::FunctionCallbackInfo<Value>& args ) {
	args.GetReturnValue().Set( String::NewFromUtf8( args.GetIsolate(), "undefined :) Stringify is not completed" ) );
}


void escapeJSON6( const v8::FunctionCallbackInfo<Value>& args ) {
	Isolate* isolate = Isolate::GetCurrent();
	if( args.Length() == 0 ) {
		isolate->ThrowException( Exception::TypeError(
			String::NewFromUtf8( isolate, TranslateText( "Missing parameter, string to escape" ) ) ) );
		return;
	}
	char *msg;
	String::Utf8Value tmp( args[0] );
	size_t outlen;
	msg = json6_escape_string_length( *tmp, tmp.length(), &outlen );
	args.GetReturnValue().Set( String::NewFromUtf8( isolate, msg, NewStringType::kNormal, (int)outlen ).ToLocalChecked() );
	Release( msg );
}
