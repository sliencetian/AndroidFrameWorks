/*
 * Copyright (C) 2024 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <initializer_list>
#include <string>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <utils/Log.h>

// Note: tflite headers have warnings, so we disable them here.
#pragma clang diagnostic ignored "-Wsign-compare"
#pragma clang diagnostic ignored "-Wunused-parameter"

#include <flatbuffers/flexbuffers.h>
#include <tensorflow/lite/core/api/error_reporter.h>
#include <tensorflow/lite/delegates/gpu/delegate.h>
#include <tensorflow/lite/delegates/nnapi/nnapi_delegate.h>
#include <tensorflow/lite/interpreter.h>
#include <tensorflow/lite/kernels/register.h>
#include <tensorflow/lite/model.h>
#include <tensorflow/lite/schema/schema_conversion_utils.h>
#include <tensorflow/lite/schema/schema_generated.h>
#include <tensorflow/lite/version.h>

namespace tflite {
namespace {

using flatbuffers::Offset;
using flatbuffers::Vector;
using ::testing::ElementsAre;

// A tflite ErrorReporter that logs to logcat.
class LoggingErrorReporter : public tflite::ErrorReporter {
  public:
    int Report(const char* format, va_list args) override {
        return LOG_PRI_VA(ANDROID_LOG_ERROR, LOG_TAG, format, args);
    }
};

// The tflite stable API is through creating a FlatBuffer model with a specific
// schema version.
//
// Here we create a simple model that performs a 2D convolutional filter
// and test that it can be created and works.

// Adapted from external/tensorflow/tensorflow/lite/toco/tflite/import_test.cc

class ModelTest : public ::testing::Test {
  protected:
    ~ModelTest() { CleanUp(); }

    template <typename T>
    Offset<Vector<unsigned char>> CreateDataVector(const std::vector<T>& data) {
        return builder_.CreateVector(reinterpret_cast<const uint8_t*>(data.data()),
                                     sizeof(T) * data.size());
    }

    Offset<Vector<Offset<::tflite::Buffer>>> BuildBuffers() {
        auto input1881 = ::tflite::CreateBuffer(builder_);  // input buffer not assigned data.
        auto filter1331 = ::tflite::CreateBuffer(
                builder_, CreateDataVector<float>({1.f, 2.f, 1.f, 2.f, 4.f, 2.f, 1.f, 2.f, 1.f}));
        auto bias1 = ::tflite::CreateBuffer(builder_, CreateDataVector<float>({0.f}));
        auto output1881 = ::tflite::CreateBuffer(builder_);  // output buffer not assigned data.
        return builder_.CreateVector(
                std::vector<Offset<::tflite::Buffer>>({input1881, filter1331, bias1, output1881}));
    }

    Offset<Vector<Offset<::tflite::Tensor>>> BuildTensors() {
        auto input1881 = ::tflite::CreateTensor(builder_, builder_.CreateVector<int>({1, 8, 8, 1}),
                                                ::tflite::TensorType_FLOAT32, 0 /* buffer */,
                                                builder_.CreateString("tensor_input"));
        auto filter1331 = ::tflite::CreateTensor(builder_, builder_.CreateVector<int>({1, 3, 3, 1}),
                                                 ::tflite::TensorType_FLOAT32, 1 /* buffer */,
                                                 builder_.CreateString("tensor_filter"));
        auto bias1 = ::tflite::CreateTensor(builder_, builder_.CreateVector<int>({1}),
                                            ::tflite::TensorType_FLOAT32, 2 /* buffer */,
                                            builder_.CreateString("tensor_bias"));
        auto output1881 = ::tflite::CreateTensor(builder_, builder_.CreateVector<int>({1, 8, 8, 1}),
                                                 ::tflite::TensorType_FLOAT32, 3 /* buffer */,
                                                 builder_.CreateString("tensor_output"));
        return builder_.CreateVector(
                std::vector<Offset<::tflite::Tensor>>({input1881, filter1331, bias1, output1881}));
    }

    Offset<Vector<Offset<::tflite::OperatorCode>>> BuildOpCodes(
            std::initializer_list<::tflite::BuiltinOperator> op_codes) {
        std::vector<Offset<::tflite::OperatorCode>> op_codes_vector;
        for (auto op : op_codes) {
            op_codes_vector.push_back(::tflite::CreateOperatorCode(builder_, (int8_t)op, 0));
        }
        return builder_.CreateVector(op_codes_vector);
    }

    Offset<::tflite::Operator> BuildConv2DOperator(std::initializer_list<int> inputs,
                                                   std::initializer_list<int> outputs) {
        auto is = builder_.CreateVector<int>(inputs);
        if (inputs.size() == 0) is = 0;
        auto os = builder_.CreateVector<int>(outputs);
        if (outputs.size() == 0) os = 0;
        auto op = ::tflite::CreateOperator(
                builder_, 0 /* opcode_index */, is, os, ::tflite::BuiltinOptions_Conv2DOptions,
                ::tflite::CreateConv2DOptions(builder_, ::tflite::Padding_SAME, 1 /* stride_w */,
                                              1 /* stride_h */,
                                              ::tflite::ActivationFunctionType_NONE)
                        .Union());
        return op;
    }

    Offset<Vector<Offset<::tflite::Operator>>> BuildOperators(
            std::initializer_list<Offset<tflite::Operator>> operators) {
        std::vector<Offset<::tflite::Operator>> operators_vector;
        for (auto op : operators) {
            operators_vector.push_back(op);
        }
        return builder_.CreateVector(operators_vector);
    }

    Offset<Vector<Offset<::tflite::SubGraph>>> BuildSubGraphs(
            Offset<Vector<Offset<::tflite::Tensor>>> tensors,
            Offset<Vector<Offset<::tflite::Operator>>> operators, int num_sub_graphs = 1) {
        std::vector<int32_t> inputs = {0};
        std::vector<int32_t> outputs = {3};
        std::vector<Offset<::tflite::SubGraph>> v;
        for (int i = 0; i < num_sub_graphs; ++i) {
            v.push_back(::tflite::CreateSubGraph(builder_, tensors, builder_.CreateVector(inputs),
                                                 builder_.CreateVector(outputs), operators,
                                                 builder_.CreateString("subgraph")));
        }
        return builder_.CreateVector(v);
    }

    enum class DelegateType {
        kCpu,
        kTpu,
    };

    void BuildTestModel(DelegateType type) {
        delegate_type_ = type;

        // auto errorReporter = std::make_unique<LoggingErrorReporter>();

        auto buffers = BuildBuffers();
        auto tensors = BuildTensors();
        auto opcodes = BuildOpCodes({::tflite::BuiltinOperator_CONV_2D});
        auto conv2DOp = BuildConv2DOperator({0, 1, 2}, {3});
        auto operators = BuildOperators({conv2DOp});
        auto subgraphs = BuildSubGraphs(tensors, operators);
        auto description = builder_.CreateString("ModelTest");

        ::tflite::FinishModelBuffer(
                builder_, ::tflite::CreateModel(builder_, TFLITE_SCHEMA_VERSION, opcodes, subgraphs,
                                                description, buffers));

        input_model_ = ::tflite::GetModel(builder_.GetBufferPointer());

        ASSERT_NE(nullptr, input_model_);

        tflite::ops::builtin::BuiltinOpResolver resolver;
        tflite::InterpreterBuilder builder(input_model_, resolver);

        ASSERT_EQ(kTfLiteOk, builder(&interpreter_));
        ASSERT_NE(nullptr, interpreter_);

        TfLiteStatus delegate_status = kTfLiteOk;
        switch (type) {
            default:
            case DelegateType::kCpu:
                break;
            case DelegateType::kTpu:
                interpreter_->SetAllowFp16PrecisionForFp32(true);
                delegate_ = NnApiDelegate();  // singleton
                delegate_status = interpreter_->ModifyGraphWithDelegate(delegate_);
                EXPECT_EQ(kTfLiteOk, delegate_status);
                break;
        }
    }

    void CleanUp() {
        // reset the values.
        delegate_ = nullptr;
        input_model_ = nullptr;
        builder_.Clear();
        interpreter_.reset();
    }

    std::string asString() {
        return std::string(reinterpret_cast<char*>(builder_.GetBufferPointer()),
                           builder_.GetSize());
    }

    flatbuffers::FlatBufferBuilder builder_;
    const ::tflite::Model* input_model_ = nullptr;
    TfLiteDelegate* delegate_ = nullptr;
    std::unique_ptr<tflite::Interpreter> interpreter_;

    DelegateType delegate_type_ = DelegateType::kCpu;
};

TEST_F(ModelTest, BuildConv) {
    for (auto type : {DelegateType::kCpu, DelegateType::kTpu}) {
        for (float inputValue : {10.f, 11.f}) {
            BuildTestModel(type);

            interpreter_->AllocateTensors();
            TfLiteTensor* input = interpreter_->input_tensor(0);
            TfLiteTensor* output = interpreter_->output_tensor(0);

            input->data.f[0] = inputValue;
            TfLiteStatus invoke_status = interpreter_->Invoke();
            ASSERT_EQ(kTfLiteOk, invoke_status);

            // Result is the point impulse multiplied by the
            // tap value of the 3 x 3 filter (starting from center).
            EXPECT_EQ(inputValue * 4.f, output->data.f[0]);
            EXPECT_EQ(inputValue * 2.f, output->data.f[1]);
            EXPECT_EQ(0.f, output->data.f[2]);
            EXPECT_EQ(inputValue * 2.f, output->data.f[8]);
            EXPECT_EQ(inputValue * 1.f, output->data.f[9]);
            EXPECT_EQ(0.f, output->data.f[10]);
            CleanUp();
        }
    }
}

}  // namespace
}  // namespace tflite
