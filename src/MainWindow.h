#pragma once

#include <QMainWindow>
#include <QString>
#include <QMap>

class QLabel;
class QSlider;
class QDockWidget;

#include <vtkSmartPointer.h>

class QVTKOpenGLNativeWidget;
class vtkGenericOpenGLRenderWindow;
class vtkImageViewer2;
class vtkImageAlgorithm;
class vtkDICOMImageReader;
class vtkTrivialProducer;
class vtkImageData;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override = default;

private slots:
    void onOpenDicom();
    void onWindowChanged(int value);
    void onLevelChanged(int value);

private:
    void setupUi();
    void showError(const QString& message);
    bool loadDicomFile(const QString& filePath);
    void updateStatusFromReader();
    void applyWindowLevel();
    void buildMetadataDock();
    void setMeta(const QString& key, const QString& value);

private:
    QVTKOpenGLNativeWidget* m_vtkWidget = nullptr;
    QLabel* m_statusLabel = nullptr;
    QDockWidget* m_metaDock = nullptr;
    QMap<QString, QLabel*> m_metaLabels;

    QSlider* m_windowSlider = nullptr;
    QSlider* m_levelSlider = nullptr;

    // VTK pipeline
    vtkSmartPointer<vtkGenericOpenGLRenderWindow> m_renderWindow;
    vtkSmartPointer<vtkImageViewer2> m_viewer;
    vtkSmartPointer<vtkImageAlgorithm> m_reader;
    vtkSmartPointer<vtkTrivialProducer> m_dummyProducer;
    vtkSmartPointer<vtkImageData> m_manualImage; 

    double m_window = 400.0;
    double m_level  = 40.0;
    QString m_currentFile;
};
