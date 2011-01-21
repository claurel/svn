// This file is part of Cosmographia.
//
// Copyright (C) 2010 Chris Laurel <claurel@gmail.com>
//
// Cosmographia is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
//
// Cosmographia is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with Cosmographia. If not, see <http://www.gnu.org/licenses/>.

#include <QtGui>

#include "UniverseView.h"
#include "Cosmographia.h"
#include "QVideoEncoder.h"
#include <vesta/GregorianDate.h>
#include <vesta/Body.h>
#include <vesta/Arc.h>
#include <vesta/Trajectory.h>
#include <vesta/Frame.h>
#include <vesta/InertialFrame.h>
#include <vesta/RotationModel.h>
#include <vesta/KeplerianTrajectory.h>
#include <vesta/Units.h>
#include <qjson/parser.h>
#include <algorithm>

using namespace vesta;


Cosmographia::Cosmographia() :
    m_fullScreenAction(NULL)
{
    m_view3d = new UniverseView();
    setCentralWidget(m_view3d);

    setWindowTitle(tr("Cosmographia"));

    /*** File Menu ***/
    QMenu* fileMenu = new QMenu("&File", this);
    QAction* saveScreenShotAction = fileMenu->addAction("&Save Screen Shot");
    QAction* recordVideoAction = fileMenu->addAction("&Record Video");
    recordVideoAction->setShortcut(QKeySequence("Ctrl+R"));
    fileMenu->addSeparator();
    QAction* loadSolarSystemAction = fileMenu->addAction("&Load Solar System");
    fileMenu->addSeparator();
    QAction* quitAction = fileMenu->addAction("&Quit");
    this->menuBar()->addMenu(fileMenu);

    connect(saveScreenShotAction, SIGNAL(triggered()), this, SLOT(saveScreenShot()));
    connect(recordVideoAction, SIGNAL(triggered()), this, SLOT(recordVideo()));
    connect(loadSolarSystemAction, SIGNAL(triggered()), this, SLOT(loadSolarSystem()));
    connect(quitAction, SIGNAL(triggered()), this, SLOT(close()));

    /*** Time Menu ***/
    QMenu* timeMenu = new QMenu("&Time", this);
    QAction* pauseAction = new QAction("&Pause", timeMenu);
    pauseAction->setCheckable(true);
    pauseAction->setShortcut(Qt::Key_Space);
    timeMenu->addAction(pauseAction);
    QAction* fasterAction = new QAction("&Faster", timeMenu);
    fasterAction->setShortcut(QKeySequence("Ctrl+L"));
    timeMenu->addAction(fasterAction);
    QAction* slowerAction = new QAction("&Slower", timeMenu);
    slowerAction->setShortcut(QKeySequence("Ctrl+K"));
    timeMenu->addAction(slowerAction);
    QAction* faster2Action = new QAction("2x Faster", timeMenu);
    faster2Action->setShortcut(QKeySequence("Ctrl+Shift+L"));
    timeMenu->addAction(faster2Action);
    QAction* slower2Action = new QAction("2x Slower", timeMenu);
    slower2Action->setShortcut(QKeySequence("Ctrl+Shift+K"));
    timeMenu->addAction(slower2Action);
    QAction* backYearAction = new QAction("Back one year", timeMenu);
    backYearAction->setShortcut(QKeySequence("Ctrl+["));
    timeMenu->addAction(backYearAction);
    QAction* forwardYearAction = new QAction("Forward one year", timeMenu);
    forwardYearAction->setShortcut(QKeySequence("Ctrl+]"));
    timeMenu->addAction(forwardYearAction);
    QAction* reverseAction = new QAction("&Reverse", timeMenu);
    reverseAction->setShortcut(QKeySequence("Ctrl+J"));
    timeMenu->addAction(reverseAction);
    QAction* nowAction = new QAction("&Current time", timeMenu);
    timeMenu->addAction(nowAction);
    this->menuBar()->addMenu(timeMenu);

    connect(pauseAction,   SIGNAL(triggered(bool)), m_view3d, SLOT(setPaused(bool)));
    connect(fasterAction,  SIGNAL(triggered()),     this,     SLOT(faster()));
    connect(slowerAction,  SIGNAL(triggered()),     this,     SLOT(slower()));
    connect(faster2Action,  SIGNAL(triggered()),     this,     SLOT(faster2()));
    connect(slower2Action,  SIGNAL(triggered()),     this,     SLOT(slower2()));
    connect(backYearAction,  SIGNAL(triggered()),     this,     SLOT(backYear()));
    connect(forwardYearAction,  SIGNAL(triggered()),     this,     SLOT(forwardYear()));
    connect(reverseAction, SIGNAL(triggered()),     this,     SLOT(reverseTime()));
    connect(nowAction,     SIGNAL(triggered()),     m_view3d, SLOT(setCurrentTime()));

    /*** Camera Menu ***/
    QMenu* cameraMenu = new QMenu("&Camera", this);
    QActionGroup* cameraFrameGroup = new QActionGroup(cameraMenu);
    QAction* inertialAction = new QAction("&Inertial Frame", cameraFrameGroup);
    inertialAction->setShortcut(QKeySequence("Ctrl+I"));
    inertialAction->setCheckable(true);
    inertialAction->setChecked(true);
    cameraMenu->addAction(inertialAction);
    QAction* bodyFixedAction = new QAction("&Body Fixed Frame", cameraFrameGroup);
    bodyFixedAction->setShortcut(QKeySequence("Ctrl+B"));
    bodyFixedAction->setCheckable(true);
    cameraMenu->addAction(bodyFixedAction);
    QAction* synodicAction = new QAction("&Synodic Frame", cameraFrameGroup);
    synodicAction->setShortcut(QKeySequence("Ctrl+Y"));
    synodicAction->setCheckable(true);
    cameraMenu->addAction(synodicAction);
    this->menuBar()->addMenu(cameraMenu);
    QAction* centerAction = new QAction("Set &Center", cameraMenu);
    centerAction->setShortcut(QKeySequence("Ctrl+C"));
    cameraMenu->addAction(centerAction);

    connect(inertialAction,  SIGNAL(triggered(bool)), m_view3d, SLOT(inertialObserver(bool)));
    connect(bodyFixedAction, SIGNAL(triggered(bool)), m_view3d, SLOT(bodyFixedObserver(bool)));
    connect(synodicAction,   SIGNAL(triggered(bool)), m_view3d, SLOT(synodicObserver(bool)));
    connect(centerAction, SIGNAL(triggered()), m_view3d, SLOT(setObserverCenter()));

    /*** Visual aids menu ***/
    QMenu* visualAidsMenu = new QMenu("&Visual Aids", this);

    // Arrows
    QAction* bodyAxesAction = new QAction("&Body axes", visualAidsMenu);
    bodyAxesAction->setCheckable(true);
    visualAidsMenu->addAction(bodyAxesAction);
    QAction* frameAxesAction = new QAction("&Frame axes", visualAidsMenu);
    frameAxesAction->setCheckable(true);
    visualAidsMenu->addAction(frameAxesAction);
    QAction* velocityAction = new QAction("&Velocity arrow", visualAidsMenu);
    velocityAction->setCheckable(true);
    visualAidsMenu->addAction(velocityAction);
    QAction* nadirAction = new QAction("&Nadir arrow", visualAidsMenu);
    nadirAction->setCheckable(true);
    visualAidsMenu->addAction(nadirAction);

    bodyAxesAction->setEnabled(false);
    frameAxesAction->setEnabled(false);
    velocityAction->setEnabled(false);
    nadirAction->setEnabled(false);

    visualAidsMenu->addSeparator();

    QAction* eqGridAction = new QAction("E&quatorial grid", visualAidsMenu);
    eqGridAction->setCheckable(true);
    visualAidsMenu->addAction(eqGridAction);
    QAction* eclipticAction = new QAction("&Ecliptic", visualAidsMenu);
    eclipticAction->setCheckable(true);
    visualAidsMenu->addAction(eclipticAction);
    QAction* eqPlaneAction = new QAction("E&quatorial plane", visualAidsMenu);
    eqPlaneAction->setCheckable(true);
    visualAidsMenu->addAction(eqPlaneAction);
    QAction* planetGridAction = new QAction("Planetographic grid", visualAidsMenu);
    planetGridAction->setCheckable(true);
    visualAidsMenu->addAction(planetGridAction);
    QAction* antennaLobeAction = new QAction("&Antenna lobe", visualAidsMenu);
    antennaLobeAction->setCheckable(true);
    visualAidsMenu->addAction(antennaLobeAction);

    visualAidsMenu->addSeparator();

    QAction* trajectoriesAction = new QAction("&Trajectories", visualAidsMenu);
    trajectoriesAction->setCheckable(true);
    visualAidsMenu->addAction(trajectoriesAction);
    QAction* planetOrbitsAction = new QAction("Planet &orbits", visualAidsMenu);
    planetOrbitsAction->setShortcut(QKeySequence("Ctrl+O"));
    planetOrbitsAction->setCheckable(true);
    visualAidsMenu->addAction(planetOrbitsAction);
    QAction* plotTrajectoryAction = new QAction("&Plot trajectory", visualAidsMenu);
    plotTrajectoryAction->setShortcut(QKeySequence("Ctrl+P"));
    visualAidsMenu->addAction(plotTrajectoryAction);
    QAction* plotTrajectoryObserverAction = new QAction("&Plot trajectory in observer frame", visualAidsMenu);
    plotTrajectoryObserverAction->setShortcut(QKeySequence("Shift+Ctrl+P"));
    visualAidsMenu->addAction(plotTrajectoryObserverAction);

    visualAidsMenu->addSeparator();
    QActionGroup* labelGroup = new QActionGroup(visualAidsMenu);
    QAction* noLabelAction = new QAction("No labels", labelGroup);
    visualAidsMenu->addAction(noLabelAction);
    noLabelAction->setCheckable(true);
    noLabelAction->setData(int(UniverseView::NoLabels));
    QAction* labelOnlyAction = new QAction("Labels only", labelGroup);
    visualAidsMenu->addAction(labelOnlyAction);
    labelOnlyAction->setCheckable(true);
    labelOnlyAction->setData(int(UniverseView::LabelsOnly));
    QAction* iconOnlyAction = new QAction("Icons only", labelGroup);
    visualAidsMenu->addAction(iconOnlyAction);
    iconOnlyAction->setCheckable(true);
    iconOnlyAction->setData(int(UniverseView::IconsOnly));
    QAction* labelAndIconAction = new QAction("Labels and icons", labelGroup);
    visualAidsMenu->addAction(labelAndIconAction);
    labelAndIconAction->setCheckable(true);
    labelAndIconAction->setData(int(UniverseView::LabelsAndIcons));
    noLabelAction->setChecked(true);

    visualAidsMenu->addSeparator();
    QAction* infoTextAction = new QAction("Info text", visualAidsMenu);
    infoTextAction->setCheckable(true);
    infoTextAction->setChecked(true);
    visualAidsMenu->addAction(infoTextAction);

    this->menuBar()->addMenu(visualAidsMenu);

    connect(bodyAxesAction, SIGNAL(triggered(bool)), m_view3d, SLOT(toggleBodyAxes(bool)));
    connect(frameAxesAction, SIGNAL(triggered(bool)), m_view3d, SLOT(toggleFrameAxes(bool)));
    connect(velocityAction, SIGNAL(triggered(bool)), m_view3d, SLOT(toggleVelocityVector(bool)));
    connect(eqGridAction, SIGNAL(triggered(bool)), m_view3d, SLOT(setEquatorialGridVisibility(bool)));
    connect(eclipticAction, SIGNAL(triggered(bool)), m_view3d, SLOT(setEclipticVisibility(bool)));
    connect(eqPlaneAction, SIGNAL(triggered(bool)), m_view3d, SLOT(setEquatorialPlaneVisibility(bool)));
    connect(planetGridAction, SIGNAL(triggered(bool)), m_view3d, SLOT(setPlanetographicGridVisibility(bool)));
    connect(antennaLobeAction, SIGNAL(triggered(bool)), m_view3d, SLOT(setAntennaLobeVisibility(bool)));
    connect(trajectoriesAction, SIGNAL(triggered(bool)), m_view3d, SLOT(setTrajectoryVisibility(bool)));
    connect(planetOrbitsAction, SIGNAL(triggered(bool)), m_view3d, SLOT(setPlanetOrbitsVisibility(bool)));
    connect(plotTrajectoryAction, SIGNAL(triggered()), m_view3d, SLOT(plotTrajectory()));
    connect(plotTrajectoryObserverAction, SIGNAL(triggered()), m_view3d, SLOT(plotTrajectoryObserver()));
    connect(infoTextAction, SIGNAL(triggered(bool)), m_view3d, SLOT(setInfoText(bool)));

    connect(labelGroup, SIGNAL(triggered(QAction*)), this, SLOT(setLabelMode(QAction*)));

    /*** Graphics menu ***/
    QMenu* graphicsMenu = new QMenu("&Graphics", this);
    QAction* normalMapAction = new QAction("&Normal map", graphicsMenu);
    normalMapAction->setCheckable(true);
    graphicsMenu->addAction(normalMapAction);
    QAction* shadowsAction = new QAction("&Shadows", graphicsMenu);
    shadowsAction->setCheckable(true);
    graphicsMenu->addAction(shadowsAction);
    QAction* atmospheresAction = new QAction("&Atmosphere", graphicsMenu);
    atmospheresAction->setCheckable(true);
    atmospheresAction->setShortcut(QKeySequence("Ctrl+A"));
    graphicsMenu->addAction(atmospheresAction);
    QAction* cloudLayerAction = new QAction("&Cloud layer", graphicsMenu);
    cloudLayerAction->setCheckable(true);
    graphicsMenu->addAction(cloudLayerAction);
    QAction* realisticPlanetsAction = new QAction("Realistic &planets", graphicsMenu);
    realisticPlanetsAction->setCheckable(true);
    graphicsMenu->addAction(realisticPlanetsAction);
    QAction* ambientLightAction = new QAction("Ambient &light", graphicsMenu);
    ambientLightAction->setCheckable(true);
    ambientLightAction->setChecked(true);
    graphicsMenu->addAction(ambientLightAction);
    QAction* reflectionsAction = new QAction("&Reflections", graphicsMenu);
    reflectionsAction->setCheckable(true);
    graphicsMenu->addAction(reflectionsAction);
    QAction* milkyWayAction = new QAction("&Milky Way", graphicsMenu);
    milkyWayAction->setCheckable(true);
    milkyWayAction->setShortcut(QKeySequence("Ctrl+M"));
    graphicsMenu->addAction(milkyWayAction);
    QAction* asteroidsAction = new QAction("As&teroids", graphicsMenu);
    asteroidsAction->setCheckable(true);
    asteroidsAction->setShortcut(QKeySequence("Ctrl+T"));
    graphicsMenu->addAction(asteroidsAction);
    QAction* highlightAsteroidsAction = new QAction("Highlight asteroid family", graphicsMenu);
    highlightAsteroidsAction->setShortcut(QKeySequence("Ctrl+Shift+T"));
    graphicsMenu->addAction(highlightAsteroidsAction);
    graphicsMenu->addSeparator();
    m_fullScreenAction = new QAction("Full Screen", graphicsMenu);
    m_fullScreenAction->setShortcut(QKeySequence("Ctrl+F"));
    m_fullScreenAction->setCheckable(true);
    graphicsMenu->addAction(m_fullScreenAction);
    connect(m_fullScreenAction, SIGNAL(toggled(bool)), this, SLOT(setFullScreen(bool)));
    QAction* anaglyphAction = new QAction("Anaglyph stereo", graphicsMenu);
    anaglyphAction->setShortcut(QKeySequence("Ctrl+Shift+A"));
    anaglyphAction->setCheckable(true);
    graphicsMenu->addAction(anaglyphAction);

    this->menuBar()->addMenu(graphicsMenu);

    connect(normalMapAction,        SIGNAL(triggered(bool)), m_view3d, SLOT(setNormalMaps(bool)));
    connect(shadowsAction,          SIGNAL(triggered(bool)), m_view3d, SLOT(setShadows(bool)));
    connect(atmospheresAction,      SIGNAL(triggered(bool)), m_view3d, SLOT(setAtmospheres(bool)));
    connect(cloudLayerAction,       SIGNAL(triggered(bool)), m_view3d, SLOT(setCloudLayerVisibility(bool)));
    connect(realisticPlanetsAction, SIGNAL(triggered(bool)), m_view3d, SLOT(setRealisticPlanets(bool)));
    connect(ambientLightAction,     SIGNAL(triggered(bool)), m_view3d, SLOT(setAmbientLight(bool)));
    connect(reflectionsAction,      SIGNAL(triggered(bool)), m_view3d, SLOT(setReflections(bool)));
    connect(milkyWayAction,         SIGNAL(triggered(bool)), m_view3d, SLOT(setMilkyWayVisibility(bool)));
    connect(asteroidsAction,        SIGNAL(triggered(bool)), m_view3d, SLOT(setAsteroidVisibility(bool)));
    connect(highlightAsteroidsAction, SIGNAL(triggered()),   m_view3d, SLOT(highlightAsteroidFamily()));
    connect(anaglyphAction,         SIGNAL(triggered(bool)), m_view3d, SLOT(setAnaglyphStereo(bool)));

    /*** Help menu ***/
    QMenu* helpMenu = new QMenu("Help", this);

    QAction* aboutAction = new QAction("About QtCosmographia", helpMenu);
    helpMenu->addAction(aboutAction);
    connect(aboutAction, SIGNAL(triggered()), this, SLOT(about()));

    this->menuBar()->addMenu(helpMenu);

    setCursor(QCursor(Qt::CrossCursor));

    loadSettings();
}


Cosmographia::~Cosmographia()
{
    saveSettings();
}


void
Cosmographia::faster()
{
    double t = m_view3d->timeScale() * 10.0;
    if (t < -1.0e7)
        t = -1.0e7;
    else if (t > 1.0e7)
        t = 1.0e7;
    m_view3d->setTimeScale(t);
}


void
Cosmographia::slower()
{
    double t = m_view3d->timeScale() * 0.1;
    if (t > 0.0)
        t = std::max(1.0e-3, t);
    else if (t < 0.0)
        t = std::min(-1.0e-3, t);
    m_view3d->setTimeScale(t);
}


void
Cosmographia::faster2()
{
    double t = m_view3d->timeScale() * 2.0;
    if (t < -1.0e7)
        t = -1.0e7;
    else if (t > 1.0e7)
        t = 1.0e7;
    m_view3d->setTimeScale(t);
}


void
Cosmographia::slower2()
{
    double t = m_view3d->timeScale() * 0.5;
    if (t > 0.0)
        t = std::max(1.0e-3, t);
    else if (t < 0.0)
        t = std::min(-1.0e-3, t);
    m_view3d->setTimeScale(t);
}


void
Cosmographia::backYear()
{
    vesta::GregorianDate d = vesta::GregorianDate::UTCDateFromTDBSec(m_view3d->simulationTime());
    m_view3d->setSimulationTime(vesta::GregorianDate(d.year() - 1, d.month(), d.day(), d.hour(), d.minute(), d.second()).toTDBSec());
}


void
Cosmographia::forwardYear()
{
    vesta::GregorianDate d = vesta::GregorianDate::UTCDateFromTDBSec(m_view3d->simulationTime());
    m_view3d->setSimulationTime(vesta::GregorianDate(d.year() + 1, d.month(), d.day(), d.hour(), d.minute(), d.second()).toTDBSec());
}


void
Cosmographia::reverseTime()
{
    m_view3d->setTimeScale(-m_view3d->timeScale());
}


void
Cosmographia::setLabelMode(QAction* action)
{
    UniverseView::LabelMode mode = (UniverseView::LabelMode) action->data().toInt();
    m_view3d->setLabelMode(mode);
}


void
Cosmographia::setFullScreen(bool enabled)
{
    if (enabled)
    {
        showFullScreen();
    }
    else
    {
        showNormal();
    }
}

void
Cosmographia::about()
{
    QMessageBox::about(this,
                       "Cosmographia",
                       "Cosmographia: "
                       "A celebration of solar system exploration.");
}


void
Cosmographia::saveScreenShot()
{
    QImage screenShot = m_view3d->grabFrameBuffer(false);

    QString defaultFileName = QDesktopServices::storageLocation(QDesktopServices::PicturesLocation) + "/image.png";
    QString saveFileName = QFileDialog::getSaveFileName(this, "Save Image As...", defaultFileName, "*.png *.jpg *.webm *.mov *.ogg");
    if (!saveFileName.isEmpty())
    {
        screenShot.save(saveFileName);
    }
}


