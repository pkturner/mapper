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


#include "georeferencing_dialog.h"

#include <vector>

#include <Qt>
#include <QtGlobal>
#include <QAbstractButton>
#include <QCheckBox>
#include <QDate>
#include <QDesktopServices>  // IWYU pragma: keep
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFlags>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLatin1Char>
#include <QLatin1String>
#include <QMessageBox>
#include <QPointF>
#include <QPushButton>
#include <QRadioButton>
#include <QScopedPointer>
#include <QSignalBlocker>
#include <QSize>
#include <QSpacerItem>
#include <QStringRef>
#include <QUrl>
#include <QUrlQuery>
#include <QVariant>
#include <QVBoxLayout>
#include <QWidget>
#include <QXmlStreamReader>
// IWYU pragma: no_include <qxmlstream.h>

#if defined(QT_NETWORK_LIB)
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#endif

#include "settings.h"
#include "core/crs_template.h"
#include "core/georeferencing.h"
#include "core/latlon.h"
#include "gui/main_window.h"
#include "gui/map/map_editor.h"
#include "gui/widgets/crs_selector.h"
#include "gui/util_gui.h"
#include "util/backports.h"  // IWYU pragma: keep
#include "util/scoped_signals_blocker.h"


namespace OpenOrienteering {

Q_STATIC_ASSERT(Georeferencing::declinationPrecision() == Util::InputProperties<Util::RotationalDegrees>::decimals());



// ### GeoreferencingDialog ###

GeoreferencingDialog::GeoreferencingDialog(MapEditorController* controller, const Georeferencing* initial, bool allow_no_georeferencing)
 : GeoreferencingDialog(controller->getWindow(), controller, controller->getMap(), initial, allow_no_georeferencing)
{
	// nothing else
}

GeoreferencingDialog::GeoreferencingDialog(QWidget* parent, Map* map, const Georeferencing* initial, bool allow_no_georeferencing)
 : GeoreferencingDialog(parent, nullptr, map, initial, allow_no_georeferencing)
{
	// nothing else
}

GeoreferencingDialog::GeoreferencingDialog(
        QWidget* parent,
        MapEditorController* controller,
        Map* map,
        const Georeferencing* initial,
        bool allow_no_georeferencing )
 : GeoDialogCommon(parent, controller, map, initial)
 , allow_no_georeferencing(allow_no_georeferencing)
 , declination_query_in_progress(false)
 , control_projected_selected(false)
{
	setWindowTitle(tr("Map Georeferencing"));
	setWindowModality(Qt::WindowModal);
	
	// Create widgets
	auto map_crs_label = Util::Headline::create(tr("Map coordinate reference system"));
	
	crs_selector = new CRSSelector(*georef, nullptr);
	crs_selector->addCustomItem(tr("- local -"), Georeferencing::Local);
	
	status_label = new QLabel(tr("Status:"));
	status_field = new QLabel();
	
	auto reference_point_label = Util::Headline::create(tr("Reference point"));
	
	ref_point_button = new QPushButton(tr("&Pick on map"));
	int ref_point_button_width = ref_point_button->sizeHint().width();
	auto geographic_datum_label = new QLabel(tr("(Datum: WGS84)"));
	int geographic_datum_label_width = geographic_datum_label->sizeHint().width();

	auto aspect_label = Util::Headline::create(tr("Aspect"));

	control_geographic_radio = new QRadioButton(tr("Geographic parameters"));
	control_projected_radio = new QRadioButton(tr("Projected parameters"));
	if (georef->getState() == Georeferencing::Geospatial)
	{
		control_geographic_radio->setChecked(true);
	}
	else
	{
		control_projected_radio->setChecked(true);
	}
	
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
	lat_edit = Util::SpinBox::create(8, -90.0, +90.0, Util::InputProperties<Util::RotationalDegrees>::unit());
	lon_edit = Util::SpinBox::create(8, -180.0, +180.0, Util::InputProperties<Util::RotationalDegrees>::unit());
	lon_edit->setWrapping(true);
	auto geographic_ref_layout = new QHBoxLayout();
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
	
	auto map_north_label = Util::Headline::create(tr("Map north"));
	
	declination_edit = Util::SpinBox::create<Util::RotationalDegrees>();
	declination_button = new QPushButton(tr("Lookup..."));
	auto declination_layout = new QHBoxLayout();
	declination_layout->addWidget(declination_edit, 1);
	declination_layout->addWidget(declination_button, 0);

	grivation_edit = Util::SpinBox::create<Util::RotationalDegrees>();
	
	show_scale_check = new QCheckBox(tr("Show scale factors"));
	auto scale_compensation_label = Util::Headline::create(tr("Scale compensation"));
	
	/*: The combined scale factor is the ratio between a length on the ground
	    and the corresponding length on the curved earth model. It is applied
	    as a factor to ground distances to get grid plane distances. */
	auto combined_factor_label = new QLabel(tr("Combined scale factor:"));
	combined_factor_edit = Util::SpinBox::create(Georeferencing::scaleFactorPrecision(), 0.001, 1000.0);
	
	/*: The auxiliary scale factor is the ratio between a length in the curved
	    earth model and the corresponding length on the ground. It is applied
	    as a factor to ground distances to get curved earth model distances. */
	auto auxiliary_factor_label = new QLabel(tr("Auxiliary scale factor:"));
	aux_factor_edit = Util::SpinBox::create(Georeferencing::scaleFactorPrecision(), 0.001, 1000.0);
	scale_widget_list = {
		scale_compensation_label,
		auxiliary_factor_label, aux_factor_edit,
		combined_factor_label, combined_factor_edit
	};
	
	buttons_box = new QDialogButtonBox(
	  QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Reset | QDialogButtonBox::Help,
	  Qt::Horizontal);
	reset_button = buttons_box->button(QDialogButtonBox::Reset);
	reset_button->setEnabled(initial);
	auto help_button = buttons_box->button(QDialogButtonBox::Help);
	
	auto edit_layout = new QFormLayout();

	edit_layout->addRow(map_crs_label);
	edit_layout->addRow(tr("&Coordinate reference system:"), crs_selector);
	crs_selector->setDialogLayout(edit_layout);
	edit_layout->addRow(status_label, status_field);
	edit_layout->addItem(Util::SpacerItem::create(this));
	
	edit_layout->addRow(aspect_label);
	edit_layout->addRow(tr("Control by:"), control_geographic_radio);
	edit_layout->addRow({}, control_projected_radio);
	edit_layout->addItem(Util::SpacerItem::create(this));

	edit_layout->addRow(reference_point_label);
	edit_layout->addRow(tr("Geographic coordinates:"), geographic_ref_layout);
	edit_layout->addRow(projected_ref_label, projected_ref_layout);
	edit_layout->addRow(tr("Map coordinates:"), map_ref_layout);
	QSpacerItem *half_space = Util::SpacerItem::create(this);
	const auto size = half_space->minimumSize();
	half_space->changeSize(size.width(), size.height()/2);
	edit_layout->addItem(half_space);
	edit_layout->addRow(show_refpoint_label, link_label);
	edit_layout->addItem(Util::SpacerItem::create(this));
	
	edit_layout->addRow(map_north_label);
	edit_layout->addRow(tr("Declination:"), declination_layout);
	edit_layout->addRow(tr("Grivation:"), grivation_edit);

	bool control_scale_factor = Settings::getInstance().getSetting(Settings::MapGeoreferencing_ControlScaleFactor).toBool();
	edit_layout->addItem(Util::SpacerItem::create(this));
	edit_layout->addRow(show_scale_check);
	edit_layout->addRow(scale_compensation_label);
	edit_layout->addRow(auxiliary_factor_label, aux_factor_edit);
	edit_layout->addRow(combined_factor_label, combined_factor_edit);
	show_scale_check->setChecked(control_scale_factor);
	for (auto scale_widget: scale_widget_list)
		scale_widget->setVisible(control_scale_factor);
	
	auto layout = new QVBoxLayout();
	layout->addLayout(edit_layout);
	layout->addStretch();
	layout->addSpacing(16);
	layout->addWidget(buttons_box);
	
	setLayout(layout);
	
	connect(crs_selector, &CRSSelector::crsChanged, this, &GeoreferencingDialog::crsEdited);
	
	connect(show_scale_check, &QAbstractButton::clicked, this, &GeoreferencingDialog::showScaleChanged);
	connect(aux_factor_edit, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &GeoreferencingDialog::auxiliaryFactorEdited);
	connect(combined_factor_edit, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &GeoreferencingDialog::combinedFactorEdited);
	
	connect(map_x_edit, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &GeoreferencingDialog::mapRefChanged);
	connect(map_y_edit, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &GeoreferencingDialog::mapRefChanged);
	connect(ref_point_button, &QPushButton::clicked, this, &GeoDialogCommon::selectMapRefPoint);
	
	connect(easting_edit, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &GeoreferencingDialog::eastingNorthingEdited);
	connect(northing_edit, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &GeoreferencingDialog::eastingNorthingEdited);
	
	connect(lat_edit, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &GeoreferencingDialog::latLonEdited);
	connect(lon_edit, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &GeoreferencingDialog::latLonEdited);
	connect(control_geographic_radio, &QRadioButton::toggled, this, &GeoreferencingDialog::controlAspectChanged);
	
	connect(declination_edit, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &GeoreferencingDialog::declinationEdited);
	connect(declination_button, &QPushButton::clicked, this, &GeoreferencingDialog::requestDeclination);
	connect(grivation_edit, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &GeoreferencingDialog::grivationEdited);
	
	connect(buttons_box, &QDialogButtonBox::accepted, this, &GeoreferencingDialog::accept);
	connect(buttons_box, &QDialogButtonBox::rejected, this, &GeoreferencingDialog::reject);
	connect(reset_button, &QPushButton::clicked, this, &GeoreferencingDialog::reset);
	connect(help_button, &QPushButton::clicked, this, &GeoreferencingDialog::showHelp);
	
	connect(georef.data(), &Georeferencing::stateChanged, this, &GeoreferencingDialog::georefStateChanged);
	connect(georef.data(), &Georeferencing::transformationChanged, this, &GeoreferencingDialog::transformationChanged);
	connect(georef.data(), &Georeferencing::projectionChanged, this, &GeoreferencingDialog::projectionChanged);
	connect(georef.data(), &Georeferencing::declinationChanged, this, &GeoreferencingDialog::declinationChanged);
	connect(georef.data(), &Georeferencing::auxiliaryScaleFactorChanged, this, &GeoreferencingDialog::auxiliaryFactorChanged);
	
	transformationChanged();
	georefStateChanged();
	if (georef->getState() == Georeferencing::Local)
		projectionChanged();
	declinationChanged();
	auxiliaryFactorChanged();
}

// slot
void GeoreferencingDialog::georefStateChanged()
{
	const QSignalBlocker block(crs_selector);
	
	switch (georef->getState())
	{
	case Georeferencing::Local:
		crs_selector->setCurrentItem(Georeferencing::Local);
		control_projected_radio->setChecked(true);
		break;
	case Georeferencing::BrokenGeospatial:
	case Georeferencing::Geospatial:
		projectionChanged();
		if (!control_projected_selected)
			control_geographic_radio->setChecked(true);
		break;
	}
	
	updateWidgets();
}

// slot
void GeoreferencingDialog::transformationChanged()
{
	ScopedMultiSignalsBlocker block(
	            map_x_edit, map_y_edit,
	            easting_edit, northing_edit,
	            grivation_edit, combined_factor_edit
	);
	
	setValueIfChanged(map_x_edit, georef->getMapRefPoint().x());
	setValueIfChanged(map_y_edit, -georef->getMapRefPoint().y());
	
	setValueIfChanged(easting_edit, georef->getProjectedRefPoint().x());
	setValueIfChanged(northing_edit, georef->getProjectedRefPoint().y());
	
	setValueIfChanged(grivation_edit, georef->getGrivation());
	setValueIfChanged(combined_factor_edit, georef->getCombinedScaleFactor());
}

// slot
void GeoreferencingDialog::projectionChanged()
{
	ScopedMultiSignalsBlocker block(
	            crs_selector,
	            lat_edit, lon_edit
	);
	
	if (georef->getState() == Georeferencing::Geospatial)
	{
		const std::vector< QString >& parameters = georef->getProjectedCRSParameters();
		auto temp = CRSTemplateRegistry().find(georef->getProjectedCRSId());
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
	}
	
	LatLon latlon = georef->getGeographicRefPoint();
	double latitude  = latlon.latitude();
	double longitude = latlon.longitude();
	setValueIfChanged(lat_edit, latitude);
	setValueIfChanged(lon_edit, longitude);
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
	
	QString error = georef->getErrorText();
	if (error.length() == 0)
		status_field->setText(tr("valid"));
	else
		status_field->setText(QLatin1String("<b style=\"color:red\">") + error + QLatin1String("</b>"));
}

// slot
void GeoreferencingDialog::declinationChanged()
{
	const QSignalBlocker block(declination_edit);
	setValueIfChanged(declination_edit, georef->getDeclination());
}

// slot
void GeoreferencingDialog::auxiliaryFactorChanged()
{
	const QSignalBlocker block(aux_factor_edit);
	setValueIfChanged(aux_factor_edit, georef->getAuxiliaryScaleFactor());
}

void GeoreferencingDialog::requestDeclination(bool no_confirm)
{
	if (georef->getState() != Georeferencing::Geospatial)
		return;
	
	/// \todo Move URL (template) to settings.
	QString user_url(QString::fromLatin1("https://www.ngdc.noaa.gov/geomag-web/"));
	QUrl service_url(user_url + QLatin1String("calculators/calculateDeclination"));
	LatLon latlon(georef->getGeographicRefPoint());
	
	if (!no_confirm)
	{
		int result = QMessageBox::question(this, tr("Online declination lookup"),
		  tr("The magnetic declination for the reference point %1° %2° will now be retrieved from <a href=\"%3\">%3</a>. Do you want to continue?").
		    arg(latlon.latitude()).arg(latlon.longitude()).arg(user_url),
		  QMessageBox::Yes | QMessageBox::No,
		  QMessageBox::Yes );
		if (result != QMessageBox::Yes)
			return;
	}
	
	QUrlQuery query;
	QDate today = QDate::currentDate();
	query.addQueryItem(QString::fromLatin1("lat1"), QString::number(latlon.latitude()));
	query.addQueryItem(QString::fromLatin1("lon1"), QString::number(latlon.longitude()));
	query.addQueryItem(QString::fromLatin1("startYear"), QString::number(today.year()));
	query.addQueryItem(QString::fromLatin1("startMonth"), QString::number(today.month()));
	query.addQueryItem(QString::fromLatin1("startDay"), QString::number(today.day()));
	
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS) || defined(Q_OS_ANDROID) || !defined(QT_NETWORK_LIB)
	// No QtNetwork or no OpenSSL: open result in system browser.
	query.addQueryItem(QString::fromLatin1("resultFormat"), QString::fromLatin1("html"));
	service_url.setQuery(query);
	QDesktopServices::openUrl(service_url);
#else
	// Use result directly
	query.addQueryItem(QString::fromLatin1("resultFormat"), QString::fromLatin1("xml"));
	service_url.setQuery(query);
	
