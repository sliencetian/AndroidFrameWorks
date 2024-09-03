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

#include <NeuralNetworks.h>
#include <android/log.h>
#include <android/sharedmem.h>
#include <audio_utils/float_test_utils.h>
#include <gtest/gtest.h>
#include <sys/mman.h>
#include <unistd.h>
#include <utils/Log.h>

#include <cmath>
#include <functional>
#include <numeric>
#include <string>
#include <vector>

#define FLOAT_EPISILON (1e-6)
#undef LOG_TAG
#define LOG_TAG "audio_nnapi_tests"

template <typename T>
static T product(const std::vector<T>& values) {
    return std::accumulate(values.begin(), values.end(), T(1), std::multiplies<>());
}

// In Android 35, NNAPI is deprecated b/283927643
//
// The deprecation hasn't made it to the developer's site:
// https://developer.android.com/ndk/reference/group/neural-networks
// External clients may bundle tflite themselves or access through
// play store services
// https://www.tensorflow.org/lite/android/play_services
//
// This test is taken from the Android NDK samples here:
// https://github.com/android/ndk-samples/blob/main/nn-samples/basic/src/main/cpp/simple_model.cpp

/**
 * AddMulModel
 *
 * Build up the hardcoded graph of tensor inputs to output.
 *
 * tensor0 ---+
 *            +--- ADD ---> intermediateOutput0 ---+
 * tensor1 ---+                                    |
 *                                                 +--- MUL---> output
 * tensor2 ---+                                    |
 *            +--- ADD ---> intermediateOutput1 ---+
 * tensor3 ---+
 *
 * Operands are a tensor specified by std::vector<uint32_t> dimensions
 * to CreateModel, and may be multidimensional.
 */

class AddMulModel {
  public:
    ~AddMulModel();

    bool CreateModel(const std::vector<uint32_t>& dimensions);
    bool Compute(float inputValue0, float inputValue1, float inputValue2, float inputValue3,
                 float* result);

  private:
    ANeuralNetworksModel* model_ = nullptr;
    ANeuralNetworksCompilation* compilation_ = nullptr;

    // For the purposes of member variables we use
    // "inputN" to correspond to "tensorN".
    //
    // We send input0 and input2 directly
    // and input1 and input3 through shared memory
    // which need declaration here.

    ANeuralNetworksMemory* memoryInput1_ = nullptr;
    ANeuralNetworksMemory* memoryInput3_ = nullptr;
    ANeuralNetworksMemory* memoryOutput_ = nullptr;

    int inputTensor1Fd_ = -1;
    int inputTensor3Fd_ = -1;
    int outputTensorFd_ = -1;

    float* inputTensor1Ptr_ = nullptr;
    float* inputTensor3Ptr_ = nullptr;
    float* outputTensorPtr_ = nullptr;

    uint32_t tensorSize_ = 0;
};

AddMulModel::~AddMulModel() {
    ANeuralNetworksCompilation_free(compilation_);
    ANeuralNetworksModel_free(model_);
    ANeuralNetworksMemory_free(memoryInput1_);
    ANeuralNetworksMemory_free(memoryInput3_);
    ANeuralNetworksMemory_free(memoryOutput_);

    if (inputTensor1Ptr_ != nullptr) {
        munmap(inputTensor1Ptr_, tensorSize_ * sizeof(float));
        inputTensor1Ptr_ = nullptr;
    }
    if (inputTensor3Ptr_ != nullptr) {
        munmap(inputTensor3Ptr_, tensorSize_ * sizeof(float));
        inputTensor3Ptr_ = nullptr;
    }
    if (outputTensorPtr_ != nullptr) {
        munmap(outputTensorPtr_, tensorSize_ * sizeof(float));
        outputTensorPtr_ = nullptr;
    }

    if (inputTensor1Fd_ != -1) close(inputTensor1Fd_);
    if (inputTensor3Fd_ != -1) close(inputTensor3Fd_);
    if (outputTensorFd_ != -1) close(outputTensorFd_);
}

