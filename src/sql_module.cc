
#include "global.h"

#ifdef INCLUDE_GUI
void editOptions( const v8::FunctionCallbackInfo<Value>& args );
#endif

//-----------------------------------------------------------
//   SQL Object
//-----------------------------------------------------------

void SqlObject::Init( Handle<Object> exports ) {
	OptionTreeObject::Init(); // SqlObject attached this

	Isolate* isolate = Isolate::GetCurrent();

	Local<FunctionTemplate> sqlTemplate;
	// Prepare constructor template
	sqlTemplate = FunctionTemplate::New( isolate, New );
	sqlTemplate->SetClassName( String::NewFromUtf8( isolate, "sack.vfs.Sqlite" ) );
	sqlTemplate->InstanceTemplate()->SetInternalFieldCount( 1 );  // need 1 implicit constructor for wrap

	// Prototype
	NODE_SET_PROTOTYPE_METHOD( sqlTemplate, "do", query );
	NODE_SET_PROTOTYPE_METHOD( sqlTemplate, "escape", escape );
	NODE_SET_PROTOTYPE_METHOD( sqlTemplate, "unescape", unescape );
	NODE_SET_PROTOTYPE_METHOD( sqlTemplate, "encode", escape );
	NODE_SET_PROTOTYPE_METHOD( sqlTemplate, "decode", unescape );
	NODE_SET_PROTOTYPE_METHOD( sqlTemplate, "end", closeDb );
	NODE_SET_PROTOTYPE_METHOD( sqlTemplate, "transaction", transact );
	NODE_SET_PROTOTYPE_METHOD( sqlTemplate, "commit", commit );
	NODE_SET_PROTOTYPE_METHOD( sqlTemplate, "autoTransact", autoTransact );

	// read a portion of the tree (passed to a callback)
	NODE_SET_PROTOTYPE_METHOD( sqlTemplate, "eo", enumOptionNodes );
	// get without create
	NODE_SET_PROTOTYPE_METHOD( sqlTemplate, "fo", findOptionNode );
	// get the node.
	NODE_SET_PROTOTYPE_METHOD( sqlTemplate, "go", getOptionNode );
	// update the value from option node
	//NODE_SET_PROTOTYPE_METHOD( sqlTemplate, "wo", writeOptionNode );
	// read value from the option node
	//NODE_SET_PROTOTYPE_METHOD( sqlTemplate, "ro", readOptionNode );


	NODE_SET_PROTOTYPE_METHOD( sqlTemplate, "op", option );
	NODE_SET_PROTOTYPE_METHOD( sqlTemplate, "getOption", option );
	//NODE_SET_PROTOTYPE_METHOD( sqlTemplate, "so", setOption );
	NODE_SET_PROTOTYPE_METHOD( sqlTemplate, "so", setOption );
	NODE_SET_PROTOTYPE_METHOD( sqlTemplate, "setOption", setOption );
	//NODE_SET_PROTOTYPE_METHOD( sqlTemplate, "makeTable", makeTable );
	NODE_SET_PROTOTYPE_METHOD( sqlTemplate, "makeTable", makeTable );
	constructor.Reset( isolate, sqlTemplate->GetFunction() );

	Local<Object> sqlfunc = sqlTemplate->GetFunction();

	SET_READONLY_METHOD(sqlfunc, "eo", enumOptionNodesInternal );
	SET_READONLY_METHOD(sqlfunc, "op", optionInternal );
	SET_READONLY_METHOD(sqlfunc, "so", setOptionInternal );
#ifdef INCLUDE_GUI
	SET_READONLY_METHOD(sqlfunc, "optionEditor", editOptions );
#endif

	exports->Set( String::NewFromUtf8( isolate, "Sqlite" ),
		sqlfunc );
}

//-----------------------------------------------------------

