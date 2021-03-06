#include "ImGuiVisitor.h"

#include <vector>
#include <algorithm>
#include <sstream>
#include <iomanip>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_access.hpp>

#include <tinyxml2.h>
#include "tinyxml2Toolkit.h"

#include "defines.h"
#include "Log.h"
#include "Scene.h"
#include "Primitives.h"
#include "ImageShader.h"
#include "ImageProcessingShader.h"
#include "MediaPlayer.h"
#include "MediaSource.h"
#include "SessionSource.h"
#include "PatternSource.h"
#include "DeviceSource.h"
#include "NetworkSource.h"
#include "MultiFileSource.h"
#include "SessionCreator.h"
#include "SessionVisitor.h"
#include "Settings.h"
#include "Mixer.h"
#include "ActionManager.h"

#include "imgui.h"
#include "ImGuiToolkit.h"
#include "BaseToolkit.h"
#include "UserInterfaceManager.h"
#include "SystemToolkit.h"


ImGuiVisitor::ImGuiVisitor()
{

}

void ImGuiVisitor::visit(Node &n)
{

}

void ImGuiVisitor::visit(Group &n)
{
    // MODEL VIEW
    ImGui::PushID(std::to_string(n.id()).c_str());

    if (ImGuiToolkit::ButtonIcon(1, 16)) {
        n.translation_.x = 0.f;
        n.translation_.y = 0.f;
        n.rotation_.z = 0.f;
        n.scale_.x = 1.f;
        n.scale_.y = 1.f;
        Action::manager().store("Geometry Reset");
    }
    ImGui::SameLine(0, 10);
    ImGui::Text("Geometry");

    if (ImGuiToolkit::ButtonIcon(6, 15)) {
        n.translation_.x = 0.f;
        n.translation_.y = 0.f;
        Action::manager().store("Position 0.0, 0.0");
    }
    ImGui::SameLine(0, 10);
    float translation[2] = { n.translation_.x, n.translation_.y};
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    if ( ImGui::SliderFloat2("Position", translation, -5.0, 5.0) )
    {
        n.translation_.x = translation[0];
        n.translation_.y = translation[1];
    }
    if (ImGui::IsItemDeactivatedAfterEdit()){
        std::ostringstream oss;
        oss << "Position " << std::setprecision(3) << n.translation_.x << ", " << n.translation_.y;
        Action::manager().store(oss.str());
    }
    if (ImGuiToolkit::ButtonIcon(3, 15))  {
        n.scale_.x = 1.f;
        n.scale_.y = 1.f;
        Action::manager().store("Scale 1.0 x 1.0");
    }
    ImGui::SameLine(0, 10);
    float scale[2] = { n.scale_.x, n.scale_.y} ;
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    if ( ImGui::SliderFloat2("Scale", scale, -MAX_SCALE, MAX_SCALE, "%.2f") )
    {
        n.scale_.x = CLAMP_SCALE(scale[0]);
        n.scale_.y = CLAMP_SCALE(scale[1]);
    }
    if (ImGui::IsItemDeactivatedAfterEdit()){
        std::ostringstream oss;
        oss << "Scale " << std::setprecision(3) << n.scale_.x << " x " << n.scale_.y;
        Action::manager().store(oss.str());
    }

    if (ImGuiToolkit::ButtonIcon(18, 9)){
        n.rotation_.z = 0.f;
        Action::manager().store("Angle 0.0");
    }
    ImGui::SameLine(0, 10);
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    ImGui::SliderAngle("Angle", &(n.rotation_.z), -180.f, 180.f) ;
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        std::ostringstream oss;
        oss << "Angle " << std::setprecision(3) << n.rotation_.z * 180.f / M_PI;
        Action::manager().store(oss.str());
    }


    ImGui::PopID();

    // spacing
    ImGui::Spacing();
}

void ImGuiVisitor::visit(Switch &n)
{
    if (n.numChildren()>0)
        n.activeChild()->accept(*this);
}

void ImGuiVisitor::visit(Scene &n)
{
    ImGui::SetNextItemOpen(true, ImGuiCond_Once);
    if (ImGui::CollapsingHeader("Scene Property Tree"))
    {
        n.root()->accept(*this);
    }
}

