{
    'targets': [
        {
            'target_name': 'node-flac',
            'sources': [
                'src/node-flac.cpp',
            ],
            "include_dirs": [
                '<!(node -e "require(\'nan\')")'
            ],
            'dependencies': [
            ],
            'libraries': [
                '-lFLAC++'
            ],
            'conditions':[
                ['OS=="mac"', {
                    'xcode_settings': {
                        'OTHER_CPLUSPLUSFLAGS': ["-std=c++11", "-stdlib=libc++", "-fPIC"],
                        'GCC_ENABLE_CPP_RTTI': 'YES',
                        'GCC_ENABLE_CPP_EXCEPTIONS': 'YES',
                        'MACOSX_DEPLOYMENT_TARGET':'10.8',
                        'CLANG_CXX_LIBRARY': 'libc++',
                        'CLANG_CXX_LANGUAGE_STANDARD':'c++11',
                        'GCC_VERSION': 'com.apple.compilers.llvm.clang.1_0'
                    },
                }],
            ]
        }
    ]
}
