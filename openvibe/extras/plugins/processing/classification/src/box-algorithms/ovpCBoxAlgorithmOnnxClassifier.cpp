///-------------------------------------------------------------------------------------------------
///
/// \file CBoxAlgorithmOnnxClassifier.cpp
/// \brief Classes of the Box ONNX Box.
/// \author Nicolas Asanov, Vidur Shah ().
/// \version 0.1.
/// \date Sat Nov 29 16:40:47 2025.
///
///-------------------------------------------------------------------------------------------------

#include "./ovpCBoxAlgorithmOnnxClassifier.hpp"

namespace OpenViBE
{
	namespace Plugins
	{
		namespace Classification
		{
			///-------------------------------------------------------------------------------------------------
			bool CBoxAlgorithmOnnxClassifier::initialize()
			{
				// Initialize output encoder
				m_stimEncoder.initialize(*this, 0);

				// Initialize input decoders
				const Kernel::IBox &boxCtx = this->getStaticBoxContext();
				CIdentifier typeID;
				for (size_t i = 0; i < boxCtx.getInputCount(); ++i)
				{
					auto decoder = new Toolkit::TStreamedMatrixDecoder<CBoxAlgorithmOnnxClassifier>();
					decoder->initialize(*this, i);
					m_decoders.push_back(decoder);
				}

				m_numInputs = boxCtx.getInputCount();

				// Initialize Onnx Runtime session
				const CString modelPath = FSettingValueAutoCast(*this->getBoxAlgorithmContext(), 0);

				try
				{
					initializeOnnxSession(modelPath);
				}
				catch (const Ort::Exception &e)
				{
					this->getLogManager() << Kernel::LogLevel_Error
																<< "Failed to load ONNX model: " << e.what() << "\n";
					return false;
				}

				this->getLogManager() << Kernel::LogLevel_Info
															<< "ONNX model loaded successfully. Classes: " << m_numClasses << "\n";

				return true;
			}

			///-------------------------------------------------------------------------------------------------
			bool CBoxAlgorithmOnnxClassifier::uninitialize()
			{
				// Print latency profiling summary
				if (m_bufferCount > 0)
				{
					double avgLatency = m_totalProcessingTime / m_bufferCount;
					this->getLogManager() << Kernel::LogLevel_Info
																<< "=== ONNX Classifier Latency Profiling Summary ===\n"
																<< "Total buffers processed: " << m_bufferCount << "\n"
																<< "Total processing time: " << m_totalProcessingTime << " ms\n"
																<< "Average latency per buffer: " << avgLatency << " ms\n"
																<< "================================================\n";
				}
				else
				{
					this->getLogManager() << Kernel::LogLevel_Info
																<< "No buffers processed - no latency data available.\n";
				}

				m_stimEncoder.uninitialize();
				for (auto decoder : m_decoders)
				{
					decoder->uninitialize();
					delete decoder;
				}
				m_decoders.clear();

				m_onnxSession.reset();
				m_inputShapes.clear();
				m_numClasses = 0;
				m_numInputs = 0;

				m_inputBuffers.clear();
				m_inputNamePtrs.clear();

				return true;
			}

			///-------------------------------------------------------------------------------------------------
			bool CBoxAlgorithmOnnxClassifier::processInput(const size_t /*index*/)
			{
				getBoxAlgorithmContext()->markAlgorithmAsReadyToProcess();
				return true;
			}

