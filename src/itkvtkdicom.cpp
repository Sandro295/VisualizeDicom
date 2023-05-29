// https://kitware.github.io/vtk-examples/site/Cxx/Widgets/ImplicitPlaneWidget2/
#include <itkImage.h>
#include <itkImageFileReader.h>
#include <itkBinaryThresholdImageFilter.h>
#include <itkImageToVTKImageFilter.h>
#include <itkGDCMImageIO.h>
#include <itkGDCMSeriesFileNames.h>
#include <itkImageSeriesReader.h>

#include <vtkSmartPointer.h>
#include <vtkImageData.h>
#include <vtkMarchingCubes.h>
#include <vtkPolyDataMapper.h>
#include <vtkActor.h>
#include <vtkCamera.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>

// #include <vtkImplicitPlaneWidget.h>
#include <vtkImplicitPlaneRepresentation.h>
#include <vtkImplicitPlaneWidget2.h>
#include <vtkClipPolyData.h>
#include <vtkNew.h>
#include <vtkPlane.h>
#include <vtkNamedColors.h>
#include <vtkProperty.h>

#include "vtkSTLReader.h"

#include <vtkCommand.h>
#include <vtkSphereSource.h>


#include <vtkAutoInit.h>
VTK_MODULE_INIT(vtkRenderingOpenGL2)
VTK_MODULE_INIT(vtkInteractionStyle)

using PixelType = int16_t;
typedef itk::Image< PixelType, 3 > ImageType;

namespace {
// Callback for the interaction
// This does the actual work: updates the vtkPlane implicit function.
// This in turn causes the pipeline to update and clip the object.
class vtkIPWCallback : public vtkCommand
{
public:
	static vtkIPWCallback* New()
	{
		return new vtkIPWCallback;
	}

	virtual void Execute(vtkObject* caller, unsigned long, void*)
	{
		auto* planeWidget = reinterpret_cast<vtkImplicitPlaneWidget2*>(caller);
		auto* rep =
			reinterpret_cast<vtkImplicitPlaneRepresentation*>(planeWidget->GetRepresentation());
		rep->GetPlane(this->plane);
	}

	vtkIPWCallback() = default;

	vtkPlane* plane{nullptr};
};

} // namespace

ImageType::Pointer readDataArrayFromDICOM(std::string dirName)
{
	using NamesGeneratorType = itk::GDCMSeriesFileNames;
	auto nameGenerator = NamesGeneratorType::New();

	nameGenerator->SetUseSeriesDetails(true);
	nameGenerator->AddSeriesRestriction("0008|0021");
	nameGenerator->SetGlobalWarningDisplay(false);
	nameGenerator->SetDirectory(dirName);

	using SeriesIdContainer = std::vector<std::string>;
	const SeriesIdContainer& seriesUID = nameGenerator->GetSeriesUIDs();
	auto seriesItr = seriesUID.begin();
	auto seriesEnd = seriesUID.end();

	if (seriesItr != seriesEnd)
	{
		std::cout << "The directory: ";
		std::cout << dirName << std::endl;
		std::cout << "Contains the following DICOM Series: ";
		std::cout << std::endl;
	}
	else
	{
		std::cout << "No DICOMs in: " << dirName << std::endl;
	}

	while (seriesItr != seriesEnd)
	{
		std::cout << seriesItr->c_str() << std::endl;
		++seriesItr;
	}

	seriesItr = seriesUID.begin();
	std::string seriesIdentifier;
	seriesIdentifier = seriesItr->c_str();
	seriesItr++;

	std::cout << "\nReading: ";
	std::cout << seriesIdentifier << std::endl;
	using FileNamesContainer = std::vector< std::string >;
	FileNamesContainer fileNames =
		nameGenerator->GetFileNames(seriesIdentifier);

	using ReaderType = itk::ImageSeriesReader< ImageType >;
	auto reader = ReaderType::New();
	using ImageIOType = itk::GDCMImageIO;
	auto dicomIO = ImageIOType::New();
	reader->SetImageIO(dicomIO);
	reader->SetFileNames(fileNames);
	reader->Update();

	return reader->GetOutput();
}