void ImGuiVisitor::visit(Primitive &n)
{
    ImGui::PushID(std::to_string(n.id()).c_str());
    ImGui::Text("Primitive %d");

    n.shader()->accept(*this);

    ImGui::PopID();
}

void ImGuiVisitor::visit(FrameBufferSurface &n)
{
    ImGui::Text("Framebuffer");
}

void ImGuiVisitor::visit(MediaSurface &n)
{
    ImGui::Text("%s", n.path().c_str());

    if (n.mediaPlayer())
        n.mediaPlayer()->accept(*this);
}

void ImGuiVisitor::visit(MediaPlayer &n)
{
    ImGui::Text("Media Player");
}

void ImGuiVisitor::visit(Shader &n)
{
    ImGui::PushID(std::to_string(n.id()).c_str());

    // Base color
//    if (ImGuiToolkit::ButtonIcon(10, 2)) {
//        n.blending = Shader::BLEND_OPACITY;
//        n.color = glm::vec4(1.f, 1.f, 1.f, 1.f);
//    }
//    ImGui::SameLine(0, 10);
//    ImGui::ColorEdit3("Color", glm::value_ptr(n.color), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel ) ;
//    ImGui::SameLine(0, 5);
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    int mode = n.blending;
    if (ImGui::Combo("Blending", &mode, "Normal\0Screen\0Subtract\0Multiply\0Soft light"
                     "\0Hard light\0Soft subtract\0Lighten only\0") ) {
        n.blending = Shader::BlendMode(mode);

        std::ostringstream oss;
        oss << "Blending ";
        switch(n.blending) {
        case Shader::BLEND_OPACITY:
            oss<<"Normal";
            break;
        case Shader::BLEND_SCREEN:
            oss<<"Screen";
            break;
        case Shader::BLEND_SUBTRACT:
            oss<<"Subtract";
            break;
        case Shader::BLEND_MULTIPLY:
            oss<<"Multiply";
            break;
        case Shader::BLEND_HARD_LIGHT:
            oss<<"Hard light";
            break;
        case Shader::BLEND_SOFT_LIGHT:
            oss<<"Soft light";
            break;
        case Shader::BLEND_SOFT_SUBTRACT:
            oss<<"Soft subtract";
            break;
        case Shader::BLEND_LIGHTEN_ONLY:
            oss<<"Lighten only";
            break;
        case Shader::BLEND_NONE:
            oss<<"None";
            break;
        }
        Action::manager().store(oss.str());
    }

    ImGui::PopID();
}

//void ImGuiVisitor::visit(ImageShader &n)
//{
//    ImGui::PushID(std::to_string(n.id()).c_str());
//    // get index of the mask used in this ImageShader
//    int item_current = n.mask;
////    if (ImGuiToolkit::ButtonIcon(10, 3)) n.mask = 0;
////    ImGui::SameLine(0, 10);
//    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
//    // combo list of masks
//    if ( ImGui::Combo("Mask", &item_current, ImageShader::mask_names, IM_ARRAYSIZE(ImageShader::mask_names) ) )
//    {
//        if (item_current < (int) ImageShader::mask_presets.size())
//            n.mask = item_current;
//        else {
//            // TODO ask for custom mask
//        }
//        Action::manager().store("Mask "+ std::string(ImageShader::mask_names[n.mask]));
//    }
//    ImGui::PopID();
//}

