/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-FileContributor: grxtor
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (C) 2026 Project Tick
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "SkinPreviewSurface.h"

#include <QDebug>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QProcessEnvironment>
#include <QSurfaceFormat>
#include <QVector3D>
#include <QWheelEvent>
#include <QtMath>

#include <cmath>

#include "BuildConfig.h"
#include "minecraft/skins/SkinEntry.h"
#include "rainbow.h"
#include "ui/dialogs/skins/render/CubeMesh.h"
#include "ui/dialogs/skins/render/PlayerScene.h"

namespace skinrender
{
	namespace
	{
		/* Shaders live in the core resource bundle; the model one applies the
		 * matrices, the background one passes clip-space vertices straight
		 * through, and both share the fragment shader. */
		const char* const kModelVertexShader =
			":/shaders/vshader_skin_model.glsl";
		const char* const kBackgroundVertexShader =
			":/shaders/vshader_skin_background.glsl";
		const char* const kFragmentShader = ":/shaders/fshader.glsl";

		/* Camera. The model spans roughly y = -24 to y = +8, and the camera
		 * targets its chest, so these frame it head to toe at the default
		 * distance. */
		constexpr float kInitialDistance = 48.0f;
		constexpr float kMinimumDistance = 16.0f;
		constexpr float kOrbitDegreesPerPixel = 0.5f;
		constexpr float kZoomPerWheelUnit = 0.01f;
		const QVector3D kCameraTarget(0, -8, 0);
		constexpr qreal kNearPlane = 15.0;
		constexpr qreal kFieldOfView = 45.0;

		/* A colour that reads as different from `colour` without leaving the
		 * theme: nudge dark colours lighter and light colours darker. The
		 * asymmetry is deliberate -- the eye tolerates far less lightening of
		 * a dark surface than darkening of a light one. */
		QColor contrastingColour(const QColor& colour)
		{
			if (Rainbow::luma(colour) < 0.5) {
				return Rainbow::lighten(colour, 0.05);
			}
			return Rainbow::darken(colour, 0.2);
		}

		/* The chequerboard behind the model, in two shades of the dialog's
		 * own background colour. */
		QImage chequerboard(int width, int height, int tileSize,
							const QColor& base)
		{
			QImage image(width, height, QImage::Format_RGB888);

			const bool darkBase = Rainbow::luma(base) < 0.5;
			const qreal contrast = darkBase ? 0.05 : 0.45;
			const auto shade = [darkBase, contrast](const QColor& colour) {
				return darkBase ? Rainbow::lighten(colour, contrast, 1.0)
								: Rainbow::darken(colour, contrast, 1.0);
			};

			const QColor light = shade(base);
			const QColor dark = shade(contrastingColour(base));

			for (int y = 0; y < height; ++y) {
				for (int x = 0; x < width; ++x) {
					const bool onLightTile =
						((x / tileSize) + (y / tileSize)) % 2 == 0;
					image.setPixelColor(x, y, onLightTile ? light : dark);
				}
			}
			return image;
		}
	} // namespace

	SkinPreviewSurface::SkinPreviewSurface(SkinPreviewSource* source,
										   QColor background)
		: QOpenGLWindow(), QOpenGLFunctions(), m_distance(kInitialDistance),
		  m_background(background), m_source(source)
	{
		QSurfaceFormat format = QSurfaceFormat::defaultFormat();
		/* Without an explicit request the default format may come back with
		 * no depth buffer at all, and the model then draws in whatever order
		 * the boxes happen to be submitted. */
		format.setDepthBufferSize(24);
		setFormat(format);
	}

	SkinPreviewSurface::~SkinPreviewSurface()
	{
		/* Every one of these owns a GL object, so the context has to be
		 * current while they go away. They are also created in
		 * initializeGL(), not in the constructor, so a surface that was never
		 * shown has nothing to release -- hence the null checks. */
		makeCurrent();

		delete m_scene;
		m_scene = nullptr;

		delete m_backgroundQuad;
		m_backgroundQuad = nullptr;

		if (m_backgroundTexture) {
			if (m_backgroundTexture->isCreated()) {
				m_backgroundTexture->destroy();
			}
			delete m_backgroundTexture;
			m_backgroundTexture = nullptr;
		}

		for (QOpenGLShaderProgram** program : {&m_modelProgram,
											   &m_backgroundProgram}) {
			if (!*program) {
				continue;
			}
			if ((*program)->isLinked()) {
				(*program)->release();
			}
			(*program)->removeAllShaders();
			delete *program;
			*program = nullptr;
		}

		doneCurrent();
	}

