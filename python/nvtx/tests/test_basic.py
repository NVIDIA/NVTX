# Copyright 2020-2022 NVIDIA Corporation.  All rights reserved.
#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

import pickle

import pytest

import nvtx


@pytest.mark.parametrize(
    "message",
    [
        None,
        "",
        "x",
        "abc",
        "abc def"
    ]
)
@pytest.mark.parametrize(
    "color",
    [
        None,
        "red",
        "green",
        "blue"
    ]
)
@pytest.mark.parametrize(
    "domain",
    [
        None,
        "",
        "x",
        "abc",
        "abc def"
    ]
)
def test_annotate_context_manager(message, color, domain):
    with nvtx.annotate(message=message, color=color, domain=domain):
        pass


@pytest.mark.parametrize(
    "message",
    [
        None,
        "",
        "x",
        "abc",
        "abc def"
    ]
)
@pytest.mark.parametrize(
    "color",
    [
        None,
        "red",
        "green",
        "blue"
    ]
)
@pytest.mark.parametrize(
    "domain",
    [
        None,
        "",
        "x",
        "abc",
        "abc def"
    ]
)
def test_annotate_decorator(message, color, domain):
    @nvtx.annotate(message=message, color=color, domain=domain)
    def foo():
        pass

    foo()

    
def test_pickle_annotate():
    orig = nvtx.annotate(message="foo", color="blue", domain="test")
    pickled = pickle.dumps(orig)
    unpickled = pickle.loads(pickled)

    assert orig.attributes.message == unpickled.attributes.message
    assert orig.attributes.color == unpickled.attributes.color
    assert orig.domain == unpickled.domain


def test_domain_reuse():
    a = nvtx._lib.Domain("x")
    b = nvtx._lib.Domain("x")
    assert a is b

    c = nvtx._lib.Domain("y")
    assert a is not c


@pytest.mark.parametrize(
    "message",
    [
        None,
        "",
        "x",
        "abc",
        "abc def"
    ]
)
@pytest.mark.parametrize(
    "color",
    [
        None,
        "red",
        "green",
        "blue"
    ]
)
@pytest.mark.parametrize(
    "domain",
    [
        None,
        "",
        "x",
        "abc",
        "abc def"
    ]
)
@pytest.mark.parametrize(
    "category",
    [
        None,
        "",
        "y"
        "x",
        "abc",
        "abc def",
        0,
        1,
    ]
)
def test_categories_basic(message, color, domain, category):
    with nvtx.annotate(message=message, domain=domain, category=category):
        pass


def test_get_category_id():
    dom = nvtx._lib.Domain("foo")
    id1 = dom.get_category_id("bar")
    id2 = dom.get_category_id("bar")
    assert id1 == id2
    id3 = dom.get_category_id("baz")
    assert id2 != id3


@pytest.mark.parametrize(
    "message",
    [
        None,
        "abc",
    ]
)
@pytest.mark.parametrize(
    "color",
    [
        None,
        "red",
    ]
)
@pytest.mark.parametrize(
    "domain",
    [
        None,
        "abc",
    ]
)
@pytest.mark.parametrize(
    "category",
    [
        None,
        "abc",
        1,
    ]
)
def test_start_end(message, color, domain, category):
    rng = nvtx.start_range(message, color, domain, category)
    nvtx.end_range(rng)




@pytest.mark.parametrize(
    "message",
    [
        None,
        "abc",
    ]
)
@pytest.mark.parametrize(
    "color",
    [
        None,
        "red",
    ]
)
@pytest.mark.parametrize(
    "domain",
    [
        None,
        "abc",
    ]
)
@pytest.mark.parametrize(
    "category",
    [
        None,
        "abc",
        1,
    ]
)
def test_push_pop(message, color, domain, category):
    nvtx.push_range(message, color, domain, category)
    nvtx.pop_range()
    


@pytest.mark.parametrize(
    "message",
    [
        None,
        "abc",
    ]
)
@pytest.mark.parametrize(
    "color",
    [
        None,
        "red",
    ]
)
@pytest.mark.parametrize(
    "domain",
    [
        None,
        "abc",
    ]
)
@pytest.mark.parametrize(
    "category",
    [
        None,
        "abc",
        1,
    ]
)
def test_mark(message, color, domain, category):
    nvtx.mark(message, color, domain, category)