void ImGuiVisitor::visit(ImageProcessingShader &n)
{
    ImGui::PushID(std::to_string(n.id()).c_str());

    ImGuiToolkit::Icon(6, 2);
    ImGui::SameLine(0, 10);
    ImGui::Text("Filters");

    if (ImGuiToolkit::ButtonIcon(6, 4)) {
        n.gamma = glm::vec4(1.f, 1.f, 1.f, 1.f);
        Action::manager().store("Gamma & Color");
    }
    ImGui::SameLine(0, 10);
    ImGui::ColorEdit3("Gamma Color", glm::value_ptr(n.gamma), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel) ;
    if (ImGui::IsItemDeactivatedAfterEdit())
        Action::manager().store("Gamma Color changed");

    ImGui::SameLine(0, 5);
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    ImGui::SliderFloat("Gamma", &n.gamma.w, 0.5f, 10.f, "%.2f", 2.f);
    if (ImGui::IsItemDeactivatedAfterEdit()){
        std::ostringstream oss;
        oss << "Gamma " << std::setprecision(2) << n.gamma.w;
        Action::manager().store(oss.str());
    }

//    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
//    ImGui::SliderFloat4("Levels", glm::value_ptr(n.levels), 0.0, 1.0);

    if (ImGuiToolkit::ButtonIcon(5, 16)) {
        n.brightness = 0.f;
        n.contrast = 0.f;
        Action::manager().store("B & C  0.0 0.0");
    }
    ImGui::SameLine(0, 10);
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    float bc[2] = { n.brightness, n.contrast};
    if ( ImGui::SliderFloat2("B & C", bc, -1.0, 1.0) )
    {
        n.brightness = bc[0];
        n.contrast = bc[1];
    }
    if (ImGui::IsItemDeactivatedAfterEdit()){
        std::ostringstream oss;
        oss << "B & C  " << std::setprecision(2) << n.brightness << " " << n.contrast;
        Action::manager().store(oss.str());
    }

    if (ImGuiToolkit::ButtonIcon(9, 16)) {
        n.saturation = 0.f;
        Action::manager().store("Saturation 0.0");
    }
    ImGui::SameLine(0, 10);
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    ImGui::SliderFloat("Saturation", &n.saturation, -1.0, 1.0);
    if (ImGui::IsItemDeactivatedAfterEdit()){
        std::ostringstream oss;
        oss << "Saturation " << std::setprecision(2) << n.saturation;
        Action::manager().store(oss.str());
    }

    if (ImGuiToolkit::ButtonIcon(12, 4)) {
        n.hueshift = 0.f;
        Action::manager().store("Hue shift 0.0");
    }
    ImGui::SameLine(0, 10);
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    ImGui::SliderFloat("Hue shift", &n.hueshift, 0.0, 1.0);
    if (ImGui::IsItemDeactivatedAfterEdit()){
        std::ostringstream oss;
        oss << "Hue shift " << std::setprecision(2) << n.hueshift;
        Action::manager().store(oss.str());
    }

    if (ImGuiToolkit::ButtonIcon(18, 1)) {
        n.nbColors = 0;
        Action::manager().store("Posterize None");
    }
    ImGui::SameLine(0, 10);
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    ImGui::SliderInt("Posterize", &n.nbColors, 0, 16, n.nbColors == 0 ? "None" : "%d colors");
    if (ImGui::IsItemDeactivatedAfterEdit()){
        std::ostringstream oss;
        oss << "Posterize ";
        if (n.nbColors == 0) oss << "None"; else oss << n.nbColors;
        Action::manager().store(oss.str());
    }

    if (ImGuiToolkit::ButtonIcon(8, 1)) {
        n.threshold = 0.f;
        Action::manager().store("Threshold None");
    }
    ImGui::SameLine(0, 10);
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    ImGui::SliderFloat("Threshold", &n.threshold, 0.0, 1.0, n.threshold < 0.001 ? "None" : "%.2f");
    if (ImGui::IsItemDeactivatedAfterEdit()){
        std::ostringstream oss;
        oss << "Threshold ";
        if (n.threshold < 0.001) oss << "None"; else oss << std::setprecision(2) << n.threshold;
        Action::manager().store(oss.str());
    }

    if (ImGuiToolkit::ButtonIcon(3, 1)) {
        n.lumakey = 0.f;
        Action::manager().store("Lumakey 0.0");
    }
    ImGui::SameLine(0, 10);
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    ImGui::SliderFloat("Lumakey", &n.lumakey, 0.0, 1.0);
    if (ImGui::IsItemDeactivatedAfterEdit()){
        std::ostringstream oss;
        oss << "Lumakey " << std::setprecision(2) << n.lumakey;
        Action::manager().store(oss.str());
    }

    if (ImGuiToolkit::ButtonIcon(13, 4)) {
        n.chromakey = glm::vec4(0.f, 0.8f, 0.f, 1.f);
        n.chromadelta = 0.f;
        Action::manager().store("Chromakey & Color Reset");
    }
    ImGui::SameLine(0, 10);
    ImGui::ColorEdit3("Chroma color", glm::value_ptr(n.chromakey), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel  ) ;    
    if (ImGui::IsItemDeactivatedAfterEdit())
        Action::manager().store("Chroma color changed");
    ImGui::SameLine(0, 5);
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    ImGui::SliderFloat("Chromakey", &n.chromadelta, 0.0, 1.0, n.chromadelta < 0.001 ? "None" : "Tolerance %.2f");
    if (ImGui::IsItemDeactivatedAfterEdit()){
        std::ostringstream oss;
        oss << "Chromakey ";
        if (n.chromadelta < 0.001) oss << "None"; else oss << std::setprecision(2) << n.chromadelta;
        Action::manager().store(oss.str());
    }

    if (ImGuiToolkit::ButtonIcon(6, 16)) {
        n.invert = 0;
        Action::manager().store("Invert None");
    }
    ImGui::SameLine(0, 10);
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    if (ImGui::Combo("Invert", &n.invert, "None\0Invert Color\0Invert Luminance\0"))
        Action::manager().store("Invert " + std::string(n.invert<1 ? "None": (n.invert>1 ? "Luminance" : "Color")));

    if (ImGuiToolkit::ButtonIcon(1, 7)) {
        n.filterid = 0;
        Action::manager().store("Filter None");
    }
    ImGui::SameLine(0, 10);
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    if (ImGui::Combo("Filter", &n.filterid, ImageProcessingShader::filter_names, IM_ARRAYSIZE(ImageProcessingShader::filter_names) ) )
        Action::manager().store("Filter " + std::string(ImageProcessingShader::filter_names[n.filterid]));

    ImGui::PopID();

    ImGui::Spacing();
}

void ImGuiVisitor::visit (Source& s)
{
    ImGui::PushID(std::to_string(s.id()).c_str());
    // blending
    s.blendingShader()->accept(*this);

    // preview
    float preview_width = ImGui::GetContentRegionAvail().x IMGUI_RIGHT_ALIGN;
    float preview_height = 4.5f * ImGui::GetFrameHeightWithSpacing();
    ImVec2 pos = ImGui::GetCursorPos(); // remember where we were...

    float space = ImGui::GetStyle().ItemSpacing.y;
    float width = preview_width;
    float height = s.frame()->projectionArea().y * width / ( s.frame()->projectionArea().x * s.frame()->aspectRatio());
    if (height > preview_height - space) {
        height = preview_height - space;
        width = height * s.frame()->aspectRatio() * ( s.frame()->projectionArea().x / s.frame()->projectionArea().y);
    }
    // centered image
    ImGui::SetCursorPos( ImVec2(pos.x + 0.5f * (preview_width-width), pos.y + 0.5f * (preview_height-height-space)) );
    ImGui::Image((void*)(uintptr_t) s.frame()->texture(), ImVec2(width, height));

    // inform on visibility status
    ImGui::SetCursorPos( ImVec2(preview_width + 20, pos.y ) );
    if (s.active()) {
        if (s.blendingShader()->color.a > 0.f)
            ImGuiToolkit::HelpMarker("Visible", ICON_FA_EYE);
        else
            ImGuiToolkit::HelpMarker("Not visible", ICON_FA_EYE_SLASH);
    }
    else
        ImGuiToolkit::HelpMarker("Inactive", ICON_FA_SNOWFLAKE);

    // Inform on workspace
    ImGui::SetCursorPos( ImVec2(preview_width + 20, pos.y + ImGui::GetFrameHeightWithSpacing()) );
    if (s.workspace() == Source::BACKGROUND)
        ImGuiToolkit::HelpIcon("in Background",10, 16);
    else if (s.workspace() == Source::FOREGROUND)
        ImGuiToolkit::HelpIcon("in Foreground",12, 16);
    else
        ImGuiToolkit::HelpIcon("in Workspace",11, 16);

    // locking
    ImGui::SetCursorPos( ImVec2(preview_width + 20, pos.y + 2.f * ImGui::GetFrameHeightWithSpacing()) );
    const char *tooltip[2] = {"Unlocked", "Locked"};
    bool l = s.locked();
    if (ImGuiToolkit::IconToggle(15,6,17,6, &l, tooltip ) ) {
        s.setLocked(l);
        if (l) {
            Mixer::selection().clear();
            Action::manager().store(s.name() + std::string(": lock."));
        }
        else {
            Mixer::selection().set(&s);
            Action::manager().store(s.name() + std::string(": unlock."));
        }
    }

    // toggle enable/disable image processing
    bool on = s.imageProcessingEnabled();
    ImGui::SetCursorPos( ImVec2(preview_width + 15, pos.y + 3.5f * ImGui::GetFrameHeightWithSpacing()) );
    if ( ImGuiToolkit::ButtonToggle(ICON_FA_MAGIC, &on) ){
        std::ostringstream oss;
        oss << s.name() << ": " << ( on ? "Enable Filter" : "Disable Filter");
        Action::manager().store(oss.str());
    }
    s.setImageProcessingEnabled(on);

    // image processing pannel
    if (s.imageProcessingEnabled()) {

        // menu icon for image processing
        ImGui::SetCursorPos( ImVec2( preview_width - ImGui::GetTextLineHeight(), pos.y + 4.5f * ImGui::GetFrameHeightWithSpacing())); // ...come back
        if (ImGuiToolkit::IconButton(5, 8))
            ImGui::OpenPopup( "MenuImageProcessing" );
        if (ImGui::BeginPopup( "MenuImageProcessing" ))
        {
            if (s.processingshader_link_.connected()) {
                if (ImGui::MenuItem( "Unfollow" )){
                    s.processingshader_link_.disconnect();
                }
            }
            else {
                if (ImGui::MenuItem("Reset" )){
                    ImageProcessingShader defaultvalues;
                    s.processingShader()->copy(defaultvalues);
                    s.processingshader_link_.disconnect();
                    std::ostringstream oss;
                    oss << s.name() << ": " << "Reset Filter";
                    Action::manager().store(oss.str());
                }
                if (ImGui::MenuItem("Copy" )){
                    std::string clipboard = SessionVisitor::getClipboard(s.processingShader());
                    if (!clipboard.empty())
                        ImGui::SetClipboardText(clipboard.c_str());
                }
                const char *clipboard = ImGui::GetClipboardText();
                const bool can_paste = (clipboard != nullptr && SessionLoader::isClipboard(clipboard));
                if (ImGui::MenuItem("Paste", NULL, false, can_paste)) {
                    SessionLoader::applyImageProcessing(s, clipboard);
                    std::ostringstream oss;
                    oss << s.name() << ": " << "Change Filter";
                    Action::manager().store(oss.str());
                }
//                ImGui::Separator();
//                if (ImGui::BeginMenu("Follow"))
//                {
//                    for (auto mpit = Mixer::manager().session()->begin();
//                         mpit != Mixer::manager().session()->end(); mpit++ )
//                    {
//                        std::string label = (*mpit)->name();
//                        if ( (*mpit)->id() != s.id() &&
//                             (*mpit)->imageProcessingEnabled() &&
//                             !(*mpit)->processingshader_link_.connected()) {
//                            if (ImGui::MenuItem( label.c_str() )){
//                                s.processingshader_link_.connect(*mpit);
//                                s.touch();
//                            }
//                        }
//                    }
//                    ImGui::EndMenu();
//                }
            }

            ImGui::EndPopup();
        }

        // full panel for image processing
        ImGui::SetCursorPos( ImVec2( pos.x, pos.y + preview_height)); // ...come back

        if (s.processingshader_link_.connected()) {
            ImGuiToolkit::Icon(6, 2);
            ImGui::SameLine(0, 10);
            ImGui::Text("Filters");
            Source *target = s.processingshader_link_.source();
            ImGui::Text("Following");
            if ( target != nullptr && ImGui::Button(target->name().c_str(), ImVec2(IMGUI_RIGHT_ALIGN, 0)) )
                Mixer::manager().setCurrentSource(target);
        }
        else
            s.processingShader()->accept(*this);
    }

    ImGui::PopID();
}