void SqlObject::New( const v8::FunctionCallbackInfo<Value>& args ) {
	Isolate* isolate = args.GetIsolate();
	if( args.IsConstructCall() ) {
		char *dsn;
		SqlObject* obj;
		if( args.Length() > 0 ) {
			String::Utf8Value arg( args[0] );
			dsn = *arg;
			obj = new SqlObject( dsn );
		}
		else {
			obj = new SqlObject( ":memory:" );
		}
		obj->Wrap( args.This() );
		args.GetReturnValue().Set( args.This() );
	} else {
		const int argc = 1;
		Local<Value> argv[1];
		if( args.Length() > 0 )
			argv[0] = args[0];
		Local<Function> cons = Local<Function>::New( isolate, constructor );
		args.GetReturnValue().Set( cons->NewInstance( isolate->GetCurrentContext(), args.Length(), argv ).ToLocalChecked() );
	}
}

void SqlObject::doWrap( SqlObject *sql, Local<Object> o ) {
	sql->Wrap( o );
}

//-----------------------------------------------------------

int IsTextAnyNumber( CTEXTSTR text, double *fNumber, int64_t *iNumber )
{
	CTEXTSTR pCurrentCharacter;
	int decimal_count, s, begin = TRUE, digits;
	// remember where we started...
	// if the first segment is indirect, collect it and only it
	// as the number... making indirects within a number what then?

	decimal_count = 0;
	s = 0;
	digits = 0;
	pCurrentCharacter = text;
	while( pCurrentCharacter[0] )
	{
		pCurrentCharacter = text;
		while( pCurrentCharacter && *pCurrentCharacter )
		{
			if( *pCurrentCharacter == '.' )
			{
				if( !decimal_count && digits )
					decimal_count++;
				else
					break;
			}
			else if( ((*pCurrentCharacter) == '-') && begin)
			{
				s++;
			}
			else if( ((*pCurrentCharacter) < '0') || ((*pCurrentCharacter) > '9') )
			{
				break;
			}
			else {
				digits++;
				if( !decimal_count && digits > 11 )
               return 0;
			}
			begin = FALSE;
			pCurrentCharacter++;
		}
		// invalid character - stop, we're to abort.
		if( *pCurrentCharacter )
			break;

		//while( pText );
	}

	if( *pCurrentCharacter || ( decimal_count > 1 ) || !digits )
	{
		// didn't collect enough meaningful info to be a number..
		// or information in this state is
		return 0;
	}
	if( decimal_count == 1 )
	{
		if( fNumber )
			(*fNumber) = FloatCreateFromText( text, NULL );
		// return specifically it's a floating point number
		return 2;
	}
	if( iNumber )
		(*iNumber) = IntCreateFromText( text );
	// return yes, and it's an int number
	return 1;
}
//-----------------------------------------------------------
void SqlObject::closeDb( const v8::FunctionCallbackInfo<Value>& args ) {
	//Isolate* isolate = args.GetIsolate();
	SqlObject *sql = ObjectWrap::Unwrap<SqlObject>( args.This() );
	CloseDatabase( sql->odbc );
}

void SqlObject::autoTransact( const v8::FunctionCallbackInfo<Value>& args ) {
	//Isolate* isolate = args.GetIsolate();

	SqlObject *sql = ObjectWrap::Unwrap<SqlObject>( args.This() );
	SetSQLAutoTransact( sql->odbc, args[0]->BooleanValue() );
}
//-----------------------------------------------------------
void SqlObject::transact( const v8::FunctionCallbackInfo<Value>& args ) {
	//Isolate* isolate = args.GetIsolate();

	SqlObject *sql = ObjectWrap::Unwrap<SqlObject>( args.This() );
	SQLBeginTransact( sql->odbc );
}
//-----------------------------------------------------------
void SqlObject::commit( const v8::FunctionCallbackInfo<Value>& args ) {
	//Isolate* isolate = args.GetIsolate();

	SqlObject *sql = ObjectWrap::Unwrap<SqlObject>( args.This() );
	SQLCommit( sql->odbc );
}
//-----------------------------------------------------------

