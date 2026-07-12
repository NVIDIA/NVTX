# SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://nvidia.github.io/NVTX/LICENSE.txt for license information.

import dataclasses
import struct
from typing import FrozenSet

from nvtx._lib import EntryKind, PayloadEntryType


@dataclasses.dataclass(frozen=True)
class _EntryStorageRequirement:
    allowed_kinds: FrozenSet[str]
    itemsize: int
    require_native_byte_order: bool
    description: str


_TIMESTAMP_STORAGE = _EntryStorageRequirement(
    allowed_kinds=frozenset(("i", "u")),
    itemsize=8,
    require_native_byte_order=False,
    description="64-bit integer storage",
)
_NATIVE_UINT64_STORAGE = _EntryStorageRequirement(
    allowed_kinds=frozenset(("u",)),
    itemsize=8,
    require_native_byte_order=True,
    description="native uint64 storage",
)
_NATIVE_UINT32_STORAGE = _EntryStorageRequirement(
    allowed_kinds=frozenset(("u",)),
    itemsize=4,
    require_native_byte_order=True,
    description="native uint32 storage",
)
_NATIVE_UINTPTR_STORAGE = _EntryStorageRequirement(
    allowed_kinds=frozenset(("u",)),
    itemsize=struct.calcsize("P"),
    require_native_byte_order=True,
    description="native pointer-sized unsigned integer storage",
)


_ENTRY_STORAGE_REQUIREMENTS = {
    EntryKind.RANGE_BEGIN: _TIMESTAMP_STORAGE,
    EntryKind.RANGE_END: _TIMESTAMP_STORAGE,
    EntryKind.MARK: _TIMESTAMP_STORAGE,
    EntryKind.COUNTER_TIMESTAMP: _TIMESTAMP_STORAGE,
    PayloadEntryType.RANGE_ID: _NATIVE_UINT64_STORAGE,
    PayloadEntryType.CATEGORY: _NATIVE_UINT32_STORAGE,
    PayloadEntryType.COLOR_ARGB: _NATIVE_UINT32_STORAGE,
    PayloadEntryType.SCOPE_ID: _NATIVE_UINT64_STORAGE,
    PayloadEntryType.REGISTERED_STRING: _NATIVE_UINTPTR_STORAGE,
}


def _validate_entry_storage(dtype, entry):
    requirement = _ENTRY_STORAGE_REQUIREMENTS.get(entry)
    if requirement is None:
        return

    if (
        dtype.kind not in requirement.allowed_kinds
        or dtype.itemsize != requirement.itemsize
        or (requirement.require_native_byte_order and not dtype.isnative)
    ):
        raise TypeError(
            f"{type(entry).__name__}.{entry.name} requires "
            f"{requirement.description}; got dtype {dtype}"
        )