	bool SkinPreviewSurface::isAvailable()
	{
		/* Escape hatch for machines where creating a context succeeds but
		 * then wedges the driver. Checked before touching GL at all, so it
		 * works even when probing itself is what crashes. */
		const QString disableVariable =
			QStringLiteral("%1_DISABLE_GLVULKAN").arg(BuildConfig.KITE_ENVNAME);
		if (!QProcessEnvironment::systemEnvironment()
				 .value(disableVariable)
				 .isEmpty()) {
			qDebug() << "3D skin preview disabled by" << disableVariable;
			return false;
		}

		QOpenGLContext probe;
		return probe.create();
	}

	void SkinPreviewSurface::loadShaders()
	{
		const auto build = [this](const char* vertexPath) {
			auto* program = new QOpenGLShaderProgram(this);
			if (!program->addCacheableShaderFromSourceFile(
					QOpenGLShader::Vertex, QLatin1String(vertexPath))) {
				qCritical() << "Skin preview vertex shader failed:"
							<< program->log();
				delete program;
				return static_cast<QOpenGLShaderProgram*>(nullptr);
			}
			if (!program->addCacheableShaderFromSourceFile(
					QOpenGLShader::Fragment, QLatin1String(kFragmentShader))) {
				qCritical() << "Skin preview fragment shader failed:"
							<< program->log();
				delete program;
				return static_cast<QOpenGLShaderProgram*>(nullptr);
			}
			if (!program->link()) {
				qCritical() << "Skin preview shader link failed:"
							<< program->log();
				delete program;
				return static_cast<QOpenGLShaderProgram*>(nullptr);
			}
			return program;
		};

		m_modelProgram = build(kModelVertexShader);
		m_backgroundProgram = build(kBackgroundVertexShader);
	}

	void SkinPreviewSurface::initializeGL()
	{
		initializeOpenGLFunctions();
		glClearColor(0, 0, 1, 1);

		loadShaders();
		buildBackgroundTexture(32, 32, 1);

		/* The dialog has finished building by now, so asking it what to show
		 * gives the actual current selection rather than an empty model that
		 * would have to be replaced immediately. */
		QImage skinTexture;
		QImage capeTexture;
		bool slim = false;
		if (m_source) {
			if (const SkinEntry* skin = m_source->previewSkin()) {
				skinTexture = skin->texture();
				slim = skin->arms() == SkinEntry::Arms::Slim;
				capeTexture = m_source->previewCape(skin->capeId());
			}
		}

		m_scene = new PlayerScene(skinTexture, slim, capeTexture);
		m_backgroundQuad = CubeMesh::plane();
	}

	void SkinPreviewSurface::resizeGL(int w, int h)
	{
		const qreal aspect = qreal(w) / qreal(h ? h : 1);

		const qreal halfFovRadians = qDegreesToRadians(kFieldOfView / 2.0);
		const qreal sine = std::sin(halfFovRadians);
		if (sine == 0) {
			return;
		}
		const qreal cotangent = std::cos(halfFovRadians) / sine;

		/* Reverse-Z infinite perspective projection: depth is mapped so that
		 * precision is concentrated near the camera, and there is no far
		 * plane to clip the model against when zoomed out.
		 *
		 * The vertex shader flips the result back into the conventional
		 * [0, w] range, because the GL 2.0 context this runs in cannot be
		 * told to clear depth to 0. */
		m_projection.setToIdentity();
		m_projection(0, 0) = cotangent / aspect;
		m_projection(1, 1) = cotangent;
		m_projection(2, 2) = 0.0;
		m_projection(3, 2) = -1.0;
		m_projection(2, 3) = kNearPlane;
		m_projection(3, 3) = 0.0;
	}

	void SkinPreviewSurface::paintGL()
	{
		/* Fractional display scaling gives a window whose device pixel size
		 * is not what GL defaulted the viewport to, and the model ends up
		 * drawn into a corner. Setting it explicitly costs nothing. */
		const qreal ratio = devicePixelRatio();
		if (ratio != 1.0) {
			const QSize scaled = size() * ratio;
			glViewport(0, 0, scaled.width(), scaled.height());
		}

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LESS);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		if (m_backgroundProgram && m_backgroundQuad && m_backgroundTexture) {
			m_backgroundProgram->bind();
			drawBackground();
			m_backgroundProgram->release();
		}

