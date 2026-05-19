# SPDX-FileCopyrightText: Copyright (c) 2020-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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
from nvtx.nvtx import dummy_domain
from .conftest import (verify_registration_events, verify_push, verify_pop, verify_mark,
                       verify_start, verify_end)


def test_annotate_context_manager(nvtx_events, message, color, domain, payload, category):
    ann = nvtx.annotate(
        message=message, color=color, domain=domain, payload=payload, category=category)
    with ann:
        pass

    if nvtx_events:
        verify_registration_events(nvtx_events, domain, message, category)
        verify_push(nvtx_events, domain, message, color=color, payload=payload, category=category)
        verify_pop(nvtx_events, domain)
    else:
        assert ann.domain is dummy_domain


def test_annotate_decorator(nvtx_events, message, color, domain, payload, category):
    def foo():
        pass
    orig_foo = foo
    foo = nvtx.annotate(message=message, color=color, domain=domain, payload=payload,
                        category=category)(foo)

    foo()

    if nvtx_events:
        if message is None:
            message = "foo"
        verify_registration_events(nvtx_events, domain, message, category)
        verify_push(nvtx_events, domain, message, color=color, payload=payload, category=category)
        verify_pop(nvtx_events, domain)
    else:
        assert orig_foo is foo


def test_annotate_context_manager_with_exception(nvtx_events, message, color, domain, category,
                                                 payload):
    ann = nvtx.annotate(message=message, color=color, domain=domain, category=category,
                        payload=payload)
    with pytest.raises(Exception):
        with ann:
            raise Exception()

    if nvtx_events:
        verify_registration_events(nvtx_events, domain, message, category)
        verify_push(nvtx_events, domain, message, color=color, category=category, payload=payload)
        verify_pop(nvtx_events, domain)
    else:
        assert ann.domain is dummy_domain


def test_annotate_decorator_with_exception(nvtx_events, message, color, domain, category, payload):
    @nvtx.annotate(message=message, color=color, domain=domain, category=category, payload=payload)
    def foo():
        raise Exception()

    with pytest.raises(Exception):
        foo()

    if nvtx_events:
        if message is None:
            message = "foo"
        verify_registration_events(nvtx_events, domain, message, category)
        verify_push(nvtx_events, domain, message, color=color, category=category, payload=payload)
        verify_pop(nvtx_events, domain)


def foo():
    """
    Dummy function to be annotated by the below test.
    It must be global to be picklable.
    """
    pass


def test_pickle_annotate(nvtx_events, message, color, domain, category, payload):
    global foo
    orig_foo = foo
    foo = nvtx.annotate(
        message=message, color=color, domain=domain, category=category, payload=payload)(foo)
    try:
        unpickled = pickle.loads(pickle.dumps(foo))
        unpickled()
        if nvtx_events:
            if message is None:
                message = "foo"
            verify_registration_events(nvtx_events, domain, message, category)
            verify_push(nvtx_events, domain, message, color=color, category=category,
                        payload=payload)
            verify_pop(nvtx_events, domain)
        else:
            assert orig_foo is foo
    finally:
        foo = orig_foo


def test_start_end(nvtx_events, message, color, domain, category, payload):
    range_ids = []
    range_id = nvtx.start_range(message, color, domain, category, payload)
    nvtx.end_range(range_id)
    range_ids.append(range_id)

    domain_obj = nvtx.get_domain(domain)
    range_id = domain_obj.start_range(
        domain_obj.get_event_attributes(
            message=message, color=color, category=category, payload=payload))
    domain_obj.end_range(range_id)
    range_ids.append(range_id)

    range_id = domain_obj.start_range(
        message=message, color=color, category=category, payload=payload)
    domain_obj.end_range(range_id)
    range_ids.append(range_id)

    range_id = domain_obj.start_range(domain_obj.get_event_attributes(),
        message=message, color=color, category=category, payload=payload)
    domain_obj.end_range(range_id)
    range_ids.append(range_id)

    if nvtx_events:
        verify_registration_events(nvtx_events, domain, message, category)
        for range_id in range_ids:
            verify_start(nvtx_events, domain, message, color=color, category=category,
                        payload=payload)
            verify_end(nvtx_events, domain, range_id)
    else:
        assert domain_obj is dummy_domain


def test_push_pop(nvtx_events, message, color, domain, category, payload):
    nvtx.push_range(message, color, domain, category, payload)
    nvtx.pop_range(domain)

    domain_obj = nvtx.get_domain(domain)
    domain_obj.push_range(domain_obj.get_event_attributes(message, color, category, payload))
    domain_obj.pop_range()
    domain_obj.push_range(message=message, color=color, category=category, payload=payload)
    domain_obj.pop_range()
    domain_obj.push_range(domain_obj.get_event_attributes(),
                          message=message, color=color, category=category, payload=payload)
    domain_obj.pop_range()
    push_pop_count = 4
    if nvtx_events:
        verify_registration_events(nvtx_events, domain, message, category)
        for _ in range(push_pop_count):
            verify_push(nvtx_events, domain, message, color=color, category=category,
                        payload=payload)
            verify_pop(nvtx_events, domain)
    else:
        assert domain_obj is dummy_domain


def test_mark(nvtx_events, message, color, domain, category, payload):
    nvtx.mark(message, color, domain, category, payload)

    domain_obj = nvtx.get_domain(domain)
    attributes = domain_obj.get_event_attributes(message, color, category, payload)
    domain_obj.mark(attributes)
    if nvtx_events:
        verify_registration_events(nvtx_events, domain, message, category)
        verify_mark(nvtx_events, domain, message, color=color, category=category,
                    payload=payload)
        verify_mark(nvtx_events, domain, message, color=color, category=category,
                    payload=payload)
    else:
        assert domain_obj is dummy_domain


def test_set_event_attributes(nvtx_events, message, color, domain, category, payload):
    domain_obj = nvtx.get_domain(domain)
    attributes = domain_obj.get_event_attributes()
    domain_obj.set_event_attributes(
        attributes, message=message, color=color, category=category, payload=payload)
    domain_obj.mark(attributes)
    if nvtx_events:
        verify_registration_events(nvtx_events, domain, message, category)
        verify_mark(nvtx_events, domain, message, color=color, category=category,
                    payload=payload)
    else:
        assert domain_obj is dummy_domain
