# ----------------------------------------------------------------------------
# -                        Open3D: www.open3d.org                            -
# ----------------------------------------------------------------------------
# The MIT License (MIT)
#
# Copyright (c) 2018 www.open3d.org
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.
# ----------------------------------------------------------------------------

import open3d as o3d
import numpy as np
import pytest

try:
    import torch
    import torch.utils.dlpack
except ImportError:
    _torch_imported = False
else:
    _torch_imported = True


def list_devices():
    """
    If Open3D is built with CUDA support:
    - If cuda device is available, returns [Device("CPU:0"), Device("CUDA:0")].
    - If cuda device is not available, returns [Device("CPU:0")].

    If Open3D is built without CUDA support:
    - returns [Device("CPU:0")].
    """
    devices = [o3d.Device("CPU:" + str(0))]
    if _torch_imported:
        if (o3d.cuda.device_count() != torch.cuda.device_count()):
            raise RuntimeError(
                "o3d.cuda.device_count() != torch.cuda.device_count(), "
                "{} != {}".format(o3d.cuda.device_count(),
                                  torch.cuda.device_count()))
    else:
        print("Warning: PyTorch is not imported")
    if o3d.cuda.device_count() > 0:
        devices.append(o3d.Device("CUDA:0"))
    return devices


@pytest.mark.parametrize("device", list_devices())
def test_creation(device):
    # Shape takes tuple, list or o3d.SizeVector
    t = o3d.Tensor.empty((2, 3), o3d.Dtype.Float32, device=device)
    assert t.shape == o3d.SizeVector([2, 3])
    t = o3d.Tensor.empty([2, 3], o3d.Dtype.Float32, device=device)
    assert t.shape == o3d.SizeVector([2, 3])
    t = o3d.Tensor.empty(o3d.SizeVector([2, 3]),
                         o3d.Dtype.Float32,
                         device=device)
    assert t.shape == o3d.SizeVector([2, 3])

    # Test zeros and ones
    t = o3d.Tensor.zeros((2, 3), o3d.Dtype.Float32, device=device)
    np.testing.assert_equal(t.cpu().numpy(), np.zeros((2, 3), dtype=np.float32))
    t = o3d.Tensor.ones((2, 3), o3d.Dtype.Float32, device=device)
    np.testing.assert_equal(t.cpu().numpy(), np.ones((2, 3), dtype=np.float32))

    # Automatic casting of dtype.
    t = o3d.Tensor.full((2,), False, o3d.Dtype.Float32, device=device)
    np.testing.assert_equal(t.cpu().numpy(),
                            np.full((2,), False, dtype=np.float32))
    t = o3d.Tensor.full((2,), 3.5, o3d.Dtype.UInt8, device=device)
    np.testing.assert_equal(t.cpu().numpy(), np.full((2,), 3.5, dtype=np.uint8))


@pytest.mark.parametrize("shape", [(), (0,), (1,), (0, 2), (0, 0, 2),
                                   (2, 0, 3)])
@pytest.mark.parametrize("device", list_devices())
def test_creation_special_shapes(shape, device):
    o3_t = o3d.Tensor.full(shape, 3.14, o3d.Dtype.Float32, device=device)
    np_t = np.full(shape, 3.14, dtype=np.float32)
    np.testing.assert_equal(o3_t.cpu().numpy(), np_t)


def test_dtype():
    dtype = o3d.Dtype.Int32
    assert o3d.DtypeUtil.byte_size(dtype) == 4
    assert "{}".format(dtype) == "Dtype.Int32"


def test_device():
    device = o3d.Device()
    assert device.get_type() == o3d.Device.DeviceType.CPU
    assert device.get_id() == 0

    device = o3d.Device("CUDA", 1)
    assert device.get_type() == o3d.Device.DeviceType.CUDA
    assert device.get_id() == 1

    device = o3d.Device("CUDA:2")
    assert device.get_type() == o3d.Device.DeviceType.CUDA
    assert device.get_id() == 2

    assert o3d.Device("CUDA", 1) == o3d.Device("CUDA:1")
    assert o3d.Device("CUDA", 1) != o3d.Device("CUDA:0")

    assert o3d.Device("CUDA", 1).__str__() == "CUDA:1"


def test_size_vector():
    # List
    sv = o3d.SizeVector([-1, 2, 3])
    assert "{}".format(sv) == "{-1, 2, 3}"

    # Tuple
    sv = o3d.SizeVector((-1, 2, 3))
    assert "{}".format(sv) == "{-1, 2, 3}"

    # Numpy 1D array
    sv = o3d.SizeVector(np.array([-1, 2, 3]))
    assert "{}".format(sv) == "{-1, 2, 3}"

    # Empty
    sv = o3d.SizeVector()
    assert "{}".format(sv) == "{}"
    sv = o3d.SizeVector([])
    assert "{}".format(sv) == "{}"
    sv = o3d.SizeVector(())
    assert "{}".format(sv) == "{}"
    sv = o3d.SizeVector(np.array([]))
    assert "{}".format(sv) == "{}"

    # Automatic int casting (not rounding to nearest)
    sv = o3d.SizeVector((1.9, 2, 3))
    assert "{}".format(sv) == "{1, 2, 3}"

    # Automatic casting negative
    sv = o3d.SizeVector((-1.5, 2, 3))
    assert "{}".format(sv) == "{-1, 2, 3}"

    # 2D list exception
    with pytest.raises(ValueError):
        sv = o3d.SizeVector([[1, 2], [3, 4]])

    # 2D Numpy array exception
    with pytest.raises(ValueError):
        sv = o3d.SizeVector(np.array([[1, 2], [3, 4]]))

    # Garbage input
    with pytest.raises(ValueError):
        sv = o3d.SizeVector(["foo", "bar"])


