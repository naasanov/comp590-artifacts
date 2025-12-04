#pragma once

#include "../../defines.hpp"

#include <openvibe/ov_all.h>

#include <gtk/gtk.h>

#include "CTopographicMapDatabase.hpp"

#include <vector>

namespace OpenViBE {
namespace Plugins {
namespace SimpleVisualization {
/**
 * This class contains everything necessary to setup a GTK window and display
 * a 2D topographic map
 */
class CTopographicMap2DView final : public CTopographicMapDrawable
{
public:
	enum class EProjection { Axial, Radial, NumProjection };

	enum class EView { Top, Left, Right, Back };

	/**
	 * \brief Constructor
	 * \param mapDatabase Datastore
	 * \param interpolation Interpolation mode
	 * \param delay Delay to apply to displayed data
	 */
	CTopographicMap2DView(CTopographicMapDatabase& mapDatabase, EInterpolationType interpolation, double delay);

	/**
	 * \brief Destructor
	 */
	~CTopographicMap2DView() override;

	/** \name CSignalDisplayDrawable implementation */
	//@{

	/**
	 * \brief Initialize widgets
	 */
	void Init() override;

	/**
	 * \brief Redraw map
	 */
	void Redraw() override;

	//@}

	/** \name CTopographicMapDrawable implementation */
	//@{

	/**
	 * \brief Get matrix of sample points coordinates (places where to interpolate values)
	 * \return Pointer to matrix of sample points coordinates
	 */
	CMatrix* GetSampleCoordinatesMatrix() override;

	/**
	 * \brief Set matrix of sample points values (values interpolated at places specified in sample coordinates matrix)
	 * \param [in] matrix Pointer to matrix of sample points values
	 * \return True if values were successfully set, false otherwise
	 */
	bool SetSampleValuesMatrix(CMatrix* matrix) override;

	//@}

	/**
	 * \brief Get pointers to plugin main widget and (optional) toolbar widget
	 * \param [out] widget Pointer to main widget
	 * \param [out] toolbar Pointer to (optional) toolbar widget
	 */
	void GetWidgets(GtkWidget*& widget, GtkWidget*& toolbar) const;

	/**
	 * \brief Get ID of current view
	 * \return ID of current view
	 */
	EView GetCurrentView() const { return m_currentView; }

	/** \name Callbacks */
	//@{

	void ResizeCB(size_t /*width*/, size_t /*height*/) { m_needResize = true; }
	void ToggleElectrodesCB();
	void SetProjectionCB(GtkWidget* widget);
	void SetViewCB(GtkWidget* widget);
	void SetInterpolationCB(GtkWidget* widget);
	void SetDelayCB(const double delay) const { m_mapDatabase.SetDelay(delay); }

	//@}

private:
	//draw color palette
	void drawPalette(size_t x, size_t y, size_t width, size_t height) const;

	//draw face (ears, nose, neck)
	void drawFace(size_t x, size_t y, size_t width, size_t height) const;

	//draw head
	void drawHead() const;

	//draw RGB buffer
	void drawPotentials() const;

	//draw electrodes corresponding to visible channels as rings
	void drawElectrodes() const;

	/**
	 * \brief Get channel position in 2D
	 * \param index Index of channel which position is to be retrieved
	 * \param x X coordinate of channel location, if channel is visible
	 * \param y Y coordinate of channel location, if channel is visible
	 * \return True if channel is visible in current view, false otherwise
	 */
	bool getChannel2DPosition(size_t index, gint& x, gint& y) const;

	//update RGB buffer with interpolated values
	void refreshPotentials() const;

	//draw a box in RGB buffer
	void drawBoxToBuffer(size_t x, size_t y, size_t width, size_t height, uint8_t red, uint8_t green, uint8_t blue) const;

	void enableElectrodeButtonSignals(bool enable);
	void enableProjectionButtonSignals(bool enable);
	void enableViewButtonSignals(bool enable);
	void enableInterpolationButtonSignals(bool enable);

	/**
	 * \brief Compute normalized coordinates of 2D samples
	 * \remarks This method should first be called with bComputeCoordinates = false, allowing caller
	 * to resize data structures appropriately, and then it may be called with bComputeCoordinates = true
	 * \param all If false, this method only computes the number of visible samples
	 * \return Number of visible samples (samples lying within the actual skull area)
	 */
	size_t computeSamplesNormalizedCoordinates(bool all);

	void resizeData();

	void redrawClipmask();

	double getThetaFromCartesianCoordinates(const std::array<double, 3>& cartesian) const;

	double getPhiFromCartesianCoordinates(const std::array<double, 3>& cartesian) const;

	bool compute2DCoordinates(double theta, double phi, size_t skullCenterX, size_t skullCenterY, gint& x, gint& y) const;

	//! The database that contains the information to use to draw the signals
	CTopographicMapDatabase& m_mapDatabase;

	//Maximum delay that can be applied to displayed data
	double m_maxDelay = 2.0;