		if (m_modelProgram && m_scene) {
			const float yaw = qDegreesToRadians(m_yaw);
			const float pitch = qDegreesToRadians(m_pitch);

			/* Orbit around the target: yaw sweeps horizontally, pitch lifts
			 * the camera while keeping it aimed at the chest. */
			const QVector3D eye(m_distance * qCos(pitch) * qCos(yaw),
								m_distance * qSin(pitch) + kCameraTarget.y(),
								m_distance * qCos(pitch) * qSin(yaw));

			QMatrix4x4 view;
			view.lookAt(eye, kCameraTarget, QVector3D(0, 1, 0));

			m_modelProgram->bind();
			m_modelProgram->setUniformValue("mvp_matrix",
											m_projection * view);
			m_scene->draw(m_modelProgram);
			m_modelProgram->release();
		}

		/* Wayland does not settle the fractional scale factor until after the
		 * first frame has been presented, so that frame is drawn at the wrong
		 * size no matter what. Asking for one more repaint is what makes the
		 * preview appear correctly instead of needing a nudge from the user. */
		if (m_firstFrame) {
			m_firstFrame = false;
			update();
		}
	}

	void SkinPreviewSurface::buildBackgroundTexture(int width, int height,
													int tileSize)
	{
		m_backgroundTexture = new QOpenGLTexture(
			chequerboard(width, height, tileSize, m_background));
		m_backgroundTexture->setMinificationFilter(QOpenGLTexture::Nearest);
		m_backgroundTexture->setMagnificationFilter(QOpenGLTexture::Nearest);
	}

	void SkinPreviewSurface::drawBackground()
	{
		/* The quad is already in clip space and has to lose every depth
		 * argument with the model, so the test is off *and* writing is
		 * masked -- otherwise it would fill the depth buffer and hide the
		 * model behind it. */
		glDisable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);

		m_backgroundTexture->bind();
		m_backgroundProgram->setUniformValue("texture", 0);
		m_backgroundQuad->draw(m_backgroundProgram);
		m_backgroundTexture->release();

		glDepthMask(GL_TRUE);
		glEnable(GL_DEPTH_TEST);
	}

	void SkinPreviewSurface::showSkin(const SkinEntry* skin)
	{
		if (!skin || !m_scene) {
			return;
		}
		m_scene->setSlim(skin->arms() == SkinEntry::Arms::Slim);
		m_scene->setSkin(skin->texture());
		update();
	}

	void SkinPreviewSurface::showCape(const QImage& cape)
	{
		if (!m_scene) {
			return;
		}
		m_scene->setCapeVisible(!cape.isNull());
		m_scene->setCape(cape);
		update();
	}

	void SkinPreviewSurface::setElytraVisible(bool visible)
	{
		if (!m_scene) {
			return;
		}
		m_scene->setElytraVisible(visible);
		update();
	}

	void SkinPreviewSurface::mousePressEvent(QMouseEvent* event)
	{
		/* pos() rather than position(): the latter only exists from Qt 6, and
		 * this builds against Qt 5.15 as well. Integer pixels are all the
		 * orbit needs anyway. */
		m_lastMousePosition = event->pos();
		m_dragging = true;
	}

	void SkinPreviewSurface::mouseReleaseEvent(QMouseEvent* event)
	{
		Q_UNUSED(event)
		m_dragging = false;
	}

	void SkinPreviewSurface::mouseMoveEvent(QMouseEvent* event)
	{
		/* Some Wayland compositors deliver a move without ever delivering
		 * the release, which leaves the model stuck to the pointer. Trusting
		 * the button state on every move instead of only the release event is
		 * what unsticks it. */
		if (!(event->buttons() & Qt::LeftButton)) {
			m_dragging = false;
			return;
		}
		if (!m_dragging) {
			return;
		}

		const QPoint position = event->pos();
		const QPoint delta = position - m_lastMousePosition;

		m_yaw += delta.x() * kOrbitDegreesPerPixel;
		m_pitch += delta.y() * kOrbitDegreesPerPixel;

		/* Keep yaw in one turn's worth so it cannot drift off into values
		 * where float precision starts to show. */
		if (m_yaw > 360.0f) {
			m_yaw -= 360.0f;
		} else if (m_yaw < 0.0f) {
			m_yaw += 360.0f;
		}

		m_lastMousePosition = position;
		update();
	}

	void SkinPreviewSurface::wheelEvent(QWheelEvent* event)
	{
		m_distance -= event->angleDelta().y() * kZoomPerWheelUnit;
		/* Only a near limit: closer than this and the camera ends up inside
		 * the model. Zooming out is harmless because the projection has no
		 * far plane. */
		m_distance = qMax(kMinimumDistance, m_distance);
		update();
	}
} // namespace skinrender