@pytest.mark.parametrize("device", list_devices())
def test_tensor_constructor(device):
    dtype = o3d.Dtype.Int32

    # Numpy array
    np_t = np.array([[0, 1, 2], [3, 4, 5]])
    o3_t = o3d.Tensor(np_t, dtype, device)
    np.testing.assert_equal(np_t, o3_t.cpu().numpy())

    # 2D list
    li_t = [[0, 1, 2], [3, 4, 5]]
    no3_t = o3d.Tensor(li_t, dtype, device)
    np.testing.assert_equal(li_t, o3_t.cpu().numpy())

    # 2D list, inconsistent length
    li_t = [[0, 1, 2], [3, 4]]
    with pytest.raises(ValueError):
        o3_t = o3d.Tensor(li_t, dtype, device)

    # Automatic casting
    np_t_double = np.array([[0., 1.5, 2.], [3., 4., 5.]])
    np_t_int = np.array([[0, 1, 2], [3, 4, 5]])
    o3_t = o3d.Tensor(np_t_double, dtype, device)
    np.testing.assert_equal(np_t_int, o3_t.cpu().numpy())

    # Special strides
    np_t = np.random.randint(10, size=(10, 10))[1:10:2, 1:10:3].T
    o3_t = o3d.Tensor(np_t, dtype, device)
    np.testing.assert_equal(np_t, o3_t.cpu().numpy())

    # Boolean
    np_t = np.array([True, False, True], dtype=np.bool)
    o3_t = o3d.Tensor([True, False, True], o3d.Dtype.Bool, device)
    np.testing.assert_equal(np_t, o3_t.cpu().numpy())
    o3_t = o3d.Tensor(np_t, o3d.Dtype.Bool, device)
    np.testing.assert_equal(np_t, o3_t.cpu().numpy())


def test_tensor_from_to_numpy():
    # a->b copy; b, c share memory
    a = np.ones((2, 2))
    b = o3d.Tensor(a)
    c = b.numpy()

    c[0, 1] = 200
    r = np.array([[1., 200.], [1., 1.]])
    np.testing.assert_equal(r, b.numpy())
    np.testing.assert_equal(r, c)

    # a, b, c share memory
    a = np.array([[1., 1.], [1., 1.]])
    b = o3d.Tensor.from_numpy(a)
    c = b.numpy()

    a[0, 0] = 100
    c[0, 1] = 200
    r = np.array([[100., 200.], [1., 1.]])
    np.testing.assert_equal(r, a)
    np.testing.assert_equal(r, b.numpy())
    np.testing.assert_equal(r, c)

    # Special strides
    ran_t = np.random.randint(10, size=(10, 10)).astype(np.int32)
    src_t = ran_t[1:10:2, 1:10:3].T
    o3d_t = o3d.Tensor.from_numpy(src_t)  # Shared memory
    dst_t = o3d_t.numpy()
    np.testing.assert_equal(dst_t, src_t)

    dst_t[0, 0] = 100
    np.testing.assert_equal(dst_t, src_t)
    np.testing.assert_equal(dst_t, o3d_t.numpy())

    src_t[0, 1] = 200
    np.testing.assert_equal(dst_t, src_t)
    np.testing.assert_equal(dst_t, o3d_t.numpy())


def test_tensor_to_numpy_scope():
    src_t = np.array([[10., 11., 12.], [13., 14., 15.]])

    def get_dst_t():
        o3d_t = o3d.Tensor(src_t)  # Copy
        dst_t = o3d_t.numpy()
        return dst_t

    dst_t = get_dst_t()
    np.testing.assert_equal(dst_t, src_t)


@pytest.mark.parametrize("device", list_devices())
def test_tensor_to_pytorch_scope(device):
    if not _torch_imported:
        return

    src_t = np.array([[10, 11, 12.], [13., 14., 15.]])

    def get_dst_t():
        o3d_t = o3d.Tensor(src_t, device=device)  # Copy
        dst_t = torch.utils.dlpack.from_dlpack(o3d_t.to_dlpack())
        return dst_t

    dst_t = get_dst_t().cpu().numpy()
    np.testing.assert_equal(dst_t, src_t)


@pytest.mark.parametrize("device", list_devices())
def test_tensor_from_to_pytorch(device):
    if not _torch_imported:
        return

    device_id = device.get_id()
    device_type = device.get_type()

    # a, b, c share memory
    a = torch.ones((2, 2))
    if device_type == o3d.Device.DeviceType.CUDA:
        a = a.cuda(device_id)
    b = o3d.Tensor.from_dlpack(torch.utils.dlpack.to_dlpack(a))
    c = torch.utils.dlpack.from_dlpack(b.to_dlpack())

    a[0, 0] = 100
    c[0, 1] = 200
    r = np.array([[100., 200.], [1., 1.]])
    np.testing.assert_equal(r, a.cpu().numpy())
    np.testing.assert_equal(r, b.cpu().numpy())
    np.testing.assert_equal(r, c.cpu().numpy())

    # Special strides
    np_r = np.random.randint(10, size=(10, 10)).astype(np.int32)
    th_r = torch.Tensor(np_r)
    th_t = th_r[1:10:2, 1:10:3].T
    if device_type == o3d.Device.DeviceType.CUDA:
        th_t = th_t.cuda(device_id)

    o3_t = o3d.Tensor.from_dlpack(torch.utils.dlpack.to_dlpack(th_t))
    np.testing.assert_equal(th_t.cpu().numpy(), o3_t.cpu().numpy())

    th_t[0, 0] = 100
    np.testing.assert_equal(th_t.cpu().numpy(), o3_t.cpu().numpy())


