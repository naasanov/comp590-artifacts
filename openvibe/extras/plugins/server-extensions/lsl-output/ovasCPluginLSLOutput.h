///-------------------------------------------------------------------------------------------------
/// 
/// \file ovasCPluginLSLOutput.h
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

#pragma once

#if defined TARGET_HAS_ThirdPartyLSL

#include <lsl_cpp.h>
#include "ovasIAcquisitionServerPlugin.h"

namespace OpenViBE {
namespace AcquisitionServer {
class CAcquisitionServer;

namespace Plugins {
class CPluginLSLOutput final : public IAcquisitionServerPlugin
{
	// Plugin interface
public:
	explicit CPluginLSLOutput(const Kernel::IKernelContext& ctx);
	~CPluginLSLOutput() override;

	bool startHook(const std::vector<CString>& selectedChannelNames, const size_t sampling, const size_t nChannel, const size_t nSamplePerSentBlock) override;
	void stopHook() override;
	void loopHook(std::deque<std::vector<float>>& buffers, CStimulationSet& stimSet, const uint64_t start, const uint64_t end,
				  const uint64_t sampleTime) override;

	// Plugin implementation
	bool m_IsLSLOutputEnabled      = false;
	std::string m_SignalStreamName = "openvibeSignal";
	std::string m_SignalStreamID;
	std::string m_MarkerStreamName = "openvibeMarkers";
	std::string m_MarkerStreamID;

private:
	lsl::stream_outlet* m_signalOutlet   = nullptr;
	lsl::stream_outlet* m_stimulusOutlet = nullptr;

	size_t m_nSamplePerSentBlock = 0;

	bool m_useOVTimestamps = false;
	CTime m_startTime      = CTime(0);
};
}  // namespace Plugins
}  // namespace AcquisitionServer
}  // namespace OpenViBE

#endif // TARGET_HAS_ThirdPartyLSL
