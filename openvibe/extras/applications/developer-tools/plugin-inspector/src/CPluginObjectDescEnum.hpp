#pragma once

#include "base.hpp"
#include <string>

namespace OpenViBE {
namespace PluginInspector {
class CPluginObjectDescEnum
{
public:
	explicit CPluginObjectDescEnum(const Kernel::IKernelContext& ctx): m_kernelCtx(ctx) { }
	virtual ~CPluginObjectDescEnum() { }

	virtual bool EnumeratePluginObjectDesc();
	virtual bool EnumeratePluginObjectDesc(const CIdentifier& parentClassID);

	virtual bool Callback(const Plugins::IPluginObjectDesc& pod) = 0;

	static std::string Transform(const std::string& in, const bool removeSlash = false);

protected:
	enum class EColors
	{
		BackgroundPlayerStarted,
		BoxBackground, BoxBackgroundSelected, BoxBackgroundMissing, BoxBackgroundObsolete, BoxBackgroundDeprecated,
		BoxBorder, BoxBorderSelected,
		BoxInputBackground, BoxInputBorder,
		BoxOutputBackground, BoxOutputBorder,
		BoxSettingBackground, BoxSettingBorder,
		Link, LinkSelected,
		SelectionArea, SelectionAreaBorder
	};

	virtual Kernel::ILogManager& getLogManager() const { return m_kernelCtx.getLogManager(); }
	virtual Kernel::CErrorManager& getErrorManager() const { return m_kernelCtx.getErrorManager(); }
	const Kernel::IKernelContext& m_kernelCtx;

	static GdkColor colorFromIdentifier(const CIdentifier& identifier)
	{
		GdkColor color;
		const uint64_t res = identifier.id();

		color.pixel = guint16(0);
		color.red   = guint16(((res) & 0xffff) | 0x8000);
		color.green = guint16(((res >> 16) & 0xffff) | 0x8000);
		color.blue  = guint16(((res >> 32) & 0xffff) | 0x8000);

		return color;
	}

	static void gdkDrawRoundedRectangle(GdkDrawable* drawable, GdkGC* gc, const gboolean fill, const gint x, const gint y, const gint w, const gint h,
										const gint r = 8)
	{
		if (fill) {
			gdk_draw_rectangle(drawable, gc, TRUE, x + r, y, w - 2 * r, h);
			gdk_draw_rectangle(drawable, gc, TRUE, x, y + r, w, h - 2 * r);
		}
		else {
			gdk_draw_line(drawable, gc, x + r, y, x + w - r, y);
			gdk_draw_line(drawable, gc, x + r, y + h, x + w - r, y + h);
			gdk_draw_line(drawable, gc, x, y + r, x, y + h - r);
			gdk_draw_line(drawable, gc, x + w, y + r, x + w, y + h - r);
		}
		gdk_draw_arc(drawable, gc, fill, x + w - r * 2, y, r * 2, r * 2, 0 * 64, 90 * 64);
		gdk_draw_arc(drawable, gc, fill, x, y, r * 2, r * 2, 90 * 64, 90 * 64);
		gdk_draw_arc(drawable, gc, fill, x, y + h - r * 2, r * 2, r * 2, 180 * 64, 90 * 64);
		gdk_draw_arc(drawable, gc, fill, x + w - r * 2, y + h - r * 2, r * 2, r * 2, 270 * 64, 90 * 64);
	}
};
}  // namespace PluginInspector
}  // namespace OpenViBE
