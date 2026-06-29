# SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

import dataclasses
from typing import TYPE_CHECKING, Optional

if TYPE_CHECKING:
    from nvtx._lib import EntryKind, PayloadEntryType

try:
    import numpy as np
except ImportError:
    np = None


NVTX_DTYPE_METADATA_KEY_NAME = "nvidia_nvtx_payload_metadata"


@dataclasses.dataclass(frozen=True)
class _PayloadMetadata:
    """
    Internal metadata stored on NumPy dtypes created by nvtx.numpy_dtype().
    """

    counter_semantics: Optional[object] = None
    entry_kind: Optional["EntryKind"] = None
    entry_type: Optional["PayloadEntryType"] = None


def _nvtx_metadata_from_dtype(dtype):
    metadata = getattr(dtype, "metadata", None)
    if metadata is None:
        return None
    return metadata.get(NVTX_DTYPE_METADATA_KEY_NAME)


def _dtype_metadata_key(dtype):
    """
    Return the NVTX metadata that participates in payload schema identity.
    """

    if np is None or not isinstance(dtype, np.dtype):
        return None

    metadata = _nvtx_metadata_from_dtype(dtype)
    subdtype_key = None
    subdtype = dtype.subdtype
    if subdtype:
        subdtype_key = _dtype_metadata_key(subdtype[0])

    field_keys = ()
    fields = dtype.fields
    if fields is not None and dtype.names is not None:
        # Iterate dtype.names, not dtype.fields to avoid alias duplicates.
        field_keys = tuple(
            (field_name, _dtype_metadata_key(fields[field_name][0]))
            for field_name in dtype.names
        )
    return metadata, subdtype_key, field_keys


@dataclasses.dataclass(frozen=True)
class PayloadSchemaKey:
    """
    Hashable identity for a NumPy dtype-derived NVTX payload schema.
    """

    dtype: object
    counter_group: bool = False
    schema_flags: int = 0
    metadata_key: object = dataclasses.field(init=False, repr=False)

    def __post_init__(self):
        object.__setattr__(self, "metadata_key", _dtype_metadata_key(self.dtype))

    @property
    def metadata(self):
        if self.metadata_key is None:
            return None
        return self.metadata_key[0]