int main(int argc, char ** argv)
{
	if (argc < 2)
	{
		std::cerr << "Usage: " << std::endl;
		std::cerr << argv[0] << " [DicomDirectory  [outputFileName  [seriesName]]]";
		std::cerr << "\nIf DicomDirectory is not specified, current directory is used\n";
	}

	std::string dirName = "."; //current directory by default
	if (argc > 1)
		dirName = argv[1];

	auto data = readDataArrayFromDICOM(dirName);

	// auto reader =
	// 	vtkSmartPointer<vtkSTLReader>::New();
	// reader->SetFileName(dirName);
	// reader->Update();

	vtkNew<vtkNamedColors> colors;

	typedef itk::BinaryThresholdImageFilter <ImageType, ImageType>
		BinaryThresholdImageFilterType;

	int16_t lowerThreshold = 0;
	int16_t upperThreshold = (1<<15) - 1;

	auto thresholdFilter
		= BinaryThresholdImageFilterType::New();
	thresholdFilter->SetInput(data);
	thresholdFilter->SetLowerThreshold(lowerThreshold);
	thresholdFilter->SetUpperThreshold(upperThreshold);
	thresholdFilter->SetInsideValue(255);
	thresholdFilter->SetOutsideValue(0);

	typedef itk::ImageToVTKImageFilter<ImageType> ConnectorType;
	ConnectorType::Pointer connector = ConnectorType::New();
	connector->SetInput(thresholdFilter->GetOutput());
	// connector->SetInput(data);

	connector->Update();

	vtkNew<vtkMarchingCubes> surface;
	surface->SetInputData(connector->GetOutput());
	surface->ComputeNormalsOn();
	surface->SetValue(0, 100);

	vtkNew<vtkSphereSource> sphereSource;
	sphereSource->SetRadius(10.0);

	// Setup a visualization pipeline.
	vtkNew<vtkPlane> plane;
	vtkNew<vtkClipPolyData> clipper;
	clipper->SetClipFunction(plane);
	// clipper->InsideOutOn();

	clipper->SetInputConnection(surface->GetOutputPort());
    // clipper->SetInputConnection(sphereSource->GetOutputPort());

	vtkNew<vtkPolyDataMapper> mapper;
	mapper->SetInputConnection(clipper->GetOutputPort());
	// mapper->ScalarVisibilityOff();
	vtkNew<vtkActor> actor;
	actor->SetMapper(mapper);

	vtkNew<vtkProperty> backFaces;
	backFaces->SetDiffuseColor(colors->GetColor3d("Gold").GetData());

	actor->SetBackfaceProperty(backFaces);

  	// A renderer and render window.
	vtkNew<vtkRenderer> renderer;
	vtkNew<vtkRenderWindow> renderWindow;
	renderWindow->AddRenderer(renderer);
	renderWindow->SetWindowName("ImplicitPlaneWidget2");

	renderer->AddActor(actor);
	renderer->SetBackground(colors->GetColor3d("SlateGray").GetData());

  	// An interactor.
	vtkNew<vtkRenderWindowInteractor> renderWindowInteractor;
	renderWindowInteractor->SetRenderWindow(renderWindow);

	// The callback will do the work.
	vtkNew<vtkIPWCallback> myCallback;
	myCallback->plane = plane;

	vtkNew<vtkImplicitPlaneRepresentation> rep;
	rep->SetPlaceFactor(1.25); // This must be set prior to placing the widget.
	rep->PlaceWidget(actor->GetBounds());
	rep->SetNormal(plane->GetNormal());

	vtkNew<vtkImplicitPlaneWidget2> planeWidget;
	planeWidget->SetInteractor(renderWindowInteractor);
	planeWidget->SetRepresentation(rep);
	planeWidget->AddObserver(vtkCommand::InteractionEvent, myCallback);

	renderer->GetActiveCamera()->Azimuth(-60);
	renderer->GetActiveCamera()->Elevation(30);
	renderer->ResetCamera();
	renderer->GetActiveCamera()->Zoom(0.75);

	// Render and interact.
	renderWindowInteractor->Initialize();
	renderWindow->Render();
	planeWidget->On();

  	// Begin mouse interaction.
	renderWindowInteractor->Start();

	return EXIT_SUCCESS;
}