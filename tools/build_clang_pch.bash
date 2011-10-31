clang -std=gnu++0x  -U__GXX_EXPERIMENTAL_CXX0X__ -fno-exceptions -fgnu-runtime -x c++-header \
	$1 -o $2
