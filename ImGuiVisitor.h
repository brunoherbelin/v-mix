#ifndef IMGUIVISITOR_H
#define IMGUIVISITOR_H

#include "Visitor.h"
#include "InfoVisitor.h"

class ImGuiVisitor: public Visitor
{
    InfoVisitor info;

public:
    ImGuiVisitor();

    // Elements of Scene
    void visit (Scene& n) override;
    void visit (Node& n) override;
    void visit (Group& n) override;
    void visit (Switch& n) override;
    void visit (Primitive& n) override;
    void visit (MediaSurface& n) override;
    void visit (FrameBufferSurface& n) override;

    // Elements with attributes
    void visit (MediaPlayer& n) override;
    void visit (Shader& n) override;
    void visit (ImageProcessingShader& n) override;
    void visit (Source& s) override;
    void visit (MediaSource& s) override;
    void visit (SessionFileSource& s) override;
    void visit (SessionGroupSource& s) override;
    void visit (RenderSource& s) override;
    void visit (CloneSource& s) override;
    void visit (PatternSource& s) override;
    void visit (DeviceSource& s) override;
    void visit (NetworkSource& s) override;
    void visit (MultiFileSource& s) override;
};

#endif // IMGUIVISITOR_H
