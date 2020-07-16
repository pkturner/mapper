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


#ifndef OPENORIENTEERING_REFERENCE_SYSTEM_DIALOG_H
#define OPENORIENTEERING_REFERENCE_SYSTEM_DIALOG_H

#include <vector>

#include <QDialog>
#include <QObject>
#include <QScopedPointer>

#include "core/map_coord.h"
#include "gui/geo_dialog_common.h"

class QDialogButtonBox;
class QDoubleSpinBox;
class QLabel;
class QWidget;

namespace OpenOrienteering {

class Georeferencing;
class Map;
class MapEditorController;


/**
 * A ReferenceSystemDialog allows the user to adjust the geographic reference
 * point of a map, without changing its georeferencing projection and
 * transformation.
 */
class ReferenceSystemDialog : public GeoDialogCommon
{
Q_OBJECT
public:
	/**
	 * Constructs a new reference point dialog for the map handled by the given 
	 * controller. The optional parameter initial allows to override the current 
	 * properties of the map's georeferencing. The parameter
	 * allow_no_georeferencing determines if the okay button can
	 * be clicked while "- none -" is selected.
	 */
	ReferenceSystemDialog(MapEditorController* controller, const Georeferencing* initial = nullptr);
	
	/**
	 * Constructs a new reference point dialog for the given map. The optional 
	 * parameter initial allows to override the current properties of the map's
	 * georeferencing. Since the dialog will not know a MapEditorController,
	 * it will not allow to select a new reference point from the map.
	 * The parameter allow_no_georeferencing determines if the okay button can
	 * be clicked while "- none -" is selected.
	 */
	ReferenceSystemDialog(QWidget* parent, Map* map, const Georeferencing* initial = nullptr);
	
protected:
	/**
	 * Constructs a new reference point dialog.
	 * 
	 * The map parameter must not be nullptr, and it must not be a different
	 * map than the one handled by controller.
	 * 
	 * @param parent                  A parent widget.
	 * @param controller              A controller which operates on the map.
	 * @param map                     The map.
	 * @param initial                 An override of the map's georeferencing
	 */
	ReferenceSystemDialog(
	        QWidget* parent,
	        MapEditorController* controller,
	        Map* map,
	        const Georeferencing* initial
	);
	
public:
	/**
	 * Whether this dialog is suitable for the Georeferencing.
	 */
	static bool suitable(const Georeferencing &georef);

	/**
	 * Updates the dialog from georeferencing state changes.
	 */
	void georefStateChanged();
	
	/**
	  * Updates map and projected reference points from the georeferencing.
	  */
	void transformationChanged();
	
	/**
	  * Moves projection properties from the georeferencing to the widgets.
	  */
	void projectionChanged();
	
	/**
	  * Updates the declination widget from the georeferencing.
	  */
	void declinationChanged();
	
	/**
	 * Notifies the dialog of a change in the auxiliary scale factor.
	 */
	void auxiliaryFactorChanged();
	
	/**
	 * Sets the map coordinates of the reference point
	 */
	bool setMapRefPoint(const MapCoord& coords) override;
	
	/**
	 * Notifies the dialog that the active GeoreferencingTool was deleted.
	 */
	void toolDeleted();
	
	/**
	 * Opens this dialog's help page.
	 */
	void showHelp();
	
	/** 
	 * Resets all input fields to the values in the map's Georeferencing.
	 * 
	 * This will also reset initial values passed to the constructor.
	 */
	void reset();
	
	/** 
	 * Pushes the changes from the dialog to the map's Georeferencing
	 * and closes the dialog. The dialog's result is set to QDialog::Accepted,
	 * and the active exec() function will return.
	 */
	void accept() override;
	
protected:
	/**
	 * Sets enabled / disabled states of all widgets.
	 */
	void updateWidgets();
	
	
	/** 
	 * Notifies the dialog of a change in the CRS configuration.
	 */
	void crsEdited();
	
	/**
	 * Notifies the dialog of a change in the map reference point fields.
	 */
	void mapRefChanged();
	
	/**
	 * Notifies the dialog of a change in the easting / northing fields.
	 */
	void eastingNorthingEdited();
	
	/**
	 * Notifies the dialog of a change in the latitude / longitude fields.
	 */
	void latLonEdited();
	
private:
	/* Internal state */
	const bool is_geo;
	bool ref_points_consistent;
	bool CRS_edited;
	
	/* GUI elements */
	CRSSelector* crs_selector;
	QLabel* status_label;
	QLabel* status_field;

	QDoubleSpinBox* map_x_edit;
	QDoubleSpinBox* map_y_edit;
	QPushButton* ref_point_button;
	
	QLabel* projected_ref_label;
	QDoubleSpinBox* easting_edit;
	QDoubleSpinBox* northing_edit;
	
	QDoubleSpinBox* lat_edit;
	QDoubleSpinBox* lon_edit;
	QLabel* show_refpoint_label;
	QLabel* link_label;
	std::vector<QWidget*> ref_point_widget_list;
	
	QLabel* declination_display;
	QLabel* grivation_display;

	std::vector<QWidget*> scale_widget_list;
	QLabel* auxiliary_scale_factor_display;
	QLabel* combined_factor_display;
	
	QDialogButtonBox* buttons_box;
	QPushButton* reset_button;
};


}  // namespace OpenOrienteering

#endif
