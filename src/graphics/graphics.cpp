/* eos - A reimplementation of BioWare's Aurora engine
 * Copyright (c) 2010 Sven Hesse (DrMcCoy), Matthew Hoops (clone2727)
 *
 * The Infinity, Aurora, Odyssey and Eclipse engines, Copyright (c) BioWare corp.
 * The Electron engine, Copyright (c) Obsidian Entertainment and BioWare corp.
 *
 * This file is part of eos and is distributed under the terms of
 * the GNU General Public Licence. See COPYING for more informations.
 */

/** @file graphics/graphics.cpp
 *  The global graphics manager.
 */

#include <cmath>

#include "common/util.h"
#include "common/error.h"

#include "graphics/graphics.h"
#include "graphics/fpscounter.h"
#include "graphics/renderable.h"
#include "graphics/cube.h"

#include "graphics/images/decoder.h"

DECLARE_SINGLETON(Graphics::GraphicsManager)

namespace Graphics {

GraphicsManager::GraphicsManager() {
	_ready    = false;
	_initedGL = false;

	_screen = 0;

	_fpsCounter = new FPSCounter(3);
}

GraphicsManager::~GraphicsManager() {
	delete _fpsCounter;
}

void GraphicsManager::init() {
	uint32 sdlInitFlags = SDL_INIT_AUDIO | SDL_INIT_TIMER | SDL_INIT_VIDEO;

	// TODO: Is this actually needed on any systems? It seems to make MacOS X fail to
	//       receive any events, too.
/*
// Might be needed on unixoid OS, but it crashes Windows. Nice.
#ifndef WIN32
	sdlInitFlags |= SDL_INIT_EVENTTHREAD;
#endif
*/

	if (SDL_Init(sdlInitFlags) < 0)
		throw Common::Exception("Failed to initialize SDL: %s", SDL_GetError());

	_ready = true;
}

void GraphicsManager::deinit() {
	if (!_ready)
		return;

	clearRenderQueue();

	SDL_Quit();

	_ready    = false;
	_initedGL = false;
}

bool GraphicsManager::ready() const {
	return _ready;
}

uint32 GraphicsManager::getFPS() const {
	return _fpsCounter->getFPS();
}

void GraphicsManager::initSize(int width, int height, bool fullscreen) {
	int bpp = SDL_GetVideoInfo()->vfmt->BitsPerPixel;
	if ((bpp != 24) && (bpp != 32))
		throw Common::Exception("Need 24 or 32 bits per pixel");

	uint32 flags = SDL_OPENGL;

	if (fullscreen)
		flags |= SDL_FULLSCREEN;

	if (setupSDLGL(width, height, bpp, flags))
		return;

	// Could not initialize OpenGL, trying a different bpp value

	bpp = (bpp == 32) ? 24 : 32;

	if (!setupSDLGL(width, height, bpp, flags))
		// Still couldn't initialize OpenGL, erroring out
		throw Common::Exception("Failed setting the video mode: %s", SDL_GetError());

	// Initialize glew, for the extension entry points
	GLenum glewErr = glewInit();
	if (glewErr != GLEW_OK)
		throw Common::Exception("Failed initializing glew: %s", glewGetErrorString(glewErr));
}

bool GraphicsManager::setupSDLGL(int width, int height, int bpp, uint32 flags) {
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE    ,   8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE  ,   8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE   ,   8);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE  , bpp);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,   1);

	_screen = SDL_SetVideoMode(width, height, bpp, flags);
	if (!_screen)
		return false;

	return true;
}

void GraphicsManager::setWindowTitle(const std::string &title) {
	SDL_WM_SetCaption(title.c_str(), 0);
}

void GraphicsManager::setupScene() {
	if (!_screen)
		throw Common::Exception("No screen initialized");

	glClearColor( 0, 0, 0, 0 );
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glViewport(0, 0, _screen->w, _screen->h);

	perspective(60.0, ((GLdouble) _screen->w) / ((GLdouble) _screen->h), 1.0, 1000.0);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glShadeModel(GL_SMOOTH);
	glClearColor(0.0, 0.0, 0.0, 0.5);
	glClearDepth(1.0);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	_initedGL = true;
}

void GraphicsManager::clearRenderQueue() {
	Common::StackLock lock(_queueMutex);

	// Notify all objects in the queue that they have been kicked out
	for (RenderQueue::iterator it = _renderQueue.begin(); it != _renderQueue.end(); ++it)
		(*it)->kickedOutOfRenderQueue();

	// Clear the queue
	_renderQueue.clear();
}

GraphicsManager::RenderQueueRef GraphicsManager::addToRenderQueue(Renderable &renderable) {
	Common::StackLock lock(_queueMutex);

	_renderQueue.push_back(&renderable);

	return --_renderQueue.end();
}

