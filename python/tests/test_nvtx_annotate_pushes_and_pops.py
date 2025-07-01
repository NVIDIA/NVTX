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

import unittest.mock

import pytest

import nvtx


def test_annotate_ctx_manager_pops():
    """
    Test that the NVTX range is properly popped when the context manager is exited normally.
    """
    # Create a mock domain that behaves like a real domain
    mock_domain = unittest.mock.MagicMock()
    mock_domain.handle = unittest.mock.MagicMock()
    mock_domain.get_event_attributes.return_value = unittest.mock.MagicMock()
    mock_domain.get_registered_string.return_value = unittest.mock.MagicMock()

    # Mock get_domain to return our mock domain instead of dummy_domain
    with unittest.mock.patch(
        "nvtx.nvtx.get_domain", return_value=mock_domain
    ), unittest.mock.patch(
        "nvtx.nvtx.libnvtx_push_range"
    ) as mock_push, unittest.mock.patch(
        "nvtx.nvtx.libnvtx_pop_range"
    ) as mock_pop:

        with nvtx.annotate():
            pass

        mock_push.assert_called_once()
        mock_pop.assert_called_once()


def test_annotate_ctx_manager_pops_with_exception():
    """
    Test that the NVTX range is properly popped when the context manager throws an exception.
    """
    # Create a mock domain that behaves like a real domain
    mock_domain = unittest.mock.MagicMock()
    mock_domain.handle = unittest.mock.MagicMock()
    mock_domain.get_event_attributes.return_value = unittest.mock.MagicMock()
    mock_domain.get_registered_string.return_value = unittest.mock.MagicMock()

    # Mock get_domain to return our mock domain instead of dummy_domain
    with unittest.mock.patch(
        "nvtx.nvtx.get_domain", return_value=mock_domain
    ), unittest.mock.patch(
        "nvtx.nvtx.libnvtx_push_range"
    ) as mock_push, unittest.mock.patch(
        "nvtx.nvtx.libnvtx_pop_range"
    ) as mock_pop:

        with pytest.raises(Exception):
            with nvtx.annotate():
                raise Exception("test")

        mock_push.assert_called_once()

        # Make sure that pop_range was called even though an exception was raised
        # inside the context manager for nvtx.annotate
        mock_pop.assert_called_once()


def test_annotate_decorator_pushes_and_pops():
    """
    Test that the NVTX range is properly popped when a decorated function exits normally.
    """
    # Create a mock domain that behaves like a real domain
    mock_domain = unittest.mock.MagicMock()
    mock_domain.handle = unittest.mock.MagicMock()
    mock_domain.get_event_attributes.return_value = unittest.mock.MagicMock()
    mock_domain.get_registered_string.return_value = unittest.mock.MagicMock()

    # Mock get_domain to return our mock domain instead of dummy_domain
    with unittest.mock.patch(
        "nvtx.nvtx.get_domain", return_value=mock_domain
    ), unittest.mock.patch(
        "nvtx.nvtx.libnvtx_push_range"
    ) as mock_push, unittest.mock.patch(
        "nvtx.nvtx.libnvtx_pop_range"
    ) as mock_pop:

        @nvtx.annotate(message="foo", color="blue", domain="test")
        def foo():
            pass

        foo()

        mock_push.assert_called_once()
        mock_pop.assert_called_once()


def test_annotate_decorator_pushes_and_pops_with_exception():
    """
    Test that the NVTX range is properly popped when a decorated function raises an exception.
    """
    # Create a mock domain that behaves like a real domain
    mock_domain = unittest.mock.MagicMock()
    mock_domain.handle = unittest.mock.MagicMock()
    mock_domain.get_event_attributes.return_value = unittest.mock.MagicMock()
    mock_domain.get_registered_string.return_value = unittest.mock.MagicMock()

    # Mock get_domain to return our mock domain instead of dummy_domain
    with unittest.mock.patch(
        "nvtx.nvtx.get_domain", return_value=mock_domain
    ), unittest.mock.patch(
        "nvtx.nvtx.libnvtx_push_range"
    ) as mock_push, unittest.mock.patch(
        "nvtx.nvtx.libnvtx_pop_range"
    ) as mock_pop:

        @nvtx.annotate(message="foo", color="blue", domain="test")
        def foo():
            raise Exception("test")

        with pytest.raises(Exception):
            foo()

        mock_push.assert_called_once()

        # Make sure that pop_range was called even though an exception was raised
        # inside the function decorated with nvtx.annotate
        mock_pop.assert_called_once()
