COPTS_ANDROID = ["-std=c11",
                 "-x c",
                 "-D__ANDROID_API__=24",
                 # "-Indk",
                 # "-Ithird_party/ndk",
                 # "-Ithird_party/ndk/sysroot",
                 # "-Ithird_party/ndk/sysroot/usr",
                 # "-Ithird_party/ndk/sysroot/usr/include",
                 # "-Iexternal/openocf/third_party/ndk",
                 # "-Iexternal/openocf/third_party/ndk/sysroot/usr/include",
                 # "-Ithird_party/ndk/sysroot/usr/include",

                 # "-Iexternal/androidndk/ndk/sysroot/usr/include",
	         # "--sysroot=external/androidndk/ndk/sysroot",
	         # "-isystem", "external/androidndk/ndk/sysroot/usr/include",
                 # "-isysroot=external/androidndk/ndk/sysroot",
]