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


#include "map_alignment_dialog.h"

#include <vector>

#include <Qt>
#include <QtGlobal>
#include <QtMath>
#include <QAbstractButton>
#include <QButtonGroup>
#include <QCheckBox>
#include <QCursor>
#include <QDate>
#include <QDebug>
#include <QDesktopServices>  // IWYU pragma: keep
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFlags>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLatin1Char>
#include <QLatin1String>
#include <QLocale>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPixmap>
#include <QPointF>
#include <QPushButton>
#include <QRadioButton>
#include <QRectF>
#include <QSignalBlocker>
#include <QSize>
#include <QSpacerItem>
#include <QSpinBox>
#include <QStringRef>
#include <QTimer>
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
#include "core/georeferencing.h"
#include "core/latlon.h"
#include "core/map.h" //?
#include "core/map_printer.h"
#include "gui/main_window.h"
#include "gui/map/map_editor.h"
#include "gui/util_gui.h"
#include "templates/template.h"
#include "util/backports.h"  // IWYU pragma: keep
#include "util/scoped_signals_blocker.h"


namespace OpenOrienteering {

Q_STATIC_ASSERT(Georeferencing::declinationPrecision() == Util::InputProperties<Util::RotationalDegrees>::decimals());



// ### MapAlignmentDialog ###

MapAlignmentDialog::MapAlignmentDialog(MapEditorController* controller, const Georeferencing* initial)
 : MapAlignmentDialog(controller->getWindow(), controller, controller->getMap(), initial)
{
	// nothing else
}

MapAlignmentDialog::MapAlignmentDialog(QWidget* parent, Map* map, const Georeferencing* initial)
 : MapAlignmentDialog(parent, nullptr, map, initial)
{
	// nothing else
}

MapAlignmentDialog::MapAlignmentDialog(
        QWidget* parent,
        MapEditorController* controller,
        Map* map,
        const Georeferencing* initial )
 : GeoDialogCommon(parent, controller, map, initial)
 , declination_query_in_progress(false)
{
	setWindowTitle(tr("Map alignment"));
	setWindowModality(Qt::WindowModal);
	
	// Create widgets
	auto reference_point_label = Util::Headline::create(tr("Reference point"));
	
	ref_point_button = new QPushButton(tr("&Pick on map"));
	int ref_point_button_width = ref_point_button->sizeHint().width();
	
	map_x_edit = Util::SpinBox::create<MapCoordF>(tr("mm"));
	map_y_edit = Util::SpinBox::create<MapCoordF>(tr("mm"));
	ref_point_button->setEnabled(controller);
	auto map_ref_layout = new QHBoxLayout();
	map_ref_layout->addWidget(map_x_edit, 1);
	map_ref_layout->addWidget(new QLabel(tr("X", "x coordinate")), 0);
	map_ref_layout->addWidget(map_y_edit, 1);
	map_ref_layout->addWidget(new QLabel(tr("Y", "y coordinate")), 0);
	map_ref_layout->addWidget(ref_point_button, 0);
	
	auto map_north_label = Util::Headline::create(tr("Map north"));
	
	declination_edit = Util::SpinBox::create_optional<Util::RotationalDegrees>();
	declination_button = new QPushButton(tr("Lookup..."));
	auto declination_layout = new QHBoxLayout();
	declination_layout->addWidget(declination_edit, 1);
	declination_layout->addWidget(declination_button, 0);
	
	grivation_edit = Util::SpinBox::create<Util::RotationalDegrees>();
	
	scale_edit = Util::SpinBox::create(1, 9999999, {}, 500);
	scale_edit->setPrefix(QStringLiteral("1 : "));
	scale_edit->setButtonSymbols(QAbstractSpinBox::NoButtons);
	scale_edit->setValue(static_cast<int>(map->getScaleDenominator()));
	
	show_scale_check = new QCheckBox(tr("Show scale factors"));
	
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

	edit_layout->addRow(reference_point_label);
	edit_layout->addRow(tr("Map coordinates:"), map_ref_layout);
	edit_layout->addItem(Util::SpacerItem::create(this));
	
	edit_layout->addRow(map_north_label);
	edit_layout->addRow(tr("Declination:"), declination_layout);
	edit_layout->addRow(tr("Grivation:"), grivation_edit);

	bool control_scale_factor = Settings::getInstance().getSetting(Settings::MapGeoreferencing_ControlScaleFactor).toBool();
	edit_layout->addItem(Util::SpacerItem::create(this));
	edit_layout->addRow(Util::Headline::create(tr("Scale")));
	edit_layout->addRow(tr("Scale:"), scale_edit);
	edit_layout->addRow(show_scale_check);
	edit_layout->addRow(auxiliary_factor_label, aux_factor_edit);
	edit_layout->addRow(combined_factor_label, combined_factor_edit);
	show_scale_check->setChecked(control_scale_factor);
	for (auto scale_widget: scale_widget_list)
		scale_widget->setVisible(control_scale_factor);
	
	edit_layout->addItem(Util::SpacerItem::create(this));
	edit_layout->addRow(Util::Headline::create(tr("Options")));

	adjust_symbols_check = new QCheckBox(tr("Scale symbol sizes"));
	if (map->getNumSymbols() > 0)
		adjust_symbols_check->setChecked(true);
	adjust_symbols_check->setEnabled(false);
	edit_layout->addRow(adjust_symbols_check);
	
	adjust_templates_check = new QCheckBox(tr("Align non-georeferenced templates"));
	bool have_non_georeferenced_template = false;
	for (int i = 0; i < map->getNumTemplates() && !have_non_georeferenced_template; ++i)
		have_non_georeferenced_template = !map->getTemplate(i)->isTemplateGeoreferenced();
	for (int i = 0; i < map->getNumClosedTemplates() && !have_non_georeferenced_template; ++i)
		have_non_georeferenced_template = !map->getClosedTemplate(i)->isTemplateGeoreferenced();
	if (have_non_georeferenced_template)
		adjust_templates_check->setChecked(true);
	else
		adjust_templates_check->setEnabled(false);
	edit_layout->addRow(adjust_templates_check);

	auto layout = new QVBoxLayout();
	layout->addLayout(edit_layout);
	layout->addStretch();
	layout->addSpacing(16);
	layout->addWidget(buttons_box);
	
	setLayout(layout);
	
	connect(scale_edit, QOverload<int>::of(&QSpinBox::valueChanged), this, &MapAlignmentDialog::scaleEdited);
	connect(show_scale_check, &QAbstractButton::clicked, this, &MapAlignmentDialog::showScaleChanged);
	connect(aux_factor_edit, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MapAlignmentDialog::auxiliaryFactorEdited);
	connect(combined_factor_edit, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MapAlignmentDialog::combinedFactorEdited);
	
	connect(map_x_edit, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MapAlignmentDialog::mapRefChanged);
	connect(map_y_edit, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MapAlignmentDialog::mapRefChanged);
	connect(ref_point_button, &QPushButton::clicked, this, &GeoDialogCommon::selectMapRefPoint);
	
	connect(declination_edit, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MapAlignmentDialog::declinationEdited);
	connect(declination_button, &QPushButton::clicked, this, &MapAlignmentDialog::requestDeclination);
	connect(grivation_edit, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MapAlignmentDialog::grivationEdited);
	
	connect(buttons_box, &QDialogButtonBox::accepted, this, &MapAlignmentDialog::accept);
	connect(buttons_box, &QDialogButtonBox::rejected, this, &MapAlignmentDialog::reject);
	connect(reset_button, &QPushButton::clicked, this, &MapAlignmentDialog::reset);
	connect(help_button, &QPushButton::clicked, this, &MapAlignmentDialog::showHelp);
	
	connect(georef.data(), &Georeferencing::transformationChanged, this, &MapAlignmentDialog::transformationChanged);
	connect(georef.data(), &Georeferencing::declinationChanged, this, &MapAlignmentDialog::declinationChanged);
	connect(georef.data(), &Georeferencing::auxiliaryScaleFactorChanged, this, &MapAlignmentDialog::auxiliaryFactorChanged);
	
	transformationChanged();
	declinationChanged();
	auxiliaryFactorChanged();
}

// slot
void MapAlignmentDialog::transformationChanged()
{
	ScopedMultiSignalsBlocker block(
	            map_x_edit, map_y_edit,
	            grivation_edit, combined_factor_edit
	);
	
	setValueIfChanged(map_x_edit, georef->getMapRefPoint().x());
	setValueIfChanged(map_y_edit, -georef->getMapRefPoint().y());
	
	setValueIfChanged(grivation_edit, georef->getGrivation());
	setValueIfChanged(combined_factor_edit, georef->getCombinedScaleFactor());
}

// slot
void MapAlignmentDialog::declinationChanged()
{
	const QSignalBlocker block(declination_edit);
	setValueIfChanged(declination_edit, georef->hasDeclination() ? georef->getDeclination() : declination_edit->minimum());

	updateWidgets();
}

// slot
void MapAlignmentDialog::auxiliaryFactorChanged()
{
	const QSignalBlocker block(aux_factor_edit);
	setValueIfChanged(aux_factor_edit, georef->getAuxiliaryScaleFactor());

	updateWidgets();
}

void MapAlignmentDialog::requestDeclination(bool no_confirm)
{
	if (georef->getState() == Georeferencing::Local)
		return;
	
	/// \todo Move URL (template) to settings.
	QString user_url(QString::fromLatin1("https://www.ngdc.noaa.gov/geomag-web/"));
	QUrl service_url(user_url + QLatin1String("calculators/calculateDeclination"));
	LatLon latlon(georef->getGeographicRefPoint());
	
	if (!no_confirm)
	{
		int result = QMessageBox::question(this, tr("Online declination lookup"),
		  trUtf8("The magnetic declination for the reference point %1° %2° will now be retrieved from <a href=\"%3\">%3</a>. Do you want to continue?").
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
	connect(network, &QNetworkAccessManager::finished, this, &MapAlignmentDialog::declinationReplyFinished);
	network->get(QNetworkRequest(service_url));
#endif
}

bool MapAlignmentDialog::setMapRefPoint(const MapCoord& coords)
{
	georef->setMapRefPoint(coords);
	reset_button->setEnabled(true);
	return true;
}

void MapAlignmentDialog::showHelp()
{
	Util::showHelp(parentWidget(), "georeferencing.html");
}

void MapAlignmentDialog::reset()
{
	*georef.data() = *initial_georef;
	{
		const QSignalBlocker block(scale_edit);
		scale_edit->setValue(initial_georef->getScaleDenominator());
	}
	adjust_symbols_check->setEnabled(false);
	reset_button->setEnabled(false);
}

void MapAlignmentDialog::accept()
{
	auto const grivation_change_degrees = georef->getGrivation() - initial_georef->getGrivation();
	auto const scale_factor_change = georef->getCombinedScaleFactor() / initial_georef->getCombinedScaleFactor();
	auto const scale_change = double(georef->getScaleDenominator()) / double(initial_georef->getScaleDenominator());

	MapCoord center = initial_georef->getMapRefPoint();
	const bool adjust_symbols = adjust_symbols_check->isChecked();
	const bool adjust_reference_point = true;
	const bool adjust_declination = false;
	const bool adjust_templates = adjust_templates_check->isChecked();
	const double rotation = M_PI * grivation_change_degrees / 180;
	map->rotateMap(rotation, center, adjust_reference_point, adjust_declination, adjust_templates);
	map->changeScale(georef->getScaleDenominator(), 1.0/scale_factor_change, center, adjust_symbols, true, adjust_reference_point, false, adjust_templates);

	auto const initial_map_ref_point = initial_georef->getMapRefPoint();
	// CRSes are identical; no need to go via geographic ref point
	auto const shifted_map_ref_point = georef->toMapCoordF(initial_georef->toProjectedCoords(MapCoordF(initial_map_ref_point)));
	auto const map_object_shift = MapCoord(shifted_map_ref_point) - initial_map_ref_point;
	map->shiftMap(map_object_shift, false, adjust_templates);

	// Replicate the coordinate transformation that has been applied to map objects.
	QTransform from_initial;
	from_initial.translate(shifted_map_ref_point.x(), shifted_map_ref_point.y());
	from_initial.rotateRadians(-rotation);
	from_initial.scale(1.0/(scale_change*scale_factor_change), 1.0/(scale_change*scale_factor_change));
	from_initial.translate(-initial_map_ref_point.x(), -initial_map_ref_point.y());

	// Adjust map's print area.
	if (map->hasPrinterConfig())
	{
		auto printer_config = map->printerConfig();
		auto const initial_print_center = MapCoordF(printer_config.print_area.center());
		auto const shifted_print_center = MapCoordF(from_initial.map(QPointF(initial_print_center)));
		printer_config.print_area.moveCenter(shifted_print_center);
		map->setPrinterConfig(printer_config);
	}

	// Adjust the MapView and the PrintWidget.
	emit mapObjectsShifted(from_initial);
	
	GeoDialogCommon::accept();
}

void MapAlignmentDialog::updateWidgets()
{
	ref_point_button->setEnabled(controller);
	
	declination_edit->setEnabled(georef->getState() == Georeferencing::Geospatial && georef->hasGeographicRefPoint());
	updateDeclinationButton();
	aux_factor_edit->setEnabled(georef->getState() == Georeferencing::Geospatial && georef->hasGeographicRefPoint());
	
	buttons_box->button(QDialogButtonBox::Ok)->setEnabled(georef->isValid());
}

void MapAlignmentDialog::updateDeclinationButton()
{
	bool enabled = georef->getState() == Georeferencing::Geospatial
		&& georef->hasGeographicRefPoint()
		&& !declination_query_in_progress;
	declination_button->setEnabled(enabled);
	declination_button->setText(declination_query_in_progress ? tr("Loading...") : tr("Lookup..."));
}

void MapAlignmentDialog::combinedFactorEdited(double value)
{
	georef->setCombinedScaleFactor(value);
	reset_button->setEnabled(true);
}

void MapAlignmentDialog::grivationEdited(double value)
{
	georef->setGrivation(value);
	reset_button->setEnabled(true);
}

void MapAlignmentDialog::scaleEdited(int value)
{
	georef->setScaleDenominator(value);
	adjust_symbols_check->setEnabled(map->getNumSymbols() > 0);
	reset_button->setEnabled(true);
}

void MapAlignmentDialog::showScaleChanged(bool checked)
{
	Settings::getInstance().setSetting(Settings::MapGeoreferencing_ControlScaleFactor, checked);
	for (auto scale_widget: scale_widget_list)
		scale_widget->setVisible(checked);
}

void MapAlignmentDialog::auxiliaryFactorEdited(double value)
{
	georef->setAuxiliaryScaleFactor(value);
	reset_button->setEnabled(true);
}

void MapAlignmentDialog::mapRefChanged()
{
	MapCoord coord(map_x_edit->value(), -1 * map_y_edit->value());
	setMapRefPoint(coord);
}

void MapAlignmentDialog::declinationEdited(double value)
{
	if (value != declination_edit->minimum())
		georef->setDeclination(value);
	reset_button->setEnabled(true);
}

void MapAlignmentDialog::declinationReplyFinished(QNetworkReply* reply)
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
									setValueIfChanged(declination_edit, Georeferencing::roundDeclination(declination));
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
