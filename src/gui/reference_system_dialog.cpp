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


#include "reference_system_dialog.h"

#include <Qt>
#include <QtGlobal>
#include <QCursor>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLatin1Char>
#include <QLatin1String>
#include <QLineF>
#include <QLocale>
#include <QMessageBox>
#include <QPointF>
#include <QPushButton>
#include <QWidget>

#if defined(QT_NETWORK_LIB)
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#endif

#include "settings.h"
#include "core/georeferencing.h"
#include "core/latlon.h"
#include "core/map.h"
#include "gui/geo_dialog_common.h"
#include "gui/main_window.h"
#include "gui/map/map_editor.h"
#include "gui/util_gui.h"
#include "gui/widgets/crs_selector.h"
#include "util/scoped_signals_blocker.h"


namespace OpenOrienteering {

Q_STATIC_ASSERT(Georeferencing::declinationPrecision() == Util::InputProperties<Util::RotationalDegrees>::decimals());



// ### ReferenceSystemDialog ###

ReferenceSystemDialog::ReferenceSystemDialog(MapEditorController* controller, const Georeferencing* initial)
 : ReferenceSystemDialog(controller->getWindow(), controller, controller->getMap(), initial)
{
	// nothing else
}

ReferenceSystemDialog::ReferenceSystemDialog(QWidget* parent, Map* map, const Georeferencing* initial)
 : ReferenceSystemDialog(parent, nullptr, map, initial)
{
	// nothing else
}

ReferenceSystemDialog::ReferenceSystemDialog(
        QWidget* parent,
        MapEditorController* controller,
        Map* map,
        const Georeferencing* initial )
 : GeoDialogCommon(parent, controller, map, initial)
 , is_geo(georef->hasGeographicRefPoint())
 , ref_points_consistent(true)
 , CRS_edited(false)
 , crs_selector()
{
	setWindowTitle(tr("Map reference system"));
	setWindowModality(Qt::WindowModal);
	
	// Create widgets
	auto reference_point_label = Util::Headline::create(tr("Reference point"));
	
	ref_point_button = new QPushButton(tr("&Pick on map"));
	int ref_point_button_width = ref_point_button->sizeHint().width();
	auto geographic_datum_label = new QLabel(tr("(Datum: WGS84)"));
	int geographic_datum_label_width = geographic_datum_label->sizeHint().width();
	
	map_x_edit = Util::SpinBox::create<MapCoordF>(tr("mm"));
	map_y_edit = Util::SpinBox::create<MapCoordF>(tr("mm"));
	ref_point_button->setEnabled(controller);
	auto map_ref_layout = new QHBoxLayout();
	map_ref_layout->addWidget(map_x_edit, 1);
	map_ref_layout->addWidget(new QLabel(tr("X", "x coordinate")), 0);
	map_ref_layout->addWidget(map_y_edit, 1);
	map_ref_layout->addWidget(new QLabel(tr("Y", "y coordinate")), 0);
	if (ref_point_button_width < geographic_datum_label_width)
		map_ref_layout->addSpacing(geographic_datum_label_width - ref_point_button_width);
	map_ref_layout->addWidget(ref_point_button, 0);
	
	easting_edit = Util::SpinBox::create<Util::RealMeters>(tr("m"));
	northing_edit = Util::SpinBox::create<Util::RealMeters>(tr("m"));
	auto projected_ref_layout = new QHBoxLayout();
	projected_ref_layout->addWidget(easting_edit, 1);
	projected_ref_layout->addWidget(new QLabel(tr("E", "west / east")), 0);
	projected_ref_layout->addWidget(northing_edit, 1);
	projected_ref_layout->addWidget(new QLabel(tr("N", "north / south")), 0);
	projected_ref_layout->addSpacing(qMax(ref_point_button_width, geographic_datum_label_width));
	
	projected_ref_label = new QLabel();
	lat_edit = Util::SpinBox::create_optional(8, -90.0, +90.0, Util::InputProperties<Util::RotationalDegrees>::unit());
	lon_edit = Util::SpinBox::create_optional(8, -180.0, +180.0, Util::InputProperties<Util::RotationalDegrees>::unit());
	lon_edit->setWrapping(true);
	QWidget *geographic_ref_widget = {};
	if (!is_geo)
	{
		// To hide and manage the geographic_ref_layout
		geographic_ref_widget = new QWidget();
		geographic_ref_widget->setVisible(false);
	}
	auto geographic_ref_layout = is_geo ? new QHBoxLayout() : new QHBoxLayout(geographic_ref_widget);
	geographic_ref_layout->addWidget(lat_edit, 1);
	geographic_ref_layout->addWidget(new QLabel(tr("N", "north")), 0);
	geographic_ref_layout->addWidget(lon_edit, 1);
	geographic_ref_layout->addWidget(new QLabel(tr("E", "east")), 0);
	if (geographic_datum_label_width < ref_point_button_width)
		geographic_ref_layout->addSpacing(ref_point_button_width - geographic_datum_label_width);
	geographic_ref_layout->addWidget(geographic_datum_label, 0);
	
	show_refpoint_label = new QLabel(tr("Show reference point in:"));
	link_label = new QLabel();
	link_label->setOpenExternalLinks(true);
	ref_point_widget_list = {
		map_x_edit, map_y_edit,
		easting_edit, northing_edit,
		lat_edit, lon_edit
	};

	auto map_crs_label = Util::Headline::create(tr("Map coordinate reference system"));
	
	crs_selector = new CRSSelector(*georef, nullptr);
	crs_selector->addCustomItem(tr("- local -"), Georeferencing::Local);
	
	status_label = new QLabel(tr("Status:"));
	status_field = new QLabel();
	
	auto map_north_label = Util::Headline::create(tr("Map north"));
	
	declination_display = new QLabel();
	auto declination_label = new QLabel(tr("Declination:"));
	grivation_display = new QLabel();
	
	auto scale_compensation_label = Util::Headline::create(tr("Scale compensation"));
	
	/*: The combined scale factor is the ratio between a length on the ground
	    and the corresponding length on the curved earth model. It is applied
	    as a factor to ground distances to get grid plane distances. */
	auto combined_factor_label = new QLabel(tr("Combined scale factor:"));
	combined_factor_display = new QLabel();
	auto geographic_ref_label = new QLabel(tr("Geographic coordinates:"));
	
	/*: The auxiliary scale factor is the ratio between a length in the curved
	    earth model and the corresponding length on the ground. It is applied
	    as a factor to ground distances to get curved earth model distances. */
	auto auxiliary_factor_label = new QLabel(tr("Auxiliary scale factor:"));
	auxiliary_scale_factor_display = new QLabel();
	scale_widget_list = {
		scale_compensation_label,
		auxiliary_factor_label, auxiliary_scale_factor_display,
		combined_factor_label, combined_factor_display
	};
	std::vector<QWidget*> crs_widget_list = {
		geographic_ref_label,
		show_refpoint_label, link_label
	};
	
	buttons_box = new QDialogButtonBox(
	  QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Reset | QDialogButtonBox::Help,
	  Qt::Horizontal);
	reset_button = buttons_box->button(QDialogButtonBox::Reset);
	reset_button->setEnabled(initial);
	auto help_button = buttons_box->button(QDialogButtonBox::Help);
	
	auto edit_layout = new QFormLayout();
	
	edit_layout->addRow(reference_point_label);
	if (is_geo)
		edit_layout->addRow(geographic_ref_label, geographic_ref_layout);
	else
		edit_layout->addRow(geographic_ref_label, geographic_ref_widget);
	edit_layout->addRow(projected_ref_label, projected_ref_layout);
	edit_layout->addRow(tr("Map coordinates:"), map_ref_layout);
	QSpacerItem *half_space = Util::SpacerItem::create(this);
	const auto size = half_space->minimumSize();
	half_space->changeSize(size.width(), size.height()/2);
	edit_layout->addItem(half_space);
	edit_layout->addRow(show_refpoint_label, link_label);
	edit_layout->addItem(Util::SpacerItem::create(this));

	edit_layout->addRow(map_crs_label);
	edit_layout->addRow(tr("&Coordinate reference system:"), crs_selector);
	crs_selector->setDialogLayout(edit_layout);
	edit_layout->addRow(status_label, status_field);
	edit_layout->addItem(Util::SpacerItem::create(this));
	
	edit_layout->addRow(map_north_label);
	edit_layout->addRow(declination_label, declination_display);
	edit_layout->addRow(tr("Grivation:"), grivation_display);
		
	bool control_scale_factor = Settings::getInstance().getSetting(Settings::MapGeoreferencing_ControlScaleFactor).toBool();
	edit_layout->addItem(Util::SpacerItem::create(this));
	edit_layout->addRow(scale_compensation_label);
	edit_layout->addRow(auxiliary_factor_label, auxiliary_scale_factor_display);
	edit_layout->addRow(combined_factor_label, combined_factor_display);
	if (!is_geo)
		for (auto scale_widget: crs_widget_list)
			scale_widget->setVisible(false);
	if (!georef->hasDeclination())
	{
		declination_label->setVisible(false);
		declination_display->setVisible(false);
	}
	if (!control_scale_factor)
		for (auto scale_widget: scale_widget_list)
			scale_widget->setVisible(false);
	
	auto layout = new QVBoxLayout();
	layout->addLayout(edit_layout);
	layout->addStretch();
	layout->addSpacing(16);
	layout->addWidget(buttons_box);
	
	setLayout(layout);
	
	connect(crs_selector, &CRSSelector::crsChanged, this, &ReferenceSystemDialog::crsEdited);
	
	connect(map_x_edit, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ReferenceSystemDialog::mapRefChanged);
	connect(map_y_edit, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ReferenceSystemDialog::mapRefChanged);
	connect(ref_point_button, &QPushButton::clicked, this, &GeoDialogCommon::selectMapRefPoint);

	connect(easting_edit, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ReferenceSystemDialog::eastingNorthingEdited);
	connect(northing_edit, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ReferenceSystemDialog::eastingNorthingEdited);
	
	if (is_geo)
	{
		connect(lat_edit, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ReferenceSystemDialog::latLonEdited);
		connect(lon_edit, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ReferenceSystemDialog::latLonEdited);
	}
	
	connect(buttons_box, &QDialogButtonBox::accepted, this, &ReferenceSystemDialog::accept);
	connect(buttons_box, &QDialogButtonBox::rejected, this, &ReferenceSystemDialog::reject);
	connect(reset_button, &QPushButton::clicked, this, &ReferenceSystemDialog::reset);
	connect(help_button, &QPushButton::clicked, this, &ReferenceSystemDialog::showHelp);
	
	connect(georef.data(), &Georeferencing::stateChanged, this, &ReferenceSystemDialog::georefStateChanged);
	connect(georef.data(), &Georeferencing::transformationChanged, this, &ReferenceSystemDialog::transformationChanged);
	connect(georef.data(), &Georeferencing::projectionChanged, this, &ReferenceSystemDialog::projectionChanged);
	connect(georef.data(), &Georeferencing::declinationChanged, this, &ReferenceSystemDialog::declinationChanged);
	connect(georef.data(), &Georeferencing::auxiliaryScaleFactorChanged, this, &ReferenceSystemDialog::auxiliaryFactorChanged);

	updateWidgets();

	transformationChanged();
	georefStateChanged();
	declinationChanged();
	auxiliaryFactorChanged();

	// Ensure no mismatch between geographic and projected reference points.
	if (georef->hasGeographicRefPoint())
		georef->setProjectedRefPoint(georef->getProjectedRefPoint(), Georeferencing::UpdateGeographicParameter);
}

// static method
bool ReferenceSystemDialog::suitable(const Georeferencing &georef)
{
	return georef.hasGeographicRefPoint() && (georef.getState() == Georeferencing::Geospatial || georef.hasDeclination());
}

// slot
void ReferenceSystemDialog::georefStateChanged()
{
	if (georef->getState() == Georeferencing::Local)
		crs_selector->setCurrentItem(Georeferencing::Local);

	bool enable = georef->getState() == Georeferencing::Geospatial && georef->hasGeographicRefPoint();
	for (auto ref_point_widget: ref_point_widget_list)
		ref_point_widget->setEnabled(enable);

	projectionChanged();
}

// slot
void ReferenceSystemDialog::transformationChanged()
{
	ScopedMultiSignalsBlocker block(
	            map_x_edit, map_y_edit,
	            easting_edit, northing_edit,
	            lat_edit, lon_edit
	);
	setValueIfChanged(map_x_edit, georef->getMapRefPoint().x());
	setValueIfChanged(map_y_edit, -georef->getMapRefPoint().y());

	setValueIfChanged(easting_edit, georef->getProjectedRefPoint().x());
	setValueIfChanged(northing_edit, georef->getProjectedRefPoint().y());

	QString grivation_text = trUtf8("%1 °", "degree value").arg(QLocale().toString(georef->getGrivation(), 'f', Georeferencing::declinationPrecision()));
	grivation_display->setText(grivation_text);

	QString combined_text = trUtf8("%1", "scale factor value").arg(QLocale().toString(georef->getCombinedScaleFactor(), 'f', Georeferencing::scaleFactorPrecision()));
	combined_factor_display->setText(combined_text);
}

// slot
void ReferenceSystemDialog::projectionChanged()
{
	ScopedMultiSignalsBlocker block(
	            crs_selector,
	            lat_edit, lon_edit
	);
	
	LatLon latlon = georef->getGeographicRefPoint();
	double latitude  = latlon.latitude();
	double longitude = latlon.longitude();
	switch (georef->getState())
	{
	case Georeferencing::Local:
		break;
	default:
		qDebug() << "Unhandled georeferencing state:" << georef->getState();
		Q_FALLTHROUGH();
	case Georeferencing::Geospatial:
	case Georeferencing::BrokenGeospatial:
		const std::vector< QString >& parameters = georef->getProjectedCRSParameters();
		auto temp = CRSTemplateRegistry().find(georef->getProjectedCRSId());
		if (georef->getState() == Georeferencing::Geospatial)
		{
			if (!temp || temp->parameters().size() != parameters.size())
			{
				// The CRS id is not there anymore or the number of parameters has changed.
				// Enter as custom spec.
				crs_selector->setCurrentCRS(CRSTemplateRegistry().find(QString::fromLatin1("PROJ.4")), { georef->getProjectedCRSSpec() });
			}
			else
			{
				crs_selector->setCurrentCRS(temp, parameters);
			}
			
			QString osm_link =
			  QString::fromLatin1("http://www.openstreetmap.org/?lat=%1&lon=%2&zoom=18&layers=M").
			  arg(latitude).arg(longitude);
			QString worldofo_link =
			  QString::fromLatin1("http://maps.worldofo.com/?zoom=15&lat=%1&lng=%2").
			  arg(latitude).arg(longitude);
			link_label->setText(
			  tr("<a href=\"%1\">OpenStreetMap</a> | <a href=\"%2\">World of O Maps</a>").
			  arg(osm_link, worldofo_link)
			);
		}
	}
	
	if (georef->hasGeographicRefPoint())
	{
		setValueIfChanged(lat_edit, latitude);
		setValueIfChanged(lon_edit, longitude);
		ref_points_consistent = true;
	}
	else
	{
		setValueIfChanged(lat_edit, lat_edit->minimum());
		setValueIfChanged(lon_edit, lon_edit->minimum());
		ref_points_consistent = false;
	}
	
	// Declination is used when recasting CRS.
	if (crs_selector)
		crs_selector->setEnabled(georef->hasDeclination() && georef->hasGeographicRefPoint());

	QString error = georef->getErrorText();
	if (error.length() == 0)
		status_field->setText(tr("valid"));
	else
		status_field->setText(QLatin1String("<b style=\"color:red\">") + error + QLatin1String("</b>"));
	
	updateWidgets();
}

// slot
void ReferenceSystemDialog::declinationChanged()
{
	QString text = georef->hasDeclination()
		? trUtf8("%1 °", "degree value").arg(QLocale().toString(georef->getDeclination(), 'f', Georeferencing::declinationPrecision()))
		: tr("no value");
	declination_display->setText(text);
}

// slot
void ReferenceSystemDialog::auxiliaryFactorChanged()
{
	QString aux_text = trUtf8("%1", "scale factor value").arg(QLocale().toString(georef->getAuxiliaryScaleFactor(), 'f', Georeferencing::scaleFactorPrecision()));
	auxiliary_scale_factor_display->setText(aux_text);
}

void ReferenceSystemDialog::showHelp()
{
	Util::showHelp(parentWidget(), "georeferencing.html");
}

void ReferenceSystemDialog::reset()
{
	*georef.data() = *initial_georef;
	ref_points_consistent = true;
	CRS_edited = false;
	reset_button->setEnabled(false);
	updateWidgets();
}

void ReferenceSystemDialog::accept()
{
	auto const declination_change_degrees = std::lround(std::fabs(georef->getDeclination() - initial_georef->getDeclination()));
	auto const scale_factor_change_percent = std::lround(std::fabs(std::log(georef->getAuxiliaryScaleFactor() / initial_georef->getAuxiliaryScaleFactor()))*100);
	if (declination_change_degrees > 0 || scale_factor_change_percent > 0)
	{
		if (declination_change_degrees/180.0*M_PI > scale_factor_change_percent/100.0)
			QMessageBox::information(this, tr("Warning"), tr("Because changing the reference point leaves the grivation unchanged, "
															 "declination at the new reference point differs from the original by %1°. "
															 "Declination can be adjusted in \"Realign map\".")
									 .arg(declination_change_degrees));
		else
			QMessageBox::information(this, tr("Warning"),
									 tr("Because changing the reference point leaves the combined scale factor unchanged, "
										"auxiliary scale factor at the new reference point differs from the original by %1%. "
										"Auxiliary scale factor can be adjusted in \"Realign map\".")
									 .arg(scale_factor_change_percent));
	}

	if (CRS_edited && initial_georef->getState() != Georeferencing::Local && georef->getState() != Georeferencing::Local)
	{
		QRectF map_extent = map->calculateExtent();
		std::list<QPointF> extent_corners( { map_extent.topLeft(),
											 map_extent.topRight(),
											 map_extent.bottomRight(),
											 map_extent.bottomLeft() } );
		double shift_meters = 0;
		bool initial_projection_ok = true;
		bool new_projection_ok = true;
		for (QPointF corner : extent_corners)
		{
			// Find its geographic coordinates in the initial georeferencing and the new
			// georeferencing. Convert both of these into geographic coordinates of the new
			// georeferencing to calculate the distance.
			bool ok = false;
			const MapCoordF corner_coords(corner);
			const auto geographic_by_initial = initial_georef->toGeographicCoords(corner_coords, &ok);
			if (!ok)
				initial_projection_ok = false;
			else
			{
				ok = false;
				const QPointF projected_from_initial = georef->toProjectedCoords(geographic_by_initial, &ok);
				if (!ok)
					new_projection_ok = false;
				else
				{
					const QPointF projected_from_new = georef->toProjectedCoords(corner_coords);
					const double shift = QLineF(projected_from_initial, projected_from_new).length();
					if (shift_meters < shift)
						shift_meters = shift;
				}
			}
		}
		if (initial_projection_ok)
		{
			if (new_projection_ok)
			{
				QMessageBox::information(this, tr("Notice"), tr("Changing the CRS moved the geographic positions of objects by up to %1 meters.").arg(shift_meters, 0, 'f', 2));
			}
			else
			{
				QMessageBox::information(this, tr("Error"), 
				  tr("Changed to invalid CRS in Recast reference system dialog.") + QLatin1Char('\n') + QLatin1Char('\n') +
				  tr("Please report this as a bug.") );
			}
		}
	}

	GeoDialogCommon::accept();
}

void ReferenceSystemDialog::updateWidgets()
{
	ref_point_button->setEnabled(controller && map_x_edit->isEnabled());

	if (crs_selector->currentCRSTemplate())
		projected_ref_label->setText(crs_selector->currentCRSTemplate()->coordinatesName(crs_selector->parameters()) + QLatin1Char(':'));
	else
		projected_ref_label->setText(tr("Local coordinates") + QLatin1Char(':'));
	
	buttons_box->button(QDialogButtonBox::Ok)->setEnabled(georef->isValid() && ref_points_consistent);
}

bool ReferenceSystemDialog::setMapRefPoint(const MapCoord& coords)
{
	ref_points_consistent = georef->setMapRefPoint(coords, true);
	reset_button->setEnabled(true);
	return ref_points_consistent;
}

void ReferenceSystemDialog::mapRefChanged()
{
	MapCoord coord(map_x_edit->value(), -1 * map_y_edit->value());
	setMapRefPoint(coord);
	updateWidgets();
}

void ReferenceSystemDialog::eastingNorthingEdited()
{
	double easting   = easting_edit->value();
	double northing  = northing_edit->value();
	ref_points_consistent = georef->setProjectedRefPoint(QPointF(easting, northing), Georeferencing::UpdateGeographicParameter, true);
	reset_button->setEnabled(true);
	updateWidgets();
}

void ReferenceSystemDialog::latLonEdited()
{
	double latitude  = lat_edit->value();
	double longitude = lon_edit->value();
	if (latitude != lat_edit->minimum() && longitude != lon_edit->minimum())
		ref_points_consistent = georef->setGeographicRefPoint(LatLon(latitude, longitude), Georeferencing::UpdateGeographicParameter, true);
	else
		ref_points_consistent = false;
	reset_button->setEnabled(true);
	updateWidgets();
}

void ReferenceSystemDialog::crsEdited()
{
	Georeferencing georef_copy = *georef;
	
	auto crs_template = crs_selector->currentCRSTemplate();
	auto spec = crs_selector->currentCRSSpec();
	
	auto selected_item_id = crs_selector->currentCustomItem();
	switch (selected_item_id)
	{
	default:
		qWarning("Unsupported CRS item id");
		return;
	case Georeferencing::Local:
		georef_copy.setLocalState();
		ref_points_consistent = true;
		break;
	case -1:
		// CRS from list
		Q_ASSERT(crs_template);

		ref_points_consistent = georef_copy.setProjectedCRS(crs_template->id(), spec, crs_selector->parameters(),
															Georeferencing::UpdateGridParameter);
		Q_ASSERT(georef_copy.getState() != Georeferencing::Local);
		break;
	}
	
	// Apply all changes at once
	*georef = georef_copy;
	CRS_edited = true;
	reset_button->setEnabled(true);
	updateWidgets();
}


}  // namespace OpenOrienteering
