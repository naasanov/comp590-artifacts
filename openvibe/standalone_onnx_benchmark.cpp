///-------------------------------------------------------------------------------------------------
///
/// \file standalone_onnx_benchmark.cpp
/// \brief Standalone ONNX classifier benchmark without OpenViBE overhead
/// \author Nicolas Asanov
/// \date December 1, 2025
///
/// Compile with:
/// g++ -std=c++17 -O3 standalone_onnx_benchmark.cpp -o standalone_onnx_benchmark \
///     -I/path/to/onnxruntime/include \
///     -L/path/to/onnxruntime/lib \
///     -lonnxruntime
///
///-------------------------------------------------------------------------------------------------

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <chrono>
#include <memory>
#include <cmath>
#include <onnxruntime/core/session/onnxruntime_cxx_api.h>

class StandaloneOnnxClassifier
{
public:
  StandaloneOnnxClassifier(const std::string &modelPath)
      : m_onnxEnv(ORT_LOGGING_LEVEL_WARNING, "StandaloneClassifier")
  {

    // Configure session options
    m_sessionOptions.SetIntraOpNumThreads(1);
    m_sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_BASIC);

    // Load the model
    m_onnxSession = std::make_unique<Ort::Session>(
        m_onnxEnv,
        modelPath.c_str(),
        m_sessionOptions);

    // Get input shape info
    m_inputShape = m_onnxSession->GetInputTypeInfo(0)
                       .GetTensorTypeAndShapeInfo()
                       .GetShape();

    // Get output shape to determine number of classes
    m_numClasses = m_onnxSession->GetOutputTypeInfo(0)
                       .GetTensorTypeAndShapeInfo()
                       .GetShape()
                       .back();

    std::cout << "Model loaded successfully. Input shape: [";
    for (size_t i = 0; i < m_inputShape.size(); ++i)
    {
      std::cout << m_inputShape[i];
      if (i < m_inputShape.size() - 1)
        std::cout << ", ";
    }
    std::cout << "], Classes: " << m_numClasses << "\n";
  }

  int classify(const std::vector<float> &inputBuffer)
  {
    Ort::MemoryInfo memoryInfo = Ort::MemoryInfo::CreateCpu(
        OrtAllocatorType::OrtArenaAllocator,
        OrtMemType::OrtMemTypeDefault);

    Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
        memoryInfo,
        const_cast<float *>(inputBuffer.data()),
        inputBuffer.size(),
        m_inputShape.data(),
        m_inputShape.size());

    // Get input/output names
    Ort::AllocatorWithDefaultOptions allocator;
    auto inputName = m_onnxSession->GetInputNameAllocated(0, allocator);
    auto outputName = m_onnxSession->GetOutputNameAllocated(0, allocator);

    const char *inputNames[] = {inputName.get()};
    const char *outputNames[] = {outputName.get()};

    // Run inference
    auto outputTensors = m_onnxSession->Run(
        Ort::RunOptions{nullptr},
        inputNames,
        &inputTensor,
        1,
        outputNames,
        1);

    // Get output data
    float *outputData = outputTensors[0].GetTensorMutableData<float>();

    // Find class with highest probability
    int predictedClass = 0;
    float maxProb = outputData[0];
    for (size_t i = 1; i < m_numClasses; ++i)
    {
      if (outputData[i] > maxProb)
      {
        maxProb = outputData[i];
        predictedClass = i;
      }
    }

    return predictedClass;
  }

  size_t getInputSize() const
  {
    size_t size = 1;
    for (const auto &dim : m_inputShape)
    {
      if (dim > 0)
        size *= dim;
    }
    return size;
  }

private:
  Ort::Env m_onnxEnv;
  std::unique_ptr<Ort::Session> m_onnxSession;
  Ort::SessionOptions m_sessionOptions;
  std::vector<int64_t> m_inputShape;
  size_t m_numClasses;
};

// CSV data loader
class CSVDataLoader
{
public:
  CSVDataLoader(const std::string &filename) : m_filename(filename) {}

  bool loadData(size_t samplesPerEpoch)
  {
    std::ifstream file(m_filename);
    if (!file.is_open())
    {
      std::cerr << "Failed to open CSV file: " << m_filename << "\n";
      return false;
    }

    std::string line;
    // Skip header line
    std::getline(file, line);

    // Parse header to get channel count
    std::stringstream headerStream(line);
    std::string value;
    std::vector<std::string> headers;
    while (std::getline(headerStream, value, ','))
    {
      headers.push_back(value);
    }

    // Channels are from index 2 to (headers.size() - 4)
    // Columns: Time, Epoch, [channels...], Event Id, Event Date, Event Duration
    m_numChannels = headers.size() - 5; // exclude Time, Epoch, EventId, EventDate, EventDuration

    std::cout << "CSV has " << m_numChannels << " channels\n";

    // Read all data rows
    std::vector<std::vector<double>> allSamples;
    while (std::getline(file, line))
    {
      std::stringstream ss(line);
      std::string value;
      std::vector<double> sample;

      size_t col = 0;
      while (std::getline(ss, value, ','))
      {
        // Skip Time (0), Epoch (1), and last 3 columns (Event info)
        if (col >= 2 && col < headers.size() - 3)
        {
          sample.push_back(std::stod(value));
        }
        col++;
      }

      if (sample.size() == m_numChannels)
      {
        allSamples.push_back(sample);
      }
    }

    std::cout << "Loaded " << allSamples.size() << " total samples from CSV\n";

    // Split into epochs of samplesPerEpoch
    for (size_t i = 0; i + samplesPerEpoch <= allSamples.size(); i += samplesPerEpoch)
    {
      std::vector<float> epoch;
      epoch.reserve(samplesPerEpoch * m_numChannels);

      for (size_t j = 0; j < samplesPerEpoch; ++j)
      {
        for (size_t ch = 0; ch < m_numChannels; ++ch)
        {
          epoch.push_back(static_cast<float>(allSamples[i + j][ch]));
        }
      }

      m_epochs.push_back(epoch);
    }

    std::cout << "Created " << m_epochs.size() << " epochs of " << samplesPerEpoch << " samples each\n";

    return !m_epochs.empty();
  }

