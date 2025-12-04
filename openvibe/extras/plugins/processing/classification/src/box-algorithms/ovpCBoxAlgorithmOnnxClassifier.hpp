///-------------------------------------------------------------------------------------------------
///
/// \file CBoxAlgorithmOnnxClassifier.hpp
/// \brief Classes of the Box ONNX Classifier.
/// \author Nicolas Asanov, Vidur Shah ().
/// \version 0.1.
/// \date Sat Nov 29 16:40:47 2025.
///
///-------------------------------------------------------------------------------------------------

#pragma once

// You may have to change this path to match your folder organisation
#include "../ovp_defines.h"

#include <openvibe/ov_all.h>
#include <toolkit/ovtk_all.h>
#include <onnxruntime/core/session/onnxruntime_cxx_api.h>
#include <memory>
#include <vector>
#include <chrono>

// The unique identifiers for the box and its descriptor.
// Identifier are randomly chosen by the skeleton-generator.
#define Box_OnnxClassifier OpenViBE::CIdentifier(0x855634c3, 0xa7ed9f52)
#define Box_OnnxClassifierDesc OpenViBE::CIdentifier(0xcec9e01c, 0x940cc256)
#define OV_AttributeId_Box_FlagIsUnstable OpenViBE::CIdentifier(0x666FFFFF, 0x666FFFFF)

namespace OpenViBE
{
	namespace Plugins
	{
		namespace Classification
		{
			/// <summary> The class CBoxAlgorithmOnnxClassifier describes the box ONNX Classifier. </summary>
			class CBoxAlgorithmOnnxClassifier final : virtual public Toolkit::TBoxAlgorithm<IBoxAlgorithm>
			{
			public:
				CBoxAlgorithmOnnxClassifier()
					: m_onnxEnv(ORT_LOGGING_LEVEL_WARNING, "ONNXClassifier"),
					  m_memoryInfo(Ort::MemoryInfo::CreateCpu(OrtAllocatorType::OrtArenaAllocator, OrtMemType::OrtMemTypeDefault))
				{
				}

				void release() override { delete this; }
				bool initialize() override;
				bool uninitialize() override;

				// Here is the different process callbacks possible
				//  - On clock ticks :
				// bool processClock(Kernel::CMessageClock& msg) override;
				//  - On new input received (the most common behaviour for signal processing) :
				bool processInput(const size_t index) override;

				// If you want to use processClock, you must provide the clock frequency.
				// uint64_t getClockFrequency() override { return 1LL<<32; }

				bool process() override;

				// As we do with any class in openvibe, we use the macro below to associate this box to an unique identifier.
				// The inheritance information is also made available, as we provide the superclass Toolkit::TBoxAlgorithm < IBoxAlgorithm >
			_IsDerivedFromClass_Final_(Toolkit::TBoxAlgorithm<IBoxAlgorithm>, Box_OnnxClassifier)

					protected :
					// Input decoders:
					std::vector<Toolkit::TStreamedMatrixDecoder<CBoxAlgorithmOnnxClassifier> *> m_decoders;

				// Output decoder:
				Toolkit::TStimulationEncoder<CBoxAlgorithmOnnxClassifier> m_stimEncoder;

			private:
				int runOnnxInference(const std::vector<const CMatrix *> &inputMatrices);
				void initializeOnnxSession(const CString &modelPath);
				// ONNX Runtime members
				Ort::Env m_onnxEnv;
				std::unique_ptr<Ort::Session> m_onnxSession;
				Ort::SessionOptions m_sessionOptions;

				// Model metadata
				size_t m_numInputs;
				std::vector<std::vector<int64_t>> m_inputShapes;
				std::vector<CString> m_inputNames;
				CString m_outputName;
				size_t m_numClasses;

				// Pre-allocated resources for inference
				Ort::MemoryInfo m_memoryInfo;
				std::vector<const char *> m_inputNamePtrs;
				std::vector<std::vector<float>> m_inputBuffers;

				// Latency profiling
				double m_totalProcessingTime = 0.0;
				size_t m_bufferCount = 0;
			};

			class CBoxAlgorithmOnnxClassifierListener final : public Toolkit::TBoxListener<IBoxListener>
			{
			public:
				bool onSettingValueChanged(Kernel::IBox &box, const size_t index) override;

				_IsDerivedFromClass_Final_(Toolkit::TBoxListener<IBoxListener>, CIdentifier::undefined())
			};

			/// <summary> Descriptor of the box ONNX Classifier. </summary>
			class CBoxAlgorithmOnnxClassifierDesc final : virtual public IBoxAlgorithmDesc
			{
			public:
				void release() override {}

				CString getName() const override { return "ONNX Classifier"; }
				CString getAuthorName() const override { return "Nicolas Asanov, Vidur Shah"; }
				CString getAuthorCompanyName() const override { return ""; }
				CString getShortDescription() const override { return "Classifies features based on a custom ONNX model"; }
				CString getDetailedDescription() const override { return ""; }
				CString getCategory() const override { return "Classification"; }
				CString getVersion() const override { return "0.1"; }
				CString getStockItemName() const override { return "gtk-floppy"; }

				CIdentifier getCreatedClass() const override { return Box_OnnxClassifier; }
				IPluginObject *create() override { return new CBoxAlgorithmOnnxClassifier; }

				IBoxListener *createBoxListener() const override { return new CBoxAlgorithmOnnxClassifierListener; }
				void releaseBoxListener(IBoxListener *listener) const override { delete listener; }

				bool getBoxPrototype(Kernel::IBoxProto &prototype) const override
				{
					prototype.addInput("Input Matrix", OV_TypeId_StreamedMatrix);

					prototype.addFlag(Kernel::BoxFlag_CanModifyInput);
					prototype.addFlag(Kernel::BoxFlag_CanAddInput);

					prototype.addInputSupport(OV_TypeId_StreamedMatrix);
					prototype.addInputSupport(OV_TypeId_Signal);
					prototype.addInputSupport(OV_TypeId_FeatureVector);
					prototype.addInputSupport(OV_TypeId_Spectrum);
					prototype.addInputSupport(OV_TypeId_ChannelLocalisation);

					prototype.addOutput("Classification", OV_TypeId_Stimulations);

					prototype.addSetting("Model Filepath", OV_TypeId_Filename, "");

					prototype.addFlag(OV_AttributeId_Box_FlagIsUnstable);
					return true;
				}
				_IsDerivedFromClass_Final_(IBoxAlgorithmDesc, Box_OnnxClassifierDesc)
			};

		} // namespace Classification
	} // namespace Plugins
} // namespace OpenViBE
