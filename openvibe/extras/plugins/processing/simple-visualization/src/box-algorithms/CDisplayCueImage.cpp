///-------------------------------------------------------------------------------------------------
/// 
/// \file CDisplayCueImage.cpp
/// \brief Classes implementation for the Box Display cue image.
/// \author Joan Fruitet, Jussi T. Lindgren (Inria Sophia, Inria Rennes).
/// \version 1.2.
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

#include "CDisplayCueImage.hpp"

#include <algorithm> // std::min

#include <tcptagging/IStimulusSender.h>

#if defined TARGET_OS_Linux
  #include <unistd.h>
#endif

namespace OpenViBE {
namespace Plugins {
namespace SimpleVisualization {

// This callback flushes all accumulated stimulations to the TCP Tagging 
// after the rendering has completed.
static gboolean DisplayCueImageFlushCB(gpointer data)
{
	reinterpret_cast<CDisplayCueImage*>(data)->FlushQueue();
	return false;	// Only run once
}

static gboolean DisplayCueImageResizeCB(GtkWidget* /*widget*/, GtkAllocation* allocation, gpointer data)
{
	reinterpret_cast<CDisplayCueImage*>(data)->Resize(size_t(allocation->width), size_t(allocation->height));
	return FALSE;
}

static gboolean DisplayCueImageRedrawCB(GtkWidget* /*widget*/, GdkEventExpose* /*event*/, gpointer data)
{
	reinterpret_cast<CDisplayCueImage*>(data)->Redraw();
	return TRUE;
}

bool CDisplayCueImage::initialize()
{
	m_idleFuncTag    = 0;
	m_stimulusSender = nullptr;
	//>>>> Reading Settings:

	//Number of Cues:
	m_numberOfCues = (getStaticBoxContext().getSettingCount() - NON_CUE_SETTINGS_COUNT) / 2;

	//Do we display the images in full screen?
	m_fullScreen  = FSettingValueAutoCast(*this->getBoxAlgorithmContext(), 0);
	m_scaleImages = FSettingValueAutoCast(*this->getBoxAlgorithmContext(), 1);

	//Clear screen stimulation:
	m_clearScreenStimulation = FSettingValueAutoCast(*this->getBoxAlgorithmContext(), 2);

	//Stimulation ID and images file names for each cue
	m_imageNames.resize(m_numberOfCues);
	m_stimulationsIds.resize(m_numberOfCues);
	for (size_t i = 0; i < m_numberOfCues; ++i) {
		m_imageNames[i]      = FSettingValueAutoCast(*this->getBoxAlgorithmContext(), NON_CUE_SETTINGS_COUNT + 2 * i);
		m_stimulationsIds[i] = FSettingValueAutoCast(*this->getBoxAlgorithmContext(), NON_CUE_SETTINGS_COUNT + 2 * i + 1);
	}

	//>>>> Initialisation
	m_stimulationDecoder.initialize(*this, 0);
	m_stimulationEncoder.initialize(*this, 0);

	//load the gtk builder interface
	m_builderInterface = gtk_builder_new();
	if (!m_builderInterface) {
		getBoxAlgorithmContext()->getPlayerContext()->getLogManager() << Kernel::LogLevel_Error << "Couldn't load the interface !";
		return false;
	}

	const CString uiFile = Directories::getDataDir() + "/plugins/simple-visualization/openvibe-simple-visualization-DisplayCueImage.ui";
	if (!gtk_builder_add_from_file(m_builderInterface, uiFile, nullptr)) {
		getBoxAlgorithmContext()->getPlayerContext()->getLogManager() << Kernel::LogLevel_Error << "Could not load the .ui file " << uiFile << "\n";
		return false;
	}

	gtk_builder_connect_signals(m_builderInterface, nullptr);

	m_drawingArea = GTK_WIDGET(gtk_builder_get_object(m_builderInterface, "DisplayCueImageDrawingArea"));
	g_signal_connect(G_OBJECT(m_drawingArea), "expose-event", G_CALLBACK(DisplayCueImageRedrawCB), this);
	g_signal_connect(G_OBJECT(m_drawingArea), "size-allocate", G_CALLBACK(DisplayCueImageResizeCB), this);

	//set widget bg color
	gtk_widget_modify_bg(m_drawingArea, GTK_STATE_NORMAL, &m_backgroundColor);
	gtk_widget_modify_bg(m_drawingArea, GTK_STATE_PRELIGHT, &m_backgroundColor);
	gtk_widget_modify_bg(m_drawingArea, GTK_STATE_ACTIVE, &m_backgroundColor);

	gtk_widget_modify_fg(m_drawingArea, GTK_STATE_NORMAL, &m_foregroundColor);
	gtk_widget_modify_fg(m_drawingArea, GTK_STATE_PRELIGHT, &m_foregroundColor);
	gtk_widget_modify_fg(m_drawingArea, GTK_STATE_ACTIVE, &m_foregroundColor);

	//Load the pictures:
	m_originalPictures.resize(m_numberOfCues);
	m_scaledPictures.resize(m_numberOfCues);

	for (size_t i = 0; i < m_numberOfCues; ++i) {
		m_originalPictures[i] = gdk_pixbuf_new_from_file_at_size(m_imageNames[i], -1, -1, nullptr);
		m_scaledPictures[i]   = nullptr;
		if (!m_originalPictures[i]) {
			getBoxAlgorithmContext()->getPlayerContext()->getLogManager() << Kernel::LogLevel_Error << "Error couldn't load resource file : " << m_imageNames[i]
					<< "!\n";
			return false;
		}
	}
	m_stimuliQueue.clear();

	m_stimulusSender = TCPTagging::CreateStimulusSender();

	if (!m_stimulusSender->connect("localhost", "15361")) {
		this->getLogManager() << Kernel::LogLevel_Warning << "Unable to connect to AS's TCP Tagging plugin, stimuli wont be forwarded.\n";
	}

	if (m_fullScreen) {
		GtkWidget* window = gtk_widget_get_toplevel(m_drawingArea);
		gtk_window_fullscreen(GTK_WINDOW(window));
		gtk_widget_show(window);

		// @fixme small mem leak?
		GdkCursor* cursor = gdk_cursor_new(GDK_BLANK_CURSOR);
		gdk_window_set_cursor(gtk_widget_get_window(window), cursor);
	}
	else {
		m_visualizationCtx = dynamic_cast<VisualizationToolkit::IVisualizationContext*>(this->createPluginObject(OVP_ClassId_Plugin_VisualizationCtx));
		m_visualizationCtx->setWidget(*this, m_drawingArea);
	}

	// Invalidate the drawing area in order to get the image resize already called at this point. The actual run will be smoother.
	if (GTK_WIDGET(m_drawingArea)->window) { gdk_window_invalidate_rect(GTK_WIDGET(m_drawingArea)->window, nullptr, true); }

	return true;
}

bool CDisplayCueImage::uninitialize()
{
	// Remove the possibly dangling idle loop. 
	if (m_idleFuncTag) {
		g_source_remove(m_idleFuncTag);
		m_idleFuncTag = 0;
	}

	m_stimulationDecoder.uninitialize();
	m_stimulationEncoder.uninitialize();

	if (m_stimulusSender) {
		delete m_stimulusSender;
		m_stimulusSender = nullptr;
	}

	// Close the full screen
	if (m_fullScreen) {
		GtkWidget* window = gtk_widget_get_toplevel(m_drawingArea);
		gtk_window_unfullscreen(GTK_WINDOW(window));
		gtk_widget_destroy(window);
	}

	//destroy drawing area
	if (m_drawingArea) {
		gtk_widget_destroy(m_drawingArea);
		m_drawingArea = nullptr;
	}

	// unref the xml file as it's not needed anymore
	if (m_builderInterface) {
		g_object_unref(G_OBJECT(m_builderInterface));
		m_builderInterface = nullptr;
	}

	m_stimulationsIds.clear();
	m_imageNames.clear();

	if (!m_originalPictures.empty()) {
		for (size_t i = 0; i < m_numberOfCues; ++i) { if (m_originalPictures[i]) { g_object_unref(G_OBJECT(m_originalPictures[i])); } }
		m_originalPictures.clear();
	}

	if (!m_scaledPictures.empty()) {
		for (size_t i = 0; i < m_numberOfCues; ++i) { if (m_scaledPictures[i]) { g_object_unref(G_OBJECT(m_scaledPictures[i])); } }
		m_scaledPictures.clear();
	}

	if (m_visualizationCtx) {
		this->releasePluginObject(m_visualizationCtx);
		m_visualizationCtx = nullptr;
	}

	return true;
}

bool CDisplayCueImage::processClock(Kernel::CMessageClock& /*msg*/)
{
	Kernel::IBoxIO* boxIO = getBoxAlgorithmContext()->getDynamicBoxContext();
	m_stimulationEncoder.getInputStimulationSet()->clear();

	if (this->getPlayerContext().getCurrentTime() == 0) {
		// Always send header first
		m_stimulationEncoder.encodeHeader();
		boxIO->markOutputAsReadyToSend(0, 0, 0);
	}

	if (m_imageDrawn) {
		// this is first redraw() for that image or clear screen
		// we send a stimulation to signal it. 
		// @note this practice is deprecated, the TCP Tagging should be used. We pass the stimulus here for compatibility.

		if (m_drawnImageId >= 0) {
			// it was a image
			m_stimulationEncoder.getInputStimulationSet()->push_back(m_stimulationsIds[m_drawnImageId], this->getPlayerContext().getCurrentTime(), 0);
		}
		else {
			// it was a clear_screen
			m_stimulationEncoder.getInputStimulationSet()->push_back(m_clearScreenStimulation, this->getPlayerContext().getCurrentTime(), 0);
		}

		m_imageDrawn = false;

		if (m_drawnImageId != m_requestedImageId) {
			// We must be late...
			getBoxAlgorithmContext()->getPlayerContext()->getLogManager() << Kernel::LogLevel_Warning <<
					"One image may have been skipped => we must be late...\n";
		}
	}

	m_stimulationEncoder.encodeBuffer();
	boxIO->markOutputAsReadyToSend(0, m_lastOutputChunkDate, this->getPlayerContext().getCurrentTime());
	m_lastOutputChunkDate = this->getPlayerContext().getCurrentTime();

	bool stimulusMatchedBefore = false;

	// We check if some images must be displayed
	for (size_t stim = 0; stim < m_pendingStimulationSet.size();) {
		const uint64_t date = m_pendingStimulationSet.getDate(stim);
		const uint64_t time = this->getPlayerContext().getCurrentTime();
		if (date < time) {
			const uint64_t id       = m_pendingStimulationSet.getId(stim);
			bool stimulusMatchedNow = false;
			if (id == m_clearScreenStimulation) {
				if (m_imageRequested) {
					getBoxAlgorithmContext()->getPlayerContext()->getLogManager() << Kernel::LogLevel_ImportantWarning <<
							"Clear screen was received before previous cue image in slot " << m_requestedImageId + 1 << " was displayed!!\n";
				}
				m_imageRequested      = true;
				stimulusMatchedBefore = true;
				stimulusMatchedNow    = true;
				m_requestedImageId    = -1;
			}
			else {
				for (size_t i = 0; i < m_numberOfCues; ++i) {
					if (id == m_stimulationsIds[i]) {
						if (m_imageRequested) {
							getBoxAlgorithmContext()->getPlayerContext()->getLogManager() << Kernel::LogLevel_ImportantWarning << "Previous request of slot " <<
									m_requestedImageId + 1 << " image was replaced by request for slot " << i + 1 <<
									" => Not enough time between two images!!\n";
						}
						m_imageRequested      = true;
						stimulusMatchedBefore = true;
						stimulusMatchedNow    = true;
						m_requestedImageId    = int(i);
						break;
					}
				}
			}

			if (stimulusMatchedNow) {
				// Queue the recognized stimulation to TCP Tagging to be sent after rendering
				const uint64_t sentStimulation = (m_requestedImageId >= 0 ? m_stimulationsIds[m_requestedImageId] : m_clearScreenStimulation);
				m_stimuliQueue.push_back(sentStimulation);
			}
			else {
				// Pass unrecognized stimulations to TCP Tagging. Be careful when modifying the code that the
				// stimuli received by AS keep their original time order despite the delays introduced to rendered stimuli.

				if (stimulusMatchedBefore) {
					// We have queued a cue to be drawn, so we should delay this stimulation to TCP Tagging to be processed after the cue rendering to keep the time order
					m_stimuliQueue.push_back(id);
				}
				else {
					// We have not yet queued anything to be drawn, so we can forward immediately.
					m_stimulusSender->sendStimulation(id);
				}
			}

			const double delay = CTime(time - date).toSeconds() * 1000; // delay in ms
			if (delay > 50) {
				getBoxAlgorithmContext()->getPlayerContext()->getLogManager() << Kernel::LogLevel_Warning << "Stimulation " << id <<
						" was late in processClock() : " << delay << " ms \n";
			}

			m_pendingStimulationSet.erase(stim);
		}
		else {
			// Stim is still in the future, skip it for now
			stim++;
		}
	}

	// We should show the cue image now. How this works:
	// - The gtk drawing area is invalidated
	// - Gtk will request a redraw
	// - The redraw puts in instructions to render the new image
	// - The corresponding stimulation is buffered to TCP Tagging 
	// - Callback to flush the TCP Tagging buffer is registered to be run by gtk once after the rendering
	// - Gtk renders
	// - Callback to flush the TCP Tagging buffer is called by gtk
	if (m_imageRequested && GTK_WIDGET(m_drawingArea)->window) {
		// this will trigger the callback redraw(). Since the draw will happen after the exit from this function, no point calling this in the loop above
		gdk_window_invalidate_rect(GTK_WIDGET(m_drawingArea)->window, nullptr, true);
	}

	return true;
}

bool CDisplayCueImage::processInput(const size_t /*index*/)
{
	getBoxAlgorithmContext()->markAlgorithmAsReadyToProcess();
	return true;
}

bool CDisplayCueImage::process()
{
	const Kernel::IBoxIO* boxIO = getBoxAlgorithmContext()->getDynamicBoxContext();

	// We decode and save the received stimulations.
	for (size_t input = 0; input < getBoxAlgorithmContext()->getStaticBoxContext()->getInputCount(); ++input) {
		for (size_t chunk = 0; chunk < boxIO->getInputChunkCount(input); ++chunk) {
			m_stimulationDecoder.decode(chunk, true);
			if (m_stimulationDecoder.isHeaderReceived()) {
				// nop
			}
			if (m_stimulationDecoder.isBufferReceived()) {
				for (size_t stim = 0; stim < m_stimulationDecoder.getOutputStimulationSet()->size(); ++stim) {
					// We always add the stimulations to the set to allow passing them to TCP Tagging in order in processClock()
					const uint64_t id       = m_stimulationDecoder.getOutputStimulationSet()->getId(stim);
					const uint64_t date     = m_stimulationDecoder.getOutputStimulationSet()->getDate(stim);
					const uint64_t duration = m_stimulationDecoder.getOutputStimulationSet()->getDuration(stim);

					const uint64_t time = this->getPlayerContext().getCurrentTime();
					if (date < time) {
						const double delay = CTime(time - date).toSeconds() * 1000; //delay in ms
						if (delay > 50) {
							getBoxAlgorithmContext()->getPlayerContext()->getLogManager() << Kernel::LogLevel_Warning << "Stimulation " << id <<
									" was received late: " << delay << " ms \n";
						}
					}

					if (date < boxIO->getInputChunkStartTime(input, chunk)) {
						this->getLogManager() << Kernel::LogLevel_ImportantWarning << "Input Stimulation Date before beginning of the buffer\n";
					}

					m_pendingStimulationSet.push_back(id, date, duration);
				}
			}
		}
	}

	return true;
}

//Callback called by GTK
void CDisplayCueImage::Redraw()
{
	if (m_requestedImageId >= 0) { drawCuePicture(m_requestedImageId); }
	if (m_imageRequested) {
		m_imageRequested = false;
		m_imageDrawn     = true;
		m_drawnImageId   = m_requestedImageId;

		// Set the handler to push out the queued stims after the actual rendering
		if (m_idleFuncTag == 0) { m_idleFuncTag = g_idle_add(DisplayCueImageFlushCB, this); }
	}
}

void CDisplayCueImage::drawCuePicture(const size_t cueID) const
{
	const gint width  = m_drawingArea->allocation.width;
	const gint height = m_drawingArea->allocation.height;

	// Center image
	const gint x = (width / 2) - gdk_pixbuf_get_width(m_scaledPictures[cueID]) / 2;
	const gint y = (height / 2) - gdk_pixbuf_get_height(m_scaledPictures[cueID]) / 2;
	gdk_draw_pixbuf(m_drawingArea->window, nullptr, m_scaledPictures[cueID], 0, 0, x, y, -1, -1, GDK_RGB_DITHER_NONE, 0, 0);
}

void CDisplayCueImage::Resize(const size_t width, const size_t height)
{
	for (auto& i : m_scaledPictures) { if (i) { g_object_unref(G_OBJECT(i)); } }

	if (!m_scaleImages) {
		for (size_t i = 0; i < m_scaledPictures.size(); ++i) { m_scaledPictures[i] = gdk_pixbuf_copy(m_originalPictures[i]); }
		return;
	}

	// Scale
	if (m_fullScreen) {
		for (size_t i = 0; i < m_scaledPictures.size(); ++i) {
			// Keep aspect ratio when scaling
			const int picWidth  = gdk_pixbuf_get_width(m_originalPictures[i]),
					  picHeight = gdk_pixbuf_get_height(m_originalPictures[i]);

			const double scaleWidth  = double(width) / double(picWidth),
						 scaleHeight = double(height) / double(picHeight),
						 scaleMin    = std::min<double>(scaleWidth, scaleHeight);

			const int newWidth  = int(scaleMin * picWidth),
					  newHeight = int(scaleMin * picHeight);

			m_scaledPictures[i] = gdk_pixbuf_scale_simple(m_originalPictures[i], newWidth, newHeight, GDK_INTERP_BILINEAR);
		}
	}
	else {
		const auto x = float(width < 64 ? 64 : width),
				   y = float(height < 64 ? 64 : height);
		for (size_t i = 0; i < m_scaledPictures.size(); ++i) {
			auto picWidth  = float(gdk_pixbuf_get_width(m_originalPictures[i])),
				 picHeight = float(gdk_pixbuf_get_height(m_originalPictures[i]));
			if ((x / picWidth) < (y / picHeight)) {
				picHeight = x * picHeight / (3 * picWidth);
				picWidth  = x / 3;
			}
			else {
				picWidth  = y * picWidth / (3 * picHeight);
				picHeight = y / 3;
			}
			m_scaledPictures[i] = gdk_pixbuf_scale_simple(m_originalPictures[i], int(picWidth), int(picHeight), GDK_INTERP_BILINEAR);
		}
	}
}

// Note that we don't need concurrency control here as gtk callbacks run in the main thread
void CDisplayCueImage::FlushQueue()
{
	for (const auto& stimulation : m_stimuliQueue) { m_stimulusSender->sendStimulation(stimulation); }
	m_stimuliQueue.clear();

	// This function will be automatically removed after completion, so set to 0
	m_idleFuncTag = 0;
}

}  // namespace SimpleVisualization
}  // namespace Plugins
}  // namespace OpenViBE
