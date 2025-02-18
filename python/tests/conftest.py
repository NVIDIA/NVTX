from Cython.Build import cythonize
from Cython.Build.Cythonize import run_distutils
from pathlib import Path
from setuptools.extension import Extension


def build_cython():
    """Compile Cython files inside tests/cython"""
    tests_dir = Path(__file__).parent
    cython_dir = tests_dir / 'cython'
    include_dir = tests_dir.parent.parent / 'c' / 'include'
    run_distutils((
        str(cython_dir),
        cythonize(
            Extension('*', sources=[str(cython_dir / '*.pyx')], include_dirs=[str(include_dir)]),
            compiler_directives=dict(language_level=3, embedsignature=True, profile=True))
    ))


def pytest_configure(config):
    build_cython()
