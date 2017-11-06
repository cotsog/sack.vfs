
var sack = require( '..' );
var https = sack.HTTPS;

		var opts = {  hostname: 'google.com',
					  //port : 443,
					  method : "GET",
					  ca : null,
					  rejectUnauthorized: true,
					  path : "/"
                                          //, agent : false
					};

		var res = https.get( opts );
		console.log( "Result:", res );
		{
			const statusCode = res.statusCode;
			const contentType = res.headers['content-type'] || res.headers['Content-Type'];
			let error;
                        console.log( "http get response happened...", contentType );
			if (statusCode !== 200) {
				error = new Error(`Request Failed.\n` +
						`Status Code: ${statusCode}` + JSON.stringify( opts ) );
			} else if (/^text\/javascript/.test(contentType)) {
				evalCode = true;
			} else if (/^application\/javascript/.test(contentType)) {
				evalCode = true;
			} else if (/^application\/json/.test(contentType)) {
				evalJson = true;
			}
			else {
				error = new Error(`Invalid content-type.\n` +
								`Expected application/json or application/javascript but received ${contentType}`);
        
			}
			if (error) {
				console.log(error.message);
				// consume response data to free up memory
				return;
			}
        
		};

		
console.log( "completed" );