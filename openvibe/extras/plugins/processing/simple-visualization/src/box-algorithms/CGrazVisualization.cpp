///-------------------------------------------------------------------------------------------------
/// 
/// \file CGrazVisualization.cpp
/// \brief Classes implementation for the Box Graz visualization.
/// \author Bruno Renier, Jussi T. Lindgren (INRIA/IRISA).
/// \version 0.2.
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

#include "CGrazVisualization.hpp"
#include <tcptagging/IStimulusSender.h>

#include <algorithm> // std::min, max
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <sys/timeb.h>

#if defined TARGET_OS_Linux
#include <unistd.h>
#endif

namespace OpenViBE {
namespace Plugins {
namespace SimpleVisualization {

// This callback flushes all accumulated stimulations to the TCP Tagging 
// after the rendering has completed.
static gboolean FlushCB(gpointer data)
{
	reinterpret_cast<CGrazVisualization*>(data)->FlushQueue();
	return FALSE;	// Only run once
}

static gboolean ResizeCB(GtkWidget* /*widget*/, GtkAllocation* allocation, gpointer data)
{
	reinterpret_cast<CGrazVisualization*>(data)->Resize(size_t(allocation->width), size_t(allocation->height));
	return FALSE;
}

static gboolean RedrawCB(GtkWidget* /*widget*/, GdkEventExpose* /*event*/, gpointer data)
{
	reinterpret_cast<CGrazVisualization*>(data)->Redraw();
	return TRUE;
}

// n.b. This reacts immediately to the received stimulation and doesn't use the date. Usually stimulations come from the upstream with
// chunks having a very narrow time range, so its alright for Graz that changes state only rarely. Note if multiple stimulations are 
// received in the same chunk, they'll be passed to TCP Tagging with the true delay between them lost.
void CGrazVisualization::setStimulation(const size_t /*stimulationIndex*/, const uint64_t identifier, const uint64_t /*stimulationDate*/)
{
	/*
	OVTK_GDF_Start_Of_Trial
	OVTK_GDF_Cross_On_Screen
	OVTK_GDF_Left
	OVTK_GDF_Right
	*/
	bool stateUpdated = false;

	m_LastStimulation = identifier;
	switch (identifier) {
		case OVTK_GDF_End_Of_Trial:
			m_CurrentState = EStates::Idle;
			stateUpdated = true;
			if (m_ShowAccuracy || m_DelayFeedback) {
				const double prediction = aggregatePredictions(true);
				updateConfusionMatrix(prediction);
				m_BarScale = prediction;
			}
			break;

		case OVTK_GDF_End_Of_Session:
			m_CurrentState = EStates::Idle;
			stateUpdated = true;
			if (m_ShowFeedback) {
				m_BarScale = 0;
				drawBar();
			}
			break;

		case OVTK_GDF_Cross_On_Screen:
			m_CurrentState = EStates::Reference;
			stateUpdated = true;
			break;

		case OVTK_GDF_Beep:
			// gdk_beep();
			getBoxAlgorithmContext()->getPlayerContext()->getLogManager() << Kernel::LogLevel_Trace <<
					"Beep is no more considered in 'Graz Visu', use the 'Sound player' for this!\n";
#if 0
#if defined TARGET_OS_Linux
		system("cat /local/ov_beep.wav > /dev/dsp &");
#endif
#endif
			break;

		case OVTK_GDF_Left:
			m_CurrentState = EStates::Cue;
			m_CurrentDirection = EArrowDirections::Left;
			stateUpdated       = true;
			break;

		case OVTK_GDF_Right:
			m_CurrentState = EStates::Cue;
			m_CurrentDirection = EArrowDirections::Right;
			stateUpdated       = true;
			break;

		case OVTK_GDF_Up:
			m_CurrentState = EStates::Cue;
			m_CurrentDirection = EArrowDirections::Up;
			stateUpdated       = true;
			break;

		case OVTK_GDF_Down:
			m_CurrentState = EStates::Cue;
			m_CurrentDirection = EArrowDirections::Down;
			stateUpdated       = true;
			break;

		case OVTK_GDF_Feedback_Continuous:
			// New trial starts

			m_CurrentState = EStates::ContinousFeedback;
			m_Amplitudes.clear();

		// as some trials may have artifacts and hence very high responses from e.g. LDA
		// its better to reset the max between trials
			m_MaxAmplitude = -DBL_MAX;
			m_BarScale     = 0;

			stateUpdated = true;
			break;
		default: break;
	}

	if (stateUpdated) { processState(); }

	// Queue the stimulation to be sent to TCP Tagging
	m_StimuliQueue.push_back(m_LastStimulation);
}

void CGrazVisualization::processState() const
{
	switch (m_CurrentState) {
		case EStates::Reference:
		case EStates::Cue:
		case EStates::Idle:
		case EStates::ContinousFeedback:
			if (GTK_WIDGET(m_DrawingArea)->window) { gdk_window_invalidate_rect(GTK_WIDGET(m_DrawingArea)->window, nullptr, true); }
			break;

		default:
			break;
	}
}

bool CGrazVisualization::initialize()
{
	m_StimulationDecoder.initialize(*this, 0);
	m_MatrixDecoder.initialize(*this, 1);
	m_confusionEncoder.initialize(*this, 0);
	m_oConfusion = m_confusionEncoder.getInputMatrix();

	m_ShowInstruction        = FSettingValueAutoCast(*this->getBoxAlgorithmContext(), 0);
	m_ShowFeedback           = FSettingValueAutoCast(*this->getBoxAlgorithmContext(), 1);
	m_DelayFeedback          = FSettingValueAutoCast(*this->getBoxAlgorithmContext(), 2);
	m_ShowAccuracy           = FSettingValueAutoCast(*this->getBoxAlgorithmContext(), 3);
	m_PredictionsToIntegrate = FSettingValueAutoCast(*this->getBoxAlgorithmContext(), 4);
	m_PositiveFeedbackOnly   = FSettingValueAutoCast(*this->getBoxAlgorithmContext(), 5);

	m_StimulusSender = nullptr;

	m_IdleFuncTag = 0;
	m_StimuliQueue.clear();

	if (m_PredictionsToIntegrate < 1) {
		this->getLogManager() << Kernel::LogLevel_Error << "Number of predictions to integrate must be at least 1!";
		return false;
	}

	m_oConfusion->resize(2, 2);

	//load the gtk builder interface
	m_Builder =
			gtk_builder_new(); // glade_xml_new(Directories::getDataDir() + "/plugins/simple-visualization/openvibe-simple-visualization-GrazVisualization.ui", nullptr, nullptr);
	gtk_builder_add_from_file(m_Builder,
							  Directories::getDataDir() + "/plugins/simple-visualization/openvibe-simple-visualization-GrazVisualization.ui", nullptr);

	if (!m_Builder) {
		this->getLogManager() << Kernel::LogLevel_Error << "Error: couldn't load the interface!";
		return false;
	}

	gtk_builder_connect_signals(m_Builder, nullptr);

	m_DrawingArea = GTK_WIDGET(gtk_builder_get_object(m_Builder, "GrazVisualizationDrawingArea"));
	g_signal_connect(G_OBJECT(m_DrawingArea), "expose-event", G_CALLBACK(RedrawCB), this);
	g_signal_connect(G_OBJECT(m_DrawingArea), "size-allocate", G_CALLBACK(ResizeCB), this);

#if 0
	//does nothing on the main window if the user tries to close it
	g_signal_connect(G_OBJECT(gtk_builder_get_object(m_BuilderInterface, "GrazVisualizationWindow")), "delete_event", G_CALLBACK(gtk_widget_do_nothing), nullptr);
	//creates the window
	m_mainWindow = GTK_WIDGET(gtk_builder_get_object(m_BuilderInterface, "GrazVisualizationWindow"));
#endif

	//set widget bg color
	gtk_widget_modify_bg(m_DrawingArea, GTK_STATE_NORMAL, &m_BackgroundColor);
	gtk_widget_modify_bg(m_DrawingArea, GTK_STATE_PRELIGHT, &m_BackgroundColor);
	gtk_widget_modify_bg(m_DrawingArea, GTK_STATE_ACTIVE, &m_BackgroundColor);

	gtk_widget_modify_fg(m_DrawingArea, GTK_STATE_NORMAL, &m_ForegroundColor);
	gtk_widget_modify_fg(m_DrawingArea, GTK_STATE_PRELIGHT, &m_ForegroundColor);
	gtk_widget_modify_fg(m_DrawingArea, GTK_STATE_ACTIVE, &m_ForegroundColor);

	//arrows
	m_OriginalLeftArrow = gdk_pixbuf_new_from_file_at_size(
		Directories::getDataDir() + "/plugins/simple-visualization/openvibe-simple-visualization-GrazVisualization-leftArrow.png", -1, -1, nullptr);
	m_OriginalRightArrow = gdk_pixbuf_new_from_file_at_size(
		Directories::getDataDir() + "/plugins/simple-visualization/openvibe-simple-visualization-GrazVisualization-rightArrow.png", -1, -1, nullptr);
	m_OriginalUpArrow = gdk_pixbuf_new_from_file_at_size(
		Directories::getDataDir() + "/plugins/simple-visualization/openvibe-simple-visualization-GrazVisualization-upArrow.png", -1, -1, nullptr);
	m_OriginalDownArrow = gdk_pixbuf_new_from_file_at_size(
		Directories::getDataDir() + "/plugins/simple-visualization/openvibe-simple-visualization-GrazVisualization-downArrow.png", -1, -1, nullptr);

	if (!m_OriginalLeftArrow || !m_OriginalRightArrow || !m_OriginalUpArrow || !m_OriginalDownArrow) {
		this->getLogManager() << Kernel::LogLevel_Error << "Error couldn't load arrow resource files!\n";
		return false;
	}

	//bar
	m_OriginalBar = gdk_pixbuf_new_from_file_at_size(
		Directories::getDataDir() + "/plugins/simple-visualization/openvibe-simple-visualization-GrazVisualization-bar.png", -1, -1, nullptr);
	if (!m_OriginalBar) {
		this->getLogManager() << Kernel::LogLevel_Error << "Error couldn't load bar resource file!\n";
		return false;
	}

#if 0
	gtk_widget_show_all(m_mainWindow);
#endif
	m_visualizationCtx = dynamic_cast<VisualizationToolkit::IVisualizationContext*>(this->createPluginObject(OVP_ClassId_Plugin_VisualizationCtx));
	m_visualizationCtx->setWidget(*this, m_DrawingArea);

	m_StimulusSender = TCPTagging::CreateStimulusSender();

	if (!m_StimulusSender->connect("localhost", "15361")) {
		this->getLogManager() << Kernel::LogLevel_Warning << "Unable to connect to AS's TCP Tagging plugin, stimuli wont be forwarded.\n";
	}

	return true;
}

bool CGrazVisualization::uninitialize()
{
	//***** Print Confusion Matrix *****
	this->getLogManager() << Kernel::LogLevel_Info << "Confusion Matrix : \n";
	const double* buffer = m_oConfusion->getBuffer();
	std::stringstream ss;
	ss << std::setfill('0');
	size_t idx         = 0;
	size_t predictions = 0, good = 0;

	for (size_t i = 0; i < 2; ++i) {
		for (size_t j = 0; j < 2; ++j) {
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
	m_oConfusion = nullptr;
	m_confusionEncoder.uninitialize();

	if (m_IdleFuncTag) {
		m_StimuliQueue.clear();
		g_source_remove(m_IdleFuncTag);
		m_IdleFuncTag = 0;
	}

	delete m_StimulusSender;

	m_StimulationDecoder.uninitialize();
	m_MatrixDecoder.uninitialize();

	//destroy drawing area
	if (m_DrawingArea) {
		gtk_widget_destroy(m_DrawingArea);
		m_DrawingArea = nullptr;
	}

	/* unref the xml file as it's not needed anymore */
	g_object_unref(G_OBJECT(m_Builder));
	m_Builder = nullptr;

	if (m_OriginalBar) { g_object_unref(G_OBJECT(m_OriginalBar)); }
	if (m_LeftBar) { g_object_unref(G_OBJECT(m_LeftBar)); }
	if (m_RightBar) { g_object_unref(G_OBJECT(m_RightBar)); }
	if (m_LeftArrow) { g_object_unref(G_OBJECT(m_LeftArrow)); }
	if (m_RightArrow) { g_object_unref(G_OBJECT(m_RightArrow)); }
	if (m_UpArrow) { g_object_unref(G_OBJECT(m_UpArrow)); }
	if (m_DownArrow) { g_object_unref(G_OBJECT(m_DownArrow)); }
	if (m_OriginalLeftArrow) { g_object_unref(G_OBJECT(m_OriginalLeftArrow)); }
	if (m_OriginalRightArrow) { g_object_unref(G_OBJECT(m_OriginalRightArrow)); }
	if (m_OriginalUpArrow) { g_object_unref(G_OBJECT(m_OriginalUpArrow)); }
	if (m_OriginalDownArrow) { g_object_unref(G_OBJECT(m_OriginalDownArrow)); }

	if (m_visualizationCtx) {
		this->releasePluginObject(m_visualizationCtx);
		m_visualizationCtx = nullptr;
	}

	return true;
}

bool CGrazVisualization::processInput(const size_t /*index*/)
{
	getBoxAlgorithmContext()->markAlgorithmAsReadyToProcess();
	return true;
}

bool CGrazVisualization::process()
{
	Kernel::IBoxIO* boxIO = getBoxAlgorithmContext()->getDynamicBoxContext();

	for (size_t chunk = 0; chunk < boxIO->getInputChunkCount(0); ++chunk) {
		m_StimulationDecoder.decode(chunk);
		if (m_StimulationDecoder.isBufferReceived()) {
			const CStimulationSet* stimSet = m_StimulationDecoder.getOutputStimulationSet();
			for (size_t s = 0; s < stimSet->size(); ++s) { setStimulation(s, stimSet->getId(s), stimSet->getDate(s)); }
		}
	}

	for (size_t chunk = 0; chunk < boxIO->getInputChunkCount(1); ++chunk) {
		m_MatrixDecoder.decode(chunk);

		if (m_MatrixDecoder.isHeaderReceived()) {
			const CMatrix* matrix = m_MatrixDecoder.getOutputMatrix();

			if (matrix->getDimensionCount() == 0) {
				this->getLogManager() << Kernel::LogLevel_Error << "Error, dimension count is 0 for Amplitude input !\n";
				return false;
			}

			if (matrix->getDimensionCount() > 1) {
				for (size_t k = 1; k < matrix->getDimensionSize(k); ++k) {
					if (matrix->getDimensionSize(k) > 1) {
						this->getLogManager() << Kernel::LogLevel_Error << "Error, only column vectors supported as Amplitude!\n";
						return false;
					}
				}
			}

			if (matrix->getDimensionSize(0) == 0) {
				this->getLogManager() << Kernel::LogLevel_Error << "Error, need at least 1 dimension in Amplitude input !\n";
				return false;
			}
			if (matrix->getDimensionSize(0) >= 2) {
				this->getLogManager() << Kernel::LogLevel_Trace <<
						"Got 2 or more dimensions for feedback, feedback will be the difference between the first two.\n";
				m_TwoValueInput = true;
			}
			m_confusionEncoder.encodeHeader();
		}

		if (m_MatrixDecoder.isBufferReceived()) {
			setMatrixBuffer(m_MatrixDecoder.getOutputMatrix()->getBuffer());
			m_confusionEncoder.encodeBuffer();
		}
		if (m_MatrixDecoder.isEndReceived()) { m_confusionEncoder.encodeBuffer(); }
		boxIO->markOutputAsReadyToSend(0, boxIO->getInputChunkStartTime(1, chunk), boxIO->getInputChunkEndTime(1, chunk));
	}

	// After any possible rendering, we flush the accumulated stimuli. The default idle func is low priority, so it should be run after rendering by gtk.
	// Only register a single idle func, if the previous is there its just as good
	if (m_IdleFuncTag == 0) { m_IdleFuncTag = g_idle_add(FlushCB, this); }

	return true;
}

void CGrazVisualization::Redraw()
{
	switch (m_CurrentState) {
		case EStates::Reference:
			drawReferenceCross();
			break;

		case EStates::Cue:
			drawReferenceCross();
			drawArrow(m_ShowInstruction ? m_CurrentDirection : EArrowDirections::None);
			break;

		case EStates::ContinousFeedback:
			drawReferenceCross();
			if (m_ShowFeedback && !m_DelayFeedback) { drawBar(); }
			break;

		case EStates::Idle:
			if (m_ShowFeedback && m_DelayFeedback) { drawBar(); }
			break;

		default: break;
	}
	if (m_ShowAccuracy) { drawAccuracy(); }
}

void CGrazVisualization::drawReferenceCross() const
{
	const gint width  = m_DrawingArea->allocation.width,
			   height = m_DrawingArea->allocation.height;

	//increase line's width
	gdk_gc_set_line_attributes(m_DrawingArea->style->fg_gc[GTK_WIDGET_STATE(m_DrawingArea)], 1, GDK_LINE_SOLID, GDK_CAP_BUTT, GDK_JOIN_BEVEL);

	//horizontal line
	gdk_draw_line(m_DrawingArea->window, m_DrawingArea->style->fg_gc[GTK_WIDGET_STATE(m_DrawingArea)], (width / 4), (height / 2), ((3 * width) / 4),
				  (height / 2));
	//vertical line
	gdk_draw_line(m_DrawingArea->window, m_DrawingArea->style->fg_gc[GTK_WIDGET_STATE(m_DrawingArea)], (width / 2), (height / 4), (width / 2),
				  ((3 * height) / 4));

	//increase line's width
	gdk_gc_set_line_attributes(m_DrawingArea->style->fg_gc[GTK_WIDGET_STATE(m_DrawingArea)], 1, GDK_LINE_SOLID, GDK_CAP_BUTT, GDK_JOIN_BEVEL);
}

void CGrazVisualization::drawArrow(const EArrowDirections direction)
{
	const gint width  = m_DrawingArea->allocation.width,
			   height = m_DrawingArea->allocation.height;

	gint x = (width / 2), y = (height / 2);

	switch (direction) {
		case EArrowDirections::None:
			this->drawArrow(EArrowDirections::Left);
			this->drawArrow(EArrowDirections::Right);
			break;

		case EArrowDirections::Left:
			x -= gdk_pixbuf_get_width(m_LeftArrow) - 1;
			y -= gdk_pixbuf_get_height(m_LeftArrow) / 2;
			gdk_draw_pixbuf(m_DrawingArea->window, nullptr, m_LeftArrow, 0, 0, x, y, -1, -1, GDK_RGB_DITHER_NONE, 0, 0);
			break;

		case EArrowDirections::Right:
			x += 2;
			y -= gdk_pixbuf_get_height(m_RightArrow) / 2;
			gdk_draw_pixbuf(m_DrawingArea->window, nullptr, m_RightArrow, 0, 0, x, y, -1, -1, GDK_RGB_DITHER_NONE, 0, 0);
			break;

		case EArrowDirections::Up:
			x -= gdk_pixbuf_get_width(m_UpArrow) / 2;
			y -= gdk_pixbuf_get_height(m_UpArrow) - 1;
			gdk_draw_pixbuf(m_DrawingArea->window, nullptr, m_UpArrow, 0, 0, x, y, -1, -1, GDK_RGB_DITHER_NONE, 0, 0);
			break;

		case EArrowDirections::Down:
			x -= gdk_pixbuf_get_width(m_DownArrow) / 2;
			y += 2;
			gdk_draw_pixbuf(m_DrawingArea->window, nullptr, m_DownArrow, 0, 0, x, y, -1, -1, GDK_RGB_DITHER_NONE, 0, 0);
			break;

		default: break;
	}
}

void CGrazVisualization::drawBar() const
{
	const gint width  = m_DrawingArea->allocation.width;
	const gint height = m_DrawingArea->allocation.height;

	double usedScale = m_BarScale;
	if (m_PositiveFeedbackOnly) {
		// @fixme for multiclass
		const size_t trueDirection = size_t(m_CurrentDirection) - 1;
		const size_t thisVote      = (m_BarScale < 0 ? 0 : 1);
		if (trueDirection != thisVote) { usedScale = 0; }
	}

	gint w       = gint(fabs(width * fabs(usedScale) / 2));
	w            = (w > (width / 2)) ? (width / 2) : w;
	gint x       = width / 2;
	const gint h = height / 6;
	const gint y = (height / 2) - (h / 2);

	if (m_BarScale < 0) {
		x -= w;
		gdk_pixbuf_render_to_drawable(m_LeftBar, m_DrawingArea->window, nullptr, gdk_pixbuf_get_width(m_LeftBar) - w, 0, x, y, w, h, GDK_RGB_DITHER_NONE, 0, 0);
	}
	else { gdk_pixbuf_render_to_drawable(m_RightBar, m_DrawingArea->window, nullptr, 0, 0, x, y, w, h, GDK_RGB_DITHER_NONE, 0, 0); }
}

void CGrazVisualization::drawAccuracy() const
{
	std::stringstream tmp;
	PangoLayout* layout = pango_layout_new(gdk_pango_context_get());

	const double* buffer = m_oConfusion->getBuffer();

	pango_layout_set_text(layout, "L", -1);
	gdk_draw_layout(m_DrawingArea->window, m_DrawingArea->style->fg_gc[GTK_WIDGET_STATE(m_DrawingArea)], 8, 16, layout);

	tmp << std::setfill('0') << std::setw(3) << int(buffer[0]);
	pango_layout_set_text(layout, tmp.str().c_str(), -1);
	gdk_draw_layout(m_DrawingArea->window, m_DrawingArea->style->white_gc, 8 + 16, 16, layout);

	tmp.str(std::string());
	tmp << std::setw(3) << int(buffer[1]);
	pango_layout_set_text(layout, tmp.str().c_str(), -1);
	gdk_draw_layout(m_DrawingArea->window, m_DrawingArea->style->fg_gc[GTK_WIDGET_STATE(m_DrawingArea)], 8 + 56, 16, layout);

	pango_layout_set_text(layout, "R", -1);
	gdk_draw_layout(m_DrawingArea->window, m_DrawingArea->style->fg_gc[GTK_WIDGET_STATE(m_DrawingArea)], 8, 32, layout);

	tmp.str(std::string());
	tmp << std::setw(3) << int(buffer[2]);
	pango_layout_set_text(layout, tmp.str().c_str(), -1);
	gdk_draw_layout(m_DrawingArea->window, m_DrawingArea->style->fg_gc[GTK_WIDGET_STATE(m_DrawingArea)], 8 + 16, 32, layout);

	tmp.str(std::string());
	tmp << std::setw(3) << int(buffer[3]);
	pango_layout_set_text(layout, tmp.str().c_str(), -1);
	gdk_draw_layout(m_DrawingArea->window, m_DrawingArea->style->white_gc, 8 + 56, 32, layout);

	size_t predictions = 0;
	for (size_t i = 0; i < 4; ++i) { predictions += int(buffer[i]); }

	tmp.str(std::string());
	tmp << "Acc = " << std::fixed << std::setprecision(1) << (predictions == 0 ? 0.0 : 100.0 * (buffer[0] + buffer[3]) / double(predictions)) << "%";
	pango_layout_set_text(layout, tmp.str().c_str(), -1);
	gdk_draw_layout(m_DrawingArea->window, m_DrawingArea->style->white_gc, 8 + 96, 32, layout);


	g_object_unref(layout);
}

double CGrazVisualization::aggregatePredictions(const bool includeAll)
{
	double voteAggregate = 0;

	// Do we have enough predictions to integrate a result?
	if (m_Amplitudes.size() >= m_PredictionsToIntegrate) {
		// step backwards with rev iter to take the latest samples
		uint64_t count = 0;
		for (auto a = m_Amplitudes.rbegin(); a != m_Amplitudes.rend() && (includeAll || count < m_PredictionsToIntegrate); ++a, ++count) {
			voteAggregate += *a;
			m_MaxAmplitude = std::max<double>(m_MaxAmplitude, abs(*a));
		}

		voteAggregate /= m_MaxAmplitude;
		voteAggregate /= double(count);
	}

	return voteAggregate;
}

// @fixme for >2 classes
void CGrazVisualization::updateConfusionMatrix(const double prediction)
{
	if (m_CurrentDirection == EArrowDirections::Left || m_CurrentDirection == EArrowDirections::Right) {
		const size_t direction = size_t(m_CurrentDirection) - 1;
		const size_t vote      = (prediction < 0 ? 0 : 1);

		(m_oConfusion->getBuffer())[direction * 2 + vote]++;
		// std::cout << "Now " << trueDirection  << " vote " << thisVote << "\n";
	}
}

void CGrazVisualization::setMatrixBuffer(const double* buffer)
{
	if (m_CurrentState != EStates::ContinousFeedback) {
		// We're not inside a trial, discard the prediction
		return;
	}

	double predictedAmplitude;
	if (m_TwoValueInput) {
		// Ad-hoc forcing to probability (range [0,1], sum to 1). This will make scaling easier 
		// if run forever in a continuous mode. If the input is already scaled this way, no effect.
		// 
		double v0        = std::abs(buffer[0]), v1 = std::abs(buffer[1]);
		const double sum = v0 + v1;
		if (std::fabs(sum) > 0) {
			v0 = v0 / sum;
			v1 = v1 / sum;
		}
		else {
			v0 = 0.5;
			v1 = 0.5;
		}
		predictedAmplitude = v1 - v0;
	}
	else { predictedAmplitude = buffer[0]; }
	m_Amplitudes.push_back(predictedAmplitude);

	if (m_ShowFeedback && !m_DelayFeedback) {
		m_BarScale = aggregatePredictions(false);
		gdk_window_invalidate_rect(m_DrawingArea->window, nullptr, true);
	}
}


void CGrazVisualization::Resize(size_t width, size_t height)
{
	width  = (width < 8 ? 8 : width);
	height = (height < 8 ? 8 : height);

	if (m_LeftArrow) { g_object_unref(G_OBJECT(m_LeftArrow)); }
	if (m_RightArrow) { g_object_unref(G_OBJECT(m_RightArrow)); }
	if (m_UpArrow) { g_object_unref(G_OBJECT(m_UpArrow)); }
	if (m_DownArrow) { g_object_unref(G_OBJECT(m_DownArrow)); }
	if (m_RightBar) { g_object_unref(G_OBJECT(m_RightBar)); }
	if (m_LeftBar) { g_object_unref(G_OBJECT(m_LeftBar)); }

	m_LeftArrow  = gdk_pixbuf_scale_simple(m_OriginalLeftArrow, 2 * int(width) / 8, int(height) / 4, GDK_INTERP_BILINEAR);
	m_RightArrow = gdk_pixbuf_scale_simple(m_OriginalRightArrow, 2 * int(width) / 8, int(height) / 4, GDK_INTERP_BILINEAR);
	m_UpArrow    = gdk_pixbuf_scale_simple(m_OriginalUpArrow, int(width) / 4, 2 * int(height) / 8, GDK_INTERP_BILINEAR);
	m_DownArrow  = gdk_pixbuf_scale_simple(m_OriginalDownArrow, int(width) / 4, 2 * int(height) / 8, GDK_INTERP_BILINEAR);

	m_RightBar = gdk_pixbuf_scale_simple(m_OriginalBar, int(width), int(height) / 6, GDK_INTERP_BILINEAR);
	m_LeftBar  = gdk_pixbuf_flip(m_RightBar, true);
}

// Note that we don't need concurrency control here as gtk callbacks run in the main thread
void CGrazVisualization::FlushQueue()
{
	for (const auto& stimulation : m_StimuliQueue) { m_StimulusSender->sendStimulation(stimulation); }
	m_StimuliQueue.clear();

	// This function will be automatically removed after completion, so set to 0
	m_IdleFuncTag = 0;
}

}  // namespace SimpleVisualization
}  // namespace Plugins
}  // namespace OpenViBE
