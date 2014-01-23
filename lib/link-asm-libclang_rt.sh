#!/bin/sh
# Utility script for linking assembled objects into libclang_rt
# rationale: cmake build fails to include .S files
# usage: define the following variables and source this script, call function

# variables needed by the following function
# CPP=preprocessor (gcc -E)
# as=assembler
# arch={ppc,i386,...}
# builddir=.../projects/compiler-rt/lib (relative to where function is called)
# crtsrcdir=.../compiler-rt.git (absolute path)
# objdir=CMakeFiles/clang_rt.$arch.dir/$arch (relative to builddir)
# libcrt=$libdir/libclang_rt.$arch.a

assemble_clang_rt() {
pushd $builddir
for f in $crtsrcdir/lib/$arch/*.S
do      
        b=`basename $f`
	echo "Assembling $arch/$b."
        $CPP $AS_CPPFLAGS $f | $as -arch $arch -o $objdir/$b.o
done
echo "Updating static library $libcrt."
ar cru $libcrt $objdir/*.S.o
popd
}