	declination_query_in_progress = true;
	updateDeclinationButton();
	
	auto network = new QNetworkAccessManager(this);
	connect(network, &QNetworkAccessManager::finished, this, &GeoreferencingDialog::declinationReplyFinished);
	network->get(QNetworkRequest(service_url));
#endif
}

bool GeoreferencingDialog::setMapRefPoint(const MapCoord& coords)
{
	georef->setMapRefPoint(coords);
	reset_button->setEnabled(true);
	return true;
}

void GeoreferencingDialog::setKeepProjectedRefCoords()
{
	control_projected_radio->setChecked(true);
	reset_button->setEnabled(true);
}

void GeoreferencingDialog::setKeepGeographicRefCoords()
{
	control_geographic_radio->setChecked(true);
	reset_button->setEnabled(true);
}

void GeoreferencingDialog::showHelp()
{
	Util::showHelp(parentWidget(), "georeferencing.html");
}

void GeoreferencingDialog::reset()
{
	*georef.data() = *initial_georef;
	reset_button->setEnabled(false);
}

void GeoreferencingDialog::accept()
{
	// Update of geographic locations (of any objects) is implicit.
	
	GeoDialogCommon::accept();
}

void GeoreferencingDialog::updateWidgets()
{
	const bool geographic_aspect_enabled = control_geographic_radio->isChecked();
	bool projected_aspect_enabled = !geographic_aspect_enabled;

	ref_point_button->setEnabled(controller);
	
	if (crs_selector->currentCRSTemplate())
		projected_ref_label->setText(crs_selector->currentCRSTemplate()->coordinatesName(crs_selector->parameters()) + QLatin1Char(':'));
	else
		projected_ref_label->setText(tr("Local coordinates:"));
	
	bool geographic_coords_enabled = crs_selector->currentCustomItem() != Georeferencing::Local;
	status_label->setVisible(geographic_coords_enabled);
	status_field->setVisible(geographic_coords_enabled);
	easting_edit->setEnabled(projected_aspect_enabled);
	northing_edit->setEnabled(projected_aspect_enabled);
	lat_edit->setEnabled(geographic_aspect_enabled);
	lon_edit->setEnabled(geographic_aspect_enabled);
	link_label->setEnabled(geographic_coords_enabled);
	
	
	declination_edit->setEnabled(geographic_aspect_enabled);
	grivation_edit->setEnabled(projected_aspect_enabled);
	updateDeclinationButton();
	aux_factor_edit->setEnabled(geographic_aspect_enabled);
	combined_factor_edit->setEnabled(projected_aspect_enabled);
	
	buttons_box->button(QDialogButtonBox::Ok)->setEnabled(georef->getState() != Georeferencing::BrokenGeospatial);
}

