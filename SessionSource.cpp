#include <glm/gtc/matrix_transform.hpp>
#include <thread>
#include <chrono>
#include <algorithm>

#include "SessionSource.h"

#include "defines.h"
#include "Log.h"
#include "FrameBuffer.h"
#include "ImageShader.h"
#include "ImageProcessingShader.h"
#include "Resource.h"
#include "Decorations.h"
#include "SearchVisitor.h"
#include "Session.h"
#include "SessionCreator.h"
#include "Mixer.h"


SessionSource::SessionSource(uint64_t id) : Source(id), failed_(false), timer_(0), paused_(false)
{
    session_ = new Session;
}

SessionSource::~SessionSource()
{
    // delete session
    if (session_)
        delete session_;
}

Session *SessionSource::detach()
{
    // remember pointer to give away
    Session *giveaway = session_;

    // work on a new session
    session_ = new Session;

    // un-ready
    ready_ = false;

    // ask to delete me
    failed_ = true;

    // lost ref to previous session: to be deleted elsewhere...
    return giveaway;
}

bool SessionSource::failed() const
{
    return failed_;
}

uint SessionSource::texture() const
{
    if (session_ && session_->frame())
        return session_->frame()->texture();
    else
        return Resource::getTextureBlack();
}

void SessionSource::setActive (bool on)
{    
    Source::setActive(on);

    // change status of session (recursive change of internal sources)
    if (session_)
        session_->setActive(active_);
}

void SessionSource::update(float dt)
{
    if (session_ == nullptr)
        return;

    // update content
    if (active_ && !paused_) {
        session_->update(dt);
        timer_ += guint64(dt * 1000.f) * GST_USECOND;
    }

    // delete a source which failed
    if (session_->failedSource() != nullptr) {
        session_->deleteSource(session_->failedSource());
        // fail session if all sources failed
        if ( session_->numSource() < 1)
            failed_ = true;
    }

    Source::update(dt);
}

void SessionSource::replay ()
{
    if (session_) {
        for( SourceList::iterator it = session_->begin(); it != session_->end(); ++it)
            (*it)->replay();
        timer_ = 0;
    }
}


SessionFileSource::SessionFileSource(uint64_t id) : SessionSource(id), path_(""), initialized_(false), wait_for_sources_(false)
{
    // specific node for transition view
    groups_[View::TRANSITION]->visible_ = false;
    groups_[View::TRANSITION]->scale_ = glm::vec3(0.1f, 0.1f, 1.f);
    groups_[View::TRANSITION]->translation_ = glm::vec3(-1.f, 0.f, 0.f);

    frames_[View::TRANSITION] = new Switch;
    Frame *frame = new Frame(Frame::ROUND, Frame::THIN, Frame::DROP);
    frame->translation_.z = 0.1;
    frame->color = glm::vec4( COLOR_DEFAULT_SOURCE, 0.95f);
    frames_[View::TRANSITION]->attach(frame);
    frame = new Frame(Frame::ROUND, Frame::LARGE, Frame::DROP);
    frame->translation_.z = 0.01;
    frame->color = glm::vec4( COLOR_TRANSITION_SOURCE, 1.f);
    frames_[View::TRANSITION]->attach(frame);
    groups_[View::TRANSITION]->attach(frames_[View::TRANSITION]);

    overlays_[View::TRANSITION] = new Group;
    overlays_[View::TRANSITION]->translation_.z = 0.1;
    overlays_[View::TRANSITION]->visible_ = false;

    Symbol *loader = new Symbol(Symbol::DOTS);
    loader->scale_ = glm::vec3(2.f, 2.f, 1.f);
    loader->update_callbacks_.push_back(new InfiniteGlowCallback);
    overlays_[View::TRANSITION]->attach(loader);
    Symbol *center = new Symbol(Symbol::CIRCLE_POINT, glm::vec3(0.f, -1.05f, 0.1f));
    overlays_[View::TRANSITION]->attach(center);
    groups_[View::TRANSITION]->attach(overlays_[View::TRANSITION]);

    // set symbol
    symbol_ = new Symbol(Symbol::SESSION, glm::vec3(0.75f, 0.75f, 0.01f));
    symbol_->scale_.y = 1.5f;

}