void
Cosmographia::loadSettings()
{
    QSettings settings;

    settings.beginGroup("ui");
    m_fullScreenAction->setChecked(settings.value("fullscreen", true).toBool());
    setFullScreen(m_fullScreenAction->isChecked());
    settings.endGroup();
}


void
Cosmographia::saveSettings()
{
    QSettings settings;

    settings.beginGroup("ui");
    settings.setValue("fullscreen", m_fullScreenAction->isChecked());
    settings.endGroup();
}


void
Cosmographia::recordVideo()
{
    if (m_view3d->isRecordingVideo())
    {
        m_view3d->videoEncoder()->close();
        m_view3d->finishVideoRecording();
    }
    else
    {
        QString defaultFileName = QDesktopServices::storageLocation(QDesktopServices::PicturesLocation) + "/cosmo.mpeg";
        QString saveFileName = QFileDialog::getSaveFileName(this, "Save Video As...", defaultFileName, "Video (*.mkv *.mpeg *.avi)");
        if (!saveFileName.isEmpty())
        {
            QVideoEncoder* encoder = new QVideoEncoder();
            encoder->createFile(saveFileName, 848, 480, 5000000, 20);
            m_view3d->startVideoRecording(encoder);
        }
    }
}


static double doubleValue(QVariant v, double defaultValue = 0.0)
{
    bool ok = false;
    double value = v.toDouble(&ok);
    if (ok)
    {
        return value;
    }
    else
    {
        return defaultValue;
    }
}