void GeoreferencingDialog::updateDeclinationButton()
{
	/*
	bool dialog_enabled = crs_edit->getSelectedCustomItemId() != 0;
	bool proj_spec_visible = crs_edit->getSelectedCustomItemId() == 1;
	bool geographic_coords_enabled =
		dialog_enabled &&
		(proj_spec_visible ||
		 crs_edit->getSelectedCustomItemId() == -1);
	*/
	bool enabled = !declination_query_in_progress
		&& (lat_edit->isEnabled()
			|| (georef->getState() == Georeferencing::Geospatial && grivation_edit->isEnabled()));
	declination_button->setEnabled(enabled);
	declination_button->setText(declination_query_in_progress ? tr("Loading...") : tr("Lookup..."));
}

void GeoreferencingDialog::combinedFactorEdited(double value)
{
	georef->setCombinedScaleFactor(value);
	reset_button->setEnabled(true);
}

void GeoreferencingDialog::grivationEdited(double value)
{
	georef->setGrivation(value);
	reset_button->setEnabled(true);
}

void GeoreferencingDialog::crsEdited()
{
	Georeferencing georef_copy = *georef;
	
	auto crs_template = crs_selector->currentCRSTemplate();
	auto spec = crs_selector->currentCRSSpec();
	
	auto selected_item_id = crs_selector->currentCustomItem();
	switch (selected_item_id)
	{
	default:
		qWarning("Unsupported CRS item id");
		Q_FALLTHROUGH();
	case Georeferencing::Local:
		// Local
		georef_copy.setLocalState();
		break;
	case -1:
		// CRS from list
		Q_ASSERT(crs_template);
		if (spec.isEmpty())
			spec = QStringLiteral(" ");  // intentionally non-empty: enforce non-local state.
		georef_copy.setProjectedCRS(crs_template->id(), spec, crs_selector->parameters(),
									control_geographic_radio->isChecked()
									? Georeferencing::UpdateGridParameter
									: Georeferencing::UpdateGeographicParameter);
		Q_ASSERT(georef_copy.getState() != Georeferencing::Local);
		if (control_geographic_radio->isChecked())
			georef_copy.setGeographicRefPoint(georef->getGeographicRefPoint(), Georeferencing::UpdateGridParameter);
		else
			georef_copy.setProjectedRefPoint(georef->getProjectedRefPoint(), Georeferencing::UpdateGridParameter);
		break;
	}
	
	// Apply all changes at once
	*georef = georef_copy;
	reset_button->setEnabled(true);
}

