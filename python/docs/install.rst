Installation
============

``nvtx`` requires Python >=3.6,<3.10, and is tested on Linux only.

Install using `conda` (preferred):
::

   conda install -c conda-forge nvtx

Install using `pip`:
::

   python -m pip install nvtx

Or `conda`:
::

   conda install -c conda-forge nvtx

On Windows, or another system for which pre-built packages are not
published, you can try to install from source. This requires: (1) that
Cython is installed, and (2) that the ``NVTX_PREFIX`` environment
variable is set to the directory containing ``nvtx3``, the C header
library. On systems with CUDA installed, this is typically the CUDA
include directory (e.g., ``/usr/local/cuda/include``).
::

   NVTX_DIR=/path/to/nvtx/prefix/ python -m pip install nvtx --no-binary nvtx