void SqlObject::escape( const v8::FunctionCallbackInfo<Value>& args ) {
	Isolate* isolate = args.GetIsolate();
	SqlObject *sql = ObjectWrap::Unwrap<SqlObject>( args.This() );
	if( args[0]->IsUndefined() ) return; // undefined is still undefined
	if( args[0]->IsNull() ) { 
		args.GetReturnValue().Set( args[0] ); // undefined is still undefined
		return;
	}
	String::Utf8Value tmp( args[0] );
	char *out = EscapeSQLString(sql->odbc, (*tmp) );
	args.GetReturnValue().Set( String::NewFromUtf8( isolate, out ) );
	Deallocate( char*, out );

}
void SqlObject::unescape( const v8::FunctionCallbackInfo<Value>& args ) {
	Isolate* isolate = args.GetIsolate();
	SqlObject *sql = ObjectWrap::Unwrap<SqlObject>( args.This() );
	if( args[0]->IsUndefined() ) return; // undefined is still undefined
	if( args[0]->IsNull() ) {
		args.GetReturnValue().Set( args[0] ); // undefined is still undefined
		return;
	}
	String::Utf8Value tmp( args[0] );
	size_t outlen;
	char *out = RevertEscapeBinary( (*tmp), &outlen );
	args.GetReturnValue().Set( String::NewFromUtf8( isolate, out, NewStringType::kNormal, (int)outlen ).ToLocalChecked() );
	Deallocate( char*, out );

}
//-----------------------------------------------------------
void SqlObject::query( const v8::FunctionCallbackInfo<Value>& args ) {
	Isolate* isolate = args.GetIsolate();
	String::Utf8Value tmp( args[0] );

	SqlObject *sql = ObjectWrap::Unwrap<SqlObject>( args.This() );
	sql->fields = 0;
	if( !SQLRecordQueryLen( sql->odbc, *tmp, tmp.length(), &sql->columns, &sql->result, &sql->resultLens, &sql->fields ) ) {
		const char *error;
		FetchSQLError( sql->odbc, &error );
		isolate->ThrowException( Exception::Error(
			String::NewFromUtf8( isolate, error ) ) );
		return;
	}
	if( sql->columns )
	{
		Local<Array> records = Array::New( isolate );
		Local<Object> record = Object::New( isolate );
		if( sql->result ) {
			int row = 0;
			do {
				record = Object::New( isolate );
				for( int n = 0; n < sql->columns; n++ ) {
					if( sql->result[n] ) {
						double f;
						int64_t i;
						int type = IsTextAnyNumber( sql->result[n], &f, &i );
						if( type == 2 )
							record->Set( String::NewFromUtf8( isolate, sql->fields[n] )
										  , Number::New( isolate, f )
										  );
						else if( type == 1 )
							record->Set( String::NewFromUtf8( isolate, sql->fields[n] )
										  , Number::New( isolate, (double)i )
										  );
						else
							record->Set( String::NewFromUtf8( isolate, sql->fields[n] )
										  , String::NewFromUtf8( isolate, sql->result[n], NewStringType::kNormal, (int)sql->resultLens[n] ).ToLocalChecked()
										  );
					}
					else
						record->Set( String::NewFromUtf8( isolate, sql->fields[n] )
									  , Null(isolate)
									  );
				}
				records->Set( row++, record );
			} while( FetchSQLRecord( sql->odbc, &sql->result ) );
		}
		SQLEndQuery( sql->odbc );
		args.GetReturnValue().Set( records );
	}
	else
	{
		SQLEndQuery( sql->odbc );
		args.GetReturnValue().Set( True( isolate ) );
	}
}

//-----------------------------------------------------------


Persistent<Function> SqlObject::constructor;
SqlObject::SqlObject( const char *dsn )
{
   odbc = ConnectToDatabase( dsn );
   SetSQLThreadProtect( odbc, FALSE );
   //SetSQLAutoClose( odbc, TRUE );
   optionInitialized = FALSE;
}

SqlObject::~SqlObject() {
	CloseDatabase( odbc );
}

//-----------------------------------------------------------