/**
 * Create a graph that consists of three operations: two additions and a
 * multiplication.
 * The sums created by the additions are the inputs to the multiplication. In
 * essence, we are creating a graph that computes:
 *        (tensor0 + tensor1) * (tensor2 + tensor3).
 *
 * tensor0 ---+
 *            +--- ADD ---> intermediateOutput0 ---+
 * tensor1 ---+                                    |
 *                                                 +--- MUL---> output
 * tensor2 ---+                                    |
 *            +--- ADD ---> intermediateOutput1 ---+
 * tensor3 ---+
 *
 * All four tensors are inputs to the model. Their
 * values will be provided when we execute the model. These values can change
 * from execution to execution.
 *
 * Besides the four input tensors, an optional fused activation function can
 * also be defined for ADD and MUL. In this example, we'll simply set it to
 * NONE.
 *
 * The graph then has 8 operands:
 *  - 4 tensors that are inputs to the model. These are fed to the two
 *      ADD operations.
 *  - 1 fuse activation operand reused for the ADD operations and the MUL
 * operation.
 *  - 2 intermediate tensors, representing outputs of the ADD operations and
 * inputs to the MUL operation.
 *  - 1 model output.
 *
 * @return true for success, false otherwise
 */
bool AddMulModel::CreateModel(const std::vector<uint32_t>& dimensions) {
    int32_t status;

    // Create the ANeuralNetworksModel handle.
    status = ANeuralNetworksModel_create(&model_);
    if (status != ANEURALNETWORKS_NO_ERROR) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "ANeuralNetworksModel_create failed");
        return false;
    }

    // everything else is computed.
    tensorSize_ = product(dimensions);

    ANeuralNetworksOperandType float32TensorType{
            .type = ANEURALNETWORKS_TENSOR_FLOAT32,
            .dimensionCount = static_cast<uint32_t>(dimensions.size()),
            .dimensions = dimensions.data(),
            .scale = 0.0f,
            .zeroPoint = 0,
    };
    ANeuralNetworksOperandType scalarInt32Type{
            .type = ANEURALNETWORKS_INT32,
            .dimensionCount = 0,
            .dimensions = nullptr,
            .scale = 0.0f,
            .zeroPoint = 0,
    };

    /**
     * Add operands and operations to construct the model.
     *
     * Operands are implicitly identified by the order in which they are added to
     * the model, starting from 0.
     *
     * These indexes are not returned by the model_addOperand call. The
     * application must manage these values. Here, we use opIdx to do the
     * bookkeeping.
     */
    uint32_t opIdx = 0;

    // We first add the operand for the NONE activation function, and set its
    // value to ANEURALNETWORKS_FUSED_NONE.
    // This constant scalar operand will be used for all 3 operations.
    status = ANeuralNetworksModel_addOperand(model_, &scalarInt32Type);
    uint32_t fusedActivationFuncNone = opIdx++;
    if (status != ANEURALNETWORKS_NO_ERROR) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                            "ANeuralNetworksModel_addOperand failed for operand (%d)",
                            fusedActivationFuncNone);
        return false;
    }

    FuseCode fusedActivationCodeValue = ANEURALNETWORKS_FUSED_NONE;
    status = ANeuralNetworksModel_setOperandValue(model_, fusedActivationFuncNone,
                                                  &fusedActivationCodeValue,
                                                  sizeof(fusedActivationCodeValue));
    if (status != ANEURALNETWORKS_NO_ERROR) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                            "ANeuralNetworksModel_setOperandValue failed for operand (%d)",
                            fusedActivationFuncNone);
        return false;
    }

    // Add operands for the tensors.

    // Add 4 input tensors.
    status = ANeuralNetworksModel_addOperand(model_, &float32TensorType);
    uint32_t tensor0 = opIdx++;
    if (status != ANEURALNETWORKS_NO_ERROR) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                            "ANeuralNetworksModel_addOperand failed for operand (%d)", tensor0);
        return false;
    }

    status = ANeuralNetworksModel_addOperand(model_, &float32TensorType);
    uint32_t tensor1 = opIdx++;
    if (status != ANEURALNETWORKS_NO_ERROR) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                            "ANeuralNetworksModel_addOperand failed for operand (%d)", tensor1);
        return false;
    }

    status = ANeuralNetworksModel_addOperand(model_, &float32TensorType);
    uint32_t tensor2 = opIdx++;
    if (status != ANEURALNETWORKS_NO_ERROR) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                            "ANeuralNetworksModel_addOperand failed for operand (%d)", tensor2);
        return false;
    }

    status = ANeuralNetworksModel_addOperand(model_, &float32TensorType);
    uint32_t tensor3 = opIdx++;
    if (status != ANEURALNETWORKS_NO_ERROR) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                            "ANeuralNetworksModel_addOperand failed for operand (%d)", tensor3);
        return false;
    }

    // intermediateOutput0 is the output of the first ADD operation.
    // Its value is computed during execution.
    status = ANeuralNetworksModel_addOperand(model_, &float32TensorType);
    uint32_t intermediateOutput0 = opIdx++;
    if (status != ANEURALNETWORKS_NO_ERROR) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                            "ANeuralNetworksModel_addOperand failed for operand (%d)",
                            intermediateOutput0);
        return false;
    }

    // intermediateOutput1 is the output of the second ADD operation.
    // Its value is computed during execution.
    status = ANeuralNetworksModel_addOperand(model_, &float32TensorType);
    uint32_t intermediateOutput1 = opIdx++;
    if (status != ANEURALNETWORKS_NO_ERROR) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                            "ANeuralNetworksModel_addOperand failed for operand (%d)",
                            intermediateOutput1);
        return false;
    }

    // multiplierOutput is the output of the MUL operation.
    // Its value will be computed during execution.
    status = ANeuralNetworksModel_addOperand(model_, &float32TensorType);
    uint32_t multiplierOutput = opIdx++;
    if (status != ANEURALNETWORKS_NO_ERROR) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                            "ANeuralNetworksModel_addOperand failed for operand (%d)",
                            multiplierOutput);
        return false;
    }

    // Add the first ADD operation.
    std::vector<uint32_t> add1InputOperands = {
            tensor0,
            tensor1,
            fusedActivationFuncNone,
    };
    status =
            ANeuralNetworksModel_addOperation(model_, ANEURALNETWORKS_ADD, add1InputOperands.size(),
                                              add1InputOperands.data(), 1, &intermediateOutput0);
    if (status != ANEURALNETWORKS_NO_ERROR) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                            "ANeuralNetworksModel_addOperation failed for ADD_1");
        return false;
    }

    // Add the second ADD operation.
    // Note the fusedActivationFuncNone is used again.
    std::vector<uint32_t> add2InputOperands = {
            tensor2,
            tensor3,
            fusedActivationFuncNone,
    };
    status =
            ANeuralNetworksModel_addOperation(model_, ANEURALNETWORKS_ADD, add2InputOperands.size(),
                                              add2InputOperands.data(), 1, &intermediateOutput1);
    if (status != ANEURALNETWORKS_NO_ERROR) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                            "ANeuralNetworksModel_addOperation failed for ADD_2");
        return false;
    }

    // Add the MUL operation.
    // Note that intermediateOutput0 and intermediateOutput1 are specified
    // as inputs to the operation.
    std::vector<uint32_t> mulInputOperands = {intermediateOutput0, intermediateOutput1,
                                              fusedActivationFuncNone};
    status = ANeuralNetworksModel_addOperation(model_, ANEURALNETWORKS_MUL, mulInputOperands.size(),
                                               mulInputOperands.data(), 1, &multiplierOutput);
    if (status != ANEURALNETWORKS_NO_ERROR) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                            "ANeuralNetworksModel_addOperation failed for MUL");
        return false;
    }

    // Identify the input and output tensors to the model.
    // Inputs: {tensor0, tensor1, tensor2, tensor3}
    // Outputs: {multiplierOutput}
    std::vector<uint32_t> modelInputOperands = {
            tensor0,
            tensor1,
            tensor2,
            tensor3,
    };
    status = ANeuralNetworksModel_identifyInputsAndOutputs(
            model_, modelInputOperands.size(), modelInputOperands.data(), 1, &multiplierOutput);
    if (status != ANEURALNETWORKS_NO_ERROR) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                            "ANeuralNetworksModel_identifyInputsAndOutputs failed");
        return false;
    }

    // Required for TPU
    status = ANeuralNetworksModel_relaxComputationFloat32toFloat16(model_, true);
    if (status != ANEURALNETWORKS_NO_ERROR) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                            "ANeuralNetworksModel_relaxComputationFloat32toFloat16 failed");
        return false;
    }

    // Finish constructing the model.
    // The values of constant and intermediate operands cannot be altered after
    // the finish function is called.
    status = ANeuralNetworksModel_finish(model_);
    if (status != ANEURALNETWORKS_NO_ERROR) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "ANeuralNetworksModel_finish failed");
        return false;
    }

    // Create the ANeuralNetworksCompilation object for the constructed model.
    status = ANeuralNetworksCompilation_create(model_, &compilation_);
    if (status != ANEURALNETWORKS_NO_ERROR) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "ANeuralNetworksCompilation_create failed");
        return false;
    }

    // Set the preference for the compilation, so that the runtime and drivers
    // can make better decisions.
    // Here we prefer to get the answer quickly, so we choose
    // ANEURALNETWORKS_PREFER_FAST_SINGLE_ANSWER.
    status = ANeuralNetworksCompilation_setPreference(compilation_,
                                                      ANEURALNETWORKS_PREFER_FAST_SINGLE_ANSWER);
    if (status != ANEURALNETWORKS_NO_ERROR) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                            "ANeuralNetworksCompilation_setPreference failed");
        return false;
    }

    // Finish the compilation.
    status = ANeuralNetworksCompilation_finish(compilation_);
    if (status != ANEURALNETWORKS_NO_ERROR) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "ANeuralNetworksCompilation_finish failed");
        return false;
    }

    inputTensor1Fd_ = ASharedMemory_create("input1", tensorSize_ * sizeof(float));
    EXPECT_NE(-1, inputTensor1Fd_);
    inputTensor1Ptr_ =
            reinterpret_cast<float*>(mmap(nullptr, tensorSize_ * sizeof(float),
                                          PROT_READ | PROT_WRITE, MAP_SHARED, inputTensor1Fd_, 0));

    EXPECT_NE(MAP_FAILED, (void*)inputTensor1Ptr_);

    status = ANeuralNetworksMemory_createFromFd(tensorSize_ * sizeof(float), PROT_READ,
                                                inputTensor1Fd_, 0, &memoryInput1_);
    if (status != ANEURALNETWORKS_NO_ERROR) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                            "ANeuralNetworksMemory_createFromFd failed for Input1");
        return false;
    }

    inputTensor3Fd_ = ASharedMemory_create("input3", tensorSize_ * sizeof(float));
    EXPECT_NE(-1, inputTensor3Fd_);
    inputTensor3Ptr_ =
            reinterpret_cast<float*>(mmap(nullptr, tensorSize_ * sizeof(float),
                                          PROT_READ | PROT_WRITE, MAP_SHARED, inputTensor3Fd_, 0));
    EXPECT_NE(MAP_FAILED, (void*)inputTensor3Ptr_);
    status = ANeuralNetworksMemory_createFromFd(tensorSize_ * sizeof(float), PROT_READ,
                                                inputTensor3Fd_, 0, &memoryInput3_);
    if (status != ANEURALNETWORKS_NO_ERROR) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                            "ANeuralNetworksMemory_createFromFd failed for Input3");
        return false;
    }

    // Set the output tensor that will be filled by executing the model.
    // We use shared memory here to minimize the copies needed for getting the
    // output data.
    outputTensorFd_ = ASharedMemory_create("output", tensorSize_ * sizeof(float));
    EXPECT_NE(-1, outputTensorFd_);

    outputTensorPtr_ = reinterpret_cast<float*>(
            mmap(nullptr, tensorSize_ * sizeof(float), PROT_READ, MAP_SHARED, outputTensorFd_, 0));
    EXPECT_NE(MAP_FAILED, (void*)outputTensorPtr_);
    status = ANeuralNetworksMemory_createFromFd(tensorSize_ * sizeof(float), PROT_READ | PROT_WRITE,
                                                outputTensorFd_, 0, &memoryOutput_);
    if (status != ANEURALNETWORKS_NO_ERROR) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                            "ANeuralNetworksMemory_createFromFd failed for Output");
        return false;
    }

    return true;
}

/**
 * Compute with the given input data.
 *
 * @param inputValue0 value to fill tensor0
 * @param inputValue1 value to fill tensor1
 * @param inputValue2 value to fill tensor2
 * @param inputValue3 value to fill tensor3
 * @param result is the output value
 * @return true on success
 */
bool AddMulModel::Compute(float inputValue0, float inputValue1, float inputValue2,
                          float inputValue3, float* result) {
    if (!result) {
        return false;
    }

    // Create an ANeuralNetworksExecution object from the compiled model.
    // Note:
    //   1. All the input and output data are tied to the ANeuralNetworksExecution
    //   object.
    //   2. Multiple concurrent execution instances could be created from the same
    //   compiled model.
    // This sample only uses one execution of the compiled model.
    ANeuralNetworksExecution* execution;
    int32_t status = ANeuralNetworksExecution_create(compilation_, &execution);
    if (status != ANEURALNETWORKS_NO_ERROR) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "ANeuralNetworksExecution_create failed");
        return false;
    }

    // Set all the elements of the first input tensor (tensor0) to the same value
    // as inputValue. It's not a realistic example but it shows how to pass a
    // small tensor to an execution.
    std::vector<float> inputTensor0(tensorSize_, inputValue0);

    // Tell the execution to associate inputTensor0 to the first of the model
    // inputs. Note that the index "0" here means the first operand of the
    // modelInput list.
    status = ANeuralNetworksExecution_setInput(execution, 0, nullptr, inputTensor0.data(),
                                               tensorSize_ * sizeof(float));
    if (status != ANEURALNETWORKS_NO_ERROR) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                            "ANeuralNetworksExecution_setInput failed for input1");
        return false;
    }

    // ALTERNATIVELY
    // Set the values of the second input operand (tensor1) to be inputValue1.
    // In reality, the values in the shared memory region will be manipulated by
    // other modules or processes.

    for (size_t i = 0; i < tensorSize_; i++) {
        inputTensor1Ptr_[i] = inputValue1;
    }
    status = ANeuralNetworksExecution_setInputFromMemory(execution, 1, nullptr, memoryInput1_, 0,
                                                         tensorSize_ * sizeof(float));
    if (status != ANEURALNETWORKS_NO_ERROR) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                            "ANeuralNetworksExecution_setInputFromMemory failed for input1");
        return false;
    }

    // Set all the elements of the third input tensor (tensor2) to the same value
    // as inputValue2. It's not a realistic example but it shows how to pass a
    // small tensor to an execution.
    std::vector<float> inputTensor2(tensorSize_, inputValue2);

    status = ANeuralNetworksExecution_setInput(execution, 2, nullptr, inputTensor2.data(),
                                               tensorSize_ * sizeof(float));
    if (status != ANEURALNETWORKS_NO_ERROR) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                            "ANeuralNetworksExecution_setInput failed for input1");
        return false;
    }

    // ALTERNATIVELY
    // Set the values of the second input operand (tensor1) to be inputValue1.
    // In reality, the values in the shared memory region will be manipulated by
    // other modules or processes.

    for (size_t i = 0; i < tensorSize_; i++) {
        inputTensor3Ptr_[i] = inputValue3;
    }

    status = ANeuralNetworksExecution_setInputFromMemory(execution, 3, nullptr, memoryInput3_, 0,
                                                         tensorSize_ * sizeof(float));
    if (status != ANEURALNETWORKS_NO_ERROR) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                            "ANeuralNetworksExecution_setInputFromMemory failed for input3");
        return false;
    }

    // Set the output tensor that will be filled by executing the model.
    // We use shared memory here to minimize the copies needed for getting the
    // output data.

    status = ANeuralNetworksExecution_setOutputFromMemory(execution, 0, nullptr, memoryOutput_, 0,
                                                          tensorSize_ * sizeof(float));
    if (status != ANEURALNETWORKS_NO_ERROR) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                            "ANeuralNetworksExecution_setOutputFromMemory failed for output");
        return false;
    }

    // Start the execution of the model.
    // Note that the execution here is asynchronous, and an ANeuralNetworksEvent
    // object will be created to monitor the status of the execution.
    ANeuralNetworksEvent* event = nullptr;
    status = ANeuralNetworksExecution_startCompute(execution, &event);
    if (status != ANEURALNETWORKS_NO_ERROR) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                            "ANeuralNetworksExecution_startCompute failed");
        return false;
    }

    // Wait until the completion of the execution. This could be done on a
    // different thread. By waiting immediately, we effectively make this a
    // synchronous call.
    status = ANeuralNetworksEvent_wait(event);
    if (status != ANEURALNETWORKS_NO_ERROR) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "ANeuralNetworksEvent_wait failed");
        return false;
    }

    ANeuralNetworksEvent_free(event);
    ANeuralNetworksExecution_free(execution);

    // Validate the results.
    const float goldenRef = (inputValue0 + inputValue1) * (inputValue2 + inputValue3);
    float* outputTensorPtr = reinterpret_cast<float*>(
            mmap(nullptr, tensorSize_ * sizeof(float), PROT_READ, MAP_SHARED, outputTensorFd_, 0));
    for (size_t idx = 0; idx < tensorSize_; idx++) {
        float delta = outputTensorPtr[idx] - goldenRef;
        delta = (delta < 0.0f) ? (-delta) : delta;
        if (delta > FLOAT_EPISILON) {
            __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                                "Output computation Error: output0(%f), delta(%f) @ idx(%zu)",
                                outputTensorPtr[0], delta, idx);
        }
    }
    *result = outputTensorPtr[0];
    return true;
}

TEST(audio_nnapi_tests, AddMulModel) {
    AddMulModel model;
    float result = 0.f;
    EXPECT_EQ(true, model.CreateModel({5, 10, 2, 2}));  // 5x10x2x2 tensor
    EXPECT_EQ(true, model.Compute(10.f, 11.f, 12.f, 13.f, &result));
    EXPECT_EQ((10.f + 11.f) * (12.f + 13.f), result);

    EXPECT_EQ(true, model.Compute(5.f, 6.f, 7.f, 8.f, &result));
    EXPECT_EQ((5.f + 6.f) * (7.f + 8.f), result);

#if 0
    // Enable this for precision testing.

    // Precision test for CPU
    // The ARM does subnormals
    //
    // single precision float
    EXPECT_EQ(127, android::audio_utils::test::test_max_exponent<float>());
    EXPECT_EQ(-149, android::audio_utils::test::test_min_exponent<float>());
    EXPECT_EQ(23, android::audio_utils::test::test_mantissa<float>());

    // double precision float
    EXPECT_EQ(1023, android::audio_utils::test::test_max_exponent<double>());
    EXPECT_EQ(-1074, android::audio_utils::test::test_min_exponent<double>());
    EXPECT_EQ(52, android::audio_utils::test::test_mantissa<double>());

    // Precision test for Edge TPU
    // Is it float16 or bfloat16?
    //
    // Edge TPU appears to be float16 at the moment, with one bit
    // of subnormal.
    //
    // max_exponent: 15
    // min_exponent: -15
    // mantissa: 10
    //
    // functor to double input.
    auto twice = [&model](float x) {
      float result = 0;
      model.Compute(x, x, 1.f, 0.f, &result);
      return result;
    };
    EXPECT_EQ(15, android::audio_utils::test::test_max_exponent<float>(twice));

    // functor to halve input.
    auto half = [&model](float x) {
      float result = 0;
      model.Compute(x, 0, 0.5f, 0.f, &result);
      return result;
    };
    EXPECT_EQ(-15, android::audio_utils::test::test_min_exponent<float>(half));

    // functor to increment input.
    auto inc = [&model](float x) {
      float result = 0;
      model.Compute(x, 1.f, 1.f, 0.f, &result);
      return result;
    };
    EXPECT_EQ(10, android::audio_utils::test::test_mantissa<float>(inc));
#endif
}
