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


#include "shift_map_dialog.h"

#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QRadioButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include "core/georeferencing.h"
#include "core/map.h"
#include "core/map_printer.h"
#include "gui/util_gui.h"


namespace OpenOrienteering {

ShiftMapDialog::ShiftMapDialog(const Map& map, QWidget* parent, Qt::WindowFlags f)
: QDialog(parent, f)
{
	setWindowTitle(tr("Shift (translate) all objects"));
	
	auto* layout = new QFormLayout();
	
	layout->addRow(Util::Headline::create(tr("Shift parameters")));
	
	rightward_adjust_edit = Util::SpinBox::create<MapCoordF>(tr("mm"));
	upward_adjust_edit = Util::SpinBox::create<MapCoordF>(tr("mm"));
	auto map_ref_layout = new QHBoxLayout();
	map_ref_layout->addWidget(rightward_adjust_edit, 1);
	map_ref_layout->addWidget(new QLabel(tr("rightward")), 0);
	map_ref_layout->addWidget(upward_adjust_edit, 1);
	map_ref_layout->addWidget(new QLabel(tr("upward")), 0);
	
	easting_adjust_edit = Util::SpinBox::create<Util::RealMeters>(tr("m"));
	northing_adjust_edit = Util::SpinBox::create<Util::RealMeters>(tr("m"));
	auto projected_ref_layout = new QHBoxLayout();
	projected_ref_layout->addWidget(easting_adjust_edit, 1);
	projected_ref_layout->addWidget(new QLabel(tr("eastward")), 0);
	projected_ref_layout->addWidget(northing_adjust_edit, 1);
	projected_ref_layout->addWidget(new QLabel(tr("northward")), 0);
	
	const Georeferencing& georeferencing = map.getGeoreferencing();

	layout->addRow(tr("Map coordinates"), map_ref_layout);
	auto const projected_ref_text(georeferencing.getProjectedCoordinatesName());
	layout->addRow(projected_ref_text, projected_ref_layout);
	
	layout->addItem(Util::SpacerItem::create(this));
	layout->addRow(Util::Headline::create(tr("Options")));
	
	adjust_ref_point_check = new QCheckBox(tr("Adjust georeferencing reference point"));
	if (map.getGeoreferencing().getState() != Georeferencing::Local)
		adjust_ref_point_check->setChecked(true);
	else
		adjust_ref_point_check->setEnabled(false);
	layout->addRow(adjust_ref_point_check);
	
	adjust_templates_check = new QCheckBox(tr("Shift non-georeferenced templates"));
	adjust_templates_check->setChecked(true);
	layout->addRow(adjust_templates_check);
	
	
	QDialogButtonBox* button_box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal);
	
	auto* box_layout = new QVBoxLayout();
	box_layout->addLayout(layout);
	box_layout->addItem(Util::SpacerItem::create(this));
	box_layout->addStretch();
	box_layout->addWidget(button_box);
	
	setLayout(box_layout);
	
	connect(rightward_adjust_edit, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ShiftMapDialog::rightwardLeftwardEdited);
	connect(upward_adjust_edit, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ShiftMapDialog::rightwardLeftwardEdited);
	connect(easting_adjust_edit, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ShiftMapDialog::eastingNorthingEdited);
	connect(northing_adjust_edit, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ShiftMapDialog::eastingNorthingEdited);

	connect(adjust_ref_point_check, &QAbstractButton::clicked, this, &ShiftMapDialog::updateWidgets);

	connect(button_box, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
	
	updateWidgets();

	map_to_projected.rotate(-georeferencing.getGrivation());
	double scale = georeferencing.getCombinedScaleFactor() * georeferencing.getScaleDenominator() / 1000.0; // to meters
	map_to_projected.scale(scale, -scale);
	projected_to_map = map_to_projected.inverted();
}

void ShiftMapDialog::rightwardLeftwardEdited()
{
	const QSignalBlocker block1(easting_adjust_edit), block2(northing_adjust_edit);
	double rightward = rightward_adjust_edit->value();
	double upward    = -1 * upward_adjust_edit->value();
	auto const projected_vector = map_to_projected.map(QPointF(rightward, upward));
	easting_adjust_edit->setValue(projected_vector.x());
	northing_adjust_edit->setValue(projected_vector.y());
}

void ShiftMapDialog::eastingNorthingEdited()
{
	const bool adjust_ref_point = adjust_ref_point_check->isChecked();
	if (!adjust_ref_point)
	{
		const QSignalBlocker block1(rightward_adjust_edit), block2(upward_adjust_edit);
		double easting   = easting_adjust_edit->value();
		double northing  = northing_adjust_edit->value();
		auto const map_vector = projected_to_map.map(QPointF(easting, northing));
		rightward_adjust_edit->setValue(map_vector.x());
		upward_adjust_edit->setValue(-1 * map_vector.y());
	}
}

void ShiftMapDialog::updateWidgets()
{
	const bool adjust_ref_point = adjust_ref_point_check->isChecked();
	if (adjust_ref_point)
	{
		// Set the easting & northing shifts to 0 and disable its editing.
		const QSignalBlocker block1(easting_adjust_edit), block2(northing_adjust_edit);
		easting_adjust_edit->setValue(0.0);
		northing_adjust_edit->setValue(0.0);
		easting_adjust_edit->setEnabled(false);
	    northing_adjust_edit->setEnabled(false);
	}
	else
	{
		const QSignalBlocker block1(easting_adjust_edit), block2(northing_adjust_edit);
		easting_adjust_edit->setEnabled(true);
	    northing_adjust_edit->setEnabled(true);
		double rightward = rightward_adjust_edit->value();
		double upward    = -1 * upward_adjust_edit->value();
		auto const projected_vector = map_to_projected.map(QPointF(rightward, upward));
		easting_adjust_edit->setValue(projected_vector.x());
		northing_adjust_edit->setValue(projected_vector.y());
	}
}

void ShiftMapDialog::shift(Map& map) const
{
	makeShift()(map);
}

ShiftMapDialog::ShiftOp ShiftMapDialog::makeShift() const
{
	auto adjust_templates = adjust_templates_check->isChecked();
	auto adjust_ref_point = adjust_ref_point_check->isChecked();
	auto const map_object_shift = MapCoordF(rightward_adjust_edit->value(), -1 * upward_adjust_edit->value());

	QTransform map_translation = QTransform().translate(map_object_shift.x(), map_object_shift.y());

#if 0
	// Don't bother to preserve the objects in their views. Not sure
	// whether this is what's desired eventually as the point of this
	// operation is to move the objects.

	// Adjust map's print area.
	if (map.hasPrinterConfig())
	{
		auto printer_config = map.printerConfig();
		auto const initial_print_center = MapCoordF(printer_config.print_area.center());
		auto const shifted_print_center = MapCoordF(map_translation.map(QPointF(initial_print_center)));
		printer_config.print_area.moveCenter(shifted_print_center);
		map.setPrinterConfig(printer_config);
	}

	// Adjust the MapView and the PrintWidget.
	emit mapObjectsShifted(map_translation);
#endif

	return [map_object_shift, adjust_ref_point, adjust_templates](Map& map) {
		map.shiftMap(MapCoord(map_object_shift), adjust_ref_point, adjust_templates);
	};
}


}  // namespace OpenOrienteering
