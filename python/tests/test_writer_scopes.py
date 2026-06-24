# SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

import nvtx
from nvtx.writer import PredefinedScope


def test_predefined_scopes():
    expected = {
        "NONE": 0,
        "ROOT": 1,
        "CURRENT_HW_MACHINE": 2,
        "CURRENT_HW_SOCKET": 3,
        "CURRENT_HW_CPU_PHYSICAL": 4,
        "CURRENT_HW_CPU_LOGICAL": 5,
        "CURRENT_HW_INNERMOST": 15,
        "CURRENT_HYPERVISOR": 16,
        "CURRENT_VM": 17,
        "CURRENT_KERNEL": 18,
        "CURRENT_CONTAINER": 19,
        "CURRENT_OS": 20,
        "CURRENT_SW_PROCESS": 21,
        "CURRENT_SW_THREAD": 22,
        "CURRENT_SW_INNERMOST": 31,
    }

    for name, scope_id in expected.items():
        scope = getattr(PredefinedScope, name)
        assert isinstance(scope, PredefinedScope)
        assert scope.value == scope_id
        assert scope is getattr(PredefinedScope, name)

    assert set(PredefinedScope.__members__) == set(expected)
    assert nvtx.PredefinedScope is PredefinedScope


def test_predefined_scope_is_not_an_int():
    assert not isinstance(PredefinedScope.ROOT, int)