void SessionFileSource::load(const std::string &p, uint recursion)
{
    path_ = p;

    // delete session
    if (session_) {
        delete session_;
        session_ = nullptr;
    }

    // init session
    if ( path_.empty() ) {
        // empty session
        session_ = new Session;
        Log::Warning("Empty Session filename provided.");
    }
    else {
        // launch a thread to load the session file
        sessionLoader_ = std::async(std::launch::async, Session::load, path_, recursion);
        Log::Notify("Opening %s", p.c_str());
    }

    // will be ready after init and one frame rendered
    initialized_ = false;
    ready_ = false;
}

void SessionFileSource::init()
{
    // init is first about getting the loaded session
    if (session_ == nullptr) {
        // did the loader finish ?
        if (sessionLoader_.wait_for(std::chrono::milliseconds(4)) == std::future_status::ready) {
            session_ = sessionLoader_.get();
            if (session_ == nullptr)
                failed_ = true;
        }
    }
    else {

        if (wait_for_sources_) {

            // force update of of all sources
            active_ = true;
            touch();

            //  update to draw framebuffer
            session_->update(dt_);

            // if all sources are ready, done with initialization!
            auto unintitializedsource = std::find_if_not(session_->begin(), session_->end(), Source::isInitialized);
            if (unintitializedsource == session_->end()) {
                // done init
                wait_for_sources_ = false;
                initialized_ = true;
                Log::Info("Source Session %s loaded %d sources.", path_.c_str(), session_->numSource());
            }
        }
        else if ( !failed_ ) {

            // set resolution
            session_->setResolution( session_->config(View::RENDERING)->scale_ );

            //  update to draw framebuffer
            session_->update(dt_);

            // get the texture index from framebuffer of session, apply it to the surface
            texturesurface_->setTextureIndex( session_->frame()->texture() );

            // create Frame buffer matching size of session
            FrameBuffer *renderbuffer = new FrameBuffer( session_->frame()->resolution() );

            // set the renderbuffer of the source and attach rendering nodes
            attach(renderbuffer);

            // wait for all sources to init
            if (session_->numSource() > 0)
                wait_for_sources_ = true;
            else {
                initialized_ = true;
                Log::Info("New Session created (%d x %d).", renderbuffer->width(), renderbuffer->height());
            }
        }
    }

    if (initialized_)
    {
        // remove the loading icon
        Node *loader = overlays_[View::TRANSITION]->back();
        overlays_[View::TRANSITION]->detach(loader);
        delete loader;
        // deep update to reorder
        ++View::need_deep_update_;
    }
}

void SessionFileSource::render()
{
    if ( !initialized_ )
        init();
    else {
        // render the media player into frame buffer
        renderbuffer_->begin();
        texturesurface_->draw(glm::identity<glm::mat4>(), renderbuffer_->projection());
        renderbuffer_->end();
        ready_ = true;
    }
}

void SessionFileSource::accept(Visitor& v)
{
    Source::accept(v);
    if (!failed())
        v.visit(*this);
}


