// Builds the original demo model (a small motorised linear stage) used for the website's
// screenshots, so no real customer or project geometry appears there. Build against OCCT and
// run: ./linear-stage out.step
#include <BRepAdaptor_Curve.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <Interface_Static.hxx>
#include <Quantity_Color.hxx>
#include <STEPCAFControl_Writer.hxx>
#include <TDataStd_Name.hxx>
#include <TDocStd_Document.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <XCAFApp_Application.hxx>
#include <XCAFDoc_ColorTool.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <gp_Ax2.hxx>
#include <cmath>
#include <cstdio>

static TopoDS_Shape box(double x, double y, double z, double dx, double dy, double dz) {
    return BRepPrimAPI_MakeBox(gp_Pnt(x, y, z), dx, dy, dz).Shape();
}
static TopoDS_Shape cylX(double x, double y, double z, double r, double len) {
    return BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(x, y, z), gp::DX()), r, len).Shape();
}
static TopoDS_Shape cylZ(double x, double y, double z, double r, double h) {
    return BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(x, y, z), gp::DZ()), r, h).Shape();
}
static TopoDS_Shape cut(const TopoDS_Shape& a, const TopoDS_Shape& b) { return BRepAlgoAPI_Cut(a, b).Shape(); }
static TopoDS_Shape fuse(const TopoDS_Shape& a, const TopoDS_Shape& b) { return BRepAlgoAPI_Fuse(a, b).Shape(); }
static TopoDS_Shape filletParallel(const TopoDS_Shape& s, const gp_Dir& dir, double r) {
    try {
        BRepFilletAPI_MakeFillet f(s);
        int n = 0;
        for (TopExp_Explorer e(s, TopAbs_EDGE); e.More(); e.Next()) {
            BRepAdaptor_Curve c(TopoDS::Edge(e.Current()));
            if (c.GetType() != GeomAbs_Line) continue;
            if (c.Line().Direction().IsParallel(dir, 1e-6)) { f.Add(r, TopoDS::Edge(e.Current())); ++n; }
        }
        if (n == 0) return s;
        f.Build();
        return f.IsDone() ? f.Shape() : s;
    } catch (...) { return s; }
}
static TopoDS_Shape hexPrismZ(double x, double y, double z, double acrossFlats, double h) {
    const double r = acrossFlats / std::sqrt(3.0);
    BRepBuilderAPI_MakePolygon p;
    for (int i = 0; i < 6; ++i) p.Add(gp_Pnt(x + r * std::cos(i * M_PI / 3), y + r * std::sin(i * M_PI / 3), z));
    p.Close();
    return BRepPrimAPI_MakePrism(BRepBuilderAPI_MakeFace(p.Wire()).Face(), gp_Vec(0, 0, h)).Shape();
}
static TopoDS_Shape capScrew(double headR, double headH, double socketAF, double shankR, double shankLen) {
    TopoDS_Shape head = cylZ(0, 0, 0, headR, headH);
    head = cut(head, hexPrismZ(0, 0, headH * 0.4, socketAF, headH));
    return fuse(head, cylZ(0, 0, -shankLen, shankR, shankLen));
}

struct Doc {
    Handle(TDocStd_Document) doc;
    Handle(XCAFDoc_ShapeTool) shapes;
    Handle(XCAFDoc_ColorTool) colours;
    TDF_Label part(const TopoDS_Shape& s, const char* name, int rgb) {
        TDF_Label l = shapes->AddShape(s, false);
        TDataStd_Name::Set(l, name);
        colours->SetColor(l, Quantity_Color(((rgb >> 16) & 255) / 255.0, ((rgb >> 8) & 255) / 255.0, (rgb & 255) / 255.0, Quantity_TOC_sRGB), XCAFDoc_ColorSurf);
        return l;
    }
    TDF_Label assembly(const char* name) {
        TDF_Label l = shapes->NewShape();
        TDataStd_Name::Set(l, name);
        return l;
    }
    TDF_Label root;
    void add(const TDF_Label& parent, const TDF_Label& child, double x = 0, double y = 0, double z = 0) {
        gp_Trsf t; t.SetTranslation(gp_Vec(x, y, z));
        // Modelled Z-up; STEP viewers commonly default to Y-up, so the top level turns it.
        if (parent == root) { gp_Trsf r; r.SetRotation(gp_Ax1(gp::Origin(), gp::DX()), -M_PI / 2); t = r * t; }
        TDF_Label c = shapes->AddComponent(parent, child, TopLoc_Location(t));
        Handle(TDataStd_Name) n;
        if (child.FindAttribute(TDataStd_Name::GetID(), n)) TDataStd_Name::Set(c, n->Get());
    }
};