void ImGuiVisitor::visit (MediaSource& s)
{
    ImGuiToolkit::Icon(s.icon().x, s.icon().y);
    ImGui::SameLine(0, 10);
    if ( s.mediaplayer()->isImage() )
        ImGui::Text("Image File");
    else
        ImGui::Text("Video File");

    // Media info
    ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + ImGui::GetContentRegionAvail().x IMGUI_RIGHT_ALIGN);
    s.accept(info);
    ImGui::Text("%s", info.str().c_str());
    ImGui::PopTextWrapPos();

    // icon (>) to open player
    if ( s.playable() ) {
        ImVec2 pos = ImGui::GetCursorPos();
        ImGui::SameLine(0, 0);
        ImGui::SameLine(0, 10.f + ImGui::GetContentRegionAvail().x IMGUI_RIGHT_ALIGN);
        if (ImGuiToolkit::IconButton(ICON_FA_PLAY_CIRCLE, "Open in Player"))
            UserInterface::manager().showSourceEditor(&s);
        ImGui::SetCursorPos(pos);
    }

    // folder
    std::string path = SystemToolkit::path_filename(s.path());
    std::string label = BaseToolkit::trunc_string(path, 25);
    label = BaseToolkit::transliterate(label);
    ImGuiToolkit::ButtonOpenUrl( label.c_str(), path.c_str(), ImVec2(IMGUI_RIGHT_ALIGN, 0) );

    ImGui::SameLine();
    ImGui::Text("Folder");
}