def test_tensor_numpy_to_open3d_to_pytorch():
    if not _torch_imported:
        return

    # Numpy -> Open3D -> PyTorch all share the same memory
    a = np.ones((2, 2))
    b = o3d.Tensor.from_numpy(a)
    c = torch.utils.dlpack.from_dlpack(b.to_dlpack())

    a[0, 0] = 100
    c[0, 1] = 200
    r = np.array([[100., 200.], [1., 1.]])
    np.testing.assert_equal(r, a)
    np.testing.assert_equal(r, b.cpu().numpy())
    np.testing.assert_equal(r, c.cpu().numpy())


@pytest.mark.parametrize("device", list_devices())
def test_binary_ew_ops(device):
    a = o3d.Tensor(np.array([4, 6, 8, 10, 12, 14]), device=device)
    b = o3d.Tensor(np.array([2, 3, 4, 5, 6, 7]), device=device)
    np.testing.assert_equal((a + b).cpu().numpy(),
                            np.array([6, 9, 12, 15, 18, 21]))
    np.testing.assert_equal((a - b).cpu().numpy(), np.array([2, 3, 4, 5, 6, 7]))
    np.testing.assert_equal((a * b).cpu().numpy(),
                            np.array([8, 18, 32, 50, 72, 98]))
    np.testing.assert_equal((a / b).cpu().numpy(), np.array([2, 2, 2, 2, 2, 2]))

    a = o3d.Tensor(np.array([4, 6, 8, 10, 12, 14]), device=device)
    a += b
    np.testing.assert_equal(a.cpu().numpy(), np.array([6, 9, 12, 15, 18, 21]))

    a = o3d.Tensor(np.array([4, 6, 8, 10, 12, 14]), device=device)
    a -= b
    np.testing.assert_equal(a.cpu().numpy(), np.array([2, 3, 4, 5, 6, 7]))

    a = o3d.Tensor(np.array([4, 6, 8, 10, 12, 14]), device=device)
    a *= b
    np.testing.assert_equal(a.cpu().numpy(), np.array([8, 18, 32, 50, 72, 98]))

    a = o3d.Tensor(np.array([4, 6, 8, 10, 12, 14]), device=device)
    a //= b
    np.testing.assert_equal(a.cpu().numpy(), np.array([2, 2, 2, 2, 2, 2]))


@pytest.mark.parametrize("device", list_devices())
def test_to(device):
    a = o3d.Tensor(np.array([0.1, 1.2, 2.3, 3.4, 4.5, 5.6]).astype(np.float32),
                   device=device)
    b = a.to(o3d.Dtype.Int32)
    np.testing.assert_equal(b.cpu().numpy(), np.array([0, 1, 2, 3, 4, 5]))
    assert b.shape == o3d.SizeVector([6])
    assert b.strides == o3d.SizeVector([1])
    assert b.dtype == o3d.Dtype.Int32
    assert b.device == a.device


@pytest.mark.parametrize("device", list_devices())
def test_unary_ew_ops(device):
    src_vals = np.array([0, 1, 2, 3, 4, 5]).astype(np.float32)
    src = o3d.Tensor(src_vals, device=device)

    rtol = 1e-5
    atol = 0
    np.testing.assert_allclose(src.sqrt().cpu().numpy(),
                               np.sqrt(src_vals),
                               rtol=rtol,
                               atol=atol)
    np.testing.assert_allclose(src.sin().cpu().numpy(),
                               np.sin(src_vals),
                               rtol=rtol,
                               atol=atol)
    np.testing.assert_allclose(src.cos().cpu().numpy(),
                               np.cos(src_vals),
                               rtol=rtol,
                               atol=atol)
    np.testing.assert_allclose(src.neg().cpu().numpy(),
                               -src_vals,
                               rtol=rtol,
                               atol=atol)
    np.testing.assert_allclose(src.exp().cpu().numpy(),
                               np.exp(src_vals),
                               rtol=rtol,
                               atol=atol)


@pytest.mark.parametrize("device", list_devices())
def test_tensorlist_operations(device):
    a = o3d.TensorList([3, 4], o3d.Dtype.Float32, device=device, size=1)
    assert a.size() == 1

    b = o3d.TensorList.from_tensor(
        o3d.Tensor(np.ones((2, 3, 4), dtype=np.float32), device=device))
    assert b.size() == 2

    c = o3d.TensorList.from_tensors(
        [o3d.Tensor(np.zeros((3, 4), dtype=np.float32), device=device)],
        device=device)
    assert c.size() == 1

    d = a + b
    assert d.size() == 3

    e = o3d.TensorList.concat(c, d)
    assert e.size() == 4

    e.push_back(o3d.Tensor(np.zeros((3, 4), dtype=np.float32), device=device))
    assert e.size() == 5

    e.extend(d)
    assert e.size() == 8

    e += a
    assert e.size() == 9


