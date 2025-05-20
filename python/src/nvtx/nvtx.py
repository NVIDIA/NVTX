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

import contextlib
import os

from functools import wraps, lru_cache
from typing import Optional, Union, Tuple

from nvtx._lib import (
    Domain,
    DummyDomain,
    dummy_domain,
    mark as libnvtx_mark,
    pop_range as libnvtx_pop_range,
    push_range as libnvtx_push_range,
    start_range as libnvtx_start_range,
    end_range as libnvtx_end_range,
)

_ENABLED = not os.getenv("NVTX_DISABLE", False)


@lru_cache(maxsize=None)
def get_domain(name: Optional[str] = None) -> Union[Domain, DummyDomain]:
    """
    Get or create a :class:`Domain` object for a domain name.
    """
    return Domain(name)


class annotate:
    """
    Annotate code ranges using a context manager or a decorator.

    Parameters
    ----------
    message
        A message associated with the annotated code range.
        When used as a decorator, defaults to the decorated function name.
        When used as a context manager, defaults to the empty string.
        Messages are cached and are registered as Registered Strings
        in NVTX.
        Caching a very large number of messages may lead to increased
        memory usage.
    color
        A color associated with the annotated code range.
        Supports `matplotlib` colors if it is available.
    domain
        A string specifying the domain under which the code range is
        scoped. The default domain is named "NVTX".
    category
        A string or an integer specifying the category within the domain
        under which the code range is scoped. If unspecified, the code
        range is not associated with a category.
    payload
        A numeric value to be associated with this event.

    Examples
    --------
    >>> import nvtx
    >>> import time

    Using a decorator:

    >>> @nvtx.annotate(color="red", domain="my_domain"):    # `message` defaults to "func"
    ... def func():
    ...     time.sleep(0.1)

    Using a context manager:

    >>> with nvtx.annotate("my_code_range", color="blue"):
    ...    time.sleep(10)
    """

    def __init__(self, message: Optional[str] = None, color: Optional[Union[str, int]] = None,
                 domain: Optional[str] = None, category: Optional[Union[str, int]] = None,
                 payload: Optional[Union[int, float]] = None):

        self.init_args = message, color, domain, category, payload
        self.domain = get_domain(domain)
        if self.domain is not dummy_domain:
            self.attributes = self.domain.get_event_attributes(message, color, category, payload)

    def __reduce__(self):
        return self.__class__, self.init_args

    def __enter__(self):
        if self.domain is not dummy_domain:
            libnvtx_push_range(self.attributes, self.domain.handle)
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        if self.domain is not dummy_domain:
            libnvtx_pop_range(self.domain.handle)
        return False

    def __call__(self, func):
        if self.domain is dummy_domain:
            return func
        if not self.attributes.message.string:
            self.attributes.message = self.domain.get_registered_string(func.__name__)

        @wraps(func)
        def inner(*args, **kwargs):
            libnvtx_push_range(self.attributes, self.domain.handle)
            result = func(*args, **kwargs)
            libnvtx_pop_range(self.domain.handle)
            return result

        return inner


def mark(message: Optional[str] = None, color: Optional[Union[str, int]] = "blue",
         domain: Optional[str] = None, category: Optional[Union[str, int]] = None,
         payload: Optional[Union[int, float]] = None):
    """
    Mark an instantaneous event.

    Parameters
    ----------
    message
        A message associated with the event.
        Messages are cached and are registered as Registered Strings
        in NVTX.
        Caching a very large number of messages may lead to increased
        memory usage.
    color
        Color associated with the event.
        Supports `matplotlib` colors if it is available.
    domain
        A string specifying the domain under which the event is scoped.
        The default domain is named "NVTX".
    category
        A string or an integer specifying the category within the domain
        under which the event is scoped. If unspecified, the event is
        not associated with a category.
    payload
        A numeric value to be associated with this event

    Notes
    -----
    For best performance, use :func:`Domain.mark`.

    Examples
    --------
    >>> import nvtx
    >>> nvtx.mark("my_marker", domain="my_domain")
    """
    domain = get_domain(domain)
    if domain is not dummy_domain:
        libnvtx_mark(domain.get_event_attributes(message, color, category, payload), domain.handle)