void ImGuiVisitor::visit (SessionFileSource& s)
{
    if (s.session() == nullptr)
        return;

    ImGuiToolkit::Icon(s.icon().x, s.icon().y);
    ImGui::SameLine(0, 10);
    ImGui::Text("Session File");

    // info
    ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + ImGui::GetContentRegionAvail().x IMGUI_RIGHT_ALIGN);
    s.accept(info);
    ImGui::Text("%s", info.str().c_str());
    ImGui::PopTextWrapPos();

    // icon (>) to open player
    if ( s.playable() ) {
        ImVec2 pos = ImGui::GetCursorPos();
        ImGui::SameLine(0, 0);
        ImGui::SameLine(0, 10.f + ImGui::GetContentRegionAvail().x IMGUI_RIGHT_ALIGN);
        if (ImGuiToolkit::IconButton(ICON_FA_PLAY_CIRCLE, "Open in Player"))
            UserInterface::manager().showSourceEditor(&s);
        ImGui::SetCursorPos(pos);
    }

    if ( ImGui::Button( ICON_FA_FILE_EXPORT " Import", ImVec2(IMGUI_RIGHT_ALIGN, 0)) )
        Mixer::manager().import( &s );
    ImGui::SameLine();
    ImGui::Text("Sources");

    if (ImGuiToolkit::ButtonIcon(3, 2)) s.session()->setFading(0.f);
    float f = s.session()->fading();
    ImGui::SameLine(0, 10);
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    if (ImGui::SliderFloat("Fading", &f, 0.0, 1.0, f < 0.001 ? "None" : "%.2f") )
        s.session()->setFading(f);
    if (ImGui::IsItemDeactivatedAfterEdit()){
        std::ostringstream oss;
        oss << s.name() << ": Fading " << std::setprecision(2) << f;
        Action::manager().store(oss.str());
    }
    if ( ImGui::Button( ICON_FA_FILE_UPLOAD " Open", ImVec2(IMGUI_RIGHT_ALIGN, 0)) )
        Mixer::manager().set( s.detach() );
    ImGui::SameLine();
    ImGui::Text("File");

    std::string path = SystemToolkit::path_filename(s.path());
    std::string label = BaseToolkit::trunc_string(path, 25);
    label = BaseToolkit::transliterate(label);
    ImGuiToolkit::ButtonOpenUrl( label.c_str(), path.c_str(), ImVec2(IMGUI_RIGHT_ALIGN, 0) );
    ImGui::SameLine();
    ImGui::Text("Folder");

}

void ImGuiVisitor::visit (SessionGroupSource& s)
{
    if (s.session() == nullptr)
        return;

    ImGuiToolkit::Icon(s.icon().x, s.icon().y);
    ImGui::SameLine(0, 10);
    ImGui::Text("Flat Sesion group");

    // info
    ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + ImGui::GetContentRegionAvail().x IMGUI_RIGHT_ALIGN);
    s.accept(info);
    ImGui::Text("%s", info.str().c_str());
    ImGui::PopTextWrapPos();

    // icon (>) to open player
    if ( s.playable() ) {
        ImVec2 pos = ImGui::GetCursorPos();
        ImGui::SameLine(0, 0);
        ImGui::SameLine(0, 10.f + ImGui::GetContentRegionAvail().x IMGUI_RIGHT_ALIGN);
        if (ImGuiToolkit::IconButton(ICON_FA_PLAY_CIRCLE, "Open in Player"))
            UserInterface::manager().showSourceEditor(&s);
        ImGui::SetCursorPos(pos);
    }

    if ( ImGui::Button( ICON_FA_UPLOAD " Expand", ImVec2(IMGUI_RIGHT_ALIGN, 0)) ){
        Mixer::manager().import( &s );
    }
}

