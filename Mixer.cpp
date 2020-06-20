#include <algorithm>
#include <thread>
#include <atomic>
#include <vector>
#include <chrono>

#include <tinyxml2.h>
#include "tinyxml2Toolkit.h"
using namespace tinyxml2;

#include "defines.h"
#include "Settings.h"
#include "Log.h"
#include "View.h"
#include "SystemToolkit.h"
//#include "GarbageVisitor.h"
#include "SessionVisitor.h"
#include "SessionCreator.h"
#include "SessionSource.h"
#include "MediaSource.h"

#include "Mixer.h"

// static semaphore to prevent multiple threads for load / save
static std::atomic<bool> sessionThreadActive_ = false;
static std::atomic<bool> sessionSwapRequested_ = false;

// static multithreaded session loading
static void loadSession(const std::string& filename, Session *session)
{
    while (sessionThreadActive_)
        std::this_thread::sleep_for( std::chrono::milliseconds(50));
    sessionThreadActive_ = true;

    // actual loading of xml file
    SessionCreator creator( session );

    if (creator.load(filename)) {
        // loaded ok
        session->setFilename(filename);
        sessionSwapRequested_ = true;

        // cosmetics load ok
        Log::Notify("Session %s loaded. %d source(s) created.", filename.c_str(), session->numSource());
    }
    else {
        // error loading
        Log::Warning("Failed to load Session file %s.", filename.c_str());
    }

    sessionThreadActive_ = false;
}

static std::atomic<bool> sessionImportRequested_ = false;
static void importSession(const std::string& filename, Session *session)
{
    while (sessionThreadActive_)
        std::this_thread::sleep_for( std::chrono::milliseconds(50));
    sessionThreadActive_ = true;

    // actual loading of xml file
    SessionCreator creator( session );

    if (creator.load(filename)) {
        // cosmetics load ok
        sessionImportRequested_ = true;

        Log::Notify("Session %s loaded. %d source(s) imported.", filename.c_str(), session->numSource());
    }
    else {
        // error loading
        Log::Warning("Failed to import Session file %s.", filename.c_str());
    }

    sessionThreadActive_ = false;
}

// static multithreaded session saving
static void saveSession(const std::string& filename, Session *session)
{
    // reset
    while (sessionThreadActive_)
        std::this_thread::sleep_for( std::chrono::milliseconds(50));
    sessionThreadActive_ = true;

    // creation of XML doc
    XMLDocument xmlDoc;

    XMLElement *version = xmlDoc.NewElement(APP_NAME);
    version->SetAttribute("major", XML_VERSION_MAJOR);
    version->SetAttribute("minor", XML_VERSION_MINOR);
    xmlDoc.InsertEndChild(version);

    // 1. list of sources
    XMLElement *sessionNode = xmlDoc.NewElement("Session");
    xmlDoc.InsertEndChild(sessionNode);
    SourceList::iterator iter;
    for (iter = session->begin(); iter != session->end(); iter++)
    {
        SessionVisitor sv(&xmlDoc, sessionNode);
        // source visitor
        (*iter)->accept(sv);
    }

    // 2. config of views
    XMLElement *views = xmlDoc.NewElement("Views");
    xmlDoc.InsertEndChild(views);
    {
        XMLElement *mixing = xmlDoc.NewElement( "Mixing" );
        mixing->InsertEndChild( SessionVisitor::NodeToXML(*session->config(View::MIXING), &xmlDoc));
        views->InsertEndChild(mixing);

        XMLElement *geometry = xmlDoc.NewElement( "Geometry" );
        geometry->InsertEndChild( SessionVisitor::NodeToXML(*session->config(View::GEOMETRY), &xmlDoc));
        views->InsertEndChild(geometry);

        XMLElement *layer = xmlDoc.NewElement( "Layer" );
        layer->InsertEndChild( SessionVisitor::NodeToXML(*session->config(View::LAYER), &xmlDoc));
        views->InsertEndChild(layer);

        XMLElement *render = xmlDoc.NewElement( "Rendering" );
        render->InsertEndChild( SessionVisitor::NodeToXML(*session->config(View::RENDERING), &xmlDoc));
        views->InsertEndChild(render);
    }

    // save file to disk
    if ( XMLSaveDoc(&xmlDoc, filename) ) {
        // all ok
        session->setFilename(filename);
        // cosmetics saved ok
        Settings::application.recentSessions.push(filename);
        Log::Notify("Session %s saved.", filename.c_str());
    }
    else {
        // error loading
        Log::Warning("Failed to save Session file %s.", filename.c_str());
    }

    sessionThreadActive_ = false;
}

