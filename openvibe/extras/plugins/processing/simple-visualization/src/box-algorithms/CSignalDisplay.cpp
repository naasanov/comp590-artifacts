///-------------------------------------------------------------------------------------------------
/// 
/// \file CSignalDisplay.cpp
/// \brief Classes implementation for the Box Signal display.
/// \author Bruno Renier, Yann Renard, Alison Cellard, Jussi T. Lindgren (INRIA/IRISA).
/// \version 0.3.
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

/*
 * Note: The signal display and its subclasses (SignalChannelDisplay, SignalDisplayView)
 * were rehauled to give a better user experience for different types of signal. However,
 * the code could likely use significant refactoring for clarity and maintainability.
 * If this is done, care should be taken that the code does not break.
 */

#include "CSignalDisplay.hpp"

#include <iostream>

#include <sstream>

#include <system/ovCTime.h>

namespace OpenViBE {
namespace Plugins {
namespace SimpleVisualization {


bool CSignalDisplay::initialize()
{
	this->getStaticBoxContext().getInputType(0, m_InputTypeID);

	if (m_InputTypeID == OV_TypeId_StreamedMatrix) { m_StreamDecoder = new Toolkit::TStreamedMatrixDecoder<CSignalDisplay>(*this, 0); }
	else if (m_InputTypeID == OV_TypeId_Signal) { m_StreamDecoder = new Toolkit::TSignalDecoder<CSignalDisplay>(*this, 0); }
	else {
		this->getLogManager() << Kernel::LogLevel_Error << "Unknown input stream type at stream 0\n";
		return false;
	}

	m_StimulationDecoder.initialize(*this, 1);
	m_UnitDecoder.initialize(*this, 2);

	m_BufferDatabase = new CBufferDatabase(*this);

	//retrieve settings
	const CIdentifier displayMode = uint64_t(FSettingValueAutoCast(*this->getBoxAlgorithmContext(), 0));
	const CIdentifier scalingMode = uint64_t(FSettingValueAutoCast(*this->getBoxAlgorithmContext(), 1));
	m_refreshInterval             = FSettingValueAutoCast(*this->getBoxAlgorithmContext(), 2);
	const double verticalScale    = FSettingValueAutoCast(*this->getBoxAlgorithmContext(), 3);
	const double verticalOffset   = FSettingValueAutoCast(*this->getBoxAlgorithmContext(), 4);
	const double timeScale        = FSettingValueAutoCast(*this->getBoxAlgorithmContext(), 5);
	const bool horizontalRuler    = FSettingValueAutoCast(*this->getBoxAlgorithmContext(), 6);
	const bool verticalRuler      = FSettingValueAutoCast(*this->getBoxAlgorithmContext(), 7);
	const bool multiview          = FSettingValueAutoCast(*this->getBoxAlgorithmContext(), 8);

	if (m_refreshInterval < 0) {
		this->getLogManager() << Kernel::LogLevel_Error << "Refresh interval must be >= 0\n";
		return false;
	}
	if (verticalScale <= 0) {
		this->getLogManager() << Kernel::LogLevel_Error << "Vertical scale must be > 0\n";
		return false;
	}
	if (timeScale <= 0) {
		this->getLogManager() << Kernel::LogLevel_Error << "Time scale must be > 0\n";
		return false;
	}

	this->getLogManager() << Kernel::LogLevel_Debug << "l_sVerticalScale=" << verticalScale << ", offset " << verticalOffset << "\n";
	this->getLogManager() << Kernel::LogLevel_Trace << "l_sScalingMode=" << scalingMode << "\n";

	//create GUI
	m_SignalDisplayView = new CSignalDisplayView(*m_BufferDatabase, displayMode, scalingMode, verticalScale, verticalOffset, timeScale, horizontalRuler,
												 verticalRuler, multiview);

	m_BufferDatabase->SetDrawable(m_SignalDisplayView);

	//parent visualization box in visualization tree
	GtkWidget* widget  = nullptr;
	GtkWidget* toolbar = nullptr;
	dynamic_cast<CSignalDisplayView*>(m_SignalDisplayView)->GetWidgets(widget, toolbar);
	m_visualizationCtx = dynamic_cast<VisualizationToolkit::IVisualizationContext*>(this->createPluginObject(OVP_ClassId_Plugin_VisualizationCtx));
	m_visualizationCtx->setWidget(*this, widget);
	if (toolbar != nullptr) { m_visualizationCtx->setToolbar(*this, toolbar); }

	m_lastScaleRefreshTime = 0;

	return true;
}

bool CSignalDisplay::uninitialize()
{
	m_UnitDecoder.uninitialize();
	m_StimulationDecoder.uninitialize();
	if (m_StreamDecoder) {
		m_StreamDecoder->uninitialize();
		delete m_StreamDecoder;
	}

	delete m_SignalDisplayView;
	delete m_BufferDatabase;
	m_SignalDisplayView = nullptr;
	m_BufferDatabase    = nullptr;

	if (m_visualizationCtx) {
		this->releasePluginObject(m_visualizationCtx);
		m_visualizationCtx = nullptr;
	}

	return true;
}

bool CSignalDisplay::processInput(const size_t /*index*/)
{
	getBoxAlgorithmContext()->markAlgorithmAsReadyToProcess();
	return true;
}

bool CSignalDisplay::process()
{
	const IDynamicBoxContext* boxCtx = getBoxAlgorithmContext()->getDynamicBoxContext();

	if (m_BufferDatabase->GetErrorStatus()) {
		this->getLogManager() << Kernel::LogLevel_Error <<
				"Buffer database reports an error. Its possible that the inputs given to the Signal Display are not supported by it.\n";
		return false;
	}

	// Subcomponents may generate errors while running e.g. in gtk callbacks, where its not safe/possible to call logmanager
	if (!dynamic_cast<CSignalDisplayView*>(m_SignalDisplayView)->m_ErrorState.empty()) {
		for (const auto& e : dynamic_cast<CSignalDisplayView*>(m_SignalDisplayView)->m_ErrorState) { this->getLogManager() << Kernel::LogLevel_Error << e; }
		return false;
	}

	// Channel units on input 2 
	for (size_t c = 0; c < boxCtx->getInputChunkCount(2); ++c) {
		m_UnitDecoder.decode(c);
		if (m_UnitDecoder.isBufferReceived()) {
			std::vector<std::pair<CString, CString>> channelUnits;
			channelUnits.resize(m_UnitDecoder.getOutputMatrix()->getDimensionSize(0));
			const double* buffer = m_UnitDecoder.getOutputMatrix()->getBuffer();
			for (size_t i = 0; i < channelUnits.size(); ++i) {
				CString unit   = this->getTypeManager().getEnumerationEntryNameFromValue(OV_TypeId_MeasurementUnit, uint64_t(buffer[i * 2 + 0]));
				CString factor = this->getTypeManager().getEnumerationEntryNameFromValue(OV_TypeId_Factor, uint64_t(buffer[i * 2 + 1]));

				channelUnits[i] = std::pair<CString, CString>(unit, factor);
			}

			if (!dynamic_cast<CSignalDisplayView*>(m_SignalDisplayView)->SetChannelUnits(channelUnits)) {
				this->getLogManager() << Kernel::LogLevel_Warning << "Unable to set channel units properly\n";
			}
		}
	}

	// Stimulations in input 1
	for (size_t c = 0; c < boxCtx->getInputChunkCount(1); ++c) {
		m_StimulationDecoder.decode(c);
		if (m_StimulationDecoder.isBufferReceived()) {
			const CStimulationSet* stimSet = m_StimulationDecoder.getOutputStimulationSet();
			const size_t count             = stimSet->size();

			m_BufferDatabase->Resize(count);

			for (size_t s = 0; s < count; ++s) {
				const uint64_t id   = stimSet->getId(s);
				const uint64_t date = stimSet->getDate(s);
				CString name        = getTypeManager().getEnumerationEntryNameFromValue(OV_TypeId_Stimulation, id);

				if (name == CString("")) { name = CString(("Id " + std::to_string(id)).c_str()); }
				dynamic_cast<CSignalDisplayView*>(m_SignalDisplayView)->OnStimulationReceivedCB(id, name);
				m_BufferDatabase->SetStimulation(s, id, date);
			}
		}
	}

	// Streamed matrix in input 0
	for (size_t c = 0; c < boxCtx->getInputChunkCount(0); ++c) {
		m_StreamDecoder->decode(c);
		if (m_StreamDecoder->isHeaderReceived()) {
			const CMatrix* matrix = static_cast<Toolkit::TStreamedMatrixDecoder<CSignalDisplay>*>(m_StreamDecoder)->getOutputMatrix();

			if (m_InputTypeID == OV_TypeId_Signal) {
				const uint64_t rate = static_cast<Toolkit::TSignalDecoder<CSignalDisplay>*>(m_StreamDecoder)->getOutputSamplingRate();
				m_BufferDatabase->SetSampling(size_t(rate));
			}

			m_BufferDatabase->SetMatrixDimensionCount(matrix->getDimensionCount());
			for (size_t i = 0; i < matrix->getDimensionCount(); ++i) {
				m_BufferDatabase->SetMatrixDimensionSize(i, matrix->getDimensionSize(i));
				for (size_t j = 0; j < matrix->getDimensionSize(i); ++j) { m_BufferDatabase->SetMatrixDimensionLabel(i, j, matrix->getDimensionLabel(i, j)); }
			}
		}

		if (m_StreamDecoder->isBufferReceived()) {
			const CMatrix* matrix = static_cast<Toolkit::TStreamedMatrixDecoder<CSignalDisplay>*>(m_StreamDecoder)->getOutputMatrix();

			const bool returnValue = m_BufferDatabase->SetMatrixBuffer(matrix->getBuffer(), boxCtx->getInputChunkStartTime(0, c),
																	   boxCtx->getInputChunkEndTime(0, c));
			if (!returnValue) { return false; }
		}
	}

	const uint64_t timeNow = getPlayerContext().getCurrentTime();
	if (m_lastScaleRefreshTime == 0 || timeNow - m_lastScaleRefreshTime > CTime(m_refreshInterval).time()) {
		dynamic_cast<CSignalDisplayView*>(m_SignalDisplayView)->RefreshScale();
		m_lastScaleRefreshTime = timeNow;
	}

#ifdef DEBUG
	out = System::Time::zgetTime();
	std::cout << "Elapsed1 " << out - in << "\n";
#endif

	return true;
}
}  // namespace SimpleVisualization
}  // namespace Plugins
}  // namespace OpenViBE
