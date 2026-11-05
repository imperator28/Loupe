// Builds a benchmark assembly of many repeated parts: the demo stage (scripts/website) placed
// N times in a grid, as one top assembly with N copies of the stage sub-assembly. Every part
// recurs N times, the case instancing and the allocator benchmark are about.
//
//   ./loupe-stage-grid linear-stage.step stage-grid.step 24
#include <IFSelect_ReturnStatus.hxx>
#include <Interface_Static.hxx>
#include <STEPCAFControl_Reader.hxx>
#include <STEPCAFControl_Writer.hxx>
#include <TDF_LabelSequence.hxx>
#include <TDataStd_Name.hxx>
#include <TDocStd_Document.hxx>
#include <XCAFApp_Application.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <gp_Trsf.hxx>

#include <cstdio>
#include <cstdlib>

int main(int argc, char** argv)
{
    if (argc < 4) {
        std::fprintf(stderr, "usage: %s stage.step out.step copies\n", argv[0]);
        return 2;
    }
    STEPCAFControl_Reader reader;
    reader.SetNameMode(true);
    reader.SetColorMode(true);
    if (reader.ReadFile(argv[1]) != IFSelect_RetDone) return 1;
    occ::handle<TDocStd_Document> document;
    XCAFApp_Application::GetApplication()->NewDocument("MDTV-XCAF", document);
    if (!reader.Transfer(document)) return 1;
    const auto shapes = XCAFDoc_DocumentTool::ShapeTool(document->Main());
    TDF_LabelSequence free;
    shapes->GetFreeShapes(free);
    if (free.Length() < 1) return 1;
    const auto stage = free.Value(1);
    const auto grid = shapes->NewShape();
    TDataStd_Name::Set(grid, "Stage Grid");
    const int copies = std::max(1, std::atoi(argv[3]));
    for (int index = 0; index < copies; ++index) {
        gp_Trsf placement;
        placement.SetTranslation(gp_Vec((index % 6) * 400.0, 0.0, (index / 6) * 300.0));
        shapes->AddComponent(grid, stage, TopLoc_Location(placement));
    }
    shapes->UpdateAssemblies();
    Interface_Static::SetCVal("write.step.unit", "MM");
    STEPCAFControl_Writer writer;
    if (!writer.Transfer(document, STEPControl_AsIs)) return 1;
    return writer.Write(argv[2]) == IFSelect_RetDone ? 0 : 1;
}