Mixer::Mixer() : session_(nullptr), back_session_(nullptr), current_view_(nullptr)
{
    // unsused initial empty session
    session_ = new Session;
    current_source_ = session_->end();
    current_source_index_ = -1;

    // auto load if Settings ask to
    if ( Settings::application.recentSessions.load_at_start &&
         Settings::application.recentSessions.filenames.size() > 0 )
        open( Settings::application.recentSessions.filenames.front() );
    else
        // initializes with a new empty session
        clear();

    // this initializes with the current view
    setView( (View::Mode) Settings::application.current_view );
}

void Mixer::update()
{
    // change session when threaded loading is finished
    if (sessionSwapRequested_) {
        sessionSwapRequested_ = false;
        // successfully loading
        if ( back_session_ ) {
            // swap front and back sessions
            swap();
            // set session filename
            Rendering::manager().mainWindow().setTitle(session_->filename());
            Settings::application.recentSessions.push(session_->filename());
        }
    }

    if (sessionImportRequested_) {
        sessionImportRequested_ = false;
        merge(back_session_);
        back_session_ = nullptr;
    }

    // compute dt
    if (update_time_ == GST_CLOCK_TIME_NONE)
        update_time_ = gst_util_get_timestamp ();
    gint64 current_time = gst_util_get_timestamp ();
    float dt = static_cast<float>( GST_TIME_AS_MSECONDS(current_time - update_time_) ) * 0.001f;
    update_time_ = current_time;

    // insert source candidate for this session
    if (candidate_sources_.size()>0) {
        insertSource(candidate_sources_.front());
        candidate_sources_.pop_front();
    }

    // update session and associated sources
    session_->update(dt);

    // delete failed sources (one by one)
    if (session()->failedSource() != nullptr)
        deleteSource(session()->failedSource());

    // update views
    mixing_.update(dt);
    geometry_.update(dt);
    layer_.update(dt);

    // optimize the reordering in depth for views;
    // deep updates shall be performed only 1 frame
    View::need_deep_update_ = false;
}

void Mixer::draw()
{
    // draw the current view in the window
    current_view_->draw();
}

// manangement of sources
Source * Mixer::createSourceFile(std::string path)
{
    // ready to create a source
    Source *s = nullptr;

    // sanity check
    if ( SystemToolkit::file_exists( path ) ) {

        // test type of file by extension
        std::string ext = SystemToolkit::extension_filename(path);
        if ( ext == "mix" )
        {
            // create a session source
            SessionSource *ss = new SessionSource();
            ss->load(path);
            s = ss;
        }
        else {
            // (try to) create media source by default
            MediaSource *ms = new MediaSource;
            ms->setPath(path);
            s = ms;
        }

        // remember in recent media
        Settings::application.recentImport.push(path);
        Settings::application.recentImport.path = SystemToolkit::path_filename(path);

        // propose a new name based on uri
        renameSource(s, SystemToolkit::base_filename(path));

    }
    else
        Log::Notify("File %s does not exist.", path.c_str());

    return s;
}

Source * Mixer::createSourceRender()
{
    // ready to create a source
    RenderSource *s = new RenderSource(session_);

    // propose a new name based on session name
    renameSource(s, SystemToolkit::base_filename(session_->filename()));

    return s;
}

Source * Mixer::createSourceClone(std::string namesource)
{
    // ready to create a source
    Source *s = nullptr;

    // origin to clone is either the given name or the current
    SourceList::iterator origin = session_->end();
    if ( !namesource.empty() )
        origin =session_->find(namesource);
    else if (current_source_ != session_->end())
        origin = current_source_;

    // have an origin, can clone it
    if (origin != session_->end()) {

        // create a source
        s = (*origin)->clone();

        // get new name
        renameSource(s, (*origin)->name());
    }

    return s;
}

void Mixer::addSource(Source *s)
{
    if (s != nullptr)
        candidate_sources_.push_back(s);
}

