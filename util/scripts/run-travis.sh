#!/usr/bin/env bash
#
# The following environment variables are required:
# - SHARED
# - BUILD_TENSORFLOW_OPS
# - BUILD_DEPENDENCY_FROM_SOURCE
# - NPROC

set -euo pipefail

echo "installing Python unit test dependencies..."
pip install --upgrade pip
pip install -U -q pytest
pip install -U -q wheel
echo

python --version
pytest --version
cmake --version

echo "cmake configure the Open3D project..."
date
if [ "$BUILD_TENSORFLOW_OPS" == "ON" ]; then
    pip install -U -q tensorflow==2.0.0
fi
mkdir build
cd build

runBenchmarks=true
OPEN3D_INSTALL_DIR=~/open3d_install
cmakeOptions="-DBUILD_SHARED_LIBS=$SHARED \
        -DBUILD_TENSORFLOW_OPS=$BUILD_TENSORFLOW_OPS \
        -DBUILD_UNIT_TESTS=ON \
        -DBUILD_BENCHMARKS=ON \
        -DCMAKE_INSTALL_PREFIX=${OPEN3D_INSTALL_DIR} \
        -DPYTHON_EXECUTABLE=$(which python)"

if [ "$BUILD_DEPENDENCY_FROM_SOURCE" == "ON" ]; then
    cmakeOptions="$cmakeOptions \
        -DBUILD_EIGEN3=ON \
        -DBUILD_FLANN=ON \
        -DBUILD_GLEW=ON \
        -DBUILD_GLFW=ON \
        -DBUILD_PNG=ON"
fi

echo
echo "Running cmake" $cmakeOptions ..
cmake $cmakeOptions ..
echo

echo "build & install Open3D..."
date
make install -j$NPROC
make install-pip-package -j$NPROC
echo

echo "running Open3D unit tests..."
date
./bin/unitTests
echo

echo "running Open3D python tests..."
date
# TODO: fix TF op library test.
pytest ../src/UnitTest/Python/ --ignore="../src/UnitTest/Python/test_tf_op_library.py"
echo

if $runBenchmarks; then
    echo "running Open3D benchmarks..."
    date
    ./bin/benchmarks
    echo
fi

echo "test find_package(Open3D)..."
date
test=$(cmake --find-package \
    -DNAME=Open3D \
    -DCOMPILER_ID=GNU \
    -DLANGUAGE=C \
    -DMODE=EXIST \
    -DCMAKE_PREFIX_PATH="${OPEN3D_INSTALL_DIR}/lib/cmake")
if [ "$test" == "Open3D found." ]; then
    echo "PASSED find_package(Open3D) in specified install path."
else
    echo "FAILED find_package(Open3D) in specified install path."
    exit 1
fi
echo

echo "test building a C++ example with installed Open3D..."
date
cd ../docs/_static/C++
mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX=${OPEN3D_INSTALL_DIR} ..
make
./TestVisualizer
echo

echo "cleanup the C++ example..."
date
cd ../
rm -rf build

echo "uninstall Open3D..."
date
cd ../../../build
make uninstall

echo "cleanup Open3D..."
date
cd ../
rm -rf build
rm -rf ${OPEN3D_INSTALL_DIR}
echo
