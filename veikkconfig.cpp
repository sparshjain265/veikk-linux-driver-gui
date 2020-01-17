#include "veikkconfig.h"
#include "ui_main.h"
#include "qpressurecurvescene.h"
#include "qscreenmapscene.h"
#include <QScreen>
#include <QGuiApplication>
#include <QFile>

// set up widgets, hook up handlers
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow{parent}, ui{new Ui::MainWindow} {
    qint8 i;

    // basic ui setup (auto-generated by qt designer)
    ui->setupUi(this);

    screen = QGuiApplication::screens().first();
    connect(screen, &QScreen::geometryChanged,
            this, &MainWindow::screenSizeChanged);

    tabWidget = findChild<QTabWidget *>("tab_widget");
    connect(tabWidget, &QTabWidget::currentChanged,
            this, &MainWindow::tabChanged);

    pressureCurveView = findChild<QGraphicsView *>("pressure_curve_view");
    pressureCurveView->setScene(new QPressureCurveScene{});
    pressureCurveView->scale(1, -1);
    connect(static_cast<QPressureCurveScene *>(pressureCurveView->scene()),
            &QPressureCurveScene::updatePressureForm,
            this, &MainWindow::updatePressureForm);
    connect(this, &MainWindow::updatePressureCurve,
            static_cast<QPressureCurveScene *>(pressureCurveView->scene()),
            &QPressureCurveScene::updatePressureCurve);

    screenMapView = findChild<QGraphicsView *>("screen_map_view");
    screenMapView->setScene(new QScreenMapScene{screen});
    connect(static_cast<QScreenMapScene *>(screenMapView->scene()),
            &QScreenMapScene::updateScreenMapForm,
            this, &MainWindow::updateScreenMapForm);
    connect(this, &MainWindow::updateScreenMapRect,
            static_cast<QScreenMapScene *>(screenMapView->scene()),
            &QScreenMapScene::updateScreenMapRect);

    for(i=0; i<4; i++) {
        pressureCoefSpinboxes[i] = findChild<QDoubleSpinBox *>
                                    ("pressure_coef_a" + QString::number(i));
        connect(pressureCoefSpinboxes[i],
                QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, &MainWindow::updatePressureCoefs);
    }

    screenMapSpinboxes[0] = screenMapXSpinBox = findChild<QSpinBox *>
                                                    ("screen_map_x");
    screenMapSpinboxes[1] = screenMapYSpinBox = findChild<QSpinBox *>
                                                    ("screen_map_y");
    screenMapSpinboxes[2] = screenMapWidthSpinBox = findChild<QSpinBox *>
                                                    ("screen_map_width");
    screenMapSpinboxes[3] = screenMapHeightSpinBox = findChild<QSpinBox *>
                                                    ("screen_map_height");
    screenWidthLineEdit = findChild<QLineEdit *>("screen_width");
    screenHeightLineEdit = findChild<QLineEdit *>("screen_height");
    screenDefaultMap = findChild<QCheckBox *>("screen_default_map");
    connect(screenDefaultMap, &QCheckBox::stateChanged,
            this, &MainWindow::setDefaultScreenMap);
    for(i=0; i<4; i++)
        connect(screenMapSpinboxes[i],
                QOverload<int>::of(&QSpinBox::valueChanged),
                this, &MainWindow::updateScreenMapParms);
    screenSizeChanged(screen->geometry());
    updateScreenMapForm(screen->geometry());

    screenOrientation = findChild<QComboBox *>("screen_orientation");
    screenOrientation->addItem("Default");
    screenOrientation->addItem("90deg CCW");
    screenOrientation->addItem("Flipped");
    screenOrientation->addItem("90deg CW");
    connect(screenOrientation,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            std::bind(&VeikkParms::setOrientation, &currentParms,
                      std::placeholders::_1));

    connect(findChild<QAction *>("action_apply_all"), &QAction::triggered,
            std::bind(&VeikkParms::applyConfig, &currentParms,
                      &restoreParms, VEIKK_MP_ALL));
    connect(findChild<QPushButton *>("apply_screen_changes"),
            &QPushButton::clicked,
            std::bind(&VeikkParms::applyConfig, &currentParms,
                      &restoreParms, VEIKK_MP_SCREEN));
    connect(findChild<QPushButton *>("apply_pressure_changes"),
            &QPushButton::clicked,
            std::bind(&VeikkParms::applyConfig, &currentParms,
                      &restoreParms, VEIKK_MP_PRESSURE_MAP));

    connect(findChild<QPushButton *>("reset_screen_changes"),
            &QPushButton::clicked, this, &MainWindow::resetScreenChanges);
    connect(findChild<QPushButton *>("reset_pressure_changes"),
            &QPushButton::clicked, this, &MainWindow::resetPressureChanges);

    pressureMapDefaults = findChild<QComboBox *>("pressure_map_defaults");
    pressureMapDefaults->addItem("Linear (default)",
                         VeikkParms::serializePressureMap(0, 100, 0, 0));
    pressureMapDefaults->addItem("Quadratic 1",
                         VeikkParms::serializePressureMap(0, 0, 100, 0));
    pressureMapDefaults->addItem("Quadratic 2",
                         VeikkParms::serializePressureMap(0, 200, -100, 0));
    pressureMapDefaults->addItem("Cubic 1",
                         VeikkParms::serializePressureMap(0, 0, 0, 100));
    pressureMapDefaults->addItem("Cubic 2",
                         VeikkParms::serializePressureMap(0, 100, 100, -100));
    pressureMapDefaults->addItem("Linear soft touch",
                         VeikkParms::serializePressureMap(0, 133, 0, 0));
    pressureMapDefaults->addItem("Constant full pressure",
                         VeikkParms::serializePressureMap(100, 0, 0, 0));
    connect(pressureMapDefaults, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::setComboBoxDefaultPressure);
}

