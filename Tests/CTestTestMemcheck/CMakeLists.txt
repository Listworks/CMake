REGEX_ESCAPE_STRING(CTEST_ESCAPED_CMAKE_CURRENT_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}")

get_filename_component(CTEST_REALPATH_CMAKE_CURRENT_BINARY_DIR
  "${CMAKE_CURRENT_BINARY_DIR}" REALPATH
)

REGEX_ESCAPE_STRING(CTEST_ESCAPED_REALPATH_CMAKE_CURRENT_BINARY_DIR
  "${CTEST_REALPATH_CMAKE_CURRENT_BINARY_DIR}"
)

foreach (_retval 0 1)
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/memtester.cxx.in" "${CMAKE_CURRENT_BINARY_DIR}/ret${_retval}.cxx" @ONLY)
endforeach ()

include_directories(${CMake_SOURCE_DIR}/Source ${CMake_BINARY_DIR}/Source)

# create binaries that we will use as a pseudo memory checker
add_executable(pseudo_valgrind "${CMAKE_CURRENT_BINARY_DIR}/ret0.cxx")
set_target_properties(pseudo_valgrind PROPERTIES OUTPUT_NAME valgrind)
target_link_libraries(pseudo_valgrind CMakeLib)

add_executable(pseudo_purify "${CMAKE_CURRENT_BINARY_DIR}/ret0.cxx")
set_target_properties(pseudo_purify PROPERTIES OUTPUT_NAME purify)
target_link_libraries(pseudo_purify CMakeLib)
add_executable(pseudo_BC "${CMAKE_CURRENT_BINARY_DIR}/ret0.cxx")
set_target_properties(pseudo_BC PROPERTIES OUTPUT_NAME BC)
target_link_libraries(pseudo_BC CMakeLib)

# binary to be used as pre- and post-memcheck command that fails
add_executable(memcheck_fail "${CMAKE_CURRENT_BINARY_DIR}/ret1.cxx")
target_link_libraries(memcheck_fail CMakeLib)

# Binaries that are used as memchecker that do not write the expected
# output file. Need to be in their own subdirectory as they have the
# same filenames.
add_subdirectory(NoLogDummyChecker)

if(APPLE)
  # filter out additional messages by Guard Malloc integrated in Xcode
  set(guard_malloc_msg "ctest\\([0-9]+\\) malloc: ")
  set(guard_malloc_lines "(${guard_malloc_msg}[^\n]*\n)*")
  set(guard_malloc_output "${guard_malloc_msg}|")
else()
  set(guard_malloc_msg "")
  set(guard_malloc_lines "")
  set(guard_malloc_output "")
endif()

# When this entire test runs under coverage or memcheck tools
# they may add output to the end, so match known cases:
#  - Bullseye adds a "BullseyeCoverage..." line.
#  - Valgrind memcheck may add extra "==..." lines.
set(other_tool_output "((${guard_malloc_output}BullseyeCoverage|==)[^\n]*\n)*")