SessionGroupSource::SessionGroupSource(uint64_t id) : SessionSource(id), resolution_(glm::vec3(0.f))
{
//    // redo frame for layers view
//    frames_[View::LAYER]->clear();

//    // Groups in LAYER have an additional border
//    Group *group = new Group;
//    Frame *frame = new Frame(Frame::ROUND, Frame::THIN, Frame::PERSPECTIVE);
//    frame->translation_.z = 0.1;
//    frame->color = glm::vec4( COLOR_DEFAULT_SOURCE, 0.95f);
//    group->attach(frame);
//    Frame *persp = new Frame(Frame::GROUP, Frame::THIN, Frame::NONE);
//    persp->translation_.z = 0.1;
//    persp->color = glm::vec4( COLOR_DEFAULT_SOURCE, 0.95f);
//    group->attach(persp);
//    frames_[View::LAYER]->attach(group);

//    group = new Group;
//    frame = new Frame(Frame::ROUND, Frame::LARGE, Frame::PERSPECTIVE);
//    frame->translation_.z = 0.1;
//    frame->color = glm::vec4( COLOR_HIGHLIGHT_SOURCE, 1.f);
//    group->attach(frame);
//    persp = new Frame(Frame::GROUP, Frame::LARGE, Frame::NONE);
//    persp->translation_.z = 0.1;
//    persp->color = glm::vec4( COLOR_HIGHLIGHT_SOURCE, 1.f);
//    group->attach(persp);
//    frames_[View::LAYER]->attach(group);

    // set symbol
    symbol_ = new Symbol(Symbol::GROUP, glm::vec3(0.75f, 0.75f, 0.01f));
    symbol_->scale_.y = 1.5f;
}

void SessionGroupSource::init()
{
    if ( resolution_.x > 0.f && resolution_.y > 0.f ) {

        session_->setResolution( resolution_ );

        //  update to draw framebuffer
        session_->update( dt_ );

        // get the texture index from framebuffer of session, apply it to the surface
        texturesurface_->setTextureIndex( session_->frame()->texture() );

        // create Frame buffer matching size of session
        FrameBuffer *renderbuffer = new FrameBuffer( session_->frame()->resolution() );

        // set the renderbuffer of the source and attach rendering nodes
        attach(renderbuffer);

        // deep update to reorder
        ++View::need_deep_update_;

        // done init
        Log::Info("Source Group (%d x %d).", int(renderbuffer->resolution().x), int(renderbuffer->resolution().y) );
    }
}

bool SessionGroupSource::import(Source *source)
{
    bool ret = false;

    if ( session_ )
    {
        SourceList::iterator its = session_->addSource(source);
        if (its != session_->end())
            ret = true;
    }

    return ret;
}

void SessionGroupSource::accept(Visitor& v)
{
    Source::accept(v);
    if (!failed())
        v.visit(*this);
}

RenderSource::RenderSource(uint64_t id) : Source(id), session_(nullptr)
{
    // set symbol
    symbol_ = new Symbol(Symbol::RENDER, glm::vec3(0.75f, 0.75f, 0.01f));
    symbol_->scale_.y = 1.5f;
}

bool RenderSource::failed() const
{
    if ( renderbuffer_ != nullptr && session_ != nullptr )
        return renderbuffer_->resolution() != session_->frame()->resolution();

    return false;
}

uint RenderSource::texture() const
{
    if (session_ && session_->frame())
        return session_->frame()->texture();
    else
        return Resource::getTextureBlack(); // getTextureTransparent ?
}

void RenderSource::init()
{
    if (session_ && session_->frame() && session_->frame()->texture() != Resource::getTextureBlack()) {

        FrameBuffer *fb = session_->frame();

        // get the texture index from framebuffer of view, apply it to the surface
        texturesurface_->setTextureIndex( fb->texture() );

        // create Frame buffer matching size of output session
        FrameBuffer *renderbuffer = new FrameBuffer( fb->resolution() );

        // set the renderbuffer of the source and attach rendering nodes
        attach(renderbuffer);

        // deep update to reorder
        ++View::need_deep_update_;

        // done init
        Log::Info("Source Render linked to session (%d x %d).", int(fb->resolution().x), int(fb->resolution().y) );
    }
}


glm::vec3 RenderSource::resolution() const
{
    if (renderbuffer_ != nullptr)
        return renderbuffer_->resolution();
    else if (session_ && session_->frame())
        return session_->frame()->resolution();
    else
        return glm::vec3(0.f);
}

void RenderSource::accept(Visitor& v)
{
    Source::accept(v);
//    if (!failed())
        v.visit(*this);
}