void Mixer::insertSource(Source *s, bool makecurrent)
{
    if ( s != nullptr )
    {
        // Add source to Session
        SourceList::iterator sit = session_->addSource(s);

        // set a default depth to the new source
        layer_.setDepth(s);
        // set a default alpha to the new source
        mixing_.setAlpha(s);

        // add sources Nodes to all views
        mixing_.scene.ws()->attach(s->group(View::MIXING));
        geometry_.scene.ws()->attach(s->group(View::GEOMETRY));
        layer_.scene.ws()->attach(s->group(View::LAYER));

        if (makecurrent) {
            // switch to Mixing view to show source created
            setView(View::MIXING);
            current_view_->update(0);
            current_view_->centerSource(s);

            // set this new source as current
            setCurrentSource( sit );
        }
    }
}

void Mixer::deleteSource(Source *s)
{
    if ( s != nullptr )
    {
        // in case it was the current source...
        unsetCurrentSource();

        // in case it was selected..
        selection().remove(s);

        // keep name for log
        std::string name = s->name();

        // remove source Nodes from all views
        mixing_.scene.ws()->detatch( s->group(View::MIXING) );
        geometry_.scene.ws()->detatch( s->group(View::GEOMETRY) );
        layer_.scene.ws()->detatch( s->group(View::LAYER) );

        // delete source
        session_->deleteSource(s);

        // log
        Log::Notify("Source %s deleted.", name.c_str());
    }
}

void Mixer::renameSource(Source *s, const std::string &newname)
{
    if ( s != nullptr )
    {
        // tentative new name
        std::string tentativename = newname;

        // refuse to rename to an empty name
        if ( newname.empty() )
            tentativename = "source";

        // trivial case : same name as current
        if ( tentativename == s->name() )
            return;

        // search for a source of the name 'tentativename'
        std::string basename = tentativename;
        int count = 1;
        while ( std::find_if(session_->begin(), session_->end(), Source::hasName(tentativename)) != session_->end() ) {
            tentativename = basename + std::to_string(++count);
        }

        // ok to rename
        s->setName(tentativename);
    }
}

void Mixer::setCurrentSource(SourceList::iterator it)
{
    // nothing to do if already current
    if ( current_source_ == it )
        return;

    // clear current (even if it is invalid)
    unsetCurrentSource();

    // change current if it is valid
    if ( it != session_->end() ) {
        current_source_ = it;
        current_source_index_ = session_->index(current_source_);

        // set selection for this only source if not already part of a selection
        if (!selection().contains(*current_source_))
            selection().set(*current_source_);

        // show status as current
        (*current_source_)->setMode(Source::CURRENT);
        Log::Info("setCurrentSource");
    }

}

Source * Mixer::findSource (Node *node)
{
    SourceList::iterator it = session_->find(node);
    if (it != session_->end())
        return *it;
    return nullptr;
}

Source * Mixer::findSource (std::string namesource)
{
    SourceList::iterator it = session_->find(namesource);
    if (it != session_->end())
        return *it;
    return nullptr;
}

void Mixer::setCurrentSource(Node *node)
{
    setCurrentSource( session_->find(node) );
}

void Mixer::setCurrentSource(std::string namesource)
{
    setCurrentSource( session_->find(namesource) );
}

void Mixer::setCurrentSource(Source *s)
{
    setCurrentSource( session_->find(s) );
}

void Mixer::setCurrentSource(int index)
{
    setCurrentSource( session_->find(index) );
}

void Mixer::setCurrentNext()
{
    SourceList::iterator it = current_source_;

    it++;

    if (it == session_->end())  {
        it = session_->begin();
    }

    setCurrentSource( it );
}

void Mixer::unsetCurrentSource()
{
    Log::Info("unsetCurrentSource");

    // discard overlay for previously current source
    if ( current_source_ != session_->end() ) {

        // current source is part of a selection, just change status
        if (selection().size() > 1) {
            (*current_source_)->setMode(Source::SELECTED);
        }
        // current source is the only selected source, unselect too
        else
        {
            // remove from selection
            selection().remove( *current_source_ );
        }
    }

    // deselect current source
    current_source_ = session_->end();
    current_source_index_ = -1;
}

int Mixer::indexCurrentSource()
{
    return current_source_index_;
}