			///-------------------------------------------------------------------------------------------------
			bool CBoxAlgorithmOnnxClassifier::process()
			{
				Kernel::IBoxIO &boxCtx = this->getDynamicBoxContext();

				std::vector<const CMatrix *> inputMatrices;
				inputMatrices.reserve(m_numInputs);

				size_t chunkIdx = 0;

				// Continually loop as long as all inputs have pending chunks
				while (true)
				{
					auto startTime = std::chrono::high_resolution_clock::now();

					// Check if ALL inputs still have chunks available
					for (size_t i = 0; i < m_numInputs; ++i)
					{
						if (chunkIdx >= boxCtx.getInputChunkCount(i))
						{
							return true; // All possible chunks processed
						}
					}

					uint64_t chunkStartTime = boxCtx.getInputChunkStartTime(0, chunkIdx);
					uint64_t chunkEndTime = boxCtx.getInputChunkEndTime(0, chunkIdx);
					bool isHeaderReceived = false;

					inputMatrices.clear();

					// Decode a chunk from all inputs
					for (size_t i = 0; i < m_numInputs; ++i)
					{
						auto decoder = m_decoders[i];
						decoder->decode(chunkIdx);
						if (decoder->isHeaderReceived())
						{
							getLogManager() << Kernel::LogLevel_Info << "Input " << i << " header received.\n";
							isHeaderReceived = true;

							// Extract matrix metadata
							const CMatrix *matrix = decoder->getOutputMatrix();

							// Validate matrix dimensions match ONNX model input shape
							const size_t inputDimCount = matrix->getDimensionCount();
							const auto expectedShape = m_inputShapes[i];

							// Validate dimension count, accounting for batch size dimension
							if (inputDimCount != (expectedShape.size() - 1))
							{
								this->getLogManager() << Kernel::LogLevel_Error
																			<< "Input " << i << " dimension count mismatch. Expected: "
																			<< (expectedShape.size() - 1) << ", Received: " << inputDimCount << "\n";
								return false;
							}

							// Validate individual dimensions, accounting for batch size dimension
							for (size_t dim = 0; dim < expectedShape.size() - 1; ++dim)
							{
								const size_t onnxDimSize = expectedShape[dim + 1];

								// Skip dynamic dimensions and batch size
								if (onnxDimSize <= 1)
									continue;

								const size_t ovDimSize = matrix->getDimensionSize(dim);
								if (ovDimSize != onnxDimSize)
								{
									this->getLogManager() << Kernel::LogLevel_Error
																				<< "Dimension " << dim << " size mismatch. Expected: " << onnxDimSize
																				<< ", Received: " << ovDimSize << "\n";
									return false;
								}
							}
						}
						else if (decoder->isBufferReceived())
						{
							const CMatrix *matrix = m_decoders[i]->getOutputMatrix();
							inputMatrices.push_back(matrix);

							// Print buffer data (first 20 elements)
							const double *buffer = matrix->getBuffer();
							const size_t elementCount = matrix->getBufferElementCount();

							getLogManager() << Kernel::LogLevel_Debug
															<< "Input " << i << " buffer received. "
															<< "Dimensions: [";
							for (size_t dim = 0; dim < matrix->getDimensionCount(); ++dim)
							{
								getLogManager() << matrix->getDimensionSize(dim);
								if (dim < matrix->getDimensionCount() - 1)
									getLogManager() << ", ";
							}
							getLogManager() << "], Elements: " << elementCount << "\n";
						}
						else if (decoder->isEndReceived())
						{
							// End of stream received. This happens only once when pressing "stop". Just pass it to the next boxes so they receive the message :
							CStimulationSet *stimSet = m_stimEncoder.getInputStimulationSet();
							stimSet->clear();

							stimSet->push_back(OVTK_StimulationId_ExperimentStop, chunkStartTime, 0);

							m_stimEncoder.encodeBuffer();
							boxCtx.markOutputAsReadyToSend(0, chunkStartTime, chunkEndTime);

							getLogManager() << Kernel::LogLevel_Info << "Stop Stimulation sent, ending stream.\n";
							m_stimEncoder.encodeEnd();
							boxCtx.markOutputAsReadyToSend(0, chunkStartTime, chunkEndTime);

							return true;
						}

						boxCtx.markInputAsDeprecated(i, chunkIdx);
					}
					chunkIdx++;

					if (isHeaderReceived)
					{
						// Encode stimulation header
						m_stimEncoder.encodeHeader();
						boxCtx.markOutputAsReadyToSend(0, chunkStartTime, chunkEndTime);
						continue;
					}

					try
					{
						// Run ONNX model inference
						int predictedClass = runOnnxInference(inputMatrices);

						// Prepare stimulation set
						CStimulationSet *stimSet = m_stimEncoder.getInputStimulationSet();
						stimSet->clear();

						uint64_t stimulationId = OVTK_StimulationId_Label_01 + predictedClass;

						stimSet->push_back(
								stimulationId,
								chunkEndTime,
								0);
					}
					catch (const Ort::Exception &e)
					{
						this->getLogManager() << Kernel::LogLevel_Error
																	<< "ONNX inference failed: " << e.what() << "\n";
						return false;
					}

					// Encode and send the output buffer
					m_stimEncoder.encodeBuffer();
					boxCtx.markOutputAsReadyToSend(0, chunkEndTime, chunkEndTime);

					// End timing and update profiling stats
					auto endTime = std::chrono::high_resolution_clock::now();
					std::chrono::duration<double, std::milli> elapsed = endTime - startTime;
					m_totalProcessingTime += elapsed.count();
					m_bufferCount++;

					this->getLogManager() << Kernel::LogLevel_Debug
																<< "Buffer processing latency: " << elapsed.count() << " ms\n";
				}

				return true;

				// for (size_t i = 0; i < boxCtx.getInputChunkCount(0); ++i) {
				// 	m_matrixDecoder.decode(i);
				// 	if (m_matrixDecoder.isHeaderReceived()) {
				// 		getLogManager() << Kernel::LogLevel_Info << "Input header received.\n";
				// 		m_stimEncoder.encodeHeader();
				// 		boxCtx.markOutputAsReadyToSend(0, boxCtx.getInputChunkStartTime(0, i), boxCtx.getInputChunkEndTime(0, i));
				// 	}
				// 	else if (m_matrixDecoder.isBufferReceived()) {
				// 		auto startTime = std::chrono::high_resolution_clock::now();

				// 		getLogManager() << Kernel::LogLevel_Debug << "Input buffer received.\n";
				// 		// Decode recieved buffer
				// 		const CMatrix* inputMatrix = m_matrixDecoder.getOutputMatrix();

				// 		const double* buffer = inputMatrix->getBuffer();
				// 		size_t elementCount = inputMatrix->getBufferElementCount();

				// 		// Validate matrix dimensions match ONNX model input shape
				// 		const size_t inputDimCount = inputMatrix->getDimensionCount();
				// 		const size_t onnxDimCount = m_inputShape.size();

				// 		// Calculate total expected elements and validate dimensions
				// 		size_t expectedElements = 1;
				// 		for (size_t dim = 0; dim < onnxDimCount; ++dim) {
				// 			if (m_inputShape[dim] > 0) {
				// 				expectedElements *= m_inputShape[dim];

				// 				// If dimension count matches, validate each dimension size
				// 				if (inputDimCount == onnxDimCount && dim < inputDimCount) {
				// 					const size_t ovDimSize = inputMatrix->getDimensionSize(dim);
				// 					const int64_t onnxDimSize = m_inputShape[dim];

				// 					// Skip validation for dynamic dimensions (-1) or batch size (first dim = 1)
				// 					if (onnxDimSize > 1 && ovDimSize != static_cast<size_t>(onnxDimSize)) {
				// 						this->getLogManager() << Kernel::LogLevel_Error
				// 							<< "Dimension " << dim << " size mismatch. Expected: " << onnxDimSize
				// 							<< ", Received: " << ovDimSize << "\n";
				// 						return false;
				// 					}
				// 				}
				// 			}
				// 		}

				// 		if (elementCount != expectedElements) {
				// 			this->getLogManager() << Kernel::LogLevel_Error
				// 				<< "Matrix size mismatch. Expected: " << expectedElements
				// 				<< " elements, Received: " << elementCount << "\n";
				// 			return false;
				// 		}

				// 		std::vector<float> inputBuffer(buffer, buffer + elementCount);

				// 		try {
				// 			// Run ONNX model inference
				// 			int predictedClass = runOnnxInference(inputBuffer);

				// 			// Prepare stimulation set
				// 			CStimulationSet* stimSet = m_stimEncoder.getInputStimulationSet();
				// 			stimSet->clear();

				// 			uint64_t stimulationId = OVTK_StimulationId_Label_01 + predictedClass;

				// 			stimSet->push_pack(
				// 				stimulationId,
				// 				boxCtx.getInputChunkEndTime(0, i),
				// 				0
				// 			);
				// 		} catch (const Ort::Exception& e) {
				// 			this->getLogManager() << Kernel::LogLevel_Error
				// 				<< "ONNX inference failed: " << e.what() << "\n";
				// 			return false;
				// 		}

				// 		// Encode the output buffer :
				// 		m_stimEncoder.encodeBuffer();
				// 		// and send it to the next boxes :
				// 		boxCtx.markOutputAsReadyToSend(0, boxCtx.getInputChunkStartTime(0, i), boxCtx.getInputChunkEndTime(0, i));

				// 		// End timing and update profiling stats
				// 		auto endTime = std::chrono::high_resolution_clock::now();
				// 		std::chrono::duration<double, std::milli> elapsed = endTime - startTime;
				// 		m_totalProcessingTime += elapsed.count();
				// 		m_bufferCount++;

				// 		this->getLogManager() << Kernel::LogLevel_Debug
				// 			<< "Buffer processing latency: " << elapsed.count() << " ms\n";

				// 	}
				// 	else if (m_matrixDecoder.isEndReceived()) {
				// 		// End of stream received. This happens only once when pressing "stop". Just pass it to the next boxes so they receive the message :
				// 		CStimulationSet* stimSet = m_stimEncoder.getInputStimulationSet();
				// 		stimSet->clear();

				// 		stimSet->push_back(OVTK_StimulationId_ExperimentStop, boxCtx.getInputChunkStartTime(0, i), 0);

				// 		m_stimEncoder.encodeBuffer();
				// 		boxCtx.markOutputAsReadyToSend(0, boxCtx.getInputChunkStartTime(0, i), boxCtx.getInputChunkEndTime(0, i));

				// 		getLogManager() << Kernel::LogLevel_Info << "Stop Stimulation sent, ending stream.\n";
				// 		m_stimEncoder.encodeEnd();
				// 		boxCtx.markOutputAsReadyToSend(0, boxCtx.getInputChunkStartTime(0, i), boxCtx.getInputChunkEndTime(0, i));
				// 	}
				// }
			}