vesta::Trajectory* loadFixedTrajectory(const QVariantMap& info)
{
    qDebug() << "Trajectory: " << info.value("type").toString();
    return NULL;
}


vesta::Trajectory* loadKeplerianTrajectory(const QVariantMap& info)
{
    qDebug() << "Trajectory: " << info.value("type").toString();

    double sma = doubleValue(info.value("semiMajorAxis"), 0.0);
    if (sma <= 0.0)
    {
        qDebug() << "Invalid semimajor axis given for Keplerian orbit.";
        return NULL;
    }

    double period = doubleValue(info.value("period"), 0.0);
    if (period <= 0.0)
    {
        qDebug() << "Invalid period given for Keplerian orbit.";
        return NULL;
    }

    OrbitalElements elements;
    elements.eccentricity = doubleValue(info.value("eccentricity"));
    elements.inclination = toRadians(doubleValue(info.value("eccentricity")));
    elements.meanMotion = toRadians(360.0) / daysToSeconds(period);
    elements.longitudeOfAscendingNode = toRadians(doubleValue(info.value("ascendingNode")));
    elements.argumentOfPeriapsis = toRadians(doubleValue(info.value("argumentOfPeriapsis")));
    elements.meanAnomalyAtEpoch = toRadians(doubleValue(info.value("meanAnomaly")));

    KeplerianTrajectory* trajectory = new KeplerianTrajectory(elements);
}


