/*
 *    Copyright 2012, 2013 Thomas Schöps
 *    Copyright 2019, 2020 Kai Pastor
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


#ifndef OPENORIENTEERING_SHIFT_MAP_DIALOG_H
#define OPENORIENTEERING_SHIFT_MAP_DIALOG_H

#include <functional>

#include <Qt>
#include <QtGlobal>
#include <QDialog>
#include <QObject>
#include <QString>

class QDoubleSpinBox;
class QCheckBox;
class QRadioButton;
class QWidget;

namespace OpenOrienteering {

class Map;


/**
 * Dialog for shifting (translation of) the whole map.
 */
class ShiftMapDialog : public QDialog
{
Q_OBJECT
public:
	using ShiftOp = std::function<void (Map&)>;

	/** Creates a new ShiftMapDialog. */
	ShiftMapDialog(const Map& map, QWidget* parent = nullptr, Qt::WindowFlags f = {});
	
	/** Performs the configured shift on the given map. */
	void shift(Map& map) const;
	
	/** Returns a shifting functor. */
	Q_REQUIRED_RESULT ShiftOp makeShift() const;
	
private slots:
	void rightwardLeftwardEdited();
	void eastingNorthingEdited();
	void updateWidgets();
	
private:
	QDoubleSpinBox* rightward_adjust_edit;
	QDoubleSpinBox* upward_adjust_edit;
	
	QDoubleSpinBox* easting_adjust_edit;
	QDoubleSpinBox* northing_adjust_edit;
	
	QCheckBox* adjust_ref_point_check;
	QCheckBox* adjust_templates_check;
	
	QTransform map_to_projected;
	QTransform projected_to_map;
};


}  // namespace OpenOrienteering

#endif