			void CBoxAlgorithmOnnxClassifier::initializeOnnxSession(const CString &modelPath)
			{
				// Configure session options
				m_sessionOptions.SetIntraOpNumThreads(1);
				m_sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_BASIC);

				// Load the model
				m_onnxSession = std::make_unique<Ort::Session>(
						m_onnxEnv,
						modelPath.toASCIIString(),
						m_sessionOptions);

				// Get input info
				m_inputShapes.reserve(m_numInputs);
				m_inputNames.reserve(m_numInputs);
				m_inputNamePtrs.reserve(m_numInputs);
				for (size_t i = 0; i < m_numInputs; ++i)
				{
					m_inputShapes.push_back(
							m_onnxSession->GetInputTypeInfo(i)
									.GetTensorTypeAndShapeInfo()
									.GetShape());
					m_inputNames.push_back(
							CString(m_onnxSession->GetInputNameAllocated(i, Ort::AllocatorWithDefaultOptions()).get()));
					m_inputNamePtrs.push_back(m_inputNames.back().toASCIIString());
				}

				// Get output name
				m_outputName = CString(
						m_onnxSession->GetOutputNameAllocated(0, Ort::AllocatorWithDefaultOptions()).get());

				// Get output shape to determine number of classes
				m_numClasses = m_onnxSession->GetOutputTypeInfo(0)
													 .GetTensorTypeAndShapeInfo()
													 .GetShape()
													 .back();

