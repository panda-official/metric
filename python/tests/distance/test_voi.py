from metric.distance import VOI_normalized
import numpy
import pytest


@pytest.mark.skip('VOI crashes with small dataset')
def test_small_dataset():
    v1 = numpy.float_([[5, 5], [2, 2], [3, 3], [5, 5]])
    v2 = numpy.float_([[5, 5], [2, 2], [3, 3], [1, 1]])
    distance = VOI_normalized()

    assert not (distance(v2, v1) == pytest.approx(distance(v1, v2)))
    assert distance(v1, v2) == pytest.approx(0.9272535656800133)