void GraphicsManager::removeFromRenderQueue(RenderQueueRef &ref) {
	Common::StackLock lock(_queueMutex);

	_renderQueue.erase(ref);
}

void GraphicsManager::renderScene() {
	Common::StackLock lock(_queueMutex);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	for (RenderQueue::iterator it = _renderQueue.begin(); it != _renderQueue.end(); ++it) {
		glPushMatrix();

		(*it)->render();

		glPopMatrix();
	}

	SDL_GL_SwapBuffers();

	_fpsCounter->finishedFrame();
}

void GraphicsManager::perspective(GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar) {
	// Basically implements gluPerspective

	GLdouble halfHeight = zNear * std::tan(fovy * 0.5 * ((PI * 2) / 360));
	GLdouble halfWidth  = halfHeight * aspect;

	glFrustum(-halfWidth, halfWidth, -halfHeight, halfHeight, zNear, zFar);
}

void GraphicsManager::reloadTextures() {
	Common::StackLock lock(_queueMutex);

	for (RenderQueue::iterator it = _renderQueue.begin(); it != _renderQueue.end(); ++it)
		(*it)->reloadTextures();
}

void GraphicsManager::toggleFullScreen() {
	setFullScreen(!_fullScreen);
}

void GraphicsManager::setFullScreen(bool fullScreen) {
	if (_fullScreen == fullScreen)
		// Nothing to do
		return;

	// Save the flags
	uint32 flags = _screen->flags;

	// Now try to change modes
	_screen = SDL_SetVideoMode(0, 0, 0, flags ^ SDL_FULLSCREEN);

	// If we could not go full screen, revert back.
	if (!_screen)
		_screen = SDL_SetVideoMode(0, 0, 0, flags);
	else
		_fullScreen = fullScreen;

	// There's no reason how this could possibly fail, but ok...
	if (!_screen)
		throw Common::Exception("Failed going to fullscreen and then failed reverting.");

	// Reintroduce glew to the surface
	GLenum glewErr = glewInit();
	if (glewErr != GLEW_OK)
		throw Common::Exception("Failed initializing glew: %s", glewGetErrorString(glewErr));

	// Reintroduce OpenGL to the surface
	setupScene();

	// Reload the objects' textures.
	reloadTextures();
}

void GraphicsManager::toggleMouseGrab() {
	// Same as ScummVM's OSystem_SDL::toggleMouseGrab()
	if (SDL_WM_GrabInput(SDL_GRAB_QUERY) == SDL_GRAB_OFF)
		SDL_WM_GrabInput(SDL_GRAB_ON);
	else
		SDL_WM_GrabInput(SDL_GRAB_OFF);
}

void GraphicsManager::changeSize(int width, int height) {
	// Save properties
	uint32 flags     = _screen->flags;
	int    bpp       = _screen->format->BitsPerPixel;
	int    oldWidth  = _screen->w;
	int    oldHeight = _screen->h;

	// Now try to change modes
	_screen = SDL_SetVideoMode(width, height, bpp, flags);

	if (!_screen) {
		// Could not change mode, revert back.
		_screen = SDL_SetVideoMode(oldWidth, oldHeight, bpp, flags);
	}

	// There's no reason how this could possibly fail, but ok...
	if (!_screen)
		throw Common::Exception("Failed going to fullscreen and then failed reverting.");

	// Reintroduce glew to the surface
	GLenum glewErr = glewInit();
	if (glewErr != GLEW_OK)
		throw Common::Exception("Failed initializing glew: %s", glewGetErrorString(glewErr));

	// Reintroduce OpenGL to the surface
	setupScene();

	// Reload the objects' textures.
	reloadTextures();
}

void GraphicsManager::createTextures(GLsizei n, TextureID *ids) {
	glGenTextures(n, ids);
}

void GraphicsManager::destroyTextures(GLsizei n, const TextureID *ids) {
	glDeleteTextures(n, ids);
}

void GraphicsManager::loadTexture(TextureID id, const byte *data, int width, int height, PixelFormat format) {
	glBindTexture(GL_TEXTURE_2D, id);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	GLint err = gluBuild2DMipmaps(GL_TEXTURE_2D, getBytesPerPixel(format),
			width, height,format, GL_UNSIGNED_BYTE, data);

	if (err != 0)
		throw Common::Exception("Failed loading texture data: %s", gluErrorString(err));
}

void GraphicsManager::loadTexture(TextureID id, const ImageDecoder *image) {
	loadTexture(id, image->getData(), image->getWidth(), image->getHeight(), image->getFormat());
}

} // End of namespace Graphics
