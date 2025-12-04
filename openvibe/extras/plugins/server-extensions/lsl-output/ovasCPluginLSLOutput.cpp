///-------------------------------------------------------------------------------------------------
/// 
/// \file ovasCPluginLSLOutput.cpp
/// \brief Acquisition Server plugin outputting signals and stimulations to LabStreamingLayer (LSL) streams.
/// \author Jussi T. Lindgren (Inria).
/// \version 0.1
/// \date 
/// \copyright Copyright (C) 2022 Inria
///
/// This program is free software: you can redistribute it and/or modify
/// it under the terms of the GNU Affero General Public License as published
/// by the Free Software Foundation, either version 3 of the License, or
/// (at your option) any later version.
///
/// This program is distributed in the hope that it will be useful,
/// but WITHOUT ANY WARRANTY; without even the implied warranty of
/// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
/// GNU Affero General Public License for more details.
///
/// You should have received a copy of the GNU Affero General Public License
/// along with this program.  If not, see <https://www.gnu.org/licenses/>.
/// 
///-------------------------------------------------------------------------------------------------

#if defined TARGET_HAS_ThirdPartyLSL

// Notes: This code should be kept compatible with changes to LSL Input Driver in OpenViBE Acquisition Server,  and LSL Export box in Designer.
#include "ovasCPluginLSLOutput.h"

#include <vector>

#include <system/ovCTime.h>
#include <lsl/Utils.hpp>

namespace OpenViBE {
namespace AcquisitionServer {
namespace Plugins {

CPluginLSLOutput::CPluginLSLOutput(const Kernel::IKernelContext& ctx)
	: IAcquisitionServerPlugin(ctx, CString("AcquisitionServer_Plugin_LabStreamingLayerOutput"))
{
	m_kernelCtx.getLogManager() << Kernel::LogLevel_Info << "Loading plugin: LSL Output\n";

	m_settings.add("LSL_EnableLSLOutput", &m_IsLSLOutputEnabled);
	m_settings.add("LSL_SignalStreamName", &m_SignalStreamName);
	m_settings.add("LSL_MarkerStreamName", &m_MarkerStreamName);
	m_settings.load();

	// These are not saved or loaded from .conf as they are supposed to be unique
	m_SignalStreamID = CIdentifier::random().str();
	m_MarkerStreamID = CIdentifier::random().str();

	while (m_MarkerStreamID == m_SignalStreamID) { m_MarkerStreamID = CIdentifier::random().str(); } // very unlikely
}

CPluginLSLOutput::~CPluginLSLOutput()
{
	if (m_signalOutlet) {
		delete m_signalOutlet;
		m_signalOutlet = nullptr;
	}
	if (m_stimulusOutlet) {
		delete m_stimulusOutlet;
		m_stimulusOutlet = nullptr;
	}
}

// Hooks


bool CPluginLSLOutput::startHook(const std::vector<CString>& selectedChannelNames, const size_t sampling, const size_t nChannel,
								 const size_t nSamplePerSentBlock)
{
	m_nSamplePerSentBlock = nSamplePerSentBlock;

	m_useOVTimestamps = m_kernelCtx.getConfigurationManager().expandAsBoolean("${LSL_UseOVTimestamps}", m_useOVTimestamps);
	m_startTime       = System::Time::zgetTime();

	if (m_IsLSLOutputEnabled) {
		m_kernelCtx.getLogManager() << Kernel::LogLevel_Trace << "Will create streams [" << m_SignalStreamName << ", id " << m_SignalStreamID << "] and ["
				<< m_MarkerStreamName << ", id " << m_MarkerStreamID << "]\n";

		// Open a signal stream 
		lsl::stream_info signalInfo(m_SignalStreamName, "signal", int(nChannel), double(sampling), lsl::cf_float32, m_SignalStreamID);

		lsl::xml_element channels = signalInfo.desc().append_child("channels");

		for (size_t i = 0; i < nChannel; ++i) {
			channels.append_child("channel").append_child_value("label", selectedChannelNames[i].toASCIIString()).append_child_value("unit", "unknown").
					 append_child_value("type", "signal");
		}

		// make a new outlet
		m_signalOutlet = new lsl::stream_outlet(signalInfo, int(m_nSamplePerSentBlock));

		// Open a stimulus stream
		lsl::stream_info stimulusInfo(m_MarkerStreamName, "Markers", 1, lsl::IRREGULAR_RATE, lsl::cf_int32, m_MarkerStreamID);

		stimulusInfo.desc().append_child("channels").append_child("channel").append_child_value("label", "Stimulations").append_child_value("type", "marker");

		m_stimulusOutlet = new lsl::stream_outlet(stimulusInfo);

		m_kernelCtx.getLogManager() << Kernel::LogLevel_Info << "LSL Output activated...\n";
	}

	// m_kernelCtx.getLogManager() << Kernel::LogLevel_Info << "Step from sampling rate is " << 1.0 / double(sampling) << "\n";

	return true;
}

void CPluginLSLOutput::loopHook(std::deque<std::vector<float>>& buffers, CStimulationSet& stimSet, const uint64_t start, const uint64_t end,
								const uint64_t /*sampleTime*/)
{
	if (m_IsLSLOutputEnabled) {
		// Output signal
		if (m_signalOutlet->have_consumers()) {
			const uint64_t sampleStep = (end - start) / static_cast<uint64_t>(m_nSamplePerSentBlock);

			if (m_useOVTimestamps) {
				const double sampleStepInSec = CTime(sampleStep).toSeconds();
				const double chunkStartInSec = CTime(start).toSeconds();
				for (size_t i = 0; i < m_nSamplePerSentBlock; ++i) { m_signalOutlet->push_sample(buffers[i], chunkStartInSec + double(i) * sampleStepInSec); }
			}
			else {
				for (size_t i = 0; i < m_nSamplePerSentBlock; ++i) {
					const double lslTime = LSL::getLSLRelativeTime(m_startTime + CTime(start + i * sampleStep));
					m_signalOutlet->push_sample(buffers[i], lslTime);
				}
			}

			// m_kernelCtx.getLogManager() << Kernel::LogLevel_Info << "Pushed first signal at " << start << "\n"; 
			// m_kernelCtx.getLogManager() << Kernel::LogLevel_Info << "Step is " << step << "\n";
		}

		// Output stimuli
		if (m_stimulusOutlet->have_consumers()) {
			for (size_t i = 0; i < stimSet.size(); ++i) {
				if (stimSet.getDate(i) >= start && stimSet.getDate(i) < end) {
					const int code    = int(stimSet.getId(i));
					const double date = m_useOVTimestamps ? CTime(stimSet.getDate(i)).toSeconds()
											: LSL::getLSLRelativeTime(m_startTime + CTime(stimSet.getDate(i)));
					m_stimulusOutlet->push_sample(&code, date);
				}
			}
		}
	}
}

void CPluginLSLOutput::stopHook()
{
	if (m_IsLSLOutputEnabled) {
		if (m_signalOutlet) {
			delete m_signalOutlet;
			m_signalOutlet = nullptr;
		}
		if (m_stimulusOutlet) {
			delete m_stimulusOutlet;
			m_stimulusOutlet = nullptr;
		}
	}
}

}  // namespace Plugins
}  // namespace AcquisitionServer
}  // namespace OpenViBE
#endif