Persistent<Function> OptionTreeObject::constructor;
OptionTreeObject::OptionTreeObject()  {
}

OptionTreeObject::~OptionTreeObject() {
}

void OptionTreeObject::New(const v8::FunctionCallbackInfo<Value>& args) {
	Isolate* isolate = args.GetIsolate();

	if (args.IsConstructCall()) {
		// Invoked as constructor: `new MyObject(...)`

		//double value = args[0]->IsUndefined() ? 0 : args[0]->NumberValue();
		OptionTreeObject* obj = new OptionTreeObject();
		//lprintf( "Wrap a new OTO %p in %p", obj, args.This()->ToObject() );
		obj->Wrap(args.This());
		args.GetReturnValue().Set(args.This());
	} else {
		// Invoked as plain function `MyObject(...)`, turn into construct call.
		Local<Function> cons = Local<Function>::New(isolate, constructor);
		args.GetReturnValue().Set(cons->NewInstance( isolate->GetCurrentContext(), 0, NULL ).ToLocalChecked());
	}
}


void OptionTreeObject::Init(  ) {
	Isolate* isolate = Isolate::GetCurrent();

	Local<FunctionTemplate> optionTemplate;
	// Prepare constructor template
	optionTemplate = FunctionTemplate::New( isolate, New );
	optionTemplate->SetClassName( String::NewFromUtf8( isolate, "sack.vfs.option.node" ) );
	optionTemplate->InstanceTemplate()->SetInternalFieldCount( 1 ); // 1 required for wrap

	// Prototype
	NODE_SET_PROTOTYPE_METHOD( optionTemplate, "eo", enumOptionNodes );
	NODE_SET_PROTOTYPE_METHOD( optionTemplate, "fo", findOptionNode );
	NODE_SET_PROTOTYPE_METHOD( optionTemplate, "go", getOptionNode );
	Local<Template> proto = optionTemplate->InstanceTemplate();

	proto->SetNativeDataProperty( String::NewFromUtf8( isolate, "value" )
			, readOptionNode
			, writeOptionNode );

	//NODE_SET_PROTOTYPE_METHOD( optionTemplate, "ro", readOptionNode );
	//NODE_SET_PROTOTYPE_METHOD( optionTemplate, "wo", writeOptionNode );

	constructor.Reset( isolate, optionTemplate->GetFunction() );
}


void SqlObject::getOptionNode( const v8::FunctionCallbackInfo<Value>& args ) {
	Isolate* isolate = args.GetIsolate();

	int argc = args.Length();

	if( argc < 1 ) {
		return;
	}

	SqlObject *sqlParent = ObjectWrap::Unwrap<SqlObject>( args.This() );
	if( !sqlParent->optionInitialized ) {
		SetOptionDatabaseOption( sqlParent->odbc );
		sqlParent->optionInitialized = TRUE;
	}

	String::Utf8Value tmp( args[0] );
	char *optionPath = StrDup( *tmp );

	Local<Function> cons = Local<Function>::New( isolate, OptionTreeObject::constructor );
	MaybeLocal<Object> o = cons->NewInstance( isolate->GetCurrentContext(), 0, NULL );
	args.GetReturnValue().Set( o.ToLocalChecked() );

	OptionTreeObject *oto = ObjectWrap::Unwrap<OptionTreeObject>( o.ToLocalChecked() );
	oto->odbc = sqlParent->odbc;
	//lprintf( "SO Get %p ", sqlParent->odbc );
	oto->node =  GetOptionIndexExx( sqlParent->odbc, NULL, optionPath, NULL, NULL, NULL, TRUE, TRUE DBG_SRC );
}


