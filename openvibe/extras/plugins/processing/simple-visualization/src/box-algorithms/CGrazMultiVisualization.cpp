///-------------------------------------------------------------------------------------------------
/// 
/// \file CGrazMultiVisualization.cpp
/// \brief Classes implementation for the Generalized Graz Visualization box.
/// \author Thibaut Monseigne (Inria).
/// \version 1.0.
/// \date 28/05/2019.
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

#include "CGrazMultiVisualization.hpp"
#include "../utils.hpp"

#include <algorithm> // std::min, max
#include <array>
#include <cmath>
#include <cstdlib>
#include <functional> // greater
#include <iomanip>
#include <sys/timeb.h>
#include <tcptagging/IStimulusSender.h>
#include <vector>

#if defined TARGET_OS_Linux
#include <unistd.h>
#endif

namespace OpenViBE {
namespace Plugins {
namespace SimpleVisualization {
//******************		
//***** STATIC *****	
//******************		
//---------------------------------------------------------------------------------------------------
static gboolean FlushCB(gpointer data)
{
	reinterpret_cast<CGrazMultiVisualization*>(data)->FlushQueue();
	return FALSE;	// Only run once
}
//---------------------------------------------------------------------------------------------------

// \remarks spend a lot of time with one problem DON'T USE LOG MANAGER IN FONCTION CALLED BY CALLBACK. Why ? Because OpenViBE
//---------------------------------------------------------------------------------------------------
static gboolean ResizeCB(GtkWidget* /*widget*/, GtkAllocation* allocation, gpointer data)
{
	reinterpret_cast<CGrazMultiVisualization*>(data)->Resize(size_t(allocation->width), size_t(allocation->height));
	return FALSE;
}
//---------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------------
static gboolean RedrawCB(GtkWidget* /*widget*/, GdkEventExpose* /*event*/, gpointer data)
{
	reinterpret_cast<CGrazMultiVisualization*>(data)->Redraw();
	return TRUE;
}
//---------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------------
/// <summary> Run scale command with the good size if you want to keep the proportion of the original pixbuf.</summary>
/// <param name="in">	The pixbuf.</param>
/// <param name="newW">	The new width.</param>
/// <param name="newH">	The new height.</param>
/// <param name="keepRatio">	True if you wan't to keep ratio of pixbuf.</param>
/// <param name="min">	True If you select the size min when you keep ratio.</param>
/// <returns> The gdk_pixbuf_scale_simple command.</returns>
static GdkPixbuf* RescalePixbuf(const GdkPixbuf* in, const int newW, const int newH, const bool keepRatio = true, const bool min = true)
{
	if (!in || newW <= 0 || newH <= 0) { return nullptr; }
	if (keepRatio) {
		const double scaleW = double(newW) / double(gdk_pixbuf_get_width(in)),
					 scaleH = double(newH) / double(gdk_pixbuf_get_height(in)),
					 scale  = min ? MIN(scaleW, scaleH) : MAX(scaleW, scaleH);
		return gdk_pixbuf_scale_simple(in, int(scale * gdk_pixbuf_get_width(in)), int(scale * gdk_pixbuf_get_height(in)), GDK_INTERP_BILINEAR);
	}
	return gdk_pixbuf_scale_simple(in, newW, newH, GDK_INTERP_BILINEAR);
}
//---------------------------------------------------------------------------------------------------

//******************************		
//***** OPENVIBE FUNCTIONS *****	
//******************************	
//---------------------------------------------------------------------------------------------------
bool CGrazMultiVisualization::initialize()
{
	//***** Codecs *****
	m_stimDecoder.initialize(*this, 0);
	m_classifDecoder.initialize(*this, 1);
	m_barSizeEncoder.initialize(*this, 0);
	m_confusionEncoder.initialize(*this, 1);
	m_iStim      = m_stimDecoder.getOutputStimulationSet();
	m_iMatrix    = m_classifDecoder.getOutputMatrix();
	m_oBarSize   = m_barSizeEncoder.getInputMatrix();
	m_oConfusion = m_confusionEncoder.getInputMatrix();

	//***** Settings *****
	m_showInstruction  = FSettingValueAutoCast(*this->getBoxAlgorithmContext(), 0);
	m_feedbackMode     = FSettingValueAutoCast(*this->getBoxAlgorithmContext(), 1);
	m_delayFeedback    = FSettingValueAutoCast(*this->getBoxAlgorithmContext(), 2);
	m_showAccuracy     = FSettingValueAutoCast(*this->getBoxAlgorithmContext(), 3);
	m_nbPredictionsMin = FSettingValueAutoCast(*this->getBoxAlgorithmContext(), 4);

	m_nbModality = (getStaticBoxContext().getSettingCount() - NON_MODALITY_SETTINGS_COUNT) / 2;
	m_barScales.resize(m_nbModality);
	for (auto& s : m_barScales) { s = 0; }
	std::vector<std::string> paths;
	paths.reserve(m_nbModality + 1);
	m_stimlist.reserve(m_nbModality);

	size_t idx = NON_MODALITY_SETTINGS_COUNT - 1; //the last non modality setting is for the none instruction feedback image
	paths.emplace_back(CString(FSettingValueAutoCast(*this->getBoxAlgorithmContext(), idx++)).toASCIIString());

	//Stimulation ID and images file names for each modality
	for (size_t i = 0; i < m_nbModality; ++i) {
		m_stimlist.emplace_back(uint64_t(FSettingValueAutoCast(*this->getBoxAlgorithmContext(), idx++)));
		paths.emplace_back(CString(FSettingValueAutoCast(*this->getBoxAlgorithmContext(), idx++)).toASCIIString());
	}

	m_oConfusion->resize(m_nbModality, m_nbModality);	// Confusion Matrix
	m_oBarSize->resize(m_nbModality);					// Bar Size Matrix

	//this->getLogManager() << Kernel::LogLevel_Warning << infos();
	//this->getLogManager() << Kernel::LogLevel_Warning << "Paths : \n";
	//for (const auto& p : paths) { this->getLogManager() << Kernel::LogLevel_Warning << p << "\n"; }

	OV_ERROR_UNLESS_KRF(m_nbPredictionsMin > 0, "Number of predictions to integrate  : " << m_nbPredictionsMin << " (expected value > 1)\n",
						Kernel::ErrorType::BadSetting);
	OV_ERROR_UNLESS_KRF(initImages(paths), "Error: couldn't load resource files!\n", Kernel::ErrorType::BadProcessing);
	paths.clear();
	OV_ERROR_UNLESS_KRF(initWindow(), "Error: couldn't load the interface!\n", Kernel::ErrorType::BadProcessing);


	//***** TCP Tagging *****
	m_stimulusSender = nullptr;
	m_idleFuncTag    = 0;
	m_stimuliQueue.clear();
	m_stimulusSender = TCPTagging::CreateStimulusSender();

	if (!m_stimulusSender->connect("localhost", "15361")) {
		this->getLogManager() << Kernel::LogLevel_Warning << "Unable to connect to AS's TCP Tagging plugin, stimuli wont be forwarded.\n";
	}
	//this->getLogManager() << Kernel::LogLevel_Warning << infos() << "\n";
	return true;
}
//---------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------------
bool CGrazMultiVisualization::uninitialize()
{
	//***** Print Confusion Matrix *****
	this->getLogManager() << Kernel::LogLevel_Info << "Confusion Matrix : \n";
	const double* buffer = m_oConfusion->getBuffer();
	std::stringstream ss;
	ss << std::setfill('0');
	size_t idx         = 0;
	size_t predictions = 0, good = 0;

	for (size_t i = 0; i < m_nbModality; ++i) {
		for (size_t j = 0; j < m_nbModality; ++j) {
			ss.str(std::string());
			const int val = int(buffer[idx++]);
			ss << std::setw(3) << val;
			predictions += val;
			this->getLogManager() << ss.str() << " ";
			if (i == j) { good += val; }
		}
		this->getLogManager() << "\n";
	}
	ss.str(std::string());
	ss << "Accuracy = " << std::fixed << std::setprecision(1) << (predictions == 0 ? 0.0 : 100.0 * double(good) / double(predictions)) << "\n";
	this->getLogManager() << ss.str();

	//***** Codecs *****
	m_iStim      = nullptr;
	m_iMatrix    = nullptr;
	m_oBarSize   = nullptr;
	m_oConfusion = nullptr;
	m_stimDecoder.uninitialize();
	m_classifDecoder.uninitialize();
	m_barSizeEncoder.uninitialize();
	m_confusionEncoder.uninitialize();

	//***** Images *****
	if (m_originalBar) { g_object_unref(G_OBJECT(m_originalBar)); }
	if (m_bar) { g_object_unref(G_OBJECT(m_bar)); }

	for (size_t i = 0; i < m_originalImgs.size(); ++i) {
		if (m_originalImgs[i]) { g_object_unref(G_OBJECT(m_originalImgs[i])); }
		if (m_smallImgs[i]) { g_object_unref(G_OBJECT(m_smallImgs[i])); }
		if (m_largeImgs[i]) { g_object_unref(G_OBJECT(m_largeImgs[i])); }
	}
	m_originalImgs.clear();
	m_smallImgs.clear();
	m_largeImgs.clear();

	//***** Window *****
	if (m_visualizationCtx) {
		this->releasePluginObject(m_visualizationCtx);
		m_visualizationCtx = nullptr;
	}
	if (m_widget) {
		gtk_widget_destroy(m_widget);
		m_widget = nullptr;
	}

	//***** TCP Tagging *****
	if (m_idleFuncTag) {
		m_stimuliQueue.clear();
		g_source_remove(m_idleFuncTag);
		m_idleFuncTag = 0;
	}

	m_stimlist.clear();
	delete m_stimulusSender;
	m_amplitudes.clear();
	return true;
}
//---------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------------
bool CGrazMultiVisualization::processInput(const size_t /*index*/)
{
	getBoxAlgorithmContext()->markAlgorithmAsReadyToProcess();
	return true;
}
//---------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------------
bool CGrazMultiVisualization::process()
{
	Kernel::IBoxIO& boxContext = this->getDynamicBoxContext();
	//**** Stimulations *****
	for (size_t i = 0; i < boxContext.getInputChunkCount(0); ++i) {
		m_needRedraw = true;
		m_stimDecoder.decode(i);
		if (m_stimDecoder.isBufferReceived()) { for (size_t s = 0; s < m_iStim->size(); ++s) { setStimulation(m_iStim->getId(s)); } }
	}

	for (size_t i = 0; i < boxContext.getInputChunkCount(1); ++i) {
		const uint64_t start = boxContext.getInputChunkStartTime(1, i),		// Time Code Chunk Start
					   end   = boxContext.getInputChunkEndTime(1, i);		// Time Code Chunk End
		m_needRedraw         = true;
		m_classifDecoder.decode(i);
		if (m_classifDecoder.isHeaderReceived()) {
			if (m_iMatrix->getBufferElementCount() != m_nbModality) {
				this->getLogManager() << Kernel::LogLevel_Error << "Error, the vector/matrix do not contain the same number of values as modalities! ("
						<< m_iMatrix->getBufferElementCount() << " VS " << m_nbModality << ")\n";
				return false;
			}
			m_confusionEncoder.encodeHeader();
			m_barSizeEncoder.encodeHeader();
		}
		if (m_classifDecoder.isBufferReceived()) {
			setMatrixBuffer(m_iMatrix->getBuffer());
			m_confusionEncoder.encodeBuffer();
			m_barSizeEncoder.encodeBuffer();
		}
		if (m_classifDecoder.isEndReceived()) {
			m_confusionEncoder.encodeEnd();
			m_barSizeEncoder.encodeEnd();
		}
		boxContext.markOutputAsReadyToSend(0, start, end);
		boxContext.markOutputAsReadyToSend(1, start, end);
	}

	// After any possible rendering, we flush the accumulated stimuli. The default idle func is low priority, so it should be run after rendering by gtk.
	// Only register a single idle func, if the previous is there its just as good
	if (m_idleFuncTag == 0) { m_idleFuncTag = g_idle_add(FlushCB, this); }

	return true;
}
//---------------------------------------------------------------------------------------------------

//*******************		
//***** DRAWING *****	
//*******************
//---------------------------------------------------------------------------------------------------
bool CGrazMultiVisualization::Redraw() const
{
	switch (m_state) {
		case EStates::Idle:					// Start Experiment & Idle
			drawReference();
			break;
		case EStates::Cross:				// Start Trial & Cross
			drawReference();
			drawCross();
			break;
		case EStates::Instruction:			// Show Instruction
			drawReference();
			drawModality();
			break;
		case EStates::Feedback:				// Feedback
			drawReference();
			if (m_feedbackMode != 3 && m_modality != -1 && m_vote != -1 && !m_delayFeedback) { drawBar(); }
			break;
		case EStates::Black:				// End Trial
			if (m_feedbackMode != 3 && m_modality != -1 && m_vote != -1 && m_delayFeedback) {
				drawReference();
				drawBar();
			}
			break;
	}
	if (m_showAccuracy) { drawAccuracy(); }
	return true;
}
//---------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------------
void CGrazMultiVisualization::drawReference() const
{
	gint currX = m_modalityX;
	// Draw for each Horizontal line, Vertical Line & modality
	for (size_t i = 1; i < m_smallImgs.size(); ++i, currX += m_modalityX) {
		gdk_draw_line(m_widget->window, m_widget->style->fg_gc[gtk_widget_get_state(m_widget)], currX - m_modalityW, m_modalityY, currX + m_modalityW,
					  m_modalityY);
		gdk_draw_line(m_widget->window, m_widget->style->fg_gc[gtk_widget_get_state(m_widget)], currX, m_margin, currX, m_modalityY);
		const gint x = currX - gdk_pixbuf_get_width(m_smallImgs[i]) / 2,
				   y = m_modalityY + (m_windowH - m_modalityY - gdk_pixbuf_get_height(m_smallImgs[i])) / 2;
		gdk_draw_pixbuf(m_widget->window, nullptr, m_smallImgs[i], 0, 0, x, y, -1, -1, GDK_RGB_DITHER_NONE, 0, 0);
	}
}
//---------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------------
void CGrazMultiVisualization::drawCross() const
{
	const gint xc   = m_windowW / 2, yc = m_windowH / 2,	// Window Center
			   size = std::min(xc, yc) / 4,					// Cross Size
			   xm   = xc - size, ym = yc - size,			// min(x,y)
			   xM   = xc + size, yM = yc + size;			// Max(x,y)
	gdk_draw_line(m_widget->window, m_widget->style->fg_gc[gtk_widget_get_state(m_widget)], xm, yc, xM, yc);
	gdk_draw_line(m_widget->window, m_widget->style->fg_gc[gtk_widget_get_state(m_widget)], xm, yc + 1, xM, yc + 1);
	gdk_draw_line(m_widget->window, m_widget->style->fg_gc[gtk_widget_get_state(m_widget)], xc, ym, xc, yM);
	gdk_draw_line(m_widget->window, m_widget->style->fg_gc[gtk_widget_get_state(m_widget)], xc + 1, ym, xc + 1, yM);
}
//---------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------------
void CGrazMultiVisualization::drawModality() const
{
	const GdkPixbuf* modality = m_largeImgs[(m_showInstruction ? m_modality + 1 : 0)];	// (m_modality + 1) Because the first element is the none instruction
	const gint w              = m_windowW / 2,
			   h              = m_windowH / 2,
			   x              = w - gdk_pixbuf_get_width(modality) / 2,
			   y              = h - gdk_pixbuf_get_height(modality) / 2;
	gdk_draw_pixbuf(m_widget->window, nullptr, modality, 0, 0, x, y, -1, -1, GDK_RGB_DITHER_NONE, 0, 0);
}
//---------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------------
void CGrazMultiVisualization::drawBar() const
{
	if ((m_feedbackMode == 0 && m_modality == m_vote) || m_feedbackMode == 1)	// Draw Positive or Best Only
	{
		const double scale = m_barScales[m_vote] < 0.0 ? 0.0 : m_barScales[m_vote];
		const gint x       = (m_vote + 1) * m_modalityX,						// Get the pos X of the bar
				   h       = gint(scale * m_barH);								// Get the height of the bar
		gdk_pixbuf_render_to_drawable(m_bar, m_widget->window, nullptr, 0, m_barH - h, x - m_barW / 2, m_modalityY - h, m_barW, h, GDK_RGB_DITHER_NONE, 0, 0);
		m_oBarSize->getBuffer()[m_vote] = 100.0 * scale;						// Update the displayed bar size 
	}
	else if (m_feedbackMode == 2)												// Draw All
	{
		gint x = m_modalityX;
		for (size_t i = 0; i < m_nbModality; ++i)								// For each Modality
		{
			const double scale = m_barScales[i] < 0.0 ? 0.0 : m_barScales[i];
			const gint h       = gint(scale * m_barH);							// Get the H of the modality
			gdk_pixbuf_render_to_drawable(m_bar, m_widget->window, nullptr, 0, m_barH - h, x - m_barW / 2, m_modalityY - h, m_barW, h, GDK_RGB_DITHER_NONE, 0,
										  0);
			x += m_modalityX;
			m_oBarSize->getBuffer()[i] = 100.0 * scale;							// Update the displayed bar size 
		}
	}
}
//---------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------------
void CGrazMultiVisualization::drawAccuracy() const
{
	const double* buffer = m_oConfusion->getBuffer();
	PangoLayout* layout  = pango_layout_new(gdk_pango_context_get());

	const gint stepX  = 40, stepY = 16;
	const gint startX = 8, startY = 16;
	gint x            = startX, y = startY;

	std::stringstream ss;
	ss << std::setfill('0');
	size_t idx         = 0;
	size_t predictions = 0, good = 0;

	for (size_t i = 0; i < m_nbModality; ++i) {
		for (size_t j = 0; j < m_nbModality; ++j) {
			ss.str(std::string());
			const int val = int(buffer[idx++]);
			ss << std::setw(3) << val;
			predictions += val;
			pango_layout_set_text(layout, ss.str().c_str(), -1);
			if (i == j) {
				gdk_draw_layout(m_widget->window, m_widget->style->white_gc, x, y, layout);
				good += val;
			}
			else { gdk_draw_layout(m_widget->window, m_widget->style->fg_gc[GTK_WIDGET_STATE(m_widget)], x, y, layout); }
			x += stepX;
		}
		x = startX;
		y += stepY;
	}
	ss.str(std::string());
	ss << "Acc = " << std::fixed << std::setprecision(1) << (predictions == 0 ? 0.0 : 100.0 * double(good) / double(predictions));
	pango_layout_set_text(layout, ss.str().c_str(), -1);
	gdk_draw_layout(m_widget->window, m_widget->style->white_gc, x + (gint(m_nbModality) * stepX), y - stepY, layout);

	g_object_unref(layout);
}
//---------------------------------------------------------------------------------------------------

//********************		
//***** COMPUTES *****	
//********************
//---------------------------------------------------------------------------------------------------
void CGrazMultiVisualization::aggregatePredictions(const bool all)
{
	if (m_amplitudes.size() >= m_nbPredictionsMin) {
		m_vote = 0;
		// step backwards with rev iter to take the mean of the latest samples
		uint64_t count = 0;
		double sum     = 0;
		std::vector<double> amp(m_nbModality, 0);
		//double maxA = -DBL_MAX;
		for (auto a = m_amplitudes.rbegin(); a != m_amplitudes.rend() && (all || count < m_nbPredictionsMin); ++a, ++count) {
			for (size_t i = 0; i < a->size(); ++i) { amp[i] += a->at(i); }
		}
		for (const auto& a : amp) { sum += a; }
		// Computes bar scale for each modality as a probability (other possibility substract the min/max value or the mean of the values)
		// Then we put this probability between 0-1 to 0-1 but when 0 is equal to 1/m_nbModality 
		const double minV    = 1.0 / double(m_nbModality),
					 factorV = 1.0 / (1 - minV);
		for (size_t i = 0; i < amp.size(); ++i) {
			m_barScales[i] = (amp[i] / sum - minV) * factorV;
			if (amp[m_vote] < amp[i]) { m_vote = int(i); }
		}
	}
}
//---------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------------
void CGrazMultiVisualization::updateConfusionMatrix() const
{
	// Col = Expected modality, Row = Computed modality
	if (m_modality != -1 && m_vote != -1) { (m_oConfusion->getBuffer())[m_vote * m_nbModality + m_modality]++; }
	m_oBarSize->resetBuffer();
}
//---------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------------
void CGrazMultiVisualization::setMatrixBuffer(const double* buffer)
{
	if (m_state != EStates::Feedback) { return; }					// No continuous feedback
	// Ad-hoc forcing to probability (range [0,1], sum to 1). This will make scaling easier 
	// if run forever in a continuous mode. If the input is already scaled this way, no effect.
	double sum = 0;
	std::vector<double> values;
	values.reserve(m_nbModality);
	for (size_t i = 0; i < m_nbModality; ++i) {
		const double v = std::abs(buffer[i]);
		values.emplace_back(v);
		sum += v;
	}

	if (sum != 0.0) { for (auto& v : values) { v /= sum; } }
	else { for (auto& v : values) { v = 1.0 / double(m_nbModality); } }

	m_amplitudes.emplace_back(values);		// Add this buffer to the list
	if (m_feedbackMode != 3 && !m_delayFeedback) {
		aggregatePredictions(false);
		gdk_window_invalidate_rect(m_widget->window, nullptr, true);
	}
}
//---------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------------
void CGrazMultiVisualization::setStimulation(const uint64_t stimulation)
{
	bool needRedraw = true;
	switch (stimulation) {
		// Classical stimulations
		case OVTK_GDF_End_Of_Session:
			m_state = EStates::Black;									// Idle State Black Screen
			break;

		case OVTK_GDF_End_Of_Trial:
			m_state = EStates::Black;									// Idle State Black Screen
			aggregatePredictions(true);
			updateConfusionMatrix();
			break;

		case OVTK_GDF_Start_Of_Trial:
		case OVTK_GDF_Cross_On_Screen:
			m_state = EStates::Cross;									// Draw Reference State and cross
			break;

		case OVTK_GDF_Feedback_Continuous:
			m_state = EStates::Feedback;								// Draw Feedback State
			break;

		default:													// Modalities stimulations
			m_modality = -1;										// Initialize modality number
			m_vote = -1;											// Initialize vote
			m_amplitudes.clear();									// Clear the previous amplitudes
			for (auto& s : m_barScales) { s = 0; }					// Reinit Bar Scales
			for (size_t i = 0; i < m_stimlist.size(); ++i) {
				if (m_stimlist[i] == stimulation) {
					m_modality = int(i);
					break;											// stop the loop (but not so usefull big number of modality is not so big generally)
				}
			}
			if (m_modality != -1) { m_state = EStates::Instruction; }	// If recognize stimulation	Draw Modality State
			else { needRedraw = false; }							// If not, we don't care about stimulations and we don't want to redraw
			break;
	}
	// Queue the stimulation to be sent to TCP Tagging
	m_stimuliQueue.push_back(stimulation);

	// Indicates that the window is invalidate (must be redrawn)
	if (needRedraw && GTK_WIDGET(m_widget)->window) { gdk_window_invalidate_rect(GTK_WIDGET(m_widget)->window, nullptr, true); }
}
//---------------------------------------------------------------------------------------------------

//**************************	
//***** CALLBACKS HACK *****	
//**************************
//---------------------------------------------------------------------------------------------------
// Note that we don't need concurrency control here as gtk callbacks run in the main thread
void CGrazMultiVisualization::FlushQueue()
{
	for (const auto i : m_stimuliQueue) { m_stimulusSender->sendStimulation(i); }
	m_stimuliQueue.clear();
	m_idleFuncTag = 0;	// This function will be automatically removed after completion, so set to 0
}
//---------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------------
void CGrazMultiVisualization::Resize(const size_t width, const size_t height)
{
	//***** Variables update *****
	m_windowW = gint(width < 8 ? 8 : width);				// Windows Width minimum to avoïd 0 sizes
	m_windowH = gint(height < 8 ? 8 : height);				// Windows Height minimum to avoïd 0 sizes
	m_margin  = gint(0.01 * double(MIN(width, height)));	// Margin 1% of the minimum between width and height

	const gint drawAreaW = gint(width) - 2 * m_margin,		// Drawing area Width (without margin)
			   drawAreaH = gint(height) - 2 * m_margin;		// Drawing area Height (without margin)

	m_barH = gint(0.80 * drawAreaH);						// Height of the Graz Bar
	m_barW = MIN(m_barH / 6, (drawAreaW / gint(m_nbModality)) - m_margin);	// Keep some proportion

	m_modalityX = drawAreaW / gint(m_nbModality + 1);		// Center X Position of the first modality (next is in x+(x+margin))
	m_modalityY = m_barH + m_margin;						// Bottom Y Position of the modalities
	m_modalityW = (m_barW + 2 * m_margin) / 2;				// Half width dedicated for the modalities

	const int wL = m_windowW / 3, hL = m_windowH / 3, wS = m_barW;

	//***** Images update *****
	for (size_t i = 0; i < m_originalImgs.size(); ++i) {
		if (m_smallImgs[i]) { g_object_unref(G_OBJECT(m_smallImgs[i])); }		// Delete
		if (m_largeImgs[i]) { g_object_unref(G_OBJECT(m_largeImgs[i])); }		// Delete
		m_smallImgs[i] = RescalePixbuf(m_originalImgs[i], wS, wS);				// Rescale
		m_largeImgs[i] = RescalePixbuf(m_originalImgs[i], wL, hL);				// Rescale
	}
	if (m_originalBar) {
		if (m_bar) { g_object_unref(G_OBJECT(m_bar)); }							// Delete
		m_bar = RescalePixbuf(m_originalBar, m_barW, m_barH, false);
	}
}
//---------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------------
bool CGrazMultiVisualization::initWindow()
{
	m_widget = GTK_WIDGET(gtk_drawing_area_new());		// Creation
	gtk_widget_set_size_request(m_widget, 400, 300);	// Minimum Window Size
	gtk_widget_set_double_buffered(m_widget, TRUE);		// Double Buffer

	//set widget basic color
	const GdkColor bgColor = InitGDKColor(0, 0, 0, 0); 		// = { 0, 0, 0, 0 };		// pixel, red, green, blue (black) vs 2013 doesn't allow this initialization
	const GdkColor fgColor = InitGDKColor(0, 0, 32768, 0);	// = { 0, 0, 32768, 0 };	// pixel, red, green, blue (dark green)

	gtk_widget_modify_bg(m_widget, GTK_STATE_NORMAL, &bgColor);
	gtk_widget_modify_bg(m_widget, GTK_STATE_PRELIGHT, &bgColor);
	gtk_widget_modify_bg(m_widget, GTK_STATE_ACTIVE, &bgColor);

	gtk_widget_modify_fg(m_widget, GTK_STATE_NORMAL, &fgColor);
	gtk_widget_modify_fg(m_widget, GTK_STATE_PRELIGHT, &fgColor);
	gtk_widget_modify_fg(m_widget, GTK_STATE_ACTIVE, &fgColor);

	m_visualizationCtx = dynamic_cast<VisualizationToolkit::IVisualizationContext*>(this->createPluginObject(OVP_ClassId_Plugin_VisualizationCtx));
	m_visualizationCtx->setWidget(*this, m_widget);
	g_signal_connect(G_OBJECT(m_widget), "size-allocate", G_CALLBACK(ResizeCB), this);
	g_signal_connect(G_OBJECT(m_widget), "expose-event", G_CALLBACK(RedrawCB), this);

	return true;
}
//---------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------------
bool CGrazMultiVisualization::initImages(const std::vector<std::string>& paths)
{
	const size_t s = paths.size();
	if (s != m_nbModality + 1) { return false; }
	m_originalImgs.resize(s);
	m_largeImgs.resize(s);
	m_smallImgs.resize(s);

	// Bar
	m_originalBar = gdk_pixbuf_new_from_file_at_size(Directories::getDataDir() + "/plugins/simple-visualization/graz/bar.png", -1, -1, nullptr);
	if (!m_originalBar) { return false; }

	// Modalities
	for (size_t i = 0; i < s; ++i) {
		m_originalImgs[i] = gdk_pixbuf_new_from_file_at_size(paths[i].c_str(), -1, -1, nullptr);
		if (!m_originalImgs[i]) { return false; }
	}

	Resize(400, 300);	// Initialization

	return true;
}
//---------------------------------------------------------------------------------------------------

//****************	
//***** MISC *****	
//****************
//---------------------------------------------------------------------------------------------------
std::string CGrazMultiVisualization::infos() const
{
	std::stringstream ss;
	ss << "\n";
	ss << "Show instruction : " << (m_showInstruction ? "yes" : "no") << ", Feedback Mode : ";
	if (m_feedbackMode == 0) { ss << "Positive Only"; }
	else if (m_feedbackMode == 1) { ss << "Best Only"; }
	else if (m_feedbackMode == 2) { ss << "All"; }
	else if (m_feedbackMode == 3) { ss << "None"; }
	ss << ", Delay : " << (m_delayFeedback ? "yes" : "no") << "\n";
	ss << "Show Accuracy : " << (m_showAccuracy ? "yes" : "no") << ", Nb Predictions : " << m_nbPredictionsMin << ", Nb Modality : " << m_nbModality << "\n";
	ss << "Number of Images : " << m_originalImgs.size() << ", " << m_smallImgs.size() << ", " << m_largeImgs.size() << "\n";
	ss << "\tWindow W : " << m_windowW << "\tWindow H : " << m_windowH << "\tMargin : " << m_margin << "\tBar W : " << m_barW << "\tBar H : " << m_barH << "\n";

	if (m_originalBar) { ss << "\tO Bar W : " << gdk_pixbuf_get_width(m_originalBar) << "\tO Bar H : " << gdk_pixbuf_get_height(m_originalBar); }
	else { ss << "\tNo O Bar"; }
	if (m_bar) { ss << "\tR Bar W : " << gdk_pixbuf_get_width(m_bar) << "\tR Bar H : " << gdk_pixbuf_get_height(m_bar); }
	else { ss << "\tNo R Bar"; }
	ss << "\n";

	for (size_t i = 0; i < m_originalImgs.size(); ++i) {
		//*
		if (m_originalImgs[i]) {
			ss << "\tO Img " << i << " W : " << gdk_pixbuf_get_width(m_originalImgs[i]) << "\tO Img " << i << " H : " << gdk_pixbuf_get_height(
				m_originalImgs[i]);
		}
		else { ss << "\tNo O Img " << i; }

		if (m_smallImgs[i]) {
			ss << "\tS Img " << i << " W : " << gdk_pixbuf_get_width(m_smallImgs[i]) << "\tS Img " << i << " H : " << gdk_pixbuf_get_height(m_smallImgs[i]);
		}
		else { ss << "\tNo S Img " << i; }

		if (m_largeImgs[i]) {
			ss << "\tL Img " << i << " W : " << gdk_pixbuf_get_width(m_largeImgs[i]) << "\tL Img " << i << " H : " << gdk_pixbuf_get_height(m_largeImgs[i]);
		}
		else { ss << "\tNo L Img " << i; }
		ss << "\n";
	}
	ss << "Stimulation List : ";
	for (const auto& stim : m_stimlist) { ss << stim << " "; }
	ss << "\n";
	return ss.str();
}
//---------------------------------------------------------------------------------------------------
}  // namespace SimpleVisualization
}  // namespace Plugins
}  // namespace OpenViBE