def push_range(message: Optional[str] = None, color: Optional[Union[str, int]] = "blue",
               domain: Optional[str] = None, category: Optional[Union[str, int]] = None,
               payload: Optional[Union[int, float]] = None):
    """
    Mark the beginning of a code range.

    Parameters
    ----------
    message
        A message associated with the annotated code range.
        Messages are cached and are registered as Registered Strings
        in NVTX.
        Caching a very large number of messages may lead to increased
        memory usage.
    color
        A color associated with the annotated code range.
        Supports `matplotlib` colors if it is available.
    domain
        Name of a domain under which the code range is scoped.
        The default domain name is "NVTX".
    category
        A string or an integer specifying the category within the domain
        under which the code range is scoped. If unspecified, the code range
        is not associated with a category.
    payload
        A numeric value to be associated with this event

    Notes
    -----
    When applicable, prefer to use :class:`annotate`.
    Otherwise, for best performance, use :func:`Domain.push_range` and :func:`Domain.pop_range`.

    Examples
    --------
    >>> import time
    >>> import nvtx
    >>> nvtx.push_range("my_code_range", domain="my_domain")
    >>> time.sleep(1)
    >>> nvtx.pop_range(domain="my_domain")
    """
    domain = get_domain(domain)
    if domain is not dummy_domain:
        libnvtx_push_range(domain.get_event_attributes(message, color, category, payload),
                           domain.handle)


def pop_range(domain: Optional[str] = None):
    """
    Mark the end of a code range that was started with :func:`push_range`.

    Parameters
    ----------
    domain
        The domain under which the code range is scoped. The default
        domain is "NVTX".
    """
    domain = get_domain(domain)
    if domain is not dummy_domain:
        libnvtx_pop_range(domain.handle)


def start_range(message: Optional[str] = None, color: Optional[Union[str, int]] = None,
                domain: Optional[str] = None, category: Optional[Union[str, int]] = None,
                payload: Optional[Union[int, float]] = None) -> Tuple[int, int]:
    """
    Mark the beginning of a process range.

    Parameters
    ----------
    message
        A message associated with the range.
        Messages are cached and are registered as Registered Strings
        in NVTX.
        Caching a very large number of messages may lead to increased
        memory usage.
    color
        A color associated with the range.
        Supports `matplotlib` colors if it is available.
    domain
        Name of a domain under which the range is scoped.
        The default domain name is "NVTX".
    category
        A string or an integer specifying the category within the domain
        under which the range is scoped. If unspecified, the range
        is not associated with a category.
    payload
            A numeric value to be associated with this event

    Returns
    -------
    A tuple of the form ``(range_id, domain_handle)`` that must be passed to :func:`end_range`.

    Notes
    -----
    For best performance, use :func:`Domain.start_range` and :func:`Domain.end_range`.

    Examples
    --------
    >>> import time
    >>> import nvtx
    >>> range_id = nvtx.start_range("my_code_range", domain="my_domain")
    >>> time.sleep(1)
    >>> nvtx.end_range(range_id)
    """
    domain = get_domain(domain)
    if domain is not dummy_domain:
        return libnvtx_start_range(
            domain.get_event_attributes(message, color, category, payload), domain.handle)


def end_range(range_id: Tuple[int, int]):
    """
    Mark the end of a code range that was started with :func:`start_range`.

    Parameters
    ----------
    range_id
        The tuple object returned by :func:`start_range`.
    """
    if range_id is not None:
        libnvtx_end_range(*range_id)


def enabled():
    """
    Returns True if nvtx is enabled.
    """
    return _ENABLED


if not enabled():

    class annotate(contextlib.nullcontext):
        def __init__(self, *args, **kwargs):
            super().__init__()

        def __call__(self, func):
            return func

    # Could use a decorator here but overheads are significant enough
    # not to. See https://github.com/NVIDIA/NVTX/pull/24 for discussion.
    def mark(message=None, color=None, domain=None, category=None, payload=None):
        pass

    def push_range(message=None, color=None, domain=None, category=None, payload=None):
        pass

    def pop_range(domain=None):
        pass

    def start_range(message=None, color=None, domain=None, category=None, payload=None):
        pass

    def end_range(range_id):
        pass