void OptionTreeObject::getOptionNode( const v8::FunctionCallbackInfo<Value>& args ) {
	Isolate* isolate = args.GetIsolate();

	int argc = args.Length();

	if( argc < 1 ) {
		return;
	}

	OptionTreeObject *parent = ObjectWrap::Unwrap<OptionTreeObject>( args.Holder() );

	String::Utf8Value tmp( args[0] );
	char *optionPath = StrDup( *tmp );

	Local<Function> cons = Local<Function>::New( isolate, constructor );
	Local<Object> o;
	//lprintf( "objecttreeobject constructor..." );
	args.GetReturnValue().Set( o = cons->NewInstance( isolate->GetCurrentContext(), 0, NULL ).ToLocalChecked() );

	OptionTreeObject *oto = ObjectWrap::Unwrap<OptionTreeObject>( o );
	oto->odbc = parent->odbc;
	//lprintf( "OTO Get %p  %p", parent->db->odbc, parent->node );
	oto->node =  GetOptionIndexExx( parent->odbc, parent->node, optionPath, NULL, NULL, NULL, TRUE, TRUE DBG_SRC );
	//lprintf( "result node: %s %p %p", optionPath, oto->node, o );
	Release( optionPath );
}

void SqlObject::findOptionNode( const v8::FunctionCallbackInfo<Value>& args ) {
	Isolate* isolate = args.GetIsolate();

	int argc = args.Length();

	if( argc < 1 ) {
		return;
	}
	String::Utf8Value tmp( args[0] );
	char *optionPath = StrDup( *tmp );
	SqlObject *sqlParent = ObjectWrap::Unwrap<SqlObject>( args.This() );
	if( !sqlParent->optionInitialized ) {
		SetOptionDatabaseOption( sqlParent->odbc );
		sqlParent->optionInitialized = TRUE;
	}
	POPTION_TREE_NODE newNode = GetOptionIndexExx( sqlParent->odbc, NULL, optionPath, NULL, NULL, NULL, FALSE, TRUE DBG_SRC );

	if( newNode ) {
		Local<Function> cons = Local<Function>::New( isolate, OptionTreeObject::constructor );
		Local<Object> o;
		args.GetReturnValue().Set( o = cons->NewInstance( isolate->GetCurrentContext(), 0, NULL ).ToLocalChecked() );

		OptionTreeObject *oto = ObjectWrap::Unwrap<OptionTreeObject>( o );
		oto->odbc = sqlParent->odbc;
		oto->node = newNode;
	}
	Release( optionPath );
	args.GetReturnValue().Set( Null(isolate) );
}


void OptionTreeObject::findOptionNode( const v8::FunctionCallbackInfo<Value>& args ) {
	Isolate* isolate = args.GetIsolate();

	int argc = args.Length();

	if( argc < 1 ) {
		return;
	}

	POPTION_TREE_NODE newOption;
	OptionTreeObject *parent = ObjectWrap::Unwrap<OptionTreeObject>( args.This() );

	String::Utf8Value tmp( args[0] );
	char *optionPath = StrDup( *tmp );
	newOption = GetOptionIndexExx( parent->odbc, parent->node, optionPath, NULL, NULL, NULL, FALSE, TRUE DBG_SRC );
	if( newOption ) {
		Local<Function> cons = Local<Function>::New( isolate, constructor );
		Local<Object> o;
		args.GetReturnValue().Set( o = cons->NewInstance( isolate->GetCurrentContext(), 0, NULL ).ToLocalChecked() );

		OptionTreeObject *oto = ObjectWrap::Unwrap<OptionTreeObject>( o );
		oto->odbc = parent->odbc;
		oto->node = newOption;
	}
	Release( optionPath );
	args.GetReturnValue().Set( Null( isolate ) );
}

struct enumArgs {
	Local<Function>cb;
	Isolate *isolate;
	PODBC odbc;
};

int CPROC invokeCallback( uintptr_t psv, CTEXTSTR name, POPTION_TREE_NODE ID, int flags ) {
	struct enumArgs *args = (struct enumArgs*)psv;
	Local<Value> argv[2];

	Local<Function> cons = OptionTreeObject::constructor.Get( args->isolate );
	Local<Object> o;
	o = cons->NewInstance( args->isolate->GetCurrentContext(), 0, NULL ).ToLocalChecked();

	OptionTreeObject *oto = OptionTreeObject::Unwrap<OptionTreeObject>( o );
	oto->odbc = args->odbc;
	oto->node = ID;

	argv[0] = o;
	argv[1] = String::NewFromUtf8( args->isolate, name );

	MaybeLocal<Value> r = args->cb->Call(Null(args->isolate), 2, argv );
	if( r.IsEmpty() )
		return 0;
	return 1;
}


