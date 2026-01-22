#include "MainWindow.h"

#include <QAction>
#include <QFileDialog>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QLabel>
#include <QSlider>
#include <QToolBar>
#include <QDockWidget>
#include <QGridLayout>
#include <QApplication>
#include <QStyle>
#include <QIcon>
#include <QCoreApplication>
#include <QSize>
#include <QImage>

#include <QVTKOpenGLNativeWidget.h>

#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkImageViewer2.h>
#include <vtkDICOMImageReader.h>
#include <vtkImageData.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkTrivialProducer.h>
#include <vtkPointData.h>
#include <vtkDataArray.h>
#include <vtkImageAlgorithm.h>
#include <vtkErrorCode.h>
#include <vtkType.h>
#include <vtkObject.h>
#include <vtkMedicalImageProperties.h>

#ifdef HAS_VTK_IOGDCM
#include <vtkGDCMImageReader.h>
#endif
#ifdef HAS_GDCM_FALLBACK
#include <gdcmImageReader.h>
#include <gdcmImage.h>
#include <gdcmPixelFormat.h>
#include <gdcmDataElement.h>
#include <gdcmAttribute.h>
#include <gdcmOrientation.h>
#endif

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setupUi();
}

void MainWindow::setupUi()
{
    setWindowTitle("DICOM Viewer");
    
    QString iconPath = QCoreApplication::applicationDirPath() + "/assets/logo_new.png";
    QIcon icon;

    auto buildIconFrom = [&](const QString& path) {
        QImage img(path);
        if (img.isNull())
            return false;

        int left = img.width(), top = img.height(), right = 0, bottom = 0;
        bool hasAlpha = img.hasAlphaChannel();
        if (hasAlpha) {
            for (int y = 0; y < img.height(); ++y) {
                const QRgb* line = reinterpret_cast<const QRgb*>(img.constScanLine(y));
                for (int x = 0; x < img.width(); ++x) {
                    if (qAlpha(line[x]) > 0) {
                        left = std::min(left, x);
                        right = std::max(right, x);
                        top = std::min(top, y);
                        bottom = std::max(bottom, y);
                    }
                }
            }
            if (right > left && bottom > top) {
                img = img.copy(QRect(QPoint(left, top), QPoint(right, bottom)));
            }
        }

        auto addSize = [&](int s) {
            icon.addPixmap(QPixmap::fromImage(img.scaled(s, s, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
        };
        addSize(256); addSize(128); addSize(64); addSize(48); addSize(32); addSize(24); addSize(16);
        return true;
    };

    if (!buildIconFrom(iconPath)) {
        buildIconFrom(QCoreApplication::applicationDirPath() + "/assets/logo_tmp.png");
    }
    if (icon.isNull()) {
        buildIconFrom(QCoreApplication::applicationDirPath() + "/assets/logo.png");
    }
    if (icon.isNull()) {
        buildIconFrom(QCoreApplication::applicationDirPath() + "/../../assets/logo_new.png");
    }
    if (icon.isNull()) {
        buildIconFrom(QCoreApplication::applicationDirPath() + "/../../assets/logo_tmp.png");
    }
    if (icon.isNull()) {
        buildIconFrom(QCoreApplication::applicationDirPath() + "/../../assets/logo.png");
    }

    if (icon.isNull()) {
        icon = QApplication::style()->standardIcon(QStyle::SP_FileDialogInfoView);
    }
    setWindowIcon(icon);

    const QString theme = R"(
        QMainWindow {
            background-color: #0b0f12;
            color: #e0e6ed;
        }
        QWidget {
            color: #e0e6ed;
            background-color: #0b0f12;
            font-family: "Segoe UI", "Inter", sans-serif;
        }
        QToolBar {
            background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #0f1720, stop:1 #111927);
            spacing: 6px;
            padding: 6px 8px;
            border: 1px solid #1f2a38;
        }
        QToolBar QToolButton {
            color: #e0e6ed;
            background: #111927;
            border: 1px solid #1f2a38;
            border-radius: 8px;
            padding: 6px 12px;
        }
        QToolBar QToolButton:hover {
            border-color: #00c8d8;
            color: #00e2f5;
        }
        QLabel {
            color: #cfd8e3;
            font-weight: 500;
        }
        QSlider::groove:horizontal {
            height: 6px;
            background: #1a2430;
            border-radius: 3px;
        }
        QSlider::sub-page:horizontal {
            background: #00c8d8;
            border-radius: 3px;
        }
        QSlider::handle:horizontal {
            background: #00c8d8;
            width: 12px;
            margin: -4px 0;
            border-radius: 6px;
            border: 1px solid #14d9e8;
        }
        QSlider::add-page:horizontal {
            background: #111927;
            border-radius: 3px;
        }
        QDockWidget {
            background: #0c131a;
            border: 1px solid #1f2a38;
            titlebar-close-icon: none;
            titlebar-normal-icon: none;
        }
        QDockWidget::title {
            background: #0f1720;
            color: #e0e6ed;
            padding: 8px 10px;
            font-weight: 600;
            border-bottom: 1px solid #1f2a38;
        }
        QDockWidget QWidget {
            background: #0c131a;
        }
        QDockWidget QLabel {
            color: #cfd8e3;
        }
    )";
    qApp->setStyleSheet(theme);

    // Toolbar
    auto* toolbar = addToolBar("Main");
    toolbar->setMovable(false);
    toolbar->setIconSize(QSize(20, 20));

    auto* openAction = new QAction("Abrir DICOM...", this);
    connect(openAction, &QAction::triggered, this, &MainWindow::onOpenDicom);
    toolbar->addAction(openAction);

    // Central widget + layout
    auto* central = new QWidget(this);
    auto* mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);

    // VTK widget
    m_vtkWidget = new QVTKOpenGLNativeWidget(central);
    m_vtkWidget->setMinimumSize(640, 480);
    m_vtkWidget->setStyleSheet("border: 1px solid #1f2a38; border-radius: 10px; background-color: #05080c;");

    // Render window + viewer
    m_renderWindow = vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New();
    m_vtkWidget->setRenderWindow(m_renderWindow);

    m_viewer = vtkSmartPointer<vtkImageViewer2>::New();
    m_viewer->SetRenderWindow(m_renderWindow);
    m_viewer->SetupInteractor(m_vtkWidget->interactor());

    // Input placeholder para evitar pipeline sem conexão até abrir um DICOM
    auto dummy = vtkSmartPointer<vtkImageData>::New();
    dummy->SetDimensions(1, 1, 1);
    dummy->AllocateScalars(VTK_UNSIGNED_CHAR, 1);
    dummy->GetPointData()->GetScalars()->Fill(0);

    m_dummyProducer = vtkSmartPointer<vtkTrivialProducer>::New();
    m_dummyProducer->SetOutput(dummy);
    m_viewer->SetInputConnection(m_dummyProducer->GetOutputPort());

    // (Opcional) controles WW/WL
    auto* controlsRow = new QWidget(central);
    auto* controlsLayout = new QHBoxLayout(controlsRow);
    controlsLayout->setContentsMargins(0, 0, 0, 0);
    controlsLayout->setSpacing(10);
    controlsRow->setStyleSheet("background: #0f1720; border: 1px solid #1f2a38; border-radius: 10px; padding: 8px;");

    auto* windowLabel = new QLabel("Window (WW):", controlsRow);
    m_windowSlider = new QSlider(Qt::Horizontal, controlsRow);
    m_windowSlider->setRange(1, 4000);
    m_windowSlider->setValue(static_cast<int>(m_window));
    connect(m_windowSlider, &QSlider::valueChanged, this, &MainWindow::onWindowChanged);

    auto* levelLabel = new QLabel("Level (WL):", controlsRow);
    m_levelSlider = new QSlider(Qt::Horizontal, controlsRow);
    m_levelSlider->setRange(-2000, 2000);
    m_levelSlider->setValue(static_cast<int>(m_level));
    connect(m_levelSlider, &QSlider::valueChanged, this, &MainWindow::onLevelChanged);

    controlsLayout->addWidget(windowLabel);
    controlsLayout->addWidget(m_windowSlider, 1);
    controlsLayout->addSpacing(10);
    controlsLayout->addWidget(levelLabel);
    controlsLayout->addWidget(m_levelSlider, 1);

    // Status label
    m_statusLabel = new QLabel("Abra um arquivo .dcm para visualizar.", central);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setStyleSheet("color: #9fb3c8;");

    mainLayout->addWidget(m_vtkWidget, 1);
    mainLayout->addWidget(controlsRow);
    mainLayout->addWidget(m_statusLabel);

    setCentralWidget(central);
    buildMetadataDock();

    // Evita renderizar sem entrada; só renderizamos após carregar DICOM
}

static vtkImageData* currentImage(vtkImageAlgorithm* reader, vtkImageData* manual)
{
    if (manual)
        return manual;
    if (reader)
        return vtkImageData::SafeDownCast(reader->GetOutputDataObject(0));
    return nullptr;
}

void MainWindow::buildMetadataDock()
{
    m_metaDock = new QDockWidget(this);
    // Custom title bar
    auto* titleWidget = new QWidget(m_metaDock);
    auto* titleLayout = new QHBoxLayout(titleWidget);
    titleLayout->setContentsMargins(10, 6, 10, 6);
    titleLayout->setSpacing(6);
    auto* titleLabel = new QLabel("Metadados", titleWidget);
    titleLabel->setStyleSheet("font-weight: 700; color: #e0e6ed;");
    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch();
    titleWidget->setLayout(titleLayout);
    m_metaDock->setTitleBarWidget(titleWidget);

    m_metaDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_metaDock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    auto* container = new QWidget(m_metaDock);
    auto* grid = new QGridLayout(container);
    grid->setContentsMargins(14, 12, 14, 12);
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(8);

    QStringList keys = {
        "Arquivo", "Dimensoes", "Spacing", "Componentes",
        "Tipo", "Window/Level", "Modality", "Paciente",
        "Estudo", "Serie"
    };

    int row = 0;
    for (const auto& k : keys) {
        auto* lblKey = new QLabel(k + ":", container);
        auto* lblVal = new QLabel("--", container);
        lblKey->setStyleSheet("font-weight: 600; color: #b9c4d1;");
        lblVal->setStyleSheet("color: #9fb3c8;");
        grid->addWidget(lblKey, row, 0, Qt::AlignLeft | Qt::AlignTop);
        grid->addWidget(lblVal, row, 1, Qt::AlignLeft | Qt::AlignTop);
        m_metaLabels.insert(k, lblVal);
        ++row;
    }

    container->setLayout(grid);
    m_metaDock->setWidget(container);
    addDockWidget(Qt::RightDockWidgetArea, m_metaDock);
}

void MainWindow::setMeta(const QString& key, const QString& value)
{
    if (auto* lbl = m_metaLabels.value(key, nullptr)) {
        lbl->setText(value);
    }
}

void MainWindow::showError(const QString& message)
{
    QMessageBox::critical(this, "Erro", message);
    m_statusLabel->setText("Erro: " + message);
}

void MainWindow::onOpenDicom()
{
    const QString filePath = QFileDialog::getOpenFileName(
        this,
        "Selecionar arquivo DICOM",
        QString(),
        "DICOM (*.dcm);;Todos os arquivos (*.*)"
    );

    if (filePath.isEmpty())
        return;

    if (!loadDicomFile(filePath))
        return;

    m_statusLabel->setText("Carregado: " + filePath);
}

bool MainWindow::loadDicomFile(const QString& filePath)
{
    // reset estado anterior
    m_reader = nullptr;
    m_manualImage = nullptr;
    m_currentFile.clear();

    auto tryRead = [](vtkImageAlgorithm* reader) -> vtkImageData* {
        const int prev = vtkObject::GetGlobalWarningDisplay();
        vtkObject::GlobalWarningDisplayOff(); // evita popup/erro no output
        reader->Update();
        vtkObject::SetGlobalWarningDisplay(prev);
        if (reader->GetErrorCode() != vtkErrorCode::NoError)
            return nullptr;
        return vtkImageData::SafeDownCast(reader->GetOutputDataObject(0));
    };

    auto hasValidDims = [](vtkImageData* img) -> bool {
        if (!img) return false;
        int dims[3] = {0, 0, 0};
        img->GetDimensions(dims);
        return dims[0] > 1 && dims[1] > 1;
    };

    vtkImageData* img = nullptr;

    // Primeira tentativa: vtkDICOMImageReader
    auto dicomReader = vtkSmartPointer<vtkDICOMImageReader>::New();
    dicomReader->SetFileName(filePath.toStdString().c_str());
    img = tryRead(dicomReader);
    if (hasValidDims(img)) {
        m_reader = dicomReader;
    }

#ifdef HAS_VTK_IOGDCM
    
    if (!hasValidDims(img)) {
        auto gdcmReader = vtkSmartPointer<vtkGDCMImageReader>::New();
        gdcmReader->SetFileName(filePath.toStdString().c_str());
        auto gdcmImg = tryRead(gdcmReader);
        if (hasValidDims(gdcmImg)) {
            m_reader = gdcmReader;
            img = gdcmImg;
        }
    }
#endif

    // Fallback: GDCM manual (para builds sem VTK::IOGDCM)
#ifdef HAS_GDCM_FALLBACK
    if (!hasValidDims(img)) {
        gdcm::ImageReader reader;
        reader.SetFileName(filePath.toStdString().c_str());
        if (reader.Read()) {
            const gdcm::Image& gimg = reader.GetImage();
            const gdcm::PixelFormat& pf = gimg.GetPixelFormat();
            int comps = pf.GetSamplesPerPixel();
            gdcm::PixelFormat::ScalarType st = pf.GetScalarType();
            int vtkType = VTK_VOID;
            switch (st) {
                case gdcm::PixelFormat::UINT8: vtkType = VTK_UNSIGNED_CHAR; break;
                case gdcm::PixelFormat::INT8: vtkType = VTK_SIGNED_CHAR; break;
                case gdcm::PixelFormat::UINT16: vtkType = VTK_UNSIGNED_SHORT; break;
                case gdcm::PixelFormat::INT16: vtkType = VTK_SHORT; break;
                case gdcm::PixelFormat::UINT32: vtkType = VTK_UNSIGNED_INT; break;
                case gdcm::PixelFormat::INT32: vtkType = VTK_INT; break;
                case gdcm::PixelFormat::FLOAT32: vtkType = VTK_FLOAT; break;
                case gdcm::PixelFormat::FLOAT64: vtkType = VTK_DOUBLE; break;
                default: vtkType = VTK_VOID; break;
            }
            if (vtkType != VTK_VOID && comps > 0) {
                const size_t bufLen = gimg.GetBufferLength();
                std::vector<unsigned char> buffer(bufLen);
                if (gimg.GetBuffer(reinterpret_cast<char*>(buffer.data()))) {
                    auto vtkImg = vtkSmartPointer<vtkImageData>::New();
                    const unsigned int* dims = gimg.GetDimensions(); 
                    vtkImg->SetDimensions(dims[0], dims[1], std::max(1u, dims[2]));
                    vtkImg->AllocateScalars(vtkType, comps);
                    const size_t vtkBytes = vtkImg->GetScalarSize() * static_cast<size_t>(dims[0]) * static_cast<size_t>(dims[1]) * static_cast<size_t>(std::max(1u, dims[2])) * comps;
                    std::memcpy(vtkImg->GetScalarPointer(), buffer.data(), std::min(bufLen, vtkBytes));
                    double spacing[3] = {1.0, 1.0, 1.0};
                    spacing[0] = gimg.GetSpacing(0);
                    spacing[1] = gimg.GetSpacing(1);
                    spacing[2] = (gimg.GetNumberOfDimensions() > 2) ? gimg.GetSpacing(2) : 1.0;
                    vtkImg->SetSpacing(spacing);
                    img = vtkImg;
                    m_reader = nullptr;
                    m_manualImage = vtkImg;
                }
            }
        }
    }
#endif

    // Se nenhum reader/manual deu certo, erro
    if (!hasValidDims(img)) {
        showError("Falha ao ler o DICOM (pode estar corrompido ou comprimido de forma nao suportada).");
        return false;
    }

    if (m_reader) {
        m_viewer->SetInputConnection(m_reader->GetOutputPort());
    } else {
        m_viewer->SetInputData(img);
    }

    // Ajuste de câmera e render
    m_viewer->GetRenderer()->ResetCamera();

    // Auto window/level simples: usa range do scalar
    double range[2] = {0.0, 0.0};
    img->GetScalarRange(range);

    if (range[1] > range[0]) {
        m_window = (range[1] - range[0]);
        m_level  = (range[1] + range[0]) / 2.0;

        m_windowSlider->blockSignals(true);
        m_levelSlider->blockSignals(true);

        m_windowSlider->setValue(static_cast<int>(std::max(1.0, std::min(4000.0, m_window))));
        m_levelSlider->setValue(static_cast<int>(std::max(-2000.0, std::min(2000.0, m_level))));

        m_windowSlider->blockSignals(false);
        m_levelSlider->blockSignals(false);
    }

    applyWindowLevel();
    m_currentFile = filePath;
    updateStatusFromReader();

    m_renderWindow->Render();
    return true;
}

static QString readTagValue(const QString& path, uint16_t group, uint16_t element)
{
#ifdef HAS_GDCM_FALLBACK
    gdcm::Reader reader;
    reader.SetFileName(path.toStdString().c_str());
    if (!reader.Read()) return "--";
    const gdcm::DataSet& ds = reader.GetFile().GetDataSet();
    gdcm::Tag tag(group, element);
    if (!ds.FindDataElement(tag)) return "--";
    const gdcm::DataElement& de = ds.GetDataElement(tag);
    const gdcm::ByteValue* bv = de.GetByteValue();
    if (!bv) return "--";
    std::string s(bv->GetPointer(), bv->GetLength());
    while (!s.empty() && (s.back() == '\0' || s.back() == ' ')) s.pop_back();
    return s.empty() ? "--" : QString::fromStdString(s);
#else
    Q_UNUSED(path);
    Q_UNUSED(group);
    Q_UNUSED(element);
    return "--";
#endif
}

void MainWindow::updateStatusFromReader()
{
    vtkImageData* img = currentImage(m_reader, m_manualImage);
    if (!img)
        return;

    int dims[3]; img->GetDimensions(dims);
    double spacing[3]; img->GetSpacing(spacing);
    const int comps = img->GetNumberOfScalarComponents();
    const int scalarType = img->GetScalarType();

    auto scalarName = [&]() -> QString {
        switch (scalarType) {
            case VTK_UNSIGNED_CHAR: return "uint8";
            case VTK_CHAR: return "int8";
            case VTK_UNSIGNED_SHORT: return "uint16";
            case VTK_SHORT: return "int16";
            case VTK_UNSIGNED_INT: return "uint32";
            case VTK_INT: return "int32";
            case VTK_FLOAT: return "float";
            case VTK_DOUBLE: return "double";
            default: return QString::number(scalarType);
        }
    }();

    QString info = QString("Imagem: %1 x %2 | comps: %3 | tipo: %4 | spacing: [%5, %6]")
        .arg(dims[0]).arg(dims[1])
        .arg(comps)
        .arg(scalarName)
        .arg(spacing[0], 0, 'f', 3)
        .arg(spacing[1], 0, 'f', 3);

    m_statusLabel->setText(info);

    // Metadados
    setMeta("Arquivo", m_currentFile.isEmpty() ? "--" : m_currentFile);
    setMeta("Dimensoes", QString("%1 x %2 x %3").arg(dims[0]).arg(dims[1]).arg(dims[2]));
    setMeta("Spacing", QString("%1, %2, %3").arg(spacing[0],0,'f',3).arg(spacing[1],0,'f',3).arg(spacing[2],0,'f',3));
    setMeta("Componentes", QString::number(comps));
    setMeta("Tipo", scalarName);
    setMeta("Window/Level", QString("%1 / %2").arg(m_window,0,'f',0).arg(m_level,0,'f',0));

    QString modality = "--", patient = "--", study = "--", series = "--";
#ifdef HAS_GDCM_FALLBACK
    if (!m_currentFile.isEmpty()) {
        modality = readTagValue(m_currentFile, 0x0008, 0x0060); // Modality
        patient  = readTagValue(m_currentFile, 0x0010, 0x0010); // PatientName
        study    = readTagValue(m_currentFile, 0x0020, 0x0010); // StudyID
        series   = readTagValue(m_currentFile, 0x0008, 0x103E); // SeriesDescription
    }
#endif
    setMeta("Modality", modality);
    setMeta("Paciente", patient);
    setMeta("Estudo", study);
    setMeta("Serie", series);
}

void MainWindow::applyWindowLevel()
{
    vtkImageData* img = currentImage(m_reader, m_manualImage);
    if (!m_viewer || !img)
        return;

    m_viewer->SetColorWindow(m_window);
    m_viewer->SetColorLevel(m_level);
    if (m_renderWindow) m_renderWindow->Render();
}

void MainWindow::onWindowChanged(int value)
{
    m_window = static_cast<double>(value);
    applyWindowLevel();
}

void MainWindow::onLevelChanged(int value)
{
    m_level = static_cast<double>(value);
    applyWindowLevel();
}
