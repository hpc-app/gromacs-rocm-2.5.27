#!/bin/sh

work_dir=/your/work/dir
${work_dir}/cmake-3.13.5/build/bin/cmake -DGMX_HWLOC=off -DBUILD_SHARED_LIBS=ON -DGMX_BUILD_FOR_COVERAGE=on -DCMAKE_BUILD_TYPE=Release -DGMX_CUDA_NB_SINGLE_COMPILATION_UNIT=on -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DGMX_MPI=on -DGMX_GPU=on -DGMX_OPENMP=on -DGMX_GPU_DETECTION_DONE=on -DHIP_TOOLKIT_ROOT_DIR=/opt/rocm/hip -DGMX_SIMD=AUTO -DCMAKE_INSTALL_PREFIX=${work_dir}/gromacs-2018.4-install -DGMX_FFT_LIBRARY=fftw3 -DFFTWF_LIBRARY=${work_dir}/fftw-3.3.8/build/lib/libfftw3f.so -DFFTWF_INCLUDE_DIR=${work_dir}/fftw-3.3.8/build/include  -DBLAS_LIBRARIES=${work_dir}/lapack-release-lapack-3.8.0/librefblas.so -DLAPACK_LIBRARIES=${work_dir}/lapack-release-lapack-3.8.0/liblapack.so -DREGRESSIONTEST_DOWNLOAD=OFF -DCMAKE_PREFIX_PATH=/opt/rocm ..
