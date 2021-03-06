/* Copyright 2018 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "absl/algorithm/algorithm.h"
#include "absl/strings/str_format.h"
#include "tensorflow/lite/interpreter.h"
#include "tensorflow/lite/testing/util.h"
#include "tensorflow/lite/tools/benchmark/benchmark_performance_options.h"
#include "tensorflow/lite/tools/benchmark/benchmark_tflite_model.h"
#include "tensorflow/lite/tools/command_line_flags.h"

namespace {
const std::string* g_fp32_model_path = nullptr;
const std::string* g_int8_model_path = nullptr;
}  // namespace

namespace tflite {
namespace benchmark {
namespace {

enum class ModelGraphType { FP32, INT8 };

BenchmarkParams CreateParams(int32_t num_runs, float min_secs, float max_secs,
                             ModelGraphType graph_type = ModelGraphType::FP32) {
  BenchmarkParams params;
  params.AddParam("num_runs", BenchmarkParam::Create<int32_t>(num_runs));
  params.AddParam("min_secs", BenchmarkParam::Create<float>(min_secs));
  params.AddParam("max_secs", BenchmarkParam::Create<float>(max_secs));
  params.AddParam("run_delay", BenchmarkParam::Create<float>(-1.0f));
  params.AddParam("num_threads", BenchmarkParam::Create<int32_t>(1));
  params.AddParam("benchmark_name", BenchmarkParam::Create<std::string>(""));
  params.AddParam("output_prefix", BenchmarkParam::Create<std::string>(""));
  params.AddParam("warmup_runs", BenchmarkParam::Create<int32_t>(1));

  if (graph_type == ModelGraphType::INT8) {
    params.AddParam("graph",
                    BenchmarkParam::Create<std::string>(*g_int8_model_path));
  } else {
    // by default, simply use the fp32 one.
    params.AddParam("graph",
                    BenchmarkParam::Create<std::string>(*g_fp32_model_path));
  }

  params.AddParam("input_layer", BenchmarkParam::Create<std::string>(""));
  params.AddParam("input_layer_shape", BenchmarkParam::Create<std::string>(""));
  params.AddParam("input_layer_value_range",
                  BenchmarkParam::Create<std::string>(""));
  params.AddParam("input_layer_value_files",
                  BenchmarkParam::Create<std::string>(""));
  params.AddParam("use_hexagon", BenchmarkParam::Create<bool>(false));
  params.AddParam("use_xnnpack", BenchmarkParam::Create<bool>(false));
  params.AddParam("use_nnapi", BenchmarkParam::Create<bool>(false));
  params.AddParam("allow_fp16", BenchmarkParam::Create<bool>(false));
  params.AddParam("require_full_delegation",
                  BenchmarkParam::Create<bool>(false));
  params.AddParam("warmup_min_secs", BenchmarkParam::Create<float>(0.5f));
  params.AddParam("use_legacy_nnapi", BenchmarkParam::Create<bool>(false));
  params.AddParam("use_gpu", BenchmarkParam::Create<bool>(false));
  params.AddParam("enable_op_profiling", BenchmarkParam::Create<bool>(false));
  params.AddParam("max_profiling_buffer_entries",
                  BenchmarkParam::Create<int32_t>(1024));
  params.AddParam("nnapi_accelerator_name",
                  BenchmarkParam::Create<std::string>(""));
  params.AddParam("nnapi_execution_preference",
                  BenchmarkParam::Create<std::string>(""));
  params.AddParam("disable_nnapi_cpu", BenchmarkParam::Create<bool>(false));
  params.AddParam("max_delegated_partitions", BenchmarkParam::Create<int>(0));
  params.AddParam("profiling_output_csv_file",
                  BenchmarkParam::Create<std::string>(""));
  params.AddParam("enable_platform_tracing",
                  BenchmarkParam::Create<bool>(false));
  return params;
}

BenchmarkParams CreateParams() { return CreateParams(2, 1.0f, 150.0f); }
BenchmarkParams CreateFp32Params() {
  return CreateParams(2, 1.0f, 150.0f, ModelGraphType::FP32);
}
BenchmarkParams CreateInt8Params() {
  return CreateParams(2, 1.0f, 150.0f, ModelGraphType::INT8);
}

std::string CreateFilePath(const std::string& file_name) {
  return std::string(getenv("TEST_TMPDIR")) + file_name;
}

void WriteInputLayerValueFile(const std::string& file_path,
                              ModelGraphType graph_type, int num_elements,
                              char file_value = 'a') {
  std::ofstream file(file_path);
  int bytes = 0;
  switch (graph_type) {
    case ModelGraphType::FP32:
      bytes = 4 * num_elements;
      break;
    case ModelGraphType::INT8:
      bytes = num_elements;
      break;
    default:
      LOG(WARNING) << absl::StrFormat(
          "ModelGraphType(enum_value:%d) is not known.", graph_type);
      LOG(WARNING) << "The size of the ModelGraphType will be 1 byte in tests.";
      bytes = num_elements;
      break;
  }
  std::vector<char> buffer(bytes, file_value);
  file.write(buffer.data(), bytes);
}

void CheckInputTensorValue(const TfLiteTensor* input_tensor,
                           char tensor_value) {
  ASSERT_THAT(input_tensor, testing::NotNull());
  EXPECT_TRUE(std::all_of(
      input_tensor->data.raw, input_tensor->data.raw + input_tensor->bytes,
      [tensor_value](char c) { return c == tensor_value; }));
}

class TestBenchmark : public BenchmarkTfLiteModel {
 public:
  explicit TestBenchmark(BenchmarkParams params)
      : BenchmarkTfLiteModel(std::move(params)) {}
  const tflite::Interpreter* GetInterpreter() { return interpreter_.get(); }

  void Prepare() {
    PrepareInputData();
    ResetInputsAndOutputs();
  }

  const TfLiteTensor* GetInputTensor(int index) {
    return index >= interpreter_->inputs().size()
               ? nullptr
               : interpreter_->input_tensor(index);
  }
};

TEST(BenchmarkTest, DoesntCrashFp32Model) {
  ASSERT_THAT(g_fp32_model_path, testing::NotNull());

  TestBenchmark benchmark(CreateFp32Params());
  benchmark.Run();
}

TEST(BenchmarkTest, DoesntCrashInt8Model) {
  ASSERT_THAT(g_int8_model_path, testing::NotNull());

  TestBenchmark benchmark(CreateInt8Params());
  benchmark.Run();
}

TEST(BenchmarkTest, DoesntCrashMultiPerfOptions) {
  ASSERT_THAT(g_fp32_model_path, testing::NotNull());

  TestBenchmark benchmark(CreateFp32Params());
  BenchmarkPerformanceOptions all_options_benchmark(&benchmark);
  all_options_benchmark.Run();
}

TEST(BenchmarkTest, DoesntCrashMultiPerfOptionsWithProfiling) {
  ASSERT_THAT(g_fp32_model_path, testing::NotNull());

  BenchmarkParams params = CreateFp32Params();
  params.Set<bool>("enable_op_profiling", true);
  TestBenchmark benchmark(std::move(params));
  BenchmarkPerformanceOptions all_options_benchmark(&benchmark);
  all_options_benchmark.Run();
}

TEST(BenchmarkTest, DoesntCrashWithExplicitInputFp32Model) {
  ASSERT_THAT(g_fp32_model_path, testing::NotNull());

  // Note: the following input-related params are *specific* to model
  // 'g_fp32_model_path' which is specified as 'lite:testdata/multi_add.bin for
  // the test.
  BenchmarkParams params = CreateFp32Params();
  params.Set<std::string>("input_layer", "a,b,c,d");
  params.Set<std::string>("input_layer_shape",
                          "1,8,8,3:1,8,8,3:1,8,8,3:1,8,8,3");
  params.Set<std::string>("input_layer_value_range", "d,1,10:b,0,100");
  TestBenchmark benchmark(std::move(params));
  benchmark.Run();
}

TEST(BenchmarkTest, DoesntCrashWithExplicitInputInt8Model) {
  ASSERT_THAT(g_int8_model_path, testing::NotNull());

  // Note: the following input-related params are *specific* to model
  // 'g_int8_model_path' which is specified as
  // 'lite:testdata/add_quantized_int8.bin for the test.
  int a_min = 1;
  int a_max = 10;
  BenchmarkParams params = CreateInt8Params();
  params.Set<std::string>("input_layer", "a");
  params.Set<std::string>("input_layer_shape", "1,8,8,3");
  params.Set<std::string>("input_layer_value_range",
                          absl::StrFormat("a,%d,%d", a_min, a_max));
  TestBenchmark benchmark(std::move(params));
  benchmark.Run();

  auto input_tensor = benchmark.GetInputTensor(0);
  ASSERT_THAT(input_tensor, testing::NotNull());
  EXPECT_TRUE(std::all_of(
      input_tensor->data.raw, input_tensor->data.raw + input_tensor->bytes,
      [a_min, a_max](int i) { return a_min <= i && i <= a_max; }));
}

TEST(BenchmarkTest, DoesntCrashWithExplicitInputValueFilesFp32Model) {
  ASSERT_THAT(g_fp32_model_path, testing::NotNull());
  char file_value_b = 'b';
  const std::string file_path_b = CreateFilePath("fp32_binary_b");
  WriteInputLayerValueFile(file_path_b, ModelGraphType::FP32, 192,
                           file_value_b);
  char file_value_d = 'd';
  const std::string file_path_d = CreateFilePath("fp32_binary_d");
  WriteInputLayerValueFile(file_path_d, ModelGraphType::FP32, 192,
                           file_value_d);

  // Note: the following input-related params are *specific* to model
  // 'g_fp32_model_path' which is specified as 'lite:testdata/multi_add.bin for
  // the test.
  BenchmarkParams params = CreateFp32Params();
  params.Set<std::string>("input_layer", "a,b,c,d");
  params.Set<std::string>("input_layer_shape",
                          "1,8,8,3:1,8,8,3:1,8,8,3:1,8,8,3");
  params.Set<std::string>("input_layer_value_files",
                          "d:" + file_path_d + ",b:" + file_path_b);
  TestBenchmark benchmark(std::move(params));
  benchmark.Run();

  CheckInputTensorValue(benchmark.GetInputTensor(1), file_value_b);
  CheckInputTensorValue(benchmark.GetInputTensor(3), file_value_d);
}

TEST(BenchmarkTest, DoesntCrashWithExplicitInputValueFilesInt8Model) {
  ASSERT_THAT(g_int8_model_path, testing::NotNull());
  const std::string file_path = CreateFilePath("int8_binary");
  char file_value = 'a';
  WriteInputLayerValueFile(file_path, ModelGraphType::INT8, 192, file_value);

  // Note: the following input-related params are *specific* to model
  // 'g_int8_model_path' which is specified as
  // 'lite:testdata/add_quantized_int8.bin for the test.
  BenchmarkParams params = CreateInt8Params();
  params.Set<std::string>("input_layer", "a");
  params.Set<std::string>("input_layer_shape", "1,8,8,3");
  params.Set<std::string>("input_layer_value_files", "a:" + file_path);
  TestBenchmark benchmark(std::move(params));
  benchmark.Run();

  CheckInputTensorValue(benchmark.GetInputTensor(0), file_value);
}

class MaxDurationWorksTestListener : public BenchmarkListener {
  void OnBenchmarkEnd(const BenchmarkResults& results) override {
    const int64_t num_actul_runs = results.inference_time_us().count();
    TFLITE_LOG(INFO) << "number of actual runs: " << num_actul_runs;
    EXPECT_GE(num_actul_runs, 1);
    EXPECT_LT(num_actul_runs, 100000000);
  }
};

TEST(BenchmarkTest, MaxDurationWorks) {
  ASSERT_THAT(g_fp32_model_path, testing::NotNull());
  TestBenchmark benchmark(CreateParams(100000000 /* num_runs */,
                                       1000000.0f /* min_secs */,
                                       0.001f /* max_secs */));
  MaxDurationWorksTestListener listener;
  benchmark.AddListener(&listener);
  benchmark.Run();
}