void GeoreferencingDialog::showScaleChanged(bool checked)
{
	Settings::getInstance().setSetting(Settings::MapGeoreferencing_ControlScaleFactor, checked);
	for (auto scale_widget: scale_widget_list)
		scale_widget->setVisible(checked);
}

void GeoreferencingDialog::auxiliaryFactorEdited(double value)
{
	georef->setAuxiliaryScaleFactor(value);
	reset_button->setEnabled(true);
}

void GeoreferencingDialog::mapRefChanged()
{
	MapCoord coord(map_x_edit->value(), -1 * map_y_edit->value());
	setMapRefPoint(coord);
}

void GeoreferencingDialog::eastingNorthingEdited()
{
	double easting   = easting_edit->value();
	double northing  = northing_edit->value();
	georef->setProjectedRefPoint(QPointF(easting, northing), Georeferencing::UpdateGeographicParameter);
	reset_button->setEnabled(true);
}

void GeoreferencingDialog::latLonEdited()
{
	double latitude  = lat_edit->value();
	double longitude = lon_edit->value();
	georef->setGeographicRefPoint(LatLon(latitude, longitude), Georeferencing::UpdateGridParameter);
	reset_button->setEnabled(true);
}

void GeoreferencingDialog::controlAspectChanged()
{
	if (georef->getState() != Georeferencing::Local || control_geographic_radio->isChecked())
		control_projected_selected = control_projected_radio->isChecked();
	updateWidgets();
}