@pytest.mark.parametrize("device", list_devices())
def test_getitem(device):
    np_t = np.array(range(24)).reshape((2, 3, 4))
    o3_t = o3d.Tensor(np_t, device=device)

    np.testing.assert_equal(o3_t[:].cpu().numpy(), np_t[:])
    np.testing.assert_equal(o3_t[0].cpu().numpy(), np_t[0])
    np.testing.assert_equal(o3_t[0, 1].cpu().numpy(), np_t[0, 1])
    np.testing.assert_equal(o3_t[0, :].cpu().numpy(), np_t[0, :])
    np.testing.assert_equal(o3_t[0, 1:3].cpu().numpy(), np_t[0, 1:3])
    np.testing.assert_equal(o3_t[0, :, :-2].cpu().numpy(), np_t[0, :, :-2])
    np.testing.assert_equal(o3_t[0, 1:3, 2].cpu().numpy(), np_t[0, 1:3, 2])
    np.testing.assert_equal(o3_t[0, 1:-1, 2].cpu().numpy(), np_t[0, 1:-1, 2])
    np.testing.assert_equal(o3_t[0, 1:3, 0:4:2].cpu().numpy(),
                            np_t[0, 1:3, 0:4:2])
    np.testing.assert_equal(o3_t[0, 1:3, 0:-1:2].cpu().numpy(),
                            np_t[0, 1:3, 0:-1:2])
    np.testing.assert_equal(o3_t[0, 1, :].cpu().numpy(), np_t[0, 1, :])

    # Slice the slice
    np.testing.assert_equal(o3_t[0:2, 1:3, 0:4][0:1, 0:2, 2:3].cpu().numpy(),
                            np_t[0:2, 1:3, 0:4][0:1, 0:2, 2:3])


@pytest.mark.parametrize("device", list_devices())
def test_setitem(device):
    np_ref = np.array(range(24)).reshape((2, 3, 4))
    o3_ref = o3d.Tensor(np_ref, device=device)

    np_t = np_ref.copy()
    o3_t = o3d.Tensor(np_t, device=device)
    np_fill_t = np.random.rand(*np_t[:].shape)
    o3_fill_t = o3d.Tensor(np_fill_t, device=device)
    np_t[:] = np_fill_t
    o3_t[:] = o3_fill_t
    np.testing.assert_equal(o3_t.cpu().numpy(), np_t)

    np_t = np_ref.copy()
    o3_t = o3d.Tensor(np_t, device=device)
    np_fill_t = np.random.rand(*np_t[0].shape)
    o3_fill_t = o3d.Tensor(np_fill_t, device=device)
    np_t[0] = np_fill_t
    o3_t[0] = o3_fill_t
    np.testing.assert_equal(o3_t.cpu().numpy(), np_t)

    np_t = np_ref.copy()
    o3_t = o3d.Tensor(np_t, device=device)
    np_fill_t = np.random.rand(*np_t[0, 1].shape)
    o3_fill_t = o3d.Tensor(np_fill_t, device=device)
    np_t[0, 1] = np_fill_t
    o3_t[0, 1] = o3_fill_t
    np.testing.assert_equal(o3_t.cpu().numpy(), np_t)

    np_t = np_ref.copy()
    o3_t = o3d.Tensor(np_t, device=device)
    np_fill_t = np.random.rand(*np_t[0, :].shape)
    o3_fill_t = o3d.Tensor(np_fill_t, device=device)
    np_t[0, :] = np_fill_t
    o3_t[0, :] = o3_fill_t
    np.testing.assert_equal(o3_t.cpu().numpy(), np_t)

    np_t = np_ref.copy()
    o3_t = o3d.Tensor(np_t, device=device)
    np_fill_t = np.random.rand(*np_t[0, 1:3].shape)
    o3_fill_t = o3d.Tensor(np_fill_t, device=device)
    np_t[0, 1:3] = np_fill_t
    o3_t[0, 1:3] = o3_fill_t
    np.testing.assert_equal(o3_t.cpu().numpy(), np_t)

    np_t = np_ref.copy()
    o3_t = o3d.Tensor(np_t, device=device)
    np_fill_t = np.random.rand(*np_t[0, :, :-2].shape)
    o3_fill_t = o3d.Tensor(np_fill_t, device=device)
    np_t[0, :, :-2] = np_fill_t
    o3_t[0, :, :-2] = o3_fill_t
    np.testing.assert_equal(o3_t.cpu().numpy(), np_t)

    np_t = np_ref.copy()
    o3_t = o3d.Tensor(np_t, device=device)
    np_fill_t = np.random.rand(*np_t[0, 1:3, 2].shape)
    o3_fill_t = o3d.Tensor(np_fill_t, device=device)
    np_t[0, 1:3, 2] = np_fill_t
    o3_t[0, 1:3, 2] = o3_fill_t
    np.testing.assert_equal(o3_t.cpu().numpy(), np_t)

    np_t = np_ref.copy()
    o3_t = o3d.Tensor(np_t, device=device)
    np_fill_t = np.random.rand(*np_t[0, 1:-1, 2].shape)
    o3_fill_t = o3d.Tensor(np_fill_t, device=device)
    np_t[0, 1:-1, 2] = np_fill_t
    o3_t[0, 1:-1, 2] = o3_fill_t
    np.testing.assert_equal(o3_t.cpu().numpy(), np_t)

    np_t = np_ref.copy()
    o3_t = o3d.Tensor(np_t, device=device)
    np_fill_t = np.random.rand(*np_t[0, 1:3, 0:4:2].shape)
    o3_fill_t = o3d.Tensor(np_fill_t, device=device)
    np_t[0, 1:3, 0:4:2] = np_fill_t
    o3_t[0, 1:3, 0:4:2] = o3_fill_t
    np.testing.assert_equal(o3_t.cpu().numpy(), np_t)

    np_t = np_ref.copy()
    o3_t = o3d.Tensor(np_t, device=device)
    np_fill_t = np.random.rand(*np_t[0, 1:3, 0:-1:2].shape)
    o3_fill_t = o3d.Tensor(np_fill_t, device=device)
    np_t[0, 1:3, 0:-1:2] = np_fill_t
    o3_t[0, 1:3, 0:-1:2] = o3_fill_t
    np.testing.assert_equal(o3_t.cpu().numpy(), np_t)

    np_t = np_ref.copy()
    o3_t = o3d.Tensor(np_t, device=device)
    np_fill_t = np.random.rand(*np_t[0, 1, :].shape)
    o3_fill_t = o3d.Tensor(np_fill_t, device=device)
    np_t[0, 1, :] = np_fill_t
    o3_t[0, 1, :] = o3_fill_t
    np.testing.assert_equal(o3_t.cpu().numpy(), np_t)

    np_t = np_ref.copy()
    o3_t = o3d.Tensor(np_t, device=device)
    np_fill_t = np.random.rand(*np_t[0:2, 1:3, 0:4][0:1, 0:2, 2:3].shape)
    o3_fill_t = o3d.Tensor(np_fill_t, device=device)
    np_t[0:2, 1:3, 0:4][0:1, 0:2, 2:3] = np_fill_t
    o3_t[0:2, 1:3, 0:4][0:1, 0:2, 2:3] = o3_fill_t
    np.testing.assert_equal(o3_t.cpu().numpy(), np_t)