vesta::Trajectory* loadTrajectory(const QVariantMap& map)
{
    QVariant typeData = map.value("type");
    if (typeData.type() != QVariant::String)
    {
        qDebug() << "Trajectory definition is missing type.";
    }

    QString type = typeData.toString();
    if (type == "Fixed")
    {
        return loadFixedTrajectory(map);
    }
    else if (type == "Keplerian")
    {
        return loadKeplerianTrajectory(map);
    }
    else
    {
        qDebug() << "Unknown trajectory type " << type;
    }

    return NULL;
}


vesta::RotationModel* loadFixedRotationModel(const QVariantMap& map)
{
    qDebug() << "RotationModel: " << map.value("type").toString();
    return NULL;
}


vesta::RotationModel* loadUniformRotationModel(const QVariantMap& map)
{
    qDebug() << "RotationModel: " << map.value("type").toString();
    return NULL;
}


vesta::RotationModel* loadRotationModel(const QVariantMap& map)
{
    QVariant typeVar = map.value("type");
    if (typeVar.type() != QVariant::String)
    {
        qDebug() << "RotationModel definition is missing type.";
    }

    QString type = typeVar.toString();
    if (type == "Fixed")
    {
        return loadFixedRotationModel(map);
    }
    else if (type == "Uniform")
    {
        return loadUniformRotationModel(map);
    }
    else
    {
        qDebug() << "Unknown rotation model type " << type;
    }

    return NULL;
}


