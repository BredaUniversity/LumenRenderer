# Copyright Contributors to the OpenVDB Project SPDX-License-Identifier: MPL-2.0

# -----------------------------------------------------------------------
if(WIN32 AND NANOVDB_CUDA_KEEP_PTX)
  file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/x64/Release")
endif()

# -----------------------------------------------------------------------
# TODO: Benchmark should probably not require gtest.
if(NOT (NANOVDB_GTEST))
  message(WARNING " - GTest required to build benchmark. Skipping.")
  return()
endif()

# -----------------------------------------------------------------------
# many of the sample projects depend on a data directory. This allows Debug
# launching from the cmake binary working directory.
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/data")

add_executable(benchmark Benchmark.cpp BenchKernels.cu Image.h)

target_include_directories(benchmark PUBLIC ${TBB_INCLUDE_DIRECTORY}
                                            ${CUDA_INCLUDE_DIRECTORY})

target_link_libraries(
  benchmark
  PRIVATE libnanovdb
          ${NANOVDB_GTEST}
          ${NANOVDB_TBB} # optional
          ${NANOVDB_OPENVDB} # optional
          ${NANOVDB_ZLIB} # optional
          ${NANOVDB_BLOSC} # optional
)

target_compile_definitions(
  benchmark
  PRIVATE "-D${NANOVDB_USE_OPENVDB_FLAG}" "-D${NANOVDB_USE_CUDA_FLAG}"
          "-D${NANOVDB_USE_TBB_FLAG}" "-D${NANOVDB_USE_BLOSC_FLAG}"
          "-D${NANOVDB_USE_ZLIB_FLAG}")

install(TARGETS benchmark DESTINATION examples)

# ---------------------------------------------------------------