@pytest.mark.parametrize("device", list_devices())
def test_cast_to_py_tensor(device):
    a = o3d.Tensor([1], device=device)
    b = o3d.Tensor([2], device=device)
    c = a + b
    assert isinstance(c, o3d.Tensor)  # Not o3d.open3d-pybind.Tensor


@pytest.mark.parametrize(
    "dim",
    [0, 1, 2, (), (0,), (1,), (2,), (0, 1), (0, 2), (1, 2), (0, 1, 2), None])
@pytest.mark.parametrize("keepdim", [True, False])
@pytest.mark.parametrize("device", list_devices())
def test_reduction_sum(dim, keepdim, device):
    np_src = np.array(range(24)).reshape((2, 3, 4))
    o3_src = o3d.Tensor(np_src, device=device)

    np_dst = np_src.sum(axis=dim, keepdims=keepdim)
    o3_dst = o3_src.sum(dim=dim, keepdim=keepdim)
    np.testing.assert_allclose(o3_dst.cpu().numpy(), np_dst)


@pytest.mark.parametrize("shape_and_axis", [
    ((), ()),
    ((0,), ()),
    ((0,), (0)),
    ((0, 2), ()),
    ((0, 2), (0)),
    ((0, 2), (1)),
])
@pytest.mark.parametrize("keepdim", [True, False])
@pytest.mark.parametrize("device", list_devices())
def test_reduction_special_shapes(shape_and_axis, keepdim, device):
    shape, axis = shape_and_axis
    np_src = np.array(np.random.rand(*shape))
    o3_src = o3d.Tensor(np_src, device=device)
    np.testing.assert_equal(o3_src.cpu().numpy(), np_src)

    np_dst = np_src.sum(axis=axis, keepdims=keepdim)
    o3_dst = o3_src.sum(dim=axis, keepdim=keepdim)
    np.testing.assert_equal(o3_dst.cpu().numpy(), np_dst)


@pytest.mark.parametrize(
    "dim",
    [0, 1, 2, (), (0,), (1,), (2,), (0, 1), (0, 2), (1, 2), (0, 1, 2), None])
@pytest.mark.parametrize("keepdim", [True, False])
@pytest.mark.parametrize("device", list_devices())
def test_reduction_mean(dim, keepdim, device):
    np_src = np.array(range(24)).reshape((2, 3, 4)).astype(np.float32)
    o3_src = o3d.Tensor(np_src, device=device)

    np_dst = np_src.mean(axis=dim, keepdims=keepdim)
    o3_dst = o3_src.mean(dim=dim, keepdim=keepdim)
    np.testing.assert_allclose(o3_dst.cpu().numpy(), np_dst)


@pytest.mark.parametrize(
    "dim",
    [0, 1, 2, (), (0,), (1,), (2,), (0, 1), (0, 2), (1, 2), (0, 1, 2), None])
@pytest.mark.parametrize("keepdim", [True, False])
@pytest.mark.parametrize("device", list_devices())
def test_reduction_prod(dim, keepdim, device):
    np_src = np.array(range(24)).reshape((2, 3, 4))
    o3_src = o3d.Tensor(np_src, device=device)

    np_dst = np_src.prod(axis=dim, keepdims=keepdim)
    o3_dst = o3_src.prod(dim=dim, keepdim=keepdim)
    np.testing.assert_allclose(o3_dst.cpu().numpy(), np_dst)


@pytest.mark.parametrize(
    "dim",
    [0, 1, 2, (), (0,), (1,), (2,), (0, 1), (0, 2), (1, 2), (0, 1, 2), None])