void ImGuiVisitor::visit (RenderSource& s)
{
    ImGuiToolkit::Icon(s.icon().x, s.icon().y);
    ImGui::SameLine(0, 10);
    ImGui::Text("Rendering Output");
    if ( ImGui::Button(IMGUI_TITLE_PREVIEW, ImVec2(IMGUI_RIGHT_ALIGN, 0)) )
        Settings::application.widget.preview = true;
}

void ImGuiVisitor::visit (CloneSource& s)
{
    ImGuiToolkit::Icon(s.icon().x, s.icon().y);
    ImGui::SameLine(0, 10);
    ImGui::Text("Clone");
    if ( ImGui::Button(s.origin()->name().c_str(), ImVec2(IMGUI_RIGHT_ALIGN, 0)) )
        Mixer::manager().setCurrentSource(s.origin());
    ImGui::SameLine();
    ImGui::Text("Source");
}

void ImGuiVisitor::visit (PatternSource& s)
{
    ImGuiToolkit::Icon(s.icon().x, s.icon().y);
    ImGui::SameLine(0, 10);
    ImGui::Text("Pattern");

    // stream info
    ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + ImGui::GetContentRegionAvail().x IMGUI_RIGHT_ALIGN);
    s.accept(info);
    ImGui::Text("%s", info.str().c_str());
    ImGui::PopTextWrapPos();

    // icon (>) to open player
    if ( s.playable() ) {
        ImVec2 pos = ImGui::GetCursorPos();
        ImGui::SameLine(0, 0);
        ImGui::SameLine(0, 10.f + ImGui::GetContentRegionAvail().x IMGUI_RIGHT_ALIGN);
        if (ImGuiToolkit::IconButton(ICON_FA_PLAY_CIRCLE, "Open in Player"))
            UserInterface::manager().showSourceEditor(&s);
        ImGui::SetCursorPos(pos);
    }

    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    if (ImGui::BeginCombo("##Patterns", Pattern::pattern_types[s.pattern()->type()].c_str()) )
    {
        for (uint p = 0; p < Pattern::pattern_types.size(); ++p){
            if (ImGui::Selectable( Pattern::pattern_types[p].c_str() )) {
                s.setPattern(p, s.pattern()->resolution());
                info.reset();
                std::ostringstream oss;
                oss << s.name() << ": Pattern " << Pattern::pattern_types[p];
                Action::manager().store(oss.str());
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::Text("Generator");
}

void ImGuiVisitor::visit (DeviceSource& s)
{
    ImGuiToolkit::Icon(s.icon().x, s.icon().y);
    ImGui::SameLine(0, 10);
    ImGui::Text("Device");

    ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + ImGui::GetContentRegionAvail().x IMGUI_RIGHT_ALIGN);
    s.accept(info);
    ImGui::Text("%s", info.str().c_str());
    ImGui::PopTextWrapPos();

    // icon (>) to open player
    if ( s.playable() ) {
        ImVec2 pos = ImGui::GetCursorPos();
        ImGui::SameLine(0, 0);
        ImGui::SameLine(0, 10.f + ImGui::GetContentRegionAvail().x IMGUI_RIGHT_ALIGN);
        if (ImGuiToolkit::IconButton(ICON_FA_PLAY_CIRCLE, "Open in Player"))
            UserInterface::manager().showSourceEditor(&s);
        ImGui::SetCursorPos(pos);
    }

    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    if (ImGui::BeginCombo("##Hardware", s.device().c_str()))
    {
        for (int d = 0; d < Device::manager().numDevices(); ++d){
            std::string namedev = Device::manager().name(d);
            if (ImGui::Selectable( namedev.c_str() )) {
                s.setDevice(namedev);
                info.reset();
                std::ostringstream oss;
                oss << s.name() << " Device " << namedev;
                Action::manager().store(oss.str());
            }
        }
        ImGui::EndCombo();
    }

}

void ImGuiVisitor::visit (NetworkSource& s)
{
    ImGuiToolkit::Icon(s.icon().x, s.icon().y);
    ImGui::SameLine(0, 10);
    ImGui::Text("Network stream");

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(IMGUI_COLOR_STREAM, 0.9f));
    ImGui::Text("%s", s.connection().c_str());
    ImGui::PopStyleColor(1);

    // network info
    ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + ImGui::GetContentRegionAvail().x IMGUI_RIGHT_ALIGN);
    s.accept(info);
    ImGui::Text("%s", info.str().c_str());
    ImGui::PopTextWrapPos();

    // icon (>) to open player
    if ( s.playable() ) {
        ImVec2 pos = ImGui::GetCursorPos();
        ImGui::SameLine(0, 0);
        ImGui::SameLine(0, 10.f + ImGui::GetContentRegionAvail().x IMGUI_RIGHT_ALIGN);
        if (ImGuiToolkit::IconButton(ICON_FA_PLAY_CIRCLE, "Open in Player"))
            UserInterface::manager().showSourceEditor(&s);
        ImGui::SetCursorPos(pos);
    }

    if ( ImGui::Button( ICON_FA_REPLY " Reconnect", ImVec2(IMGUI_RIGHT_ALIGN, 0)) )
    {
        s.setConnection(s.connection());
        info.reset();
    }
}