void GeoreferencingDialog::declinationEdited(double value)
{
	georef->setDeclination(value);
	reset_button->setEnabled(true);
}

void GeoreferencingDialog::declinationReplyFinished(QNetworkReply* reply)
{
#if defined(QT_NETWORK_LIB)
	declination_query_in_progress = false;
	updateDeclinationButton();
	
	QString error_string;
	if (reply->error() != QNetworkReply::NoError)
	{
		error_string = reply->errorString();
	}
	else
	{
		QXmlStreamReader xml(reply);
		while (xml.readNextStartElement())
		{
			if (xml.name() == QLatin1String("maggridresult"))
			{
				while(xml.readNextStartElement())
				{
					if (xml.name() == QLatin1String("result"))
					{
						while (xml.readNextStartElement())
						{
							if (xml.name() == QLatin1String("declination"))
							{
								QString text = xml.readElementText(QXmlStreamReader::IncludeChildElements);
								bool ok;
								double declination = text.toDouble(&ok);
								if (ok)
								{
									if (control_geographic_radio->isChecked())
										setValueIfChanged(declination_edit, Georeferencing::roundDeclination(declination));
									else
									{
										double grivation = declination - georef->getConvergence();

										setValueIfChanged(grivation_edit, Georeferencing::roundDeclination(grivation));
									}
									return;
								}
								else 
								{
									error_string = tr("Could not parse data.") + QLatin1Char(' ');
								}
							}
							
							xml.skipCurrentElement(); // child of result
						}
					}
					
					xml.skipCurrentElement(); // child of mapgridresult
				}
			}
			else if (xml.name() == QLatin1String("errors"))
			{
				error_string.append(xml.readElementText(QXmlStreamReader::IncludeChildElements) + QLatin1Char(' '));
			}
			
			xml.skipCurrentElement(); // child of root
		}
		
		if (xml.error() != QXmlStreamReader::NoError)
		{
			error_string.append(xml.errorString());
		}
		else if (error_string.isEmpty())
		{
			error_string = tr("Declination value not found.");
		}
	}
	
	int result = QMessageBox::critical(this, tr("Online declination lookup"),
		tr("The online declination lookup failed:\n%1").arg(error_string),
		QMessageBox::Retry | QMessageBox::Close,
		QMessageBox::Close );
	if (result == QMessageBox::Retry)
		requestDeclination(true);
#else
	Q_UNUSED(reply)
#endif
}


}  // namespace OpenOrienteering