string(REPLACE "\r\n" "\n" ctest_and_tool_outputs "
1/1 MemCheck #1: RunCMake \\.+   Passed +[0-9]+\\.[0-9]+ sec
${guard_malloc_lines}
100% tests passed, 0 tests failed out of 1
.*
-- Processing memory checking output:( )
${guard_malloc_lines}Memory checking results:
${other_tool_output}")

function(gen_mc_test_internal NAME CHECKER)
    set(SUBTEST_NAME "${NAME}")
    set(CHECKER_COMMAND "${CHECKER}")
    foreach(_file IN ITEMS CMakeLists.txt CTestConfig.cmake test.cmake)
        configure_file("${CMAKE_CURRENT_SOURCE_DIR}/${_file}.in"
            "${CMAKE_CURRENT_BINARY_DIR}/${NAME}/${_file}" @ONLY)
    endforeach()
    add_test(NAME CTestTestMemcheck${NAME}
        COMMAND ${CMAKE_CTEST_COMMAND}
        -C $<CONFIGURATION>
        -S "${CMAKE_CURRENT_BINARY_DIR}/${NAME}/test.cmake" -V
        --output-log "${CMAKE_CURRENT_BINARY_DIR}/${NAME}/testOutput.log"
        ${ARGN}
        )
endfunction(gen_mc_test_internal)

function(gen_mc_test NAME CHECKER)
    gen_mc_test_internal(${NAME} "${CHECKER}"
        -D PSEUDO_BC=$<TARGET_FILE:pseudo_BC>
        -D PSEUDO_PURIFY=$<TARGET_FILE:pseudo_purify>
        -D PSEUDO_VALGRIND=$<TARGET_FILE:pseudo_valgrind>
        -D ERROR_COMMAND=$<TARGET_FILE:memcheck_fail>
        ${ARGN}
        )
endfunction(gen_mc_test)

function(gen_mcnl_test NAME CHECKER)
    gen_mc_test_internal(${NAME} ${CHECKER}
        -D PSEUDO_BC=$<TARGET_FILE:pseudonl_BC>
        -D PSEUDO_PURIFY=$<TARGET_FILE:pseudonl_purify>
        -D PSEUDO_VALGRIND=$<TARGET_FILE:pseudonl_valgrind>
        ${ARGN}
        )
    set_tests_properties(CTestTestMemcheck${NAME}
        PROPERTIES
        PASS_REGULAR_EXPRESSION "\nCannot find memory tester output file: ${CTEST_ESCAPED_CMAKE_CURRENT_BINARY_DIR}/${NAME}/Testing/Temporary/MemoryChecker.1.log\n(.*\n)?Error in read script: ${CTEST_ESCAPED_CMAKE_CURRENT_BINARY_DIR}/${NAME}/test.cmake\n")
endfunction(gen_mcnl_test)

unset(CTEST_EXTRA_CONFIG)
unset(CTEST_EXTRA_CODE)
unset(CMAKELISTS_EXTRA_CODE)

#-----------------------------------------------------------------------------
# add ThreadSanitizer test
set(CTEST_EXTRA_CODE
"set(CTEST_MEMORYCHECK_COMMAND_OPTIONS \"report_bugs=1 history_size=5 exitcode=55\")
")
set(CMAKELISTS_EXTRA_CODE
"add_test(NAME TestSan COMMAND \"${CMAKE_COMMAND}\"
-P \"${CMAKE_CURRENT_SOURCE_DIR}/testThreadSanitizer.cmake\")
")
gen_mc_test_internal(DummyThreadSanitizer "" -DMEMCHECK_TYPE=ThreadSanitizer)
set_tests_properties(CTestTestMemcheckDummyThreadSanitizer PROPERTIES
    PASS_REGULAR_EXPRESSION
    ".*Memory checking results:.*data race.* - 1.*data race on vptr .ctor/dtor vs virtual call. - 1.*heap-use-after-free - 1.*thread leak - 1.*destroy of a locked mutex - 1.*double lock of a mutex - 1.*unlock of an unlocked mutex .or by a wrong thread. - 1.*read lock of a write locked mutex - 1.*read unlock of a write locked mutex - 1.*signal-unsafe call inside of a signal - 1.*signal handler spoils errno - 1.*lock-order-inversion .potential deadlock. - 1.*")
set(CMAKELISTS_EXTRA_CODE )
set(CTEST_EXTRA_CODE)

#-----------------------------------------------------------------------------
# add LeakSanitizer test
set(CTEST_EXTRA_CODE
"set(CTEST_MEMORYCHECK_SANITIZER_OPTIONS \"simulate_sanitizer=1 report_bugs=1 history_size=5 exitcode=55\")
")
set(CMAKELISTS_EXTRA_CODE
"add_test(NAME TestSan COMMAND \"${CMAKE_COMMAND}\"
-P \"${CMAKE_CURRENT_SOURCE_DIR}/testLeakSanitizer.cmake\")
")
gen_mc_test_internal(DummyLeakSanitizer "" -DMEMCHECK_TYPE=AddressSanitizer)
set(CMAKELISTS_EXTRA_CODE )
set(CTEST_EXTRA_CODE)
set_tests_properties(CTestTestMemcheckDummyLeakSanitizer PROPERTIES
    PASS_REGULAR_EXPRESSION
    ".*Memory checking results:.*Direct leak - 2.*Indirect leak - 1.*")

#-----------------------------------------------------------------------------
# add AddressSanitizer test
set(CTEST_EXTRA_CODE
"set(CTEST_MEMORYCHECK_SANITIZER_OPTIONS \"simulate_sanitizer=1 report_bugs=1 history_size=5 exitcode=55\")
")
set(CMAKELISTS_EXTRA_CODE
"add_test(NAME TestSan COMMAND \"${CMAKE_COMMAND}\"
-P \"${CMAKE_CURRENT_SOURCE_DIR}/testAddressSanitizer.cmake\")
")
gen_mc_test_internal(DummyAddressSanitizer "" -DMEMCHECK_TYPE=AddressSanitizer)
set(CMAKELISTS_EXTRA_CODE )
set(CTEST_EXTRA_CODE)
set_tests_properties(CTestTestMemcheckDummyAddressSanitizer PROPERTIES
    PASS_REGULAR_EXPRESSION
    ".*Memory checking results:.*heap-buffer-overflow - 1.*")

#-----------------------------------------------------------------------------
# add MemorySanitizer test
set(CTEST_EXTRA_CODE
"set(CTEST_MEMORYCHECK_COMMAND_OPTIONS \"simulate_sanitizer=1 report_bugs=1 history_size=5 exitcode=55\")
")

set(CMAKELISTS_EXTRA_CODE
"add_test(NAME TestSan COMMAND \"${CMAKE_COMMAND}\"
-P \"${CMAKE_CURRENT_SOURCE_DIR}/testMemorySanitizer.cmake\")
")
gen_mc_test_internal(DummyMemorySanitizer "" -DMEMCHECK_TYPE=MemorySanitizer)
set(CMAKELISTS_EXTRA_CODE )
set(CTEST_EXTRA_CODE)
set_tests_properties(CTestTestMemcheckDummyMemorySanitizer PROPERTIES
    PASS_REGULAR_EXPRESSION
    ".*Memory checking results:.*use-of-uninitialized-value - 1.*")

#-----------------------------------------------------------------------------
# add UndefinedBehaviorSanitizer test
set(CTEST_EXTRA_CODE
"set(CTEST_MEMORYCHECK_SANITIZER_OPTIONS \"simulate_sanitizer=1\")
")

set(CMAKELISTS_EXTRA_CODE
"add_test(NAME TestSan COMMAND \"${CMAKE_COMMAND}\"
-P \"${CMAKE_CURRENT_SOURCE_DIR}/testUndefinedBehaviorSanitizer.cmake\")
")
gen_mc_test_internal(DummyUndefinedBehaviorSanitizer "" -DMEMCHECK_TYPE=UndefinedBehaviorSanitizer)
set(CMAKELISTS_EXTRA_CODE )
set(CTEST_EXTRA_CODE)
set_tests_properties(CTestTestMemcheckDummyUndefinedBehaviorSanitizer PROPERTIES
    PASS_REGULAR_EXPRESSION
    ".*Memory checking results:.*left shift of negative value -256 - 1.*")

#-----------------------------------------------------------------------------

gen_mc_test(DummyPurify "\${PSEUDO_PURIFY}")
gen_mc_test(DummyValgrind "\${PSEUDO_VALGRIND}")
gen_mc_test(DummyBC "\${PSEUDO_BC}")
gen_mcnl_test(DummyPurifyNoLogfile "\${PSEUDO_PURIFY}")
gen_mcnl_test(DummyValgrindNoLogfile "\${PSEUDO_VALGRIND}")
gen_mcnl_test(DummyBCNoLogfile "\${PSEUDO_BC}")

set(CTEST_EXTRA_CODE "string(REPLACE \" \" \"\\\\ \" PRE_POST_COMMAND \"\${CTEST_MEMORYCHECK_COMMAND}\")

set(CTEST_CUSTOM_PRE_MEMCHECK \"\${PRE_POST_COMMAND} pre command\")
set(CTEST_CUSTOM_POST_MEMCHECK \"\${PRE_POST_COMMAND} post command \")
")
gen_mc_test(DummyValgrindPrePost "\${PSEUDO_VALGRIND}")

set(CTEST_EXTRA_CODE "set(CTEST_CUSTOM_POST_MEMCHECK \"\${ERROR_COMMAND}\")")
gen_mc_test(DummyValgrindFailPost "\${PSEUDO_VALGRIND}")

set(CTEST_EXTRA_CODE "set(CTEST_CUSTOM_PRE_MEMCHECK \"\${ERROR_COMMAND}\")")
gen_mc_test(DummyValgrindFailPre "\${PSEUDO_VALGRIND}")

unset(CTEST_EXTRA_CODE)
set(CTEST_EXTRA_CONFIG "set(CTEST_CUSTOM_MEMCHECK_IGNORE RunCMakeAgain)\n")
set(CMAKELISTS_EXTRA_CODE "add_test(NAME RunCMakeAgain COMMAND \"\${CMAKE_COMMAND}\" --version)")
gen_mc_test(DummyValgrindIgnoreMemcheck "\${PSEUDO_VALGRIND}")

unset(CTEST_EXTRA_CONFIG)
gen_mc_test(DummyValgrindTwoTargets "\${PSEUDO_VALGRIND}" "-VV")

set(CTEST_EXTRA_CONFIG "set(CTEST_MEMORYCHECK_SUPPRESSIONS_FILE \"\${CMAKE_CURRENT_BINARY_DIR}/does-not-exist\")")
unset(CMAKELISTS_EXTRA_CODE)
gen_mc_test(DummyValgrindInvalidSupFile "\${PSEUDO_VALGRIND}")

# CTest will add the logfile option before any custom options. Set the logfile
# again, this time to an empty string. This will cause the logfile to be
# missing, which will be the prove for us that the custom option is indeed used.
set(CTEST_EXTRA_CONFIG "set(CTEST_MEMORYCHECK_COMMAND_OPTIONS \"--log-file=\")")
gen_mc_test(DummyValgrindCustomOptions "\${PSEUDO_VALGRIND}")

unset(CTEST_EXTRA_CONFIG)
gen_mc_test(NotExist "\${CTEST_BINARY_DIRECTORY}/no-memcheck-exe")

gen_mc_test(Unknown "${CMAKE_COMMAND}")

string(REPLACE "\\" "\\\\" CMAKE_COMMAND_ESCAPED "${CMAKE_COMMAND}")
string(REPLACE "(" "\\(" CMAKE_COMMAND_ESCAPED "${CMAKE_COMMAND_ESCAPED}")
string(REPLACE ")" "\\)" CMAKE_COMMAND_ESCAPED "${CMAKE_COMMAND_ESCAPED}")
string(REPLACE "+" "\\+" CMAKE_COMMAND_ESCAPED "${CMAKE_COMMAND_ESCAPED}")

set_tests_properties(CTestTestMemcheckUnknown PROPERTIES
    PASS_REGULAR_EXPRESSION "Do not understand memory checker: ${CMAKE_COMMAND_ESCAPED}\n(.*\n)?Error in read script: ${CTEST_ESCAPED_CMAKE_CURRENT_BINARY_DIR}/Unknown/test.cmake\n")

set_tests_properties(CTestTestMemcheckNotExist PROPERTIES
    PASS_REGULAR_EXPRESSION "Memory checker \\(MemoryCheckCommand\\) not set, or cannot find the specified program.")

# It is a valid result if valgrind does not output any files (can e.g. happen
# if you have not compiled in debug mode), so these tests will not fail.
set_tests_properties(CTestTestMemcheckDummyValgrind
                     CTestTestMemcheckDummyValgrindPrePost
                     CTestTestMemcheckDummyPurify
    PROPERTIES
    PASS_REGULAR_EXPRESSION "${ctest_and_tool_outputs}$")

foreach (_pp Pre Post)
    string(TOLOWER ${_pp} _pp_lower)
    set_tests_properties(CTestTestMemcheckDummyValgrindFail${_pp}
        PROPERTIES
        PASS_REGULAR_EXPRESSION "\nProblem running command: ${CTEST_ESCAPED_CMAKE_CURRENT_BINARY_DIR}[^\n]*fail[^\n]*\n(.*\n)?Problem executing ${_pp_lower}-memcheck command\\(s\\\).\n(.*\n)?Error in read script: ${CTEST_ESCAPED_CMAKE_CURRENT_BINARY_DIR}/DummyValgrindFail${_pp}/test.cmake\n")
endforeach ()

set_tests_properties(CTestTestMemcheckDummyValgrindIgnoreMemcheck
    PROPERTIES
    PASS_REGULAR_EXPRESSION "\n2/2 Test #2: RunCMakeAgain .*${ctest_and_tool_outputs}$")

set_tests_properties(CTestTestMemcheckDummyBC PROPERTIES
    PASS_REGULAR_EXPRESSION "\n1/1 MemCheck #1: RunCMake \\.+   Passed +[0-9]+.[0-9]+ sec\n${guard_malloc_lines}\n100% tests passed, 0 tests failed out of 1\n(.*\n)?Error parsing XML in stream at line 1: no element found\n")

set_tests_properties(CTestTestMemcheckDummyValgrindInvalidSupFile PROPERTIES
    PASS_REGULAR_EXPRESSION "\nCannot find memory checker suppression file: ${CTEST_ESCAPED_REALPATH_CMAKE_CURRENT_BINARY_DIR}/does-not-exist\n")

set_tests_properties(CTestTestMemcheckDummyValgrindCustomOptions PROPERTIES
    PASS_REGULAR_EXPRESSION "\nCannot find memory tester output file: ${CTEST_ESCAPED_CMAKE_CURRENT_BINARY_DIR}/DummyValgrindCustomOptions/Testing/Temporary/MemoryChecker.1.log\n(.*\n)?Error in read script: ${CMAKE_CURRENT_BINARY_DIR}/DummyValgrindCustomOptions/test.cmake\n")

set_tests_properties(CTestTestMemcheckDummyValgrindTwoTargets PROPERTIES
    PASS_REGULAR_EXPRESSION
    "\nMemory check project ${CTEST_ESCAPED_CMAKE_CURRENT_BINARY_DIR}/DummyValgrindTwoTargets\n.*\n *Start 1: RunCMake\n(.*\n)?Memory check command: .* \"--log-file=${CTEST_ESCAPED_CMAKE_CURRENT_BINARY_DIR}/DummyValgrindTwoTargets/Testing/Temporary/MemoryChecker.1.log\" \"-q\".*\n *Start 2: RunCMakeAgain\n(.*\n)?Memory check command: .* \"--log-file=${CTEST_ESCAPED_CMAKE_CURRENT_BINARY_DIR}/DummyValgrindTwoTargets/Testing/Temporary/MemoryChecker.2.log\" \"-q\".*\n")


# Xcode 2.x forgets to create the output directory before linking
# the individual architectures.
if(CMAKE_OSX_ARCHITECTURES AND XCODE AND NOT "${XCODE_VERSION}" MATCHES "^[^12]")
  foreach(t
      memcheck_fail
      pseudo_BC
      pseudo_purify
      pseudo_valgrind
      )
    add_custom_command(TARGET ${t}
      PRE_BUILD COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CFG_INTDIR}"
      )
  endforeach()
endif()
