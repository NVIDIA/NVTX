#!/bin/bash
set -e -u -x

function repair_wheel {
    wheel="$1"
    if ! auditwheel show "$wheel"; then
        echo "Skipping non-platform wheel $wheel"
    else
        auditwheel repair "$wheel" --plat "manylinux2014_x86_64" -w /io/python/wheels/
    fi
}


export C_INCLUDE_PATH=/io/c/include

# Compile wheels
for PY_VERSION in 37 38 39 310; do
    PYBIN=/opt/python/cp${PY_VERSION}*/bin/
    ${PYBIN}/pip wheel /io/python/ --no-deps -w wheels/
done

# Bundle external shared libraries into the wheels
for whl in wheels/*.whl; do
    repair_wheel "$whl"
done

# Install packages and test
for PY_VERSION in 37 38 39 310; do
    PYBIN=/opt/python/cp${PY_VERSION}*/bin
    ${PYBIN}/pip install nvtx --no-index -f /io/python/wheels/
    ${PYBIN}/pip install -r /io/python/test_requirements.txt
    (cd "$HOME"; ${PYBIN}/pytest /io/python/nvtx)
done