vesta::InertialFrame* loadInertialFrame(const QString& name)
{
    qDebug() << "Inertial Frame: " << name;
    return NULL;
}


vesta::Arc* loadArc(const QVariantMap& map)
{
    vesta::Arc* arc = new vesta::Arc();

    QVariant centerData = map.value("center");
    QVariant trajectoryData = map.value("trajectory");
    QVariant rotationModelData = map.value("rotationModel");
    QVariant trajectoryFrameData = map.value("trajectoryFrame");
    QVariant bodyFrameData = map.value("bodyFrame");

    if (centerData.type() == QVariant::String)
    {
    }

    if (trajectoryData.type() == QVariant::Map)
    {
        Trajectory* trajectory = loadTrajectory(trajectoryData.toMap());
        if (trajectory)
        {
            arc->setTrajectory(trajectory);
        }
    }

    if (rotationModelData.type() == QVariant::Map)
    {
        RotationModel* rotationModel = loadRotationModel(rotationModelData.toMap());
        if (rotationModel)
        {
            arc->setRotationModel(rotationModel);
        }
    }

    if (trajectoryFrameData.type() == QVariant::String)
    {
        // Inertial frame name
        InertialFrame* frame = loadInertialFrame(trajectoryFrameData.toString());
        if (frame)
        {
            arc->setTrajectoryFrame(frame);
        }
    }
    else if (trajectoryFrameData.type() == QVariant::Map)
    {
    }

    if (bodyFrameData.type() == QVariant::String)
    {
        // Inertial frame name
        InertialFrame* frame = loadInertialFrame(bodyFrameData.toString());
        if (frame)
        {
            arc->setTrajectoryFrame(frame);
        }
    }
    else if (bodyFrameData.type() == QVariant::Map)
    {
    }

    return arc;
}


void
Cosmographia::loadSolarSystem()
{
    QString defaultFileName = QDesktopServices::storageLocation(QDesktopServices::DocumentsLocation) + "/cosmo.json";
    QString solarSystemFileName = QFileDialog::getOpenFileName(this, "Load Solar System...", defaultFileName, "Solar System Files (*.json)");
    if (!solarSystemFileName.isEmpty())
    {
        QJson::Parser parser;

        QFile solarSystemFile(solarSystemFileName);
        if (!solarSystemFile.open(QIODevice::ReadOnly))
        {
            QMessageBox::warning(this, tr("Solar System File Error"), tr("Could not open file."));
            return;
        }

        bool parseOk = false;
        QVariant result = parser.parse(&solarSystemFile, &parseOk);
        if (!parseOk)
        {
            QMessageBox::warning(this,
                                 tr("Solar System File Error"),
                                 QString("Line %1: %2").arg(parser.errorLine()).arg(parser.errorString()));
            return;
        }

        QVariantMap contents = result.toMap();
        if (contents.empty())
        {
            qDebug() << "Solar system file is empty.";
            return;
        }

        qDebug() << contents["name"];

        if (!contents.contains("bodies"))
        {
            qDebug() << "No bodies defined.";
            return;
        }

        if (contents["bodies"].type() != QVariant::List)
        {
            qDebug() << "Bodies is not a list.";
            return;
        }
        QVariantList bodies = contents["bodies"].toList();

        foreach (QVariant body, bodies)
        {
            if (body.type() != QVariant::Map)
            {
                qDebug() << "Invalid item in bodies list.";
            }
            else
            {
                QVariantMap bodyInfo = body.toMap();
                vesta::Body* body = new vesta::Body();

                vesta::Arc* arc = loadArc(bodyInfo);
                if (!arc)
                {
                    delete body;
                }
                else
                {
                    body->chronology()->addArc(arc);
                }
                qDebug() << "Body: " << bodyInfo["name"].toString();
            }
        }
    }
}
