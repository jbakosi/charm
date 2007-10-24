# gfortran.org

CMK_CC="$CMK_CC -DCMK_GFORTRAN"
CMK_CXX="$CMK_CXX -DCMK_GFORTRAN"

if test -n "$CMK_MACOSX64"
then
CMK_F90FLAGS="$CMK_F90FLAGS -m64"
fi

if test -n "$CMK_MACOSX"
then
CMK_F90FLAGS="$CMK_F90FLAGS -fno-common"
fi

CMK_CF90=`which gfortran 2>/dev/null`
CMK_FPP="/lib/cpp -P -CC"
CMK_CF90="$CMK_CF90 $CMK_F90FLAGS -fPIC -fno-second-underscore -fdollar-ok"
CMK_CF90_FIXED="$CMK_CF90 -ffixed-form "
# find f90 library:
#it can be at gfortran-install/lib/gcc-lib/i686-pc-linux-gnu/4.0.1
F90DIR=`which gfortran 2> /dev/null`
#F90DIR=$HOME/gfortran-install/bin/gfortran
readlinkcmd=`which readlink 2> /dev/null`
if test -h "$F90DIR" && test -x "$readlinkcmd"
then
  F90DIR=`readlink $F90DIR`
  test `basename $F90DIR` = "$F90DIR" && F90DIR=`which gfortran 2> /dev/null`
fi
F90DIR="`dirname $F90DIR`"
if test "$F90DIR" = '/usr/bin'
then
F90LIBDIR=/usr/lib
else
F90LIBDIR=`cd $F90DIR/../lib/gcc/ia64-unknown-linux-gnu/4.1.0; pwd`
fi
CMK_F90LIBS="-L$F90LIBDIR -lgfortran -lgcc_eh"

CMK_MOD_NAME_ALLCAPS=
CMK_MOD_EXT="mod"
CMK_F90_USE_MODDIR=1
CMK_F90_MODINC="-I"

