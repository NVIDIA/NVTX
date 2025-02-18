# SPDX-FileCopyrightText: Copyright (c) 2020-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://nvidia.github.io/NVTX/LICENSE.txt for license information.

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
@pytest.mark.parametrize(
    "payload",
    [
        None,
        1,
        1.0
    ]
)
def test_annotate_context_manager(message, color, domain, payload):
    with nvtx.annotate(message=message, color=color, domain=domain, payload=payload):
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
@pytest.mark.parametrize(
    "payload",
    [
        None,
        1,
        1.0
    ]
)
def test_annotate_decorator(message, color, domain, payload):
    @nvtx.annotate(message=message, color=color, domain=domain, payload=payload)
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
    a = nvtx.get_domain("x")
    b = nvtx.get_domain("x")
    assert a is b

    c = nvtx.get_domain("y")
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
@pytest.mark.parametrize(
    "payload",
    [
        None,
        1,
        1.0
    ]
)
def test_start_end(message, color, domain, category, payload):
    rng = nvtx.start_range(message, color, domain, category, payload)
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
@pytest.mark.parametrize(
    "payload",
    [
        None,
        1,
        1.0
    ]
)
def test_push_pop(message, color, domain, category, payload):
    nvtx.push_range(message, color, domain, category, payload)
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
@pytest.mark.parametrize(
    "payload",
    [
        None,
        1,
        1.0
    ]
)
def test_mark(message, color, domain, category, payload):
    nvtx.mark(message, color, domain, category, payload)


def test_annotation_gets_name_from_func():
    # GH #86: test that annotate() with no arguments
    # uses the name of the function as the message
    ann = nvtx.annotate()
    def foo():
        pass
    ann(foo)
    assert ann.attributes.message.string == "foo"