TEST(BenchmarkTest, ParametersArePopulatedWhenInputShapeIsNotSpecified) {
  ASSERT_THAT(g_fp32_model_path, testing::NotNull());

  TestBenchmark benchmark(CreateParams());
  benchmark.Init();
  benchmark.Prepare();

  auto interpreter = benchmark.GetInterpreter();
  auto inputs = interpreter->inputs();
  ASSERT_GE(inputs.size(), 1);
  auto input_tensor = interpreter->tensor(inputs[0]);

  // Copy input tensor to a vector
  std::vector<char> input_bytes(input_tensor->data.raw,
                                input_tensor->data.raw + input_tensor->bytes);

  benchmark.Prepare();

  // Expect data is not the same.
  EXPECT_EQ(input_bytes.size(), input_tensor->bytes);
  EXPECT_FALSE(absl::equal(input_bytes.begin(), input_bytes.end(),
                           input_tensor->data.raw,
                           input_tensor->data.raw + input_tensor->bytes));
}

}  // namespace
}  // namespace benchmark
}  // namespace tflite

int main(int argc, char** argv) {
  std::string fp32_model_path, int8_model_path;
  std::vector<tflite::Flag> flags = {
      tflite::Flag::CreateFlag("fp32_graph", &fp32_model_path,
                               "Path to a fp32 model file."),
      tflite::Flag::CreateFlag("int8_graph", &int8_model_path,
                               "Path to a int8 model file."),
  };

  g_fp32_model_path = &fp32_model_path;
  g_int8_model_path = &int8_model_path;

  const bool parse_result =
      tflite::Flags::Parse(&argc, const_cast<const char**>(argv), flags);
  if (!parse_result) {
    std::cerr << tflite::Flags::Usage(argv[0], flags);
    return 1;
  }

  ::tflite::LogToStderr();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