@pytest.mark.parametrize("keepdim", [True, False])
@pytest.mark.parametrize("device", list_devices())
def test_reduction_min(dim, keepdim, device):
    np_src = np.array(range(24))
    np.random.shuffle(np_src)
    np_src = np_src.reshape((2, 3, 4))
    o3_src = o3d.Tensor(np_src, device=device)

    np_dst = np_src.min(axis=dim, keepdims=keepdim)
    o3_dst = o3_src.min(dim=dim, keepdim=keepdim)
    np.testing.assert_allclose(o3_dst.cpu().numpy(), np_dst)


@pytest.mark.parametrize(
    "dim",
    [0, 1, 2, (), (0,), (1,), (2,), (0, 1), (0, 2), (1, 2), (0, 1, 2), None])
@pytest.mark.parametrize("keepdim", [True, False])
@pytest.mark.parametrize("device", list_devices())
def test_reduction_max(dim, keepdim, device):
    np_src = np.array(range(24))
    np.random.shuffle(np_src)
    np_src = np_src.reshape((2, 3, 4))
    o3_src = o3d.Tensor(np_src, device=device)

    np_dst = np_src.max(axis=dim, keepdims=keepdim)
    o3_dst = o3_src.max(dim=dim, keepdim=keepdim)
    np.testing.assert_allclose(o3_dst.cpu().numpy(), np_dst)


@pytest.mark.parametrize("dim", [0, 1, 2, None])
@pytest.mark.parametrize("device", list_devices())
def test_reduction_argmin_argmax(dim, device):
    np_src = np.array(range(24))
    np.random.shuffle(np_src)
    np_src = np_src.reshape((2, 3, 4))
    o3_src = o3d.Tensor(np_src, device=device)

    np_dst = np_src.argmin(axis=dim)
    o3_dst = o3_src.argmin(dim=dim)
    np.testing.assert_allclose(o3_dst.cpu().numpy(), np_dst)

    np_dst = np_src.argmax(axis=dim)
    o3_dst = o3_src.argmax(dim=dim)
    np.testing.assert_allclose(o3_dst.cpu().numpy(), np_dst)


@pytest.mark.parametrize("device", list_devices())
def test_advanced_index_get_mixed(device):
    np_src = np.array(range(24)).reshape((2, 3, 4))
    o3_src = o3d.Tensor(np_src, device=device)

    np_dst = np_src[1, 0:2, [1, 2]]
    o3_dst = o3_src[1, 0:2, [1, 2]]
    np.testing.assert_equal(o3_dst.cpu().numpy(), np_dst)

    # Subtle differences between slice and list
    np_src = np.array([0, 100, 200, 300, 400, 500, 600, 700, 800]).reshape(3, 3)
    o3_src = o3d.Tensor(np_src, device=device)
    np.testing.assert_equal(o3_src[1, 2].cpu().numpy(), np_src[1, 2])
    np.testing.assert_equal(o3_src[[1, 2]].cpu().numpy(), np_src[[1, 2]])
    np.testing.assert_equal(o3_src[(1, 2)].cpu().numpy(), np_src[(1, 2)])
    np.testing.assert_equal(o3_src[(1, 2), [1, 2]].cpu().numpy(),
                            np_src[(1, 2), [1, 2]])

    # Complex case: interleaving slice and advanced indexing
    np_src = np.array(range(120)).reshape((2, 3, 4, 5))
    o3_src = o3d.Tensor(np_src, device=device)
    o3_dst = o3_src[1, [[1, 2], [2, 1]], 0:4:2, [3, 4]]
    np_dst = np_src[1, [[1, 2], [2, 1]], 0:4:2, [3, 4]]
    np.testing.assert_equal(o3_dst.cpu().numpy(), np_dst)


@pytest.mark.parametrize("device", list_devices())
def test_advanced_index_set_mixed(device):
    np_src = np.array(range(24)).reshape((2, 3, 4))
    o3_src = o3d.Tensor(np_src, device=device)

    np_fill = np.array(([[100, 200], [300, 400]]))
    o3_fill = o3d.Tensor(np_fill, device=device)

    np_src[1, 0:2, [1, 2]] = np_fill
    o3_src[1, 0:2, [1, 2]] = o3_fill
    np.testing.assert_equal(o3_src.cpu().numpy(), np_src)

    # Complex case: interleaving slice and advanced indexing
    np_src = np.array(range(120)).reshape((2, 3, 4, 5))
    o3_src = o3d.Tensor(np_src, device=device)
    fill_shape = np_src[1, [[1, 2], [2, 1]], 0:4:2, [3, 4]].shape
    np_fill_val = np.random.randint(5000, size=fill_shape).astype(np_src.dtype)
    o3_fill_val = o3d.Tensor(np_fill_val)
    o3_src[1, [[1, 2], [2, 1]], 0:4:2, [3, 4]] = o3_fill_val
    np_src[1, [[1, 2], [2, 1]], 0:4:2, [3, 4]] = np_fill_val
    np.testing.assert_equal(o3_src.cpu().numpy(), np_src)


