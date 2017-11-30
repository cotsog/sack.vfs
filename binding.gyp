{
  "targets": [
    {
      "target_name": "sack_vfs",
      
      "sources": [ "src/vfs_module.cc",
           "src/sack/sack.cc",
           "src/sack/sqlite3.c",
           "src/com_interface.cc",
           "src/sql_module.cc",
           "src/thread_module.cc",
           "src/jsonParse.cc",
           "src/tls_interface.cc",
           "src/srg_module.cc",
           "src/websocket_module.cc",
           "src/network_module.cc",
          ],
	'defines': [
          'TARGETNAME="sack_vfs.node"',
           "__STATIC__","USE_SQLITE","USE_SQLITE_INTERFACE","FORCE_COLOR_MACROS",
           "NO_OPEN_MACRO","NO_FILEOP_ALIAS","JSON_PARSER_MAIN_SOURCE", "SQLITE_ENABLE_LOCKING_STYLE=0","SQLITE_THREADSAFE=0"
        ],
    'conditions': [
          ['OS=="linux"', {
            'defines': [
              '__LINUX__'
            ],
            'cflags_cc': ['-Wno-misleading-indentation','-Wno-parentheses','-Wno-unused-result'
			,'-Wno-char-subscripts'
			,'-Wno-empty-body','-Wno-format', '-Wno-address'
			, '-Wno-strict-aliasing', '-Wno-switch', '-Wno-missing-field-initializers' 
			, '-Wno-unused-variable', '-Wno-unused-function', '-Wno-unused-but-set-variable', '-Wno-maybe-uninitialized'
			, '-Wno-sign-compare', '-Wno-unknown-warning', '-fexceptions'
			],
            'cflags': ['-Wno-implicit-fallthrough'
			],
            'include_dirs': [
              'include/linux',
            ],
            'libraries':[ '-luuid' ]
          }],
	['node_shared_openssl=="false"', {
	      'include_dirs': [
	        '<(node_root_dir)/deps/openssl/openssl/include'
	      ],
		"conditions" : [
			["target_arch=='ia32'", {
			 "include_dirs": [ "<(node_root_dir)/deps/openssl/config/piii" ]
			}],
			["target_arch=='x64'", {
			 "include_dirs": [ "<(node_root_dir)/deps/openssl/config/k8" ]
			}],
			["target_arch=='arm'", {
			 "include_dirs": [ "<(node_root_dir)/deps/openssl/config/arm" ]
			}]
        	]	
	}],
	['OS=="mac"', {
            'defines': [
              '__LINUX__','__MAC__'
            ],
            'xcode_settings': {
                'OTHER_CFLAGS': [
                       '-Wno-self-assign', '-Wno-null-conversion', '-Wno-parentheses-equality', '-Wno-parentheses'
			,'-Wno-char-subscripts', '-Wno-null-conversion'
			,'-Wno-empty-body','-Wno-format', '-Wno-address'
			, '-Wno-strict-aliasing', '-Wno-switch', '-Wno-missing-field-initializers' 
			, '-Wno-unused-variable', '-Wno-unused-function'
			, '-Wno-sign-compare', '-Wno-null-dereference'
			, '-Wno-address-of-packed-member', '-Wno-unknown-warning-option'
			, '-Wno-unused-result', '-fexceptions'
                ],
             },
            'include_dirs': [
              'include/linux',
            ],
          }],
          ['OS=="win"', {
            'defines': [
              "NEED_SHLAPI","NEED_SHLOBJ","_CRT_SECURE_NO_WARNINGS"
            ],
            'configurations': {
              'Debug': {
                'msvs_settings': {
                  'VCCLCompilerTool': {
                    'BufferSecurityCheck': 'false',
                    'RuntimeTypeInfo': 'true',
                    'MultiProcessorCompilation' : 'true',
                    'InlineFunctionExpansion': 2,
                    'OmitFramePointers': 'true',
                    'ExceptionHandling':2

                  }
                }
              },
              'Release': {                            
                'msvs_settings': {
                  'VCCLCompilerTool': {
                    'BufferSecurityCheck': 'false',
                    'RuntimeTypeInfo': 'true',
                    'MultiProcessorCompilation' : 'true',
                    'InlineFunctionExpansion': 2,
                    'OmitFramePointers': 'true',
                    'ExceptionHandling':2
                  }
                }
              }
            },
            'sources': [
              # windows-only; exclude on other platforms.
              'src/reg_access.cc',
            ],
  	        'libraries':[ 'winmm', 'ws2_32', 'iphlpapi', 'rpcrt4', 'odbc32', 'crypt32', 'cryptui' ]
          }, { # OS != "win",
            'defines': [
              '__LINUX__',
            ],
          }]
        ],

	'otherDefinss': [ '__64__=1',
		"__NO_OPTIONS__","__NO_ODBC__" ],

    }
  ],

  "target_defaults": {
  	'include_dirs': ['src/sack']
  }
  
}

