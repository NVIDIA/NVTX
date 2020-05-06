import pickle

import nvtx


def test_pickle_annotate():
    orig = nvtx.annotate(message="foo", color="blue", domain="test")
    pickled = pickle.dumps(orig)
    unpickled = pickle.loads(pickled)

    assert orig.attributes.message == unpickled.attributes.message
    assert orig.attributes.color == unpickled.attributes.color
    assert orig.domain == unpickled.domain