  const std::vector<std::vector<float>> &getEpochs() const { return m_epochs; }
  size_t getNumChannels() const { return m_numChannels; }

private:
  std::string m_filename;
  std::vector<std::vector<float>> m_epochs;
  size_t m_numChannels;
};

int main(int argc, char *argv[])
{
  // Configuration
  const std::string modelPath = argc > 1 ? argv[1] : "eegnet_lee2019.onnx";
  const std::string csvPath = argc > 2 ? argv[2] : "test_data_s2.csv";
  const size_t numTrials = argc > 3 ? std::stoul(argv[3]) : 100; // Number of trials
  const size_t samplesPerEpoch = 385;                            // 1.54 seconds at 250Hz
  const double samplingRate = 250.0;                             // Hz
  const double epochDuration = samplesPerEpoch / samplingRate;   // 1.54 seconds

  std::cout << "=== Standalone ONNX Classifier Benchmark ===\n";
  std::cout << "Model: " << modelPath << "\n";
  std::cout << "Data: " << csvPath << "\n";
  std::cout << "Number of trials: " << numTrials << "\n";
  std::cout << "Samples per epoch: " << samplesPerEpoch << "\n";
  std::cout << "Sampling rate: " << samplingRate << " Hz\n";
  std::cout << "Epoch duration: " << epochDuration << " seconds\n";
  std::cout << "============================================\n\n";

  try
  {
    // Initialize classifier
    StandaloneOnnxClassifier classifier(modelPath);

    // Load data
    CSVDataLoader dataLoader(csvPath);
    if (!dataLoader.loadData(samplesPerEpoch))
    {
      std::cerr << "Failed to load data from CSV\n";
      return 1;
    }

    const auto &epochs = dataLoader.getEpochs();

    // Verify input size matches model
    size_t expectedInputSize = classifier.getInputSize();
    size_t actualInputSize = epochs[0].size();

    std::cout << "Expected input size: " << expectedInputSize << "\n";
    std::cout << "Actual input size: " << actualInputSize << "\n";

    if (expectedInputSize != actualInputSize)
    {
      std::cerr << "ERROR: Input size mismatch!\n";
      return 1;
    }

    // Benchmark processing
    std::cout << "\nRunning " << numTrials << " trials...\n\n";

    std::vector<double> trialAverages;

    for (size_t trial = 0; trial < numTrials; ++trial)
    {
      double totalProcessingTime = 0.0;
      size_t bufferCount = 0;

      for (const auto &epoch : epochs)
      {
        auto startTime = std::chrono::high_resolution_clock::now();

        int predictedClass = classifier.classify(epoch);

        auto endTime = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = endTime - startTime;

        totalProcessingTime += elapsed.count();
        bufferCount++;
      }

      double avgLatency = totalProcessingTime / bufferCount;
      trialAverages.push_back(avgLatency);

      std::cout << "Trial " << (trial + 1) << "/" << numTrials
                << ": Avg latency = " << avgLatency << " ms "
                << "(Total: " << totalProcessingTime << " ms for " << bufferCount << " epochs)\n";
    }

    // Calculate overall statistics
    double overallAvg = 0.0;
    double minLatency = trialAverages[0];
    double maxLatency = trialAverages[0];

    for (const auto &avg : trialAverages)
    {
      overallAvg += avg;
      if (avg < minLatency)
        minLatency = avg;
      if (avg > maxLatency)
        maxLatency = avg;
    }
    overallAvg /= trialAverages.size();

    // Calculate standard deviation
    double variance = 0.0;
    for (const auto &avg : trialAverages)
    {
      variance += (avg - overallAvg) * (avg - overallAvg);
    }
    variance /= trialAverages.size();
    double stdDev = std::sqrt(variance);

    // Print summary
    std::cout << "\n=== Latency Profiling Summary (" << numTrials << " Trials) ===\n";
    std::cout << "Epochs per trial: " << epochs.size() << "\n";
    std::cout << "Total epochs processed: " << (epochs.size() * numTrials) << "\n";
    std::cout << "Average latency per buffer: " << overallAvg << " ms\n";
    std::cout << "Min latency: " << minLatency << " ms\n";
    std::cout << "Max latency: " << maxLatency << " ms\n";
    std::cout << "Std deviation: " << stdDev << " ms\n";
    std::cout << "=================================\n";
  }
  catch (const Ort::Exception &e)
  {
    std::cerr << "ONNX Runtime error: " << e.what() << "\n";
    return 1;
  }
  catch (const std::exception &e)
  {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }

  return 0;
}
