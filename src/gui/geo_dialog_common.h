/*
 *    Copyright 2012, 2013 Thomas Schöps
 *    Copyright 2012-2015 Kai Pastor
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


#ifndef OPENORIENTEERING_GEO_DIALOG_COMMON_H
#define OPENORIENTEERING_GEO_DIALOG_COMMON_H

#include <vector>

#include <QDialog>
#include <QObject>
#include <QScopedPointer>
#include <QString>
#include <QTransform>

#include "core/map_coord.h"
#include "tools/tool.h"

class QAction;
class QCursor;
class QCheckBox;
class QDialogButtonBox;
class QDoubleSpinBox;
class QLabel;
class QMouseEvent;
class QPushButton;
class QRadioButton;
class QNetworkReply;
class QWidget;

namespace OpenOrienteering {

class CRSSelector;
class Georeferencing;
class Map;
class MapEditorController;
class MapWidget;


/**
 * A GeoDialogCommon is a dialog which provides for a map tool that selects
 * the reference point.
 */
class GeoDialogCommon : public QDialog
{
protected:
	/**
	 * Constructs a base object for the georeferencing dialog.
	 * @param parent                  A parent widget.
	 * @param controller              A controller which operates on the map.
	 * @param map                     The map.
	 * @param initial                 An override of the map's georeferencing
	 */
	GeoDialogCommon(QWidget* parent, MapEditorController* controller, Map* map, const Georeferencing* initial);
	
public:
	/**
	 * Releases resources.
	 */
	~GeoDialogCommon() override;
	
	/**
	 * Notifies the dialog that the active GeoreferencingTool was deleted.
	 */
	void toolDeleted();
	
	/**
	 * Sets the map coordinates of the reference point
	 */
	virtual bool setMapRefPoint(const MapCoord& coords) = 0;
	
	/** 
	 * Pushes the changes from the dialog to the map's Georeferencing
	 * and closes the dialog. The dialog's result is set to QDialog::Accepted,
	 * and the active exec() function will return.
	 */
	void accept() override;
	
	/**
	 * Hides the dialog and activates a GeoreferencingTool for selecting
	 * the reference point on the map.
	 */
	void selectMapRefPoint();

	/**
	 * Helper for spin boxes common in these dialogs.
	 */
	static void setValueIfChanged(QDoubleSpinBox* field, qreal value);
	
protected:
	/* Internal state */
	MapEditorController* const controller;
	Map* const map;
	const Georeferencing* initial_georef;
	QScopedPointer<Georeferencing> georef; // A working copy of the current or given initial Georeferencing
private:
	bool tool_active;
};



/** 
 * GeoreferencingTool is a helper to other dialogs which allows 
 * the user to select the position of the reference point on the map.
 * The parent dialog hides when it activates this tool. The tool
 * takes care of reactivating the dialog.
 */
class GeoreferencingTool : public MapEditorTool
{
Q_OBJECT
public:
	/** 
	 * Constructs a new tool for the given dialog and controller.
	 */
	GeoreferencingTool(
	        GeoDialogCommon* dialog,
	        MapEditorController* controller,
	        QAction* action = nullptr
	);
	
	/**
	 * Notifies the dialog that the tool is deleted.
	 */
	~GeoreferencingTool() override;
	
	/**
	 * Activates the tool.
	 */
	void init() override;
	
	/** 
	 * Consumes left and right clicks. They are handled in mouseReleaseEvent.
	 */
	bool mousePressEvent(QMouseEvent* event, const MapCoordF& map_coord, MapWidget* widget) override;
	
	/** 
	 * Reacts to the user activity by sending the reference point coordinates
	 * to the dialog (on left click) and reactivating the dialog.
	 */
	bool mouseReleaseEvent(QMouseEvent* event, const MapCoordF& map_coord, MapWidget* widget) override;
	
	/**
	 * Returns the mouse cursor that will be shown when the tool is active.
	 */
	const QCursor& getCursor() const override;
	
private:
	GeoDialogCommon* const dialog;
};


}  // namespace OpenOrienteering

#endif