	GtkBuilder* m_builderInterface = nullptr;

	GtkWidget* m_drawingArea   = nullptr;
	GdkBitmap* m_clipmask      = nullptr; //origin (m_skullX, m_skullY)
	size_t m_clipmaskWidth     = 0;
	size_t m_clipmaskHeight    = 0;
	GdkGC* m_clipmaskGC        = nullptr;
	GdkRegion* m_visibleRegion = nullptr; //reallocated whenever clipmask changes

	GdkColor m_bgColor;

	//! Active projection
	EProjection m_currentProjection = EProjection::Radial;
	//! Projection radio buttons
	GtkRadioToolButton* m_axialProjectionButton  = nullptr;
	GtkRadioToolButton* m_radialProjectionButton = nullptr;

	//! Active view
	EView m_currentView = EView::Top;
	//! View radio buttons
	GtkRadioToolButton* m_topViewButton   = nullptr;
	GtkRadioToolButton* m_leftViewButton  = nullptr;
	GtkRadioToolButton* m_rightViewButton = nullptr;
	GtkRadioToolButton* m_backViewButton  = nullptr;

	//! Interpolation type
	EInterpolationType m_currentInterpolation = EInterpolationType::Laplacian;
	GtkRadioToolButton* m_mapPotentials       = nullptr;
	GtkRadioToolButton* m_mapCurrents         = nullptr;

	//! Electrodes toggle button
	GtkToggleToolButton* m_electrodesToggleButton = nullptr;
	//! Electrodes toggle state
	bool m_electrodesToggledOn = true;

	bool m_needResize = true;

	size_t m_gridSize = 0, m_cellSize = 0;

	CMatrix m_sampleCoordinatesMatrix;

	std::vector<size_t> m_sampleValues;
	std::vector<std::pair<size_t, size_t>> m_sample2DCoordinates; //in skull coords

	size_t m_minPaletteBarHeight = 10, m_maxPaletteBarHeight = 30;
	size_t m_headWindowWidth     = 00, m_headWindowHeight    = 00;
	size_t m_paletteWindowWidth  = 00, m_paletteWindowHeight = 00;

	size_t m_skullX = 0, m_skullY = 0, m_skullDiameter = 0;
	//angles relative to 3 o'clock position, CCW, in degrees
	double m_skullOutlineStartAngle = 0.0, m_skullOutlineEndAngle = 0.0;
	double m_skullFillStartAngle    = 0.0, m_skullFillEndAngle    = 0.0;

	//determined from m_skullOutlineEndAngle
	size_t m_skullOutlineLeftPointX = 0, m_skullOutlineLeftPointY = 0;
	//determined from m_skullOutlineStartAngle
	size_t m_skullOutlineRightPointX = 0, m_skullOutlineRightPointY = 0;

	//determined from m_skullFillEndAngle
	size_t m_skullFillLeftPointX = 0, m_skullFillLeftPointY = 0;
	//determined from m_skullFillStartAngle
	size_t m_skullFillRightPointX = 0, m_skullFillRightPointY = 0;

	size_t m_skullFillBottomPointX = 0, m_skullFillBottomPointY = 0;

	/////////////////////////////
	// TOP VIEW
	/////////////////////////////
	size_t m_noseY = 0;

	/////////////////////////////
	// BOTTOM VIEW
	/////////////////////////////
	size_t m_leftNeckX  = 0, m_leftNeckY  = 0;
	size_t m_rightNeckX = 0, m_rightNeckY = 0;

	//////////////////////////////////
	// LEFT/RIGHT VIEWS
	//////////////////////////////////
	/*
		+ A
	   /
	  /
	 /
	+ B
	| C
	+----+ D
		 |
		 + E
	*/
	size_t m_noseTopX    = 0, m_noseTopY    = 0;	//A
	size_t m_noseBumpX   = 0, m_noseBumpY   = 0;	//B
	size_t m_noseTipX    = 0, m_noseTipY    = 0;	//C
	size_t m_noseBaseX   = 0, m_noseBaseY   = 0;	//D
	size_t m_noseBottomX = 0, m_noseBottomY = 0;	//E

	/**
	 * \brief Main pixmap
	 * \remarks This pixmap is 32-bit aligned. Each row is m_rowStride wide, and the pixmap has the height of the DrawingArea's
	 * window. It is pasted into the DrawingArea's window upon redraw
	 */
	//TODO
	//GdkPixmap* m_pixmap;

	/**
	 * \brief Skull pixmap
	 * \remarks This pixmap is 32-bit aligned. Each row is m_rowStride wide, and the pixmap has m_skullDiameter rows.
	 * It is pasted into the main pixmap everytime changes happen (window resizing, display options toggled on/off, etc)
	 */
	std::vector<guchar> m_skullRGBBuffer;
	size_t m_rowStride = 0;
};
}  // namespace SimpleVisualization
}  // namespace Plugins
}  // namespace OpenViBE
