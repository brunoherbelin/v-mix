#include "Settings.h"

#include <iostream>
using namespace std;

#include <tinyxml2.h>
#include "tinyxml2Toolkit.h"
using namespace tinyxml2;


Settings::Application Settings::application;
string filename = string("./") + APP_NAME + ".xml";

void Settings::Save()
{
	XMLDocument xmlDoc;
	string s;
 
    XMLDeclaration *pDec = xmlDoc.NewDeclaration();
    xmlDoc.InsertFirstChild(pDec);

    XMLElement *pRoot = xmlDoc.NewElement(application.name.c_str());
    xmlDoc.InsertEndChild(pRoot);

	s="Settings for "+application.name;
    XMLComment *pComment = xmlDoc.NewComment(s.c_str());
    pRoot->InsertEndChild(pComment);

	// block: windows
	{
		XMLElement *windowsNode = xmlDoc.NewElement( "Windows" );  

        vector<Settings::WindowConfig>::iterator iter;
		for (iter=application.windows.begin(); iter != application.windows.end(); iter++)
		{
            const Settings::WindowConfig& w=*iter;

			XMLElement *window = xmlDoc.NewElement( "Window" );            
			window->SetAttribute("name", w.name.c_str());
			window->SetAttribute("x", w.x);
			window->SetAttribute("y", w.y);
			window->SetAttribute("w", w.w);
			window->SetAttribute("h", w.h);
			window->SetAttribute("f", w.fullscreen);
            windowsNode->InsertEndChild(window);
		}

        pRoot->InsertEndChild(windowsNode);
	}

    XMLElement *applicationNode = xmlDoc.NewElement( "Application" );
    applicationNode->SetAttribute("scale", application.scale);
    applicationNode->SetAttribute("accent_color", application.accent_color);
    applicationNode->SetAttribute("preview", application.preview);
    applicationNode->SetAttribute("media_player", application.media_player);
    applicationNode->SetAttribute("shader_editor", application.shader_editor);
    pRoot->InsertEndChild(applicationNode);

    // block: views
    {
        XMLElement *viewsNode = xmlDoc.NewElement( "Views" );
        viewsNode->SetAttribute("current", application.current_view);

        map<int, Settings::ViewConfig>::iterator iter;
        for (iter=application.views.begin(); iter != application.views.end(); iter++)
        {
            const Settings::ViewConfig& v = iter->second;

            XMLElement *view = xmlDoc.NewElement( "View" );
            view->SetAttribute("name", v.name.c_str());
            view->SetAttribute("id", iter->first);

            XMLElement *scale = xmlDoc.NewElement("scale");
            scale->InsertEndChild( XMLElementFromGLM(&xmlDoc, v.scale) );
            view->InsertEndChild(scale);
            XMLElement *translation = xmlDoc.NewElement("translation");
            translation->InsertEndChild( XMLElementFromGLM(&xmlDoc, v.translation) );
            view->InsertEndChild(translation);

            viewsNode->InsertEndChild(view);
        }

        pRoot->InsertEndChild(viewsNode);
    }

    XMLError eResult = xmlDoc.SaveFile(filename.c_str());
    XMLCheckResult(eResult);
}

void Settings::Load()
{
    XMLDocument xmlDoc;
    XMLError eResult = xmlDoc.LoadFile(filename.c_str());

	// do not warn if non existing file
    if (eResult == XML_ERROR_FILE_NOT_FOUND)
        return;
	// warn on other errors
    XMLCheckResult(eResult);

    XMLElement *pRoot = xmlDoc.FirstChildElement(application.name.c_str());
    if (pRoot == nullptr) return;

    if (application.name.compare( string( pRoot->Value() ) ) != 0 ) 
        // different root name
        return;

	// block: windows
	{
		application.windows.clear(); // trash existing list

        XMLElement * pElement = pRoot->FirstChildElement("Windows");
        if (pElement == nullptr) return;

        XMLElement* windowNode = pElement->FirstChildElement("Window");
        for( ; windowNode ; windowNode=windowNode->NextSiblingElement())
        {
            const char *pName = windowNode->Attribute("name");
            Settings::WindowConfig w(pName);
			
            windowNode->QueryIntAttribute("x", &w.x); // If this fails, original value is left as-is
            windowNode->QueryIntAttribute("y", &w.y);
            windowNode->QueryIntAttribute("w", &w.w);
            windowNode->QueryIntAttribute("h", &w.h);
            windowNode->QueryBoolAttribute("f", &w.fullscreen);

			application.windows.push_back(w);
		}
	}

	XMLElement * pElement = pRoot->FirstChildElement("Application");
	if (pElement == nullptr) return;
	pElement->QueryFloatAttribute("scale", &application.scale);
    pElement->QueryIntAttribute("accent_color", &application.accent_color);
    pElement->QueryBoolAttribute("preview", &application.preview);
    pElement->QueryBoolAttribute("media_player", &application.media_player);
    pElement->QueryBoolAttribute("shader_editor", &application.shader_editor);

    // block: views
    {
        application.views.clear(); // trash existing list
        XMLElement * pElement = pRoot->FirstChildElement("Views");
        if (pElement == nullptr) return;

        pElement->QueryIntAttribute("current", &application.current_view);

        XMLElement* viewNode = pElement->FirstChildElement("View");
        for( ; viewNode ; viewNode=viewNode->NextSiblingElement())
        {
            int id = 0;
            viewNode->QueryIntAttribute("id", &id);
            application.views[id].name = viewNode->Attribute("name");

            XMLElement* scaleNode = viewNode->FirstChildElement("scale");
            tinyxml2::XMLElementToGLM( scaleNode->FirstChildElement("vec3"),
                                       application.views[id].scale);

            XMLElement* translationNode = viewNode->FirstChildElement("translation");
            tinyxml2::XMLElementToGLM( translationNode->FirstChildElement("vec3"),
                                       application.views[id].translation);

        }
    }

}


void Settings::Check()
{
	Settings::Save();

    XMLDocument xmlDoc;
    XMLError eResult = xmlDoc.LoadFile(filename.c_str());
    XMLCheckResult(eResult);

	xmlDoc.Print();
}