static void enumOptionNodes( const v8::FunctionCallbackInfo<Value>& args, SqlObject *sqlParent ) {
	struct enumArgs callbackArgs;
	callbackArgs.isolate = args.GetIsolate();

	int argc = args.Length();
	if( argc < 1 ) {
		return;
	}
	
	Isolate* isolate = args.GetIsolate();
	LOGICAL dropODBC;
	if( sqlParent ) {
		if( !sqlParent->optionInitialized ) {
			SetOptionDatabaseOption( sqlParent->odbc );
			sqlParent->optionInitialized = TRUE;
		}
		callbackArgs.odbc = sqlParent->odbc;
		dropODBC = FALSE;
	}
	else {
		callbackArgs.odbc = GetOptionODBC( GetDefaultOptionDatabaseDSN() );
		dropODBC = TRUE;
	}
	Handle<Function> arg0 = Handle<Function>::Cast( args[0] );
	Local<Function> cb( arg0 );

	callbackArgs.cb = Local<Function>::New( isolate, cb );
	callbackArgs.isolate = isolate;

	EnumOptionsEx( callbackArgs.odbc, NULL, invokeCallback, (uintptr_t)&callbackArgs );
	if( dropODBC )
		DropOptionODBC( callbackArgs.odbc );
}

void SqlObject::enumOptionNodes( const v8::FunctionCallbackInfo<Value>& args ) {
	SqlObject *sqlParent = ObjectWrap::Unwrap<SqlObject>( args.This() );
	::enumOptionNodes( args, sqlParent );
}
void SqlObject::enumOptionNodesInternal( const v8::FunctionCallbackInfo<Value>& args ) {
	::enumOptionNodes( args, NULL );
}

void OptionTreeObject::enumOptionNodes( const v8::FunctionCallbackInfo<Value>& args ) {
	struct enumArgs callbackArgs;
	callbackArgs.isolate = args.GetIsolate();

	int argc = args.Length();
	if( argc < 1 ) {
		return;
	}

	Isolate* isolate = args.GetIsolate();
	OptionTreeObject *oto = ObjectWrap::Unwrap<OptionTreeObject>( args.This() );
	Handle<Function> arg0 = Handle<Function>::Cast( args[0] );
	Local<Function> cb( arg0 );

	callbackArgs.odbc = oto->odbc;
	callbackArgs.cb = Local<Function>::New( isolate, cb );
	callbackArgs.isolate = isolate;

	EnumOptionsEx( oto->odbc, oto->node, invokeCallback, (uintptr_t)&callbackArgs );
}

void OptionTreeObject::readOptionNode( v8::Local<v8::String> field,
                              const PropertyCallbackInfo<v8::Value>& info ) {
	OptionTreeObject* oto = node::ObjectWrap::Unwrap<OptionTreeObject>( info.This() );
	char *buffer;
	size_t buflen;
	int res = (int)GetOptionStringValueEx( oto->odbc, oto->node, &buffer, &buflen DBG_SRC );
	if( !buffer || res < 0 )
		return;
	info.GetReturnValue().Set( String::NewFromUtf8( info.GetIsolate(), buffer ) );
}

void OptionTreeObject::writeOptionNode( v8::Local<v8::String> field,
                              v8::Local<v8::Value> val,
                              const PropertyCallbackInfo<void>&info ) {
	String::Utf8Value tmp( val );
	OptionTreeObject* oto = node::ObjectWrap::Unwrap<OptionTreeObject>( info.Holder() );
	SetOptionStringValueEx( oto->odbc, oto->node, *tmp );
}


