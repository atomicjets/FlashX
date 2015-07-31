#!/bin/sh

R CMD build Rpkg
version=`R --version | grep "R version" | awk '{print $3}' | awk -F. '{print $1 "." $2}'`
if [ -d ~/R ]; then
	path=`find ~/R -name Rcpp.h | grep ${version}`
fi
if [ -z "$path" ]; then
	path=`find /usr/local -name Rcpp.h`
fi

fg_lib=`pwd`/build
matrix_path=`find $fg_lib/flash-graph -name libmatrix.a`
if [ -n "$matrix_path" ]; then
	echo "find libmatrix.a"
	export FG_EIGEN=1
fi
if [ -n "$path" ]; then
	path=`dirname $path`
	echo $path
	RCPP_INCLUDE=$path FG_DIR=`pwd` FG_LIB=$fg_lib R CMD INSTALL FlashGraphR_0.3-0.tar.gz
else
	FG_DIR=`pwd` FG_LIB=$fg_lib R CMD INSTALL FlashGraphR_0.3-0.tar.gz
fi
rm FlashGraphR_0.3-0.tar.gz
