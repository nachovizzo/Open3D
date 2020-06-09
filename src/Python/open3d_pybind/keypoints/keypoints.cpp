
#include "open3d_pybind/keypoints/keypoints.h"

#include "open3d_pybind/docstring.h"

namespace open3d {
void pybind_keypoints(py::module &m) {
    py::module m_submodule = m.def_submodule("keypoints");
    pybind_iss_detector(m_submodule);
}
}  // namespace open3d