void ImGuiVisitor::visit (MultiFileSource& s)
{
    ImGuiToolkit::Icon(s.icon().x, s.icon().y);
    ImGui::SameLine(0, 10);
    ImGui::Text("Images sequence");
    static int64_t id = 0;

    // information text
    ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + ImGui::GetContentRegionAvail().x IMGUI_RIGHT_ALIGN);
    s.accept(info);
    ImGui::Text("%s", info.str().c_str());
    ImGui::PopTextWrapPos();

    // icon (>) to open player
    if ( s.playable() ) {
        ImVec2 pos = ImGui::GetCursorPos();
        ImGui::SameLine(0, 0);
        ImGui::SameLine(0, ImGui::GetContentRegionAvail().x IMGUI_RIGHT_ALIGN);
        if (ImGuiToolkit::IconButton(ICON_FA_PLAY_CIRCLE, "Open in Player"))
            UserInterface::manager().showSourceEditor(&s);
        ImGui::SetCursorPos(pos);
    }

    // change range
    static int _begin = -1;
    if (_begin < 0 || id != s.id())
        _begin = s.begin();
    static int _end = -1;
    if (_end < 0 || id != s.id())
        _end = s.end();
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    ImGui::DragIntRange2("Range", &_begin, &_end, 1, s.sequence().min, s.sequence().max);
    if (ImGui::IsItemDeactivatedAfterEdit()){
        s.setRange( _begin, _end );
        std::ostringstream oss;
        oss << s.name() << ": Range " << _begin << "-" << _end;
        Action::manager().store(oss.str());
        _begin = _end = -1;
    }

    // change framerate
    static int _fps = -1;
    if (_fps < 0 || id != s.id())
        _fps = s.framerate();
    ImGui::SetNextItemWidth(IMGUI_RIGHT_ALIGN);
    ImGui::SliderInt("Framerate", &_fps, 1, 30, "%d fps");
    if (ImGui::IsItemDeactivatedAfterEdit()){
        s.setFramerate(_fps);
        std::ostringstream oss;
        oss << s.name() << ": Framerate " << _fps << " fps";
        Action::manager().store(oss.str());
        _fps = -1;
    }

    // offer to open file browser at location
    std::string path = SystemToolkit::path_filename(s.sequence().location);
    std::string label = BaseToolkit::trunc_string(path, 25);
    label = BaseToolkit::transliterate(label);
    ImGuiToolkit::ButtonOpenUrl( label.c_str(), path.c_str(), ImVec2(IMGUI_RIGHT_ALIGN, 0) );
    ImGui::SameLine();
    ImGui::Text("Folder");

    if (id != s.id())
        id = s.id();
}