MainWindow::~MainWindow() {
    delete ui;
}

// resize views on startup, see: https://stackoverflow.com/questions/9858971
void MainWindow::showEvent(QShowEvent *evt) {
    pressureCurveView->fitInView(pressureCurveView->sceneRect());
    screenMapView->fitInView(screenMapView->sceneRect(), Qt::KeepAspectRatio);
    QWidget::showEvent(evt);
}

// on window resizing
void MainWindow::resizeEvent(QResizeEvent *evt) {
    screenMapView->fitInView(screenMapView->sceneRect(), Qt::KeepAspectRatio);
    QWidget::resizeEvent(evt);
}

// get pressure coefficients from form
void MainWindow::getPressureCoefs(qint16 *coefs) {
    qint8 i;
    for(i=0; i<4; i++)
        coefs[i] = qint16(pressureCoefSpinboxes[i]->value()*100);
}

// get screen map parms from form
QRect MainWindow::getScreenMapParms() {
    return QRect{
        screenMapXSpinBox->value(),
        screenMapYSpinBox->value(),
        screenMapWidthSpinBox->value(),
        screenMapHeightSpinBox->value()
    };
}

// on screen size changed; update screen size form, sync with currentParms,
// adjust view matrix
void MainWindow::screenSizeChanged(QRect newScreenSize) {
    screenWidthLineEdit->setText(QString::number(newScreenSize.width()));
    screenHeightLineEdit->setText(QString::number(newScreenSize.height()));
    currentParms.setScreenSize(screen->geometry());

    // fitInView() only works if sceneRect is within scene's sceneRect,
    // so resize scene's sceneRect first
    screenMapView->scene()->setSceneRect(screen->geometry());
    screenMapView->fitInView(screenMapView->scene()->sceneRect());
}

// handler for changing tab; adjust view matrices to sceneRect
void MainWindow::tabChanged(int curTab) {
    switch(curTab) {
    case 0: // screen map
        screenMapView->fitInView(screenMapView->sceneRect(),
                                 Qt::KeepAspectRatio);
        break;
    case 1: // pressure map
        pressureCurveView->fitInView(pressureCurveView->sceneRect());
        break;
    }
}