@pytest.mark.parametrize("device", list_devices())
def test_tensorlist_indexing(device):
    # 5 x (3, 4)
    dtype = o3d.Dtype.Float32
    device = o3d.Device("CPU:0")
    np_t = np.ones((5, 3, 4), dtype=np.float32)
    t = o3d.Tensor(np_t, dtype, device)

    tl = o3d.TensorList.from_tensor(t, inplace=True)

    # set slices [1, 3]
    tl.tensor()[1:5:2] = o3d.Tensor(3 * np.ones((2, 3, 4), dtype=np.float32))

    # set items [4]
    tl[-1] = o3d.Tensor(np.zeros((3, 4), dtype=np.float32))

    # get items
    np.testing.assert_allclose(tl[0].cpu().numpy(),
                               np.ones((3, 4), dtype=np.float32))
    np.testing.assert_allclose(tl[1].cpu().numpy(), 3 * np.ones(
        (3, 4), dtype=np.float32))
    np.testing.assert_allclose(tl[2].cpu().numpy(),
                               np.ones((3, 4), dtype=np.float32))
    np.testing.assert_allclose(tl[3].cpu().numpy(), 3 * np.ones(
        (3, 4), dtype=np.float32))
    np.testing.assert_allclose(tl[4].cpu().numpy(),
                               np.zeros((3, 4), dtype=np.float32))

    # push_back
    tl.push_back(o3d.Tensor(-1 * np.ones((3, 4)), dtype, device))
    assert tl.size() == 6
    np.testing.assert_allclose(tl[5].cpu().numpy(), -1 * np.ones(
        (3, 4), dtype=np.float32))

    tl += tl
    assert tl.size() == 12
    for offset in [0, 6]:
        np.testing.assert_allclose(tl[0 + offset].cpu().numpy(),
                                   np.ones((3, 4), dtype=np.float32))
        np.testing.assert_allclose(tl[1 + offset].cpu().numpy(), 3 * np.ones(
            (3, 4), dtype=np.float32))
        np.testing.assert_allclose(tl[2 + offset].cpu().numpy(),
                                   np.ones((3, 4), dtype=np.float32))
        np.testing.assert_allclose(tl[3 + offset].cpu().numpy(), 3 * np.ones(
            (3, 4), dtype=np.float32))
        np.testing.assert_allclose(tl[4 + offset].cpu().numpy(),
                                   np.zeros((3, 4), dtype=np.float32))


@pytest.mark.parametrize("device", list_devices())
def test_tensor_from_to_pytorch(device):
    if not _torch_imported:
        return


@pytest.mark.parametrize("np_func_name,o3_func_name", [("sqrt", "sqrt"),
                                                       ("sin", "sin"),
                                                       ("cos", "cos"),
                                                       ("negative", "neg"),
                                                       ("exp", "exp"),
                                                       ("abs", "abs")])
@pytest.mark.parametrize("device", list_devices())
def test_unary_elementwise(np_func_name, o3_func_name, device):
    np_t = np.array([-3, -2, -1, 9, 1, 2, 3]).astype(np.float32)
    o3_t = o3d.Tensor(np_t, device=device)

    # Test non-in-place version
    np.seterr(invalid='ignore')  # e.g. sqrt of negative should be -nan
    np.testing.assert_allclose(
        getattr(o3_t, o3_func_name)().cpu().numpy(),
        getattr(np, np_func_name)(np_t))

    # Test in-place version
    o3_func_name_inplace = o3_func_name + "_"
    getattr(o3_t, o3_func_name_inplace)()
    np.testing.assert_allclose(o3_t.cpu().numpy(),
                               getattr(np, np_func_name)(np_t))


@pytest.mark.parametrize("device", list_devices())
def test_logical_ops(device):
    np_a = np.array([True, False, True, False])
    np_b = np.array([True, True, False, False])
    o3_a = o3d.Tensor(np_a, device=device)
    o3_b = o3d.Tensor(np_b, device=device)

    o3_r = o3_a.logical_and(o3_b)
    np_r = np.logical_and(np_a, np_b)
    np.testing.assert_equal(o3_r.cpu().numpy(), np_r)

    o3_r = o3_a.logical_or(o3_b)
    np_r = np.logical_or(np_a, np_b)
    np.testing.assert_equal(o3_r.cpu().numpy(), np_r)

    o3_r = o3_a.logical_xor(o3_b)
    np_r = np.logical_xor(np_a, np_b)
    np.testing.assert_equal(o3_r.cpu().numpy(), np_r)


@pytest.mark.parametrize("device", list_devices())
def test_comparision_ops(device):
    np_a = np.array([0, 1, -1])
    np_b = np.array([0, 0, 0])
    o3_a = o3d.Tensor(np_a, device=device)
    o3_b = o3d.Tensor(np_b, device=device)

    np.testing.assert_equal((o3_a > o3_b).cpu().numpy(), np_a > np_b)
    np.testing.assert_equal((o3_a >= o3_b).cpu().numpy(), np_a >= np_b)
    np.testing.assert_equal((o3_a < o3_b).cpu().numpy(), np_a < np_b)
    np.testing.assert_equal((o3_a <= o3_b).cpu().numpy(), np_a <= np_b)
    np.testing.assert_equal((o3_a == o3_b).cpu().numpy(), np_a == np_b)
    np.testing.assert_equal((o3_a != o3_b).cpu().numpy(), np_a != np_b)


@pytest.mark.parametrize("device", list_devices())
def test_non_zero(device):
    np_x = np.array([[3, 0, 0], [0, 4, 0], [5, 6, 0]])
    np_nonzero_tuple = np.nonzero(np_x)
    o3_x = o3d.Tensor(np_x, device=device)
    o3_nonzero_tuple = o3_x.nonzero(as_tuple=True)
    for np_t, o3_t in zip(np_nonzero_tuple, o3_nonzero_tuple):
        np.testing.assert_equal(np_t, o3_t.cpu().numpy())