				// Pre-allocate float buffers (will be resized as needed)
				m_inputBuffers.resize(m_numInputs);
			}

			int CBoxAlgorithmOnnxClassifier::runOnnxInference(const std::vector<const OpenViBE::CMatrix *> &inputMatrices)
			{
				// Convert to ONNX tensors
				std::vector<Ort::Value> onnxInputTensors;
				onnxInputTensors.reserve(m_numInputs);
				for (size_t i = 0; i < m_numInputs; ++i)
				{
					const CMatrix *matrix = inputMatrices[i];
					const double *buffer = matrix->getBuffer();
					size_t elementCount = matrix->getBufferElementCount();

					// Reuse pre-allocated buffer
					m_inputBuffers[i].resize(elementCount);
					for (size_t j = 0; j < elementCount; ++j)
					{
						m_inputBuffers[i][j] = static_cast<float>(buffer[j]);
					}

					onnxInputTensors.push_back(
							Ort::Value::CreateTensor<float>(
									m_memoryInfo,
									m_inputBuffers[i].data(),
									m_inputBuffers[i].size(),
									m_inputShapes[i].data(),
									m_inputShapes[i].size()));
				}

				// Prepare output names
				const char *outputNames[] = {m_outputName.toASCIIString()};

				// Run inference
				auto outputTensors = m_onnxSession->Run(
						Ort::RunOptions{nullptr},
						m_inputNamePtrs.data(),
						onnxInputTensors.data(),
						m_numInputs,
						outputNames,
						1);

				// Get output data (probabilities for each class)
				float *outputData = outputTensors[0].GetTensorMutableData<float>();

				// Log raw outputs
				this->getLogManager() << Kernel::LogLevel_Debug << "Raw outputs: [";
				for (size_t i = 0; i < m_numClasses; ++i)
				{
					this->getLogManager() << outputData[i];
					if (i < m_numClasses - 1)
						this->getLogManager() << ", ";
				}
				this->getLogManager() << "] ";

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

				this->getLogManager()
						<< "Predicted class: " << predictedClass
						<< " (value: " << maxProb << ")\n";

				return predictedClass;
			}

