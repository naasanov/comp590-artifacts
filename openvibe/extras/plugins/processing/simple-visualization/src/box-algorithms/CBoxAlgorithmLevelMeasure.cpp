///-------------------------------------------------------------------------------------------------
/// 
/// \file CBoxAlgorithmLevelMeasure.cpp
/// \brief Classes implementation for the Box Level measure.
/// \author Yann Renard (INRIA/IRISA).
/// \version 1.0.
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

#include "CBoxAlgorithmLevelMeasure.hpp"

#include "../algorithms/CAlgorithmLevelMeasure.hpp"
#include <cmath>

namespace OpenViBE {
namespace Plugins {
namespace SimpleVisualization {

bool CBoxAlgorithmLevelMeasure::initialize()
{
	m_matrix = new CMatrix();

	m_matrixDecoder = &getAlgorithmManager().getAlgorithm(getAlgorithmManager().createAlgorithm(OVP_GD_ClassId_Algorithm_StreamedMatrixDecoder));
	m_levelMeasure  = &getAlgorithmManager().getAlgorithm(getAlgorithmManager().createAlgorithm(Algorithm_LevelMeasure));

	m_matrixDecoder->initialize();
	m_levelMeasure->initialize();

	m_matrixBuffer.initialize(m_matrixDecoder->getInputParameter(OVP_GD_Algorithm_StreamedMatrixDecoder_InputParameterId_MemoryBufferToDecode));
	m_matrixHandler.initialize(m_matrixDecoder->getOutputParameter(OVP_GD_Algorithm_StreamedMatrixDecoder_OutputParameterId_Matrix));

	m_levelMeasureMatrix.initialize(m_levelMeasure->getInputParameter(LevelMeasure_InputParameterId_Matrix));
	m_levelMeasureMainWidget.initialize(m_levelMeasure->getOutputParameter(LevelMeasure_OutputParameterId_MainWidget));
	m_levelMeasureToolbarWidget.initialize(m_levelMeasure->getOutputParameter(LevelMeasure_OutputParameterId_ToolbarWidget));

	m_matrixHandler.setReferenceTarget(m_matrix);
	m_levelMeasureMatrix.setReferenceTarget(m_matrix);

	return true;
}

bool CBoxAlgorithmLevelMeasure::uninitialize()
{
	m_levelMeasureToolbarWidget.uninitialize();
	m_levelMeasureMainWidget.uninitialize();
	m_levelMeasureMatrix.uninitialize();

	m_matrixHandler.uninitialize();
	m_matrixBuffer.uninitialize();

	m_levelMeasure->uninitialize();
	m_matrixDecoder->uninitialize();

	getAlgorithmManager().releaseAlgorithm(*m_levelMeasure);
	getAlgorithmManager().releaseAlgorithm(*m_matrixDecoder);

	delete m_matrix;
	m_matrix = nullptr;

	if (m_visualizationCtx) {
		this->releasePluginObject(m_visualizationCtx);
		m_visualizationCtx = nullptr;
	}

	return true;
}

bool CBoxAlgorithmLevelMeasure::processInput(const size_t /*index*/)
{
	getBoxAlgorithmContext()->markAlgorithmAsReadyToProcess();
	return true;
}

bool CBoxAlgorithmLevelMeasure::process()
{
	Kernel::IBoxIO& boxContext = this->getDynamicBoxContext();

	for (size_t i = 0; i < boxContext.getInputChunkCount(0); ++i) {
		m_matrixBuffer = boxContext.getInputChunk(0, i);
		m_matrixDecoder->process();
		if (m_matrixDecoder->isOutputTriggerActive(OVP_GD_Algorithm_StreamedMatrixDecoder_OutputTriggerId_ReceivedHeader)) {
			m_levelMeasure->process(LevelMeasure_InputTriggerId_Reset);
			m_visualizationCtx = dynamic_cast<VisualizationToolkit::IVisualizationContext*>(this->createPluginObject(OVP_ClassId_Plugin_VisualizationCtx));
			m_visualizationCtx->setWidget(*this, m_levelMeasureMainWidget);
			m_visualizationCtx->setToolbar(*this, m_levelMeasureToolbarWidget);
		}

		if (m_matrixDecoder->isOutputTriggerActive(OVP_GD_Algorithm_StreamedMatrixDecoder_OutputTriggerId_ReceivedBuffer)) {
			// ----- >8 ------------------------------------------------------------------------------------------------------------------------------------------------------
			// should be done in a processing box !

			double sum = 0;

			{
				double* buffer = m_matrix->getBuffer();
				size_t n       = m_matrix->getBufferElementCount();
				while (n--) {
					sum += *buffer;
					buffer++;
				}
			}

			{
				const double factor = (std::fabs(sum) > DBL_EPSILON ? 1.0 / sum : 0.5);
				double* buffer      = m_matrix->getBuffer();
				size_t n            = m_matrix->getBufferElementCount();
				while (n--) {
					*buffer *= factor;
					buffer++;
				}
			}

			// ----- >8 ------------------------------------------------------------------------------------------------------------------------------------------------------

			m_levelMeasure->process(LevelMeasure_InputTriggerId_Refresh);
		}
		if (m_matrixDecoder->isOutputTriggerActive(OVP_GD_Algorithm_StreamedMatrixDecoder_OutputTriggerId_ReceivedEnd)) { }
		boxContext.markInputAsDeprecated(0, i);
	}

	return true;
}
}  // namespace SimpleVisualization
}  // namespace Plugins
}  // namespace OpenViBE
