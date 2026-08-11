# SPDX-FileCopyrightText: Copyright (c) 2020-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# Configuration file for the Sphinx documentation builder.
#
# This file only contains a selection of the most common options. For a full
# list see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

# -- Path setup --------------------------------------------------------------

# If extensions (or modules to document with autodoc) are in another directory,
# add these directories to sys.path here. If the directory is relative to the
# documentation root, use os.path.abspath to make it absolute, like shown here.
#
# import os
# import sys
# sys.path.insert(0, os.path.abspath('.'))


# -- Project information -----------------------------------------------------

project = "nvtx"
copyright = "2020-2026, NVIDIA Corporation"
author = "NVIDIA Corporation"

# The full version, including alpha/beta/rc tags
release = "0.2.16"


# -- General configuration ---------------------------------------------------

# Add any Sphinx extension module names here, as strings. They can be
# extensions coming with Sphinx (named 'sphinx.ext.*') or your custom
# ones.
extensions = ["sphinx.ext.autodoc", "sphinx.ext.napoleon"]

# Add any paths that contain templates here, relative to this directory.
templates_path = ["_templates"]

# List of patterns, relative to source directory, that match files and
# directories to ignore when looking for source files.
# This pattern also affects html_static_path and html_extra_path.
exclude_patterns = ["_build", "Thumbs.db", ".DS_Store"]


# -- Options for HTML output -------------------------------------------------

# The theme to use for HTML and HTML Help pages.  See the documentation for
# a list of builtin themes.
#
html_theme = "nvidia_sphinx_theme"

# Add any paths that contain custom static files (such as style sheets) here,
# relative to this directory. They are copied after the builtin static files,
# so a file named "default.css" will overwrite the builtin "default.css".
# html_static_path = ["_static"]

autodoc_member_order = 'bysource'


import inspect

import nvtx
from sphinx import addnodes


_COUNTER_BASE = nvtx._lib.counters.Counter


def _is_counter_class(what, obj):
    return what == "class" and inspect.isclass(obj) and issubclass(obj, _COUNTER_BASE)


def _suppress_counter_signature(
    app, what, name, obj, options, signature, return_annotation
):
    if _is_counter_class(what, obj):
        return "", None


def _strip_counter_docstring_signature(app, what, name, obj, options, lines):
    if not _is_counter_class(what, obj):
        return
    if lines and lines[0].startswith(f"{obj.__name__}("):
        del lines[0]
        if lines and not lines[0]:
            del lines[0]


def _qualify_annotation_registered_string_refs(app, doctree):
    """Disambiguate the two RegisteredString classes in the public APIs."""
    for node in doctree.findall(addnodes.pending_xref):
        if (
            node.get("refdomain") == "py"
            and node.get("reftype") == "class"
            and node.get("reftarget") == "RegisteredString"
            and node.get("py:module") == "nvtx"
        ):
            node["reftarget"] = "nvtx._lib.lib.RegisteredString"


def setup(app):
    app.connect("autodoc-process-signature", _suppress_counter_signature)
    app.connect("autodoc-process-docstring", _strip_counter_docstring_signature)
    app.connect("doctree-read", _qualify_annotation_registered_string_refs)