static void option_( const v8::FunctionCallbackInfo<Value>& args, int internal ) {
	Isolate* isolate = args.GetIsolate();

	int argc = args.Length();
	char *sect;
	char *optname;
	char *defaultVal;

	if( argc > 0 ) {
		String::Utf8Value tmp( args[0] );
		defaultVal = StrDup( *tmp );
	}
	else
		defaultVal = StrDup( "" );

	if( argc > 1 ) {
		String::Utf8Value tmp( args[1] );
		sect = defaultVal;
		defaultVal = StrDup( *tmp );
	}
	else
		sect = NULL;

	if( argc > 2 ) {
		String::Utf8Value tmp( args[2] );
		optname = defaultVal;
		defaultVal = StrDup( *tmp );
	}
	else
		optname = NULL;

	TEXTCHAR readbuf[1024];
	PODBC use_odbc = NULL;
	if( internal ) {
		// use_odbc = NULL;
	} else 
	{
		SqlObject *sql = SqlObject::Unwrap<SqlObject>( args.This() );
		sql->fields = 0;

		if( !sql->optionInitialized ) {
			SetOptionDatabaseOption( sql->odbc );
			sql->optionInitialized = TRUE;
		}
		use_odbc = sql->odbc;
	}
	SACK_GetPrivateProfileStringExxx( use_odbc
		, sect
		, optname
		, defaultVal
		, readbuf
		, 1024
		, NULL
		, TRUE
		DBG_SRC
		);

	Local<String> returnval = String::NewFromUtf8( isolate, readbuf );
	args.GetReturnValue().Set( returnval );

	Deallocate( char*, optname );
	Deallocate( char*, sect );
	Deallocate( char*, defaultVal );
}

void SqlObject::option( const v8::FunctionCallbackInfo<Value>& args ) {
	option_( args, 0 );
}

void SqlObject::optionInternal( const v8::FunctionCallbackInfo<Value>& args ) {
	option_( args, 1 );
}

//-----------------------------------------------------------

static void setOption( const v8::FunctionCallbackInfo<Value>& args, int internal ) {
	Isolate* isolate = args.GetIsolate();

	int argc = args.Length();
	char *sect;
	char *optname;
	char *defaultVal;

	if( argc > 0 ) {
		String::Utf8Value tmp( args[0] );
		defaultVal = StrDup( *tmp );
	}
	else
		defaultVal = StrDup( "" );

	if( argc > 1 ) {
		String::Utf8Value tmp( args[1] );
		sect = defaultVal;
		defaultVal = StrDup( *tmp );
	}
	else
		sect = NULL;

	if( argc > 2 ) {
		String::Utf8Value tmp( args[2] );
		optname = defaultVal;
		defaultVal = StrDup( *tmp );
	}
	else
		optname = NULL;

	TEXTCHAR readbuf[1024];

	PODBC use_odbc = NULL;
	if( internal ) {
	} else 
	{
		SqlObject *sql = SqlObject::Unwrap<SqlObject>( args.This() );
		sql->fields = 0;
		use_odbc = sql->odbc;
	}

	SACK_WriteOptionString( use_odbc
		, sect
		, optname
		, defaultVal
	);

	Local<String> returnval = String::NewFromUtf8( isolate, readbuf );
	args.GetReturnValue().Set( returnval );

	Deallocate( char*, optname );
	Deallocate( char*, sect );
	Deallocate( char*, defaultVal );
}

void SqlObject::setOption( const v8::FunctionCallbackInfo<Value>& args ) {
	::setOption( args, 0 );
}
void SqlObject::setOptionInternal( const v8::FunctionCallbackInfo<Value>& args ) {
	::setOption( args, 1 );
}

//-----------------------------------------------------------