Source *Mixer::currentSource()
{
    if ( current_source_ == session_->end() )
        return nullptr;
    else {
        return *(current_source_);
    }
}

// management of view
void Mixer::setView(View::Mode m)
{
    switch (m) {
    case View::GEOMETRY:
        current_view_ = &geometry_;
        break;
    case View::LAYER:
        current_view_ = &layer_;
        break;
    case View::MIXING:
    default:
        current_view_ = &mixing_;
        break;
    }

    Settings::application.current_view = (int) m;
}

View *Mixer::view(View::Mode m)
{
    switch (m) {
    case View::GEOMETRY:
        return &geometry_;
    case View::LAYER:
        return &layer_;
    case View::MIXING:
        return &mixing_;
    default:
        return current_view_;
    }
}

void Mixer::save()
{
    if (!session_->filename().empty())
        saveas(session_->filename());
}

void Mixer::saveas(const std::string& filename)
{
    // optional copy of views config
    session_->config(View::MIXING)->copyTransform( mixing_.scene.root() );
    session_->config(View::GEOMETRY)->copyTransform( geometry_.scene.root() );
    session_->config(View::LAYER)->copyTransform( layer_.scene.root() );

    // launch a thread to save the session
    std::thread (saveSession, filename, session_).detach();
}

void Mixer::open(const std::string& filename)
{
    if (back_session_)
        delete back_session_;

    // create empty session
    back_session_ = new Session;

    // launch a thread to load the session into back_session
    std::thread (loadSession, filename, back_session_).detach();
}

void Mixer::import(const std::string& filename)
{
    if (back_session_)
        delete back_session_;

    // create empty session
    back_session_ = new Session;

    // launch a thread to load the session into back_session
    std::thread (importSession, filename, back_session_).detach();
}

void Mixer::merge(Session *session)
{
    if (session) {

        for ( Source *s = session->popSource(); s != nullptr; s = session->popSource())
            insertSource(s, false);

        delete session;
    }
}

void Mixer::swap()
{
    if (!back_session_)
        return;

    if (session_) {
        // clear selection
        selection().clear();
        // detatch current session's nodes from views
        for (auto source_iter = session_->begin(); source_iter != session_->end(); source_iter++)
        {
            mixing_.scene.ws()->detatch( (*source_iter)->group(View::MIXING) );
            geometry_.scene.ws()->detatch( (*source_iter)->group(View::GEOMETRY) );
            layer_.scene.ws()->detatch( (*source_iter)->group(View::LAYER) );
        }
    }

    // swap back and front
    Session *tmp = session_;
    session_ = back_session_;
    back_session_ = tmp;

    // attach new session's nodes to views
    for (auto source_iter = session_->begin(); source_iter != session_->end(); source_iter++)
    {
        mixing_.scene.ws()->attach( (*source_iter)->group(View::MIXING) );
        geometry_.scene.ws()->attach( (*source_iter)->group(View::GEOMETRY) );
        layer_.scene.ws()->attach( (*source_iter)->group(View::LAYER) );
    }

    // optional copy of views config
    mixing_.scene.root()->copyTransform( session_->config(View::MIXING) );
    geometry_.scene.root()->copyTransform( session_->config(View::GEOMETRY) );
    layer_.scene.root()->copyTransform( session_->config(View::LAYER) );

    // set resolution
    session_->setResolution( session_->config(View::RENDERING)->scale_ );

    // request reordering in depth for views
    View::need_deep_update_ = true;

    // no current source
    current_source_ = session_->end();
    current_source_index_ = -1;

    // reset timer
    update_time_ = GST_CLOCK_TIME_NONE;

    // delete back
    delete back_session_;
    back_session_ = nullptr;
}

void Mixer::clear()
{
    // delete previous back session if needed
    if (back_session_)
        delete back_session_;

    // create empty session
    back_session_ = new Session;   

    // swap current with empty
    sessionSwapRequested_ = true;

    Log::Info("New session ready.");
}

void Mixer::set(Session *s)
{
    if ( s == nullptr )
        return;

    // delete previous back session if needed
    if (back_session_)
        delete back_session_;

    // set to new given session
    back_session_ = s;

    // swap current with given session
    sessionSwapRequested_ = true;
}
