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


#ifndef OPENORIENTEERING_GEOREFERENCING_DIALOG_H
#define OPENORIENTEERING_GEOREFERENCING_DIALOG_H

#include <vector>

#include <QObject>
#include <QString>
#include <QTransform>

#include "core/map_coord.h"
#include "gui/geo_dialog_common.h"

class QCheckBox;
class QDialogButtonBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QRadioButton;
class QNetworkReply;
class QWidget;

namespace OpenOrienteering {

class CRSSelector;
class Georeferencing;
class Map;
class MapEditorController;


/**
 * A GeoreferencingDialog allows the user to adjust the georeferencing properties
 * of a map.
 */
class GeoreferencingDialog : public GeoDialogCommon
{
Q_OBJECT
public:
	/**
	 * Constructs a new georeferencing dialog for the map handled by the given 
	 * controller. The optional parameter initial allows to override the current 
	 * properties of the map's georeferencing. The parameter
	 * allow_no_georeferencing determines if the okay button can
	 * be clicked while "- none -" is selected.
	 */
	GeoreferencingDialog(MapEditorController* controller, const Georeferencing* initial = nullptr, bool allow_no_georeferencing = true);
	
	/**
	 * Constructs a new georeferencing dialog for the given map. The optional 
	 * parameter initial allows to override the current properties of the map's
	 * georeferencing. Since the dialog will not know a MapEditorController,
	 * it will not allow to select a new reference point from the map.
	 * The parameter allow_no_georeferencing determines if the okay button can
	 * be clicked while "- none -" is selected.
	 */
	GeoreferencingDialog(QWidget* parent, Map* map, const Georeferencing* initial = nullptr, bool allow_no_georeferencing = true);
	
protected:
	/**
	 * Constructs a new georeferencing dialog.
	 * 
	 * The map parameter must not be nullptr, and it must not be a different
	 * map than the one handled by controller.
	 * 
	 * @param parent                  A parent widget.
	 * @param controller              A controller which operates on the map.
	 * @param map                     The map.
	 * @param initial                 An override of the map's georeferencing
	 * @param allow_no_georeferencing Determines if the okay button can be
	 *                                be clicked while "- none -" is selected.
	 */
	GeoreferencingDialog(
	        QWidget* parent,
	        MapEditorController* controller,
	        Map* map,
	        const Georeferencing* initial,
	        bool allow_no_georeferencing
	);
	
public:
	/**
	 * Updates the dialog from georeferencing state changes.
	 */
	void georefStateChanged();
	
	/**
	  * Moves transformation properties from the georeferencing to the widgets.
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
	  * Updates the scale factor widget from the georeferencing.
	  */
	void auxiliaryFactorChanged();
	
	/**
	  * Sets visibility of scale compensation widgets.
	  */
	void showScaleChanged(bool checked);
	
	/**
	 * Triggers an online request for the magnetic declination.
	 * 
	 * @param no_confirm If true, the user will not be asked for confirmation.
	 */
	void requestDeclination(bool no_confirm = false);
	
	/**
	 * Sets the map coordinates of the reference point
	 */
	bool setMapRefPoint(const MapCoord& coords) override;
	
	/**
	 * Activates the radio button that keeps projected reference point coordinates on CRS changes.
	 */
	void setKeepProjectedRefCoords();
	
	/**
	 * Activates the radio button that keeps geographic reference point coordinates on CRS changes.
	 */
	void setKeepGeographicRefCoords();
	
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
	
signals:
	/**
	 * Indicates that objects have been rotated, scaled, and/or shifted
	 * in relation to map coordinates.
	 */
	void mapObjectsShifted(const QTransform &shift);

protected:
	/**
	 * Updates enabled / disabled states of all widgets.
	 */
	void updateWidgets();
	
	/**
	 * Updates enabled / disabled state and text of the declination query button.
	 */
	void updateDeclinationButton();
	
	
	/** 
	 * Notifies the dialog of a change in the CRS configuration.
	 */
	void crsEdited();
	
	/**
	 * Notifies the dialog of change of the control aspect buttons.
	 */
	void controlAspectChanged();
	
	/**
	 * Notifies the dialog of a change in the auxiliary scale factor.
	 */
	void auxiliaryFactorEdited(double value);
	
	/**
	 * Notifies the dialog of a change in the combined scale factor.
	 */
	void combinedFactorEdited(double value);
	
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
	
	/** 
	 * Notifies the dialog of a change in the declination field.
	 */
	void declinationEdited(double value);
	
	/**
	 * Handles replies from the online declination service.
	 */
	void declinationReplyFinished(QNetworkReply* reply);
	
	/**
	 * Notifies the dialog of a change in the grivation field.
	 */
	void grivationEdited(double value);
	
	/**
	 * Implements a click of Clear geographic button.
	 */
	void clearGeographicParameters();
	
private:
	/* Internal state */
	bool allow_no_georeferencing;
	bool declination_query_in_progress;
	bool lat_lon_are_set;
	bool control_projected_selected; // not forced by local state
	
	/* GUI elements */
	CRSSelector* crs_selector;
	QPushButton* clear_geographic_button;
	QLabel* status_label;
	QLabel* status_field;
	
	QRadioButton* control_projected_radio;
	QRadioButton* control_geographic_radio;

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
	
	QDoubleSpinBox* declination_edit;
	QPushButton* declination_button;
	QDoubleSpinBox* grivation_edit;

	QCheckBox* show_scale_check;
	std::vector<QWidget*> scale_widget_list;
	QDoubleSpinBox* aux_factor_edit;
	QDoubleSpinBox* combined_factor_edit;
	
	QPushButton* reset_button;
	QDialogButtonBox* buttons_box;
};


}  // namespace OpenOrienteering

#endif