void SqlObject::makeTable( const v8::FunctionCallbackInfo<Value>& args ) {
	Isolate* isolate = args.GetIsolate();

	int argc = args.Length();
	char *tableCommand;

	if( argc > 0 ) {
		PTABLE table;
		String::Utf8Value tmp( args[0] );
		tableCommand = StrDup( *tmp );

		SqlObject *sql = ObjectWrap::Unwrap<SqlObject>( args.This() );

		table = GetFieldsInSQLEx( tableCommand, false DBG_SRC );
		if( CheckODBCTable( sql->odbc, table, CTO_MERGE ) )

			args.GetReturnValue().Set( True(isolate) );
		args.GetReturnValue().Set( False( isolate ) );

	}
	//args.GetReturnValue().Set( returnval );
	args.GetReturnValue().Set( False( isolate ) );
}

static void callUserFunction( struct sqlite3_context*onwhat, int argc, struct sqlite3_value**argv ) {
	struct SqlObjectUserFunction *userData = (struct SqlObjectUserFunction*)PSSQL_GetSqliteFunctionData( onwhat );
	Local<Value> *args;
	if( argc > 0 ) {
		int n;
		char *text;
		int textLen;
		args = new Local<Value>[argc];
		for( n = 0; n < argc; n++ ) {
			PSSQL_GetSqliteValueText( argv[n], (const char**)&text, &textLen );
			args[n] = String::NewFromUtf8( userData->isolate, text, NewStringType::kNormal, textLen ).ToLocalChecked;
		}
	} else {
		args = NULL;
	}
	Local<Function> cb = Local<Function>::New( userData->isolate, userData->cb );
	Local<Value> str = cb->Call( userData->sql->handle(), argc, args );
	String::Utf8Value result( str->ToString() );
	PSSQL_ResultSqliteText( onwhat, *result, result.length(), 0 );
	if( argc > 0 ) {
		delete[] args;
	}
}

void SqlObject::userFunction( const v8::FunctionCallbackInfo<Value>& args ) {
	Isolate* isolate = args.GetIsolate();
	int argc = args.Length();

	if( argc > 0 ) {
		SqlObject *sql = ObjectWrap::Unwrap<SqlObject>( args.This() );
		String::Utf8Value name( args[0] );
		struct SqlObjectUserFunction *userData = NewArray( struct SqlObjectUserFunction, 1 );
		userData->isolate = isolate;
		userData->cb.Reset( isolate, Handle<Function>::Cast( args[1] ) );
		userData->sql = sql;
		userData->thread = MakeThread();
	
		PSSQL_AddSqliteFunction( sql->odbc, *name, callUserFunction, -1, userData );
		/*
		sqlite3 *db = GetODBCHandle( sql->odbc );
		rc = sqlite3_create_function(
			db //sqlite3 *,
			, *name  //const char *zFunctionName,
			, -1 //int nArg,
			, SQLITE_UTF8 //int eTextRep,
			, (void*)userData //void*,
			, callUserFunction //void (*xFunc)(sqlite3_context*,int,sqlite3_value**),
			, NULL //void (*xStep)(sqlite3_context*,int,sqlite3_value**),
			, NULL //void (*xFinal)(sqlite3_context*)
		);
		*/
	}
}

#ifdef INCLUDE_GUI
static uintptr_t RunEditor( PTHREAD thread ) {
	int (*EditOptions)( PODBC odbc, PSI_CONTROL parent, LOGICAL wait );
	extern void disableEventLoop( void );


	EditOptions = (int(*)(PODBC,PSI_CONTROL,LOGICAL))GetThreadParam( thread );
	EditOptions( NULL, NULL, TRUE );
	disableEventLoop();
	return 0;
}

void editOptions( const v8::FunctionCallbackInfo<Value>& args ){
	int (*EditOptions)( PODBC odbc, PSI_CONTROL parent, LOGICAL wait );
	extern void enableEventLoop( void );
	EditOptions = (int(*)( PODBC, PSI_CONTROL,LOGICAL))LoadFunction( "EditOptions.plugin", "EditOptionsEx" );
	if( EditOptions ) {
		enableEventLoop();
		ThreadTo( RunEditor, (uintptr_t)EditOptions );
	} else
		lprintf( "Failed to load editor..." );
}
#endif