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

/**
 * Conv2DModel
 *
 * Build up the hardcoded graph of
 *
 * input  ---+
 *            +--- CONV2D ---> output
 * filter ---+
 *
 * Operands are given by the dimensions of the input and filter tensor.
 *
 *  input: A 4-D tensor, of shape [batches, height, width, depth_in],
 *      specifying the input.
 *      Since NNAPI feature level 3, zero batches is supported for this tensor.
 * * filter: A 4-D tensor, of shape
 *      [depth_out, filter_height, filter_width, depth_in], specifying the
 *      filter.
 * * bias: A 1-D tensor, of shape [depth_out], specifying the bias.
 *
 *  output 4-D tensor, of shape
 *      [batches, out_height, out_width, depth_out].
 */

class Conv2DModel {
  public:
    ~Conv2DModel();

    bool CreateModel(uint32_t batches, uint32_t height, uint32_t width, uint32_t filter_height,
                     uint32_t filter_width, uint32_t depth_in, uint32_t depth_out, float biasValue);

    bool Compute(float inputValue, float filterValue, float* result);

  private:
    ANeuralNetworksModel* model_ = nullptr;
    ANeuralNetworksCompilation* compilation_ = nullptr;

    ANeuralNetworksMemory* memoryInput_ = nullptr;
    ANeuralNetworksMemory* memoryFilter_ = nullptr;
    ANeuralNetworksMemory* memoryOutput_ = nullptr;

    uint32_t inputSize_ = 0;
    uint32_t filterSize_ = 0;
    uint32_t biasSize_ = 0;
    uint32_t outputSize_ = 0;

    std::vector<uint32_t> inputDimensions_;
    std::vector<uint32_t> filterDimensions_;
    std::vector<uint32_t> outputDimensions_;

    int inputTensorFd_ = -1;
    int filterTensorFd_ = -1;
    int outputTensorFd_ = -1;

    float* inputTensorPtr_ = nullptr;
    float* filterTensorPtr_ = nullptr;
    float* outputTensorPtr_ = nullptr;
};

Conv2DModel::~Conv2DModel() {
    ANeuralNetworksCompilation_free(compilation_);
    ANeuralNetworksModel_free(model_);
    ANeuralNetworksMemory_free(memoryInput_);
    ANeuralNetworksMemory_free(memoryFilter_);
    ANeuralNetworksMemory_free(memoryOutput_);

    if (inputTensorPtr_ != nullptr) {
        munmap(inputTensorPtr_, inputSize_ * sizeof(float));
        inputTensorPtr_ = nullptr;
    }
    if (filterTensorPtr_ != nullptr) {
        munmap(filterTensorPtr_, filterSize_ * sizeof(float));
        filterTensorPtr_ = nullptr;
    }
    if (outputTensorPtr_ != nullptr) {
        munmap(outputTensorPtr_, outputSize_ * sizeof(float));
        outputTensorPtr_ = nullptr;
    }

    if (inputTensorFd_ != -1) close(inputTensorFd_);
    if (filterTensorFd_ != -1) close(filterTensorFd_);
    if (outputTensorFd_ != -1) close(outputTensorFd_);
}

/**
 * Create a graph that consists of a 2D convolution
 *
 * input  ---+
 *            +--- CONV2D ---> output
 * filter ---+
 *
 * 2 tensors are provided as input
 *
 *  input: A 4-D tensor, of shape [batches, height, width, depth_in],
 *      specifying the input.
 *      Since NNAPI feature level 3, zero batches is supported for this tensor.
 *  filter: A 4-D tensor, of shape
 *      [depth_out, filter_height, filter_width, depth_in], specifying the
 *      filter.
 *
 *  Note that bias must be fixed in the model for NNAPI acceleration on TPU.
 *  bias: A 1-D tensor, of shape [depth_out], specifying the bias.
 *
 *  output 4-D tensor, of shape
 *      [batches, out_height, out_width, depth_out].
 *
 * @param batches the number of samples to operate on
 * @param height of the data input
 * @param width of the data input
 * @param filter_height
 * @param filter_width
 * @param depth_in channels of input
 * @param depth_out channels of output
 * @return true for success, false otherwise
 */