int main(int argc, char** argv) {
    Doc d;
    XCAFApp_Application::GetApplication()->NewDocument("MDTV-XCAF", d.doc);
    d.shapes = XCAFDoc_DocumentTool::ShapeTool(d.doc->Main());
    d.colours = XCAFDoc_DocumentTool::ColorTool(d.doc->Main());

    // Base plate: 240 x 90 x 12 with four counterbored holes and a shallow relief pocket.
    TopoDS_Shape base = filletParallel(box(0, -45, 0, 240, 90, 12), gp::DZ(), 6);
    base = cut(base, box(70, -12, 8, 100, 24, 4.01));
    const double holes[4][2] = {{40, -33}, {40, 33}, {200, -33}, {200, 33}};
    for (auto& h : holes) {
        base = cut(base, cylZ(h[0], h[1], -1, 3.3, 14));
        base = cut(base, cylZ(h[0], h[1], 6, 5.5, 7));
    }

    // End block (used twice): bores for both rods and a bearing seat.
    TopoDS_Shape block = filletParallel(box(0, -45, 0, 24, 90, 50), gp::DX(), 3);
    block = cut(block, cylX(-1, -28, 25, 5, 26));
    block = cut(block, cylX(-1, 28, 25, 5, 26));
    block = cut(block, cylX(-1, 0, 25, 11, 26));

    TopoDS_Shape bearing = cut(cylX(0, 0, 0, 11, 7), cylX(-1, 0, 0, 4, 9));
    TopoDS_Shape rod = cylX(0, 0, 0, 5, 240);
    TopoDS_Shape leadScrew = cylX(4, 0, 37, 4, 252);

    // Carriage block with bushing bores, a nut bore and tapped holes on top.
    TopoDS_Shape carriage = filletParallel(box(85, -40, 24, 70, 80, 26), gp::DZ(), 4);
    carriage = cut(carriage, cylX(84, -28, 37, 8, 72));
    carriage = cut(carriage, cylX(84, 28, 37, 8, 72));
    carriage = cut(carriage, cylX(84, 0, 37, 6, 72));
    const double top[4][2] = {{100, -25}, {100, 25}, {140, -25}, {140, 25}};
    for (auto& h : top) carriage = cut(carriage, cylZ(h[0], h[1], 42, 2.5, 9));
    TopoDS_Shape bushing = cut(cylX(0, 0, 0, 8, 70), cylX(-1, 0, 0, 5, 72));
    TopoDS_Shape nut = fuse(cylX(79, 0, 37, 14, 6), cylX(85, 0, 37, 6, 20));
    nut = cut(nut, cylX(78, 0, 37, 4, 28));
    for (int i = 0; i < 4; ++i) nut = cut(nut, cylX(78, 9.5 * std::cos(M_PI / 4 + i * M_PI / 2), 37 + 9.5 * std::sin(M_PI / 4 + i * M_PI / 2), 1.6, 8));
    TopoDS_Shape plate = filletParallel(box(75, -45, 50, 90, 90, 6), gp::DZ(), 5);
    for (auto& h : top) plate = cut(plate, cylZ(h[0], h[1], 49, 2.75, 8));
    plate = cut(plate, cylZ(120, 0, 49, 10, 8));

    // Motor mount: plate, standoffs, coupler, motor.
    TopoDS_Shape motorPlate = filletParallel(box(240, -30, 7, 6, 60, 60), gp::DX(), 4);
    motorPlate = cut(motorPlate, cylX(239, 0, 37, 12, 8));
    TopoDS_Shape standoff = cylX(0, 0, 0, 4, 25);
    TopoDS_Shape coupler = cut(cylX(250, 0, 37, 10, 18), cylX(249, 0, 37, 2.6, 20));
    coupler = cut(coupler, box(258.5, -11, 36.5, 1, 22, 12));
    TopoDS_Shape motor = filletParallel(box(271, -21, 16, 40, 42, 42), gp::DX(), 5);
    motor = fuse(motor, cylX(268, 0, 37, 11, 3.01));
    motor = fuse(motor, cylX(254, 0, 37, 2.5, 14.5));
    TopoDS_Shape motorCap = filletParallel(box(311, -21, 16, 9, 42, 42), gp::DX(), 5);
    motorCap = cut(motorCap, box(316, -6, 10, 6, 12, 8));

    TopoDS_Shape m6 = capScrew(5, 6, 5, 3, 6);
    TopoDS_Shape m5 = capScrew(4.25, 5, 4, 2.5, 10);

    TDF_Label lBase = d.part(base, "Base Plate", 0x3A3F4A);
    TDF_Label lBlock = d.part(block, "End Block", 0xB9BFC9);
    TDF_Label lBearing = d.part(bearing, "608 Bearing", 0xD5D9DF);
    TDF_Label lRod = d.part(rod, "Guide Rod", 0xDDE1E6);
    TDF_Label lScrew = d.part(leadScrew, "Lead Screw", 0x9EA5AF);
    TDF_Label lCarriage = d.part(carriage, "Slider", 0x4A5BD6);
    TDF_Label lBushing = d.part(bushing, "Bushing", 0xB08D57);
    TDF_Label lNut = d.part(nut, "Lead Nut", 0xC9A44C);
    TDF_Label lPlate = d.part(plate, "Top Plate", 0xCBD0D8);
    TDF_Label lMotorPlate = d.part(motorPlate, "Motor Plate", 0x6B7280);
    TDF_Label lStandoff = d.part(standoff, "Standoff", 0x8E959F);
    TDF_Label lCoupler = d.part(coupler, "Coupler", 0xC0C6CE);
    TDF_Label lMotor = d.part(motor, "Stepper Motor", 0x2B2D33);
    TDF_Label lMotorCap = d.part(motorCap, "Motor Cap", 0x9AA1AB);
    TDF_Label lM6 = d.part(m6, "M6 Screw", 0x25272C);
    TDF_Label lM5 = d.part(m5, "M5 Screw", 0x25272C);

    TDF_Label root = d.assembly("Linear Stage");
    d.root = root;
    TDF_Label carriageAsm = d.assembly("Carriage");
    TDF_Label motorAsm = d.assembly("Motor Drive");

    d.add(root, lBase);
    d.add(root, lBlock, 0, 0, 12);
    d.add(root, lBlock, 216, 0, 12);
    d.add(root, lBearing, 8.5, 0, 37);
    d.add(root, lBearing, 224.5, 0, 37);
    d.add(root, lRod, 0, -28, 37);
    d.add(root, lRod, 0, 28, 37);
    d.add(root, lScrew);
    for (auto& h : holes) d.add(root, lM6, h[0], h[1], 6);

    d.add(carriageAsm, lCarriage);
    d.add(carriageAsm, lBushing, 85, -28, 37);
    d.add(carriageAsm, lBushing, 85, 28, 37);
    d.add(carriageAsm, lNut);
    d.add(carriageAsm, lPlate);
    for (auto& h : top) d.add(carriageAsm, lM5, h[0], h[1], 56);
    d.add(root, carriageAsm);

    d.add(motorAsm, lMotorPlate);
    const double so[4][2] = {{-15.5, 21.5}, {15.5, 21.5}, {-15.5, 52.5}, {15.5, 52.5}};
    for (auto& s : so) d.add(motorAsm, lStandoff, 246, s[0], s[1]);
    d.add(motorAsm, lCoupler);
    d.add(motorAsm, lMotor);
    d.add(motorAsm, lMotorCap);
    d.add(root, motorAsm);

    d.shapes->UpdateAssemblies();
    Interface_Static::SetCVal("write.step.unit", "MM");
    STEPCAFControl_Writer w;
    w.SetColorMode(true);
    w.SetNameMode(true);
    if (!w.Transfer(d.doc, STEPControl_AsIs)) { std::puts("transfer failed"); return 1; }
    return w.Write(argc > 1 ? argv[1] : "linear-stage.step") == IFSelect_RetDone ? 0 : 1;
}
