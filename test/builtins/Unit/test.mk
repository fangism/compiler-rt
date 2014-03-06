# include this Makefile in $(build_dir)/projects/compiler-rt/test and predefine:
# ARCH - ppc,i386,x86_64
# OS - darwin
# CC - compiler
# CFLAGS - options
# LLVM_SRCDIR - project root source dir
# LIBS
# VERSION - 3.x
# CXXLIB - link libstdc++ or libc++

COMPILER_RT_SRCDIR = $(LLVM_SRCDIR)/projects/compiler-rt
VPATH = $(COMPILER_RT_SRCDIR)/test/builtins/Unit
LIBDIR = ../../../lib
DEFAULT_CFLAGS = -Os -nodefaultlibs
DEFAULT_CPPFLAGS = -I$(COMPILER_RT_SRCDIR)/lib/builtins
TEST_LIBDIR = $(LIBDIR)/clang/$(VERSION)/lib/$(OS)
# LDFLAGS = -L$(TEST_LIBDIR)
TEST_LIB = $(TEST_LIBDIR)/libclang_rt.$(ARCH).a
LIBUNWIND = /usr/lib/libgcc_s.1.dylib
CXXLIB ?= -lstdc++
CXXLINK = $(CXXLIB) $(TEST_LIB) $(LIBUNWIND) $(LIBS)

.SUFFIXES: .c

all:

.c:
	$(CC) $(DEFAULT_CPPFLAGS) $(DEFAULT_CFLAGS) $(CFLAGS) $< \
		$(LDFLAGS) $(TEST_LIB) $(LIBS) -o $@

gcc_personality_test: gcc_personality_test.c gcc_personality_test_helper.cxx
	$(CC) $(DEFAULT_CPPFLAGS) $(DEFAULT_CFLAGS) $(CFLAGS) -fexceptions \
		$< $(VPATH)/gcc_personality_test_helper.cxx \
		$(LDFLAGS) $(CXXLINK) -o $@

trampoline_setup_test: trampoline_setup_test.c
	$(CC) $(DEFAULT_CPPFLAGS) $(DEFAULT_CFLAGS) $(CFLAGS) \
		-fnested-functions $< $(LDFLAGS) $(TEST_LIB) $(LIBS) -o $@

C_TESTS = absvdi2_test absvsi2_test absvti2_test adddf3vfp_test \
	addsf3vfp_test addvdi3_test addvsi3_test addvti3_test \
	ashldi3_test ashlti3_test ashrdi3_test ashrti3_test \
	bswapdi2_test bswapsi2_test clear_cache_test clzdi2_test \
	clzsi2_test clzti2_test cmpdi2_test cmpti2_test \
	comparedf2_test comparesf2_test ctzdi2_test ctzsi2_test \
	ctzti2_test divdc3_test divdf3vfp_test divdi3_test \
	divmodsi4_test divsc3_test divsf3vfp_test divsi3_test \
	divtc3_test divti3_test divxc3_test \
	enable_execute_stack_test eqdf2vfp_test eqsf2vfp_test \
	extebdsfdf2vfp_test ffsdi2_test ffsti2_test fixdfdi_test \
	fixdfsivfp_test fixdfti_test fixsfdi_test fixsfsivfp_test \
	fixsfti_test fixunsdfdi_test fixunsdfsi_test fixunsdfsivfp_test \
	fixunsdfti_test fixunssfdi_test fixunssfsi_test \
	fixunssfsivfp_test fixunssfti_test fixunstfdi_test \
	fixunsxfdi_test fixunsxfsi_test fixunsxfti_test fixxfdi_test \
	fixxfti_test floatdidf_test floatdisf_test floatdixf_test \
	floatsidfvfp_test floatsisfvfp_test floattidf_test \
	floattisf_test floattixf_test floatundidf_test floatundisf_test \
	floatundixf_test floatunssidfvfp_test floatunssisfvfp_test \
	floatuntidf_test floatuntisf_test floatuntixf_test \
	gcc_personality_test gedf2vfp_test gesf2vfp_test \
	gtdf2vfp_test gtsf2vfp_test ledf2vfp_test lesf2vfp_test \
	lshrdi3_test lshrti3_test ltdf2vfp_test ltsf2vfp_test \
	moddi3_test modsi3_test modti3_test muldc3_test \
	muldf3vfp_test muldi3_test mulodi4_test mulosi4_test \
	muloti4_test mulsc3_test mulsf3vfp_test multc3_test \
	multi3_test mulvdi3_test mulvsi3_test mulvti3_test \
	mulxc3_test nedf2vfp_test negdf2vfp_test negdi2_test \
	negsf2vfp_test negti2_test negvdi2_test negvsi2_test \
	negvti2_test nesf2vfp_test paritydi2_test paritysi2_test \
	parityti2_test popcountdi2_test popcountsi2_test \
	popcountti2_test powidf2_test powisf2_test \
	powitf2_test powixf2_test subdf3vfp_test subsf3vfp_test \
	subvdi3_test subvsi3_test subvti3_test \
	trampoline_setup_test truncdfsf2vfp_test ucmpdi2_test \
	ucmpti2_test udivdi3_test udivmoddi4_test udivmodsi4_test \
	udivmodti4_test udivsi3_test udivti3_test umoddi3_test \
	umodsi3_test umodti3_test unorddf2vfp_test unordsf2vfp_test

C_TEST_SOURCES = $(C_TESTS:=.c)
TESTS = $(C_TESTS)

all: $(TESTS)

check: all
	pass=0 ; \
	fail=0 ; \
	for t in $(TESTS) ; \
	do if ./$$t ; \
	then echo "PASS: $$t" ; pass=`expr $$pass + 1` ; \
	else echo "FAIL: $$t" ; fail=`expr $$fail + 1` ; \
	fi ; \
	done ; \
	echo "===================" ; \
	echo "$$pass tests passed." ; \
	echo "$$fail tests failed." ; \
	test "$$fail" = 0

clean:
	rm -f $(TESTS)