bool Conv2DModel::CreateModel(uint32_t batches, uint32_t height, uint32_t width,
                              uint32_t filter_height, uint32_t filter_width, uint32_t depth_in,
                              uint32_t depth_out, float biasValue) {
    int32_t status;

    inputDimensions_ = std::vector<uint32_t>{batches, height, width, depth_in};
    filterDimensions_ = std::vector<uint32_t>{depth_out, filter_height, filter_width, depth_in};
    outputDimensions_ = std::vector<uint32_t>{batches, height, width, depth_out};

    inputSize_ = product(inputDimensions_);

    filterSize_ = product(filterDimensions_);

    outputSize_ = product(outputDimensions_);

    biasSize_ = depth_out;

    // Create the ANeuralNetworksModel handle.
    status = ANeuralNetworksModel_create(&model_);
    if (status != ANEURALNETWORKS_NO_ERROR) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "ANeuralNetworksModel_create failed");
        return false;
    }

    ANeuralNetworksOperandType inputTensorType{
            .type = ANEURALNETWORKS_TENSOR_FLOAT32,
            .dimensionCount = static_cast<uint32_t>(inputDimensions_.size()),
            .dimensions = inputDimensions_.data(),
            .scale = 0.0f,
            .zeroPoint = 0,
    };

    ANeuralNetworksOperandType filterTensorType{
            .type = ANEURALNETWORKS_TENSOR_FLOAT32,
            .dimensionCount = static_cast<uint32_t>(filterDimensions_.size()),
            .dimensions = filterDimensions_.data(),
            .scale = 0.0f,
            .zeroPoint = 0,
    };

    ANeuralNetworksOperandType biasTensorType{
            .type = ANEURALNETWORKS_TENSOR_FLOAT32,
            .dimensionCount = 1,
            .dimensions = &biasSize_,
            .scale = 0.0f,
            .zeroPoint = 0,
    };

    ANeuralNetworksOperandType outputTensorType{
            .type = ANEURALNETWORKS_TENSOR_FLOAT32,
            .dimensionCount = static_cast<uint32_t>(outputDimensions_.size()),
            .dimensions = outputDimensions_.data(),
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

    // Add the operand for the NONE activation function, and set its
    // value to ANEURALNETWORKS_FUSED_NONE.
    status = ANeuralNetworksModel_addOperand(model_, &scalarInt32Type);
    uint32_t fusedActivationFuncNoneOp = opIdx++;
    if (status != ANEURALNETWORKS_NO_ERROR) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                            "ANeuralNetworksModel_addOperand failed for operand (%d)",
                            fusedActivationFuncNoneOp);
        return false;
    }

    FuseCode fusedActivationCodeValue = ANEURALNETWORKS_FUSED_NONE;
    status = ANeuralNetworksModel_setOperandValue(model_, fusedActivationFuncNoneOp,
                                                  &fusedActivationCodeValue,
                                                  sizeof(fusedActivationCodeValue));
    if (status != ANEURALNETWORKS_NO_ERROR) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                            "ANeuralNetworksModel_setOperandValue failed for operand (%d)",
                            fusedActivationFuncNoneOp);
        return false;
    }

    // Add the operand for the padding code
    // value to ANEURALNETWORKS_PADDING_SAME.
    status = ANeuralNetworksModel_addOperand(model_, &scalarInt32Type);
    uint32_t paddingCodeSameOp = opIdx++;
    if (status != ANEURALNETWORKS_NO_ERROR) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                            "ANeuralNetworksModel_addOperand failed for operand (%d)",
                            paddingCodeSameOp);
        return false;
    }

    PaddingCode paddingCodeValue = ANEURALNETWORKS_PADDING_SAME;
    status = ANeuralNetworksModel_setOperandValue(model_, paddingCodeSameOp, &paddingCodeValue,
                                                  sizeof(paddingCodeValue));
    if (status != ANEURALNETWORKS_NO_ERROR) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                            "ANeuralNetworksModel_setOperandValue failed for operand (%d)",
                            paddingCodeSameOp);
        return false;
    }

    // Add the operand for one
    status = ANeuralNetworksModel_addOperand(model_, &scalarInt32Type);
    uint32_t oneOp = opIdx++;
    if (status != ANEURALNETWORKS_NO_ERROR) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                            "ANeuralNetworksModel_addOperand failed for operand (%d)", oneOp);
        return false;
    }

    int32_t one = 1;
    status = ANeuralNetworksModel_setOperandValue(model_, oneOp, &one, sizeof(one));
    if (status != ANEURALNETWORKS_NO_ERROR) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                            "ANeuralNetworksModel_setOperandValue failed for operand (%d)", oneOp);
        return false;
    }

    // Add operands for the tensors.
    status = ANeuralNetworksModel_addOperand(model_, &inputTensorType);
    uint32_t inputOp = opIdx++;
    if (status != ANEURALNETWORKS_NO_ERROR) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                            "ANeuralNetworksModel_addOperand failed for operand (%d)", inputOp);
        return false;
    }

    status = ANeuralNetworksModel_addOperand(model_, &filterTensorType);
    uint32_t filterOp = opIdx++;
    if (status != ANEURALNETWORKS_NO_ERROR) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                            "ANeuralNetworksModel_addOperand failed for operand (%d)", filterOp);
        return false;
    }

    status = ANeuralNetworksModel_addOperand(model_, &biasTensorType);
    uint32_t biasOp = opIdx++;
    if (status != ANEURALNETWORKS_NO_ERROR) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                            "ANeuralNetworksModel_addOperand failed for operand (%d)", biasOp);
        return false;
    }

    // A bias value that isn't constant will prevent acceleration on TPU.
    std::vector<float> biases(biasSize_, biasValue);
    status = ANeuralNetworksModel_setOperandValue(model_, biasOp, biases.data(),
                                                  biases.size() * sizeof(float));
    if (status != ANEURALNETWORKS_NO_ERROR) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                            "ANeuralNetworksModel_setOperandValue failed for operand (%d)", biasOp);
        return false;
    }

    status = ANeuralNetworksModel_addOperand(model_, &outputTensorType);
    uint32_t outputOp = opIdx++;
    if (status != ANEURALNETWORKS_NO_ERROR) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                            "ANeuralNetworksModel_addOperand failed for operand (%d)", outputOp);
        return false;
    }

    // Add the CONV2D operation.
    std::vector<uint32_t> addInputOperands = {
            inputOp, filterOp, biasOp, paddingCodeSameOp, oneOp, oneOp, fusedActivationFuncNoneOp,
    };
    status = ANeuralNetworksModel_addOperation(model_, ANEURALNETWORKS_CONV_2D,
                                               addInputOperands.size(), addInputOperands.data(), 1,
                                               &outputOp);
    if (status != ANEURALNETWORKS_NO_ERROR) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                            "ANeuralNetworksModel_addOperation failed for CONV2D");
        return false;
    }

    // Identify the input and output tensors to the model.
    std::vector<uint32_t> modelInputOperands = {
            inputOp,
            filterOp,
    };
    status = ANeuralNetworksModel_identifyInputsAndOutputs(model_, modelInputOperands.size(),
                                                           modelInputOperands.data(), 1, &outputOp);
    if (status != ANEURALNETWORKS_NO_ERROR) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                            "ANeuralNetworksModel_identifyInputsAndOutputs failed");
        return false;
    }

    // Use of Float16 is required for TPU
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

    inputTensorFd_ = ASharedMemory_create("input", inputSize_ * sizeof(float));
    EXPECT_NE(-1, inputTensorFd_);
    inputTensorPtr_ =
            reinterpret_cast<float*>(mmap(nullptr, inputSize_ * sizeof(float),
                                          PROT_READ | PROT_WRITE, MAP_SHARED, inputTensorFd_, 0));
    EXPECT_NE(MAP_FAILED, (void*)inputTensorPtr_);
    status = ANeuralNetworksMemory_createFromFd(inputSize_ * sizeof(float), PROT_READ,
                                                inputTensorFd_, 0, &memoryInput_);
    if (status != ANEURALNETWORKS_NO_ERROR) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                            "ANeuralNetworksMemory_createFromFd failed for input");
        return false;
    }

    filterTensorFd_ = ASharedMemory_create("filter", filterSize_ * sizeof(float));
    EXPECT_NE(-1, filterTensorFd_);
    filterTensorPtr_ =
            reinterpret_cast<float*>(mmap(nullptr, filterSize_ * sizeof(float),
                                          PROT_READ | PROT_WRITE, MAP_SHARED, filterTensorFd_, 0));
    EXPECT_NE(MAP_FAILED, (void*)filterTensorPtr_);
    status = ANeuralNetworksMemory_createFromFd(filterSize_ * sizeof(float), PROT_READ,
                                                filterTensorFd_, 0, &memoryFilter_);
    if (status != ANEURALNETWORKS_NO_ERROR) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                            "ANeuralNetworksMemory_createFromFd failed for filter");
        return false;
    }

    outputTensorFd_ = ASharedMemory_create("output", outputSize_ * sizeof(float));
    EXPECT_NE(-1, outputTensorFd_);
    outputTensorPtr_ = reinterpret_cast<float*>(
            mmap(nullptr, outputSize_ * sizeof(float), PROT_READ, MAP_SHARED, outputTensorFd_, 0));
    EXPECT_NE(MAP_FAILED, (void*)outputTensorPtr_);

    status = ANeuralNetworksMemory_createFromFd(outputSize_ * sizeof(float), PROT_READ | PROT_WRITE,
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
 * @param inputValue to fill the input data tensor
 * @param filterValue to fill the filter tensor
 * @return true on success
 *    result is the output value.
 */
bool Conv2DModel::Compute(float inputValue, float filterValue, float* result) {
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

    for (size_t i = 0; i < inputSize_; i++) {
        inputTensorPtr_[i] = inputValue;
    }

    status = ANeuralNetworksExecution_setInputFromMemory(execution, 0, nullptr, memoryInput_, 0,
                                                         inputSize_ * sizeof(float));
    if (status != ANEURALNETWORKS_NO_ERROR) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                            "ANeuralNetworksExecution_setInputFromMemory failed for input");
        return false;
    }

    for (size_t i = 0; i < filterSize_; i++) {
        filterTensorPtr_[i] = filterValue;
    }

    status = ANeuralNetworksExecution_setInputFromMemory(execution, 1, nullptr, memoryFilter_, 0,
                                                         filterSize_ * sizeof(float));
    if (status != ANEURALNETWORKS_NO_ERROR) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                            "ANeuralNetworksExecution_setInputFromMemory failed for filter");
        return false;
    }

    // Set the output tensor that will be filled by executing the model.
    // We use shared memory here to minimize the copies needed for getting the
    // output data.
    status = ANeuralNetworksExecution_setOutputFromMemory(execution, 0, nullptr, memoryOutput_, 0,
                                                          outputSize_ * sizeof(float));
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
    *result = outputTensorPtr_[0];
    return true;
}

TEST(audio_nnapi_tests, Conv2DModel) {
    Conv2DModel model;
    float result = 0.f;
    EXPECT_EQ(true, model.CreateModel(1 /* batches */, 16 /* height */, 16 /* width */,
                                      3 /* filter_height */, 3 /* filter_width */, 1 /* depth_in */,
                                      1 /* depth_out */, 0.f /* biasValue */
                                      ));
    EXPECT_EQ(true, model.Compute(10.f, 11.f, &result));
    EXPECT_EQ((10.f * 11.f) * (2 * 2), result);

    EXPECT_EQ(true, model.Compute(4.f, 5.f, &result));
    EXPECT_EQ((4.f * 5.f) * (2 * 2), result);
}
