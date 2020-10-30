/*
 *    Copyright 2012, 2013 Thomas Schöps
 *    Copyright 2012-2020 Kai Pastor
 *
 *    This file is part of OpenOrienteering.
 *
 *    OpenOrienteering is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    OpenOrienteering is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with OpenOrienteering.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "geo_dialog_common.h"

#include <QDoubleSpinBox>
#include <QMouseEvent>
#include <QTimer>

#include "core/georeferencing.h"
#include "core/map.h"
#include "gui/map/map_editor.h"

#ifdef __clang_analyzer__
#define singleShot(A, B, C) singleShot(A, B, #C) // NOLINT 
#endif


namespace OpenOrienteering {

// ### GeoDialogCommon ###

GeoDialogCommon::GeoDialogCommon(QWidget* parent, MapEditorController* controller, Map* map, const Georeferencing* initial)
 : QDialog(parent, Qt::WindowSystemMenuHint | Qt::WindowTitleHint)
 , controller(controller)
 , map(map)
 , initial_georef(initial ? initial : &map->getGeoreferencing())
 , georef(new Georeferencing(*initial_georef))
 , tool_active(false)
{
	// nothing else
}

GeoDialogCommon::~GeoDialogCommon()
{
	if (tool_active)
		controller->setOverrideTool(nullptr);
}

void GeoDialogCommon::toolDeleted()
{
	tool_active = false;
}

void GeoDialogCommon::accept()
{
	map->setGeoreferencing(*georef);
	QDialog::accept();
}

void GeoDialogCommon::selectMapRefPoint()
{
	if (controller)
	{
		controller->setOverrideTool(new GeoreferencingTool(this, controller));
		tool_active = true;
		hide();
	}
}

// static
void GeoDialogCommon::setValueIfChanged(QDoubleSpinBox* field, qreal value)
{
	if (!qFuzzyCompare(field->value(), value))
		field->setValue(value);
}



// ### GeoreferencingTool ###

GeoreferencingTool::GeoreferencingTool(GeoDialogCommon* dialog, MapEditorController* controller, QAction* action)
 : MapEditorTool(controller, Other, action)
 , dialog(dialog)
{
	// nothing
}

GeoreferencingTool::~GeoreferencingTool()
{
	dialog->toolDeleted();
}

void GeoreferencingTool::init()
{
	setStatusBarText(tr("<b>Click</b>: Set the reference point. <b>Right click</b>: Cancel."));
	MapEditorTool::init();
}

bool GeoreferencingTool::mousePressEvent(QMouseEvent* event, const MapCoordF& /*map_coord*/, MapWidget* /*widget*/)
{
	bool handled = false;
	switch (event->button())
	{
	case Qt::LeftButton:
	case Qt::RightButton:
		handled = true;
		break;
	default:
		; // nothing
	}

	return handled;
}

bool GeoreferencingTool::mouseReleaseEvent(QMouseEvent* event, const MapCoordF& map_coord, MapWidget* /*widget*/)
{
	bool handled = false;
	switch (event->button())
	{
	case Qt::LeftButton:
		handled = dialog->setMapRefPoint(MapCoord(map_coord));
		QTimer::singleShot(0, dialog, &QDialog::exec);
		break;
	case Qt::RightButton:
		QTimer::singleShot(0, dialog, &QDialog::exec);
		handled = true;
		break;
	default:
		; // nothing
	}
	
	return handled;
}

const QCursor& GeoreferencingTool::getCursor() const
{
	static auto const cursor = scaledToScreen(QCursor{ QPixmap{ QString::fromLatin1(":/images/cursor-crosshair.png") }, 11, 11 });
	return cursor;
}


}  // namespace OpenOrienteering