			bool CBoxAlgorithmOnnxClassifierListener::onSettingValueChanged(Kernel::IBox &box, const size_t index)
			{
				if (index != 0)
				{
					return true; // Only react to first setting (model path)
				}

				CString modelPath;
				box.getSettingValue(index, modelPath);

				size_t numOutputs;
				size_t numInputs;
				std::vector<std::string> inputNames;

				try
				{
					Ort::Env env(ORT_LOGGING_LEVEL_ERROR, "MetadataReader");

					Ort::SessionOptions options;
					options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_DISABLE_ALL);

					Ort::Session session(env, modelPath.toASCIIString(), options);

					Ort::AllocatorWithDefaultOptions allocator;

					// Get number of inputs
					numInputs = session.GetInputCount();

					// Get input names and shapes
					for (size_t i = 0; i < numInputs; ++i)
					{
						auto inputNamePtr = session.GetInputNameAllocated(i, allocator);
						inputNames.push_back(std::string(inputNamePtr.get()));
					}

					// Get number of outputs
					numOutputs = session.GetOutputCount();
				}
				catch (const Ort::Exception &e)
				{
					this->getLogManager() << Kernel::LogLevel_Error
																<< "Failed to read ONNX model metadata: " << e.what() << "\n";
					return false;
				}

				// Validate at least one output exists
				if (numOutputs == 0)
				{
					this->getLogManager() << Kernel::LogLevel_Error
																<< "ONNX model must have at least one output.\n";
					return false;
				}

				// Log warning if multiple outputs (but don't block)
				if (numOutputs > 1)
				{
					this->getLogManager() << Kernel::LogLevel_Warning
																<< "ONNX model has " << numOutputs << " outputs. Using first output for classification.\n";
				}

				// Clear current inputs
				for (size_t i = 0; i < box.getInputCount(); ++i)
				{
					box.removeInput(0);
				}

				// Add inputs based on model metadata
				for (const auto &name : inputNames)
				{
					box.addInput(name.c_str(), OV_TypeId_StreamedMatrix);
				}

				this->getLogManager() << Kernel::LogLevel_Info
															<< "Box inputs updated based on ONNX model metadata. Total inputs: " << numInputs << "\n";

				return true;
			}

		} // namespace Classification
	} // namespace Plugins
} // namespace OpenViBE
