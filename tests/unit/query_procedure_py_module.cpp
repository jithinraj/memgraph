#include <gtest/gtest.h>

#include <string>

#include "query/procedure/mg_procedure_impl.hpp"
#include "query/procedure/py_module.hpp"

TEST(PyModule, MgpValueToPyObject) {
  mgp_memory memory{utils::NewDeleteResource()};
  auto *list = mgp_list_make_empty(42, &memory);
  {
    // Create a list: [null, false, true, 42, 0.1, "some text"]
    auto primitive_values = {mgp_value_make_null(&memory),
                             mgp_value_make_bool(0, &memory),
                             mgp_value_make_bool(1, &memory),
                             mgp_value_make_int(42, &memory),
                             mgp_value_make_double(0.1, &memory),
                             mgp_value_make_string("some text", &memory)};
    for (auto *val : primitive_values) {
      mgp_list_append(list, val);
      mgp_value_destroy(val);
    }
  }
  auto *list_val = mgp_value_make_list(list);
  auto *map = mgp_map_make_empty(&memory);
  mgp_map_insert(map, "list", list_val);
  mgp_value_destroy(list_val);
  auto *map_val = mgp_value_make_map(map);
  auto gil = py::EnsureGIL();
  auto py_dict = query::procedure::MgpValueToPyObject(*map_val);
  mgp_value_destroy(map_val);
  // We should now have in Python:
  // {"list": [None, False, True, 42, 0.1, "some text"]}
  ASSERT_TRUE(PyDict_Check(py_dict));
  EXPECT_EQ(PyDict_Size(py_dict), 1);
  PyObject *key = nullptr;
  PyObject *value = nullptr;
  Py_ssize_t pos = 0;
  while (PyDict_Next(py_dict, &pos, &key, &value)) {
    ASSERT_TRUE(PyUnicode_Check(key));
    EXPECT_EQ(std::string(PyUnicode_AsUTF8(key)), "list");
    ASSERT_TRUE(PyList_Check(value));
    ASSERT_EQ(PyList_Size(value), 6);
    EXPECT_EQ(PyList_GetItem(value, 0), Py_None);
    EXPECT_EQ(PyList_GetItem(value, 1), Py_False);
    EXPECT_EQ(PyList_GetItem(value, 2), Py_True);
    auto *py_long = PyList_GetItem(value, 3);
    ASSERT_TRUE(PyLong_Check(py_long));
    EXPECT_EQ(PyLong_AsLong(py_long), 42);
    auto *py_float = PyList_GetItem(value, 4);
    ASSERT_TRUE(PyFloat_Check(py_float));
    EXPECT_EQ(PyFloat_AsDouble(py_float), 0.1);
    auto *py_str = PyList_GetItem(value, 5);
    ASSERT_TRUE(PyUnicode_Check(py_str));
    EXPECT_EQ(std::string(PyUnicode_AsUTF8(py_str)), "some text");
  }
  // TODO: Vertex, Edge and Path values
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  // Initialize Python
  auto *program_name = Py_DecodeLocale(argv[0], nullptr);
  CHECK(program_name);
  // Set program name, so Python can find its way to runtime libraries relative
  // to executable.
  Py_SetProgramName(program_name);
  Py_InitializeEx(0 /* = initsigs */);
  PyEval_InitThreads();
  int test_result;
  Py_BEGIN_ALLOW_THREADS;
  test_result = RUN_ALL_TESTS();
  Py_END_ALLOW_THREADS;
  // Shutdown Python
  Py_Finalize();
  PyMem_RawFree(program_name);
  return test_result;
}