@pytest.mark.parametrize("device", list_devices())
def boolean_advanced_indexing(device):
    np_a = np.array([1, -1, -2, 3])
    o3_a = o3d.Tensor(np_a, device=device)
    np_a[np_a < 0] = 0
    o3_a[o3_a < 0] = 0
    np.testing.assert_equal(np_a, o3_a.cpu().numpy())

    np_x = np.array([[0, 1], [1, 1], [2, 2]])
    np_row_sum = np.array([1, 2, 4])
    np_y = np_x[np_row_sum <= 2, :]
    o3_x = o3d.Tensor(np_x, device=device)
    o3_row_sum = o3d.Tensor(np_row_sum)
    o3_y = o3_x[o3_row_sum <= 2, :]
    np.testing.assert_equal(np_y, o3_y.cpu().numpy())


@pytest.mark.parametrize("device", list_devices())
def test_scalar_op(device):
    # +
    a = o3d.Tensor.ones((2, 3), o3d.Dtype.Float32, device=device)
    b = a.add(1)
    np.testing.assert_equal(b.cpu().numpy(), np.full((2, 3), 2))
    b = a + 1
    np.testing.assert_equal(b.cpu().numpy(), np.full((2, 3), 2))
    b = 1 + a
    np.testing.assert_equal(b.cpu().numpy(), np.full((2, 3), 2))
    b = a + True
    np.testing.assert_equal(b.cpu().numpy(), np.full((2, 3), 2))

    # +=
    a = o3d.Tensor.ones((2, 3), o3d.Dtype.Float32, device=device)
    a.add_(1)
    np.testing.assert_equal(a.cpu().numpy(), np.full((2, 3), 2))
    a += 1
    np.testing.assert_equal(a.cpu().numpy(), np.full((2, 3), 3))
    a += True
    np.testing.assert_equal(a.cpu().numpy(), np.full((2, 3), 4))

    # -
    a = o3d.Tensor.ones((2, 3), o3d.Dtype.Float32, device=device)
    b = a.sub(1)
    np.testing.assert_equal(b.cpu().numpy(), np.full((2, 3), 0))
    b = a - 1
    np.testing.assert_equal(b.cpu().numpy(), np.full((2, 3), 0))
    b = 10 - a
    np.testing.assert_equal(b.cpu().numpy(), np.full((2, 3), 9))
    b = a - True
    np.testing.assert_equal(b.cpu().numpy(), np.full((2, 3), 0))

    # -=
    a = o3d.Tensor.ones((2, 3), o3d.Dtype.Float32, device=device)
    a.sub_(1)
    np.testing.assert_equal(a.cpu().numpy(), np.full((2, 3), 0))
    a -= 1
    np.testing.assert_equal(a.cpu().numpy(), np.full((2, 3), -1))
    a -= True
    np.testing.assert_equal(a.cpu().numpy(), np.full((2, 3), -2))

    # *
    a = o3d.Tensor.full((2, 3), 2, o3d.Dtype.Float32, device=device)
    b = a.mul(10)
    np.testing.assert_equal(b.cpu().numpy(), np.full((2, 3), 20))
    b = a * 10
    np.testing.assert_equal(b.cpu().numpy(), np.full((2, 3), 20))
    b = 10 * a
    np.testing.assert_equal(b.cpu().numpy(), np.full((2, 3), 20))
    b = a * True
    np.testing.assert_equal(b.cpu().numpy(), np.full((2, 3), 2))

    # *=
    a = o3d.Tensor.full((2, 3), 2, o3d.Dtype.Float32, device=device)
    a.mul_(10)
    np.testing.assert_equal(a.cpu().numpy(), np.full((2, 3), 20))
    a *= 10
    np.testing.assert_equal(a.cpu().numpy(), np.full((2, 3), 200))
    a *= True
    np.testing.assert_equal(a.cpu().numpy(), np.full((2, 3), 200))

    # /
    a = o3d.Tensor.full((2, 3), 20, o3d.Dtype.Float32, device=device)
    b = a.div(2)
    np.testing.assert_equal(b.cpu().numpy(), np.full((2, 3), 10))
    b = a / 2
    np.testing.assert_equal(b.cpu().numpy(), np.full((2, 3), 10))
    b = a // 2
    np.testing.assert_equal(b.cpu().numpy(), np.full((2, 3), 10))
    b = 10 / a
    np.testing.assert_equal(b.cpu().numpy(), np.full((2, 3), 0.5))
    b = 10 // a
    np.testing.assert_equal(b.cpu().numpy(), np.full((2, 3), 0.5))
    b = a / True
    np.testing.assert_equal(b.cpu().numpy(), np.full((2, 3), 20))

    # /=
    a = o3d.Tensor.full((2, 3), 20, o3d.Dtype.Float32, device=device)
    a.div_(2)
    np.testing.assert_equal(a.cpu().numpy(), np.full((2, 3), 10))
    a /= 2
    np.testing.assert_equal(a.cpu().numpy(), np.full((2, 3), 5))
    a //= 2
    np.testing.assert_equal(a.cpu().numpy(), np.full((2, 3), 2.5))
    a /= True
    np.testing.assert_equal(a.cpu().numpy(), np.full((2, 3), 2.5))