// update pressure form with given values, sync with currentParms
void MainWindow::updatePressureForm(qint16 *newCoefs) {
    qint8 i;
    qint16 coefs[4];
    for(i=0; i<4; i++) {
        pressureCoefSpinboxes[i]->blockSignals(true);
        pressureCoefSpinboxes[i]->setValue(newCoefs[i]/100.0);
        pressureCoefSpinboxes[i]->blockSignals(false);
    }
    getPressureCoefs(coefs);
    currentParms.setPressureMap(coefs);
}

// callback for changing pressure coefs in form; sync pressure visual and
// currentParms with new form values
void MainWindow::updatePressureCoefs() {
    qint16 newCoefs[4];
    getPressureCoefs(newCoefs);
    currentParms.setPressureMap(newCoefs);
    emit updatePressureCurve(newCoefs);
}

// update screen map form with given values, sync with currentParms
void MainWindow::updateScreenMapForm(QRect newScreenMap) {
    qint8 i;
    for(i=0; i<4; i++)
        screenMapSpinboxes[i]->blockSignals(true);
    screenMapXSpinBox->setValue(newScreenMap.x());
    screenMapYSpinBox->setValue(newScreenMap.y());
    screenMapWidthSpinBox->setValue(newScreenMap.width());
    screenMapHeightSpinBox->setValue(newScreenMap.height());
    for(i=0; i<4; i++)
        screenMapSpinboxes[i]->blockSignals(false);
    screenDefaultMap->setCheckState(newScreenMap==screen->geometry()
                                    ? Qt::Checked
                                    : Qt::Unchecked);
    currentParms.setScreenMap(getScreenMapParms());
}

// callback for changing screen map parms; sync screen map visual and
// currentParms with new screen map form values
void MainWindow::updateScreenMapParms() {
    QRect newScreenMap = getScreenMapParms();
    screenDefaultMap->setCheckState(newScreenMap==screen->geometry()
                                    ? Qt::Checked
                                    : Qt::Unchecked);
    currentParms.setScreenMap(newScreenMap);
    emit updateScreenMapRect(newScreenMap);
}

// callback for checking the "default screen map" checkbox; sets screen map
// to full screen and updates form and visual (and syncs currentParms)
void MainWindow::setDefaultScreenMap(int checkState) {
    if(checkState == Qt::Unchecked)
        return;
    updateScreenMapForm(screen->geometry());
    emit updateScreenMapRect(screen->geometry());
}

// callback for selecting a new pressure mapping default from the combobox
void MainWindow::setComboBoxDefaultPressure() {
    qint16 pressureCoefs[4];
    currentParms.setPressureMap(pressureMapDefaults->currentData()
                                    .toULongLong());
    currentParms.getPressureMap(pressureCoefs);
    updatePressureForm(pressureCoefs);
    emit updatePressureCurve(pressureCoefs);
}

// callback to reset screen changes; doesn't make sense to "restore
// screen size" (screen size is fixed at current geometry), so that isn't
// ever changed
void MainWindow::resetScreenChanges() {
    QRect screenMap;
    quint32 orientation;

    currentParms.restoreConfig(&restoreParms,
                               VEIKK_MP_SCREEN_MAP|VEIKK_MP_ORIENTATION);
    screenMap = currentParms.getScreenMap();
    orientation = currentParms.getOrientation();

    screenOrientation->setCurrentIndex(qint32(orientation));

    // if invalid screen map (e.g., default has value of 0)
    if(currentParms.isInvalidScreenMap()) {
        setDefaultScreenMap(Qt::Checked);
    } else {
        updateScreenMapForm(screenMap);
        emit updateScreenMapRect(screenMap);
    }
}

// callback to reset pressure changes
void MainWindow::resetPressureChanges() {
    qint16 pressureCoefs[4];
    currentParms.restoreConfig(&restoreParms, VEIKK_MP_PRESSURE_MAP);
    currentParms.getPressureMap(pressureCoefs);
    updatePressureForm(pressureCoefs);
    emit updatePressureCurve(pressureCoefs);
}
