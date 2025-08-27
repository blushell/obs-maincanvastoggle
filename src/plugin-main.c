/*
 * OBS Main Canvas Toggle Plugin
 * Copyright (C) 2024
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <https://www.gnu.org/licenses/>
 */

#include <plugin-support.h>

#ifdef ENABLE_QT
#include <QAction>
#include <QMainWindow>
#endif

#ifdef ENABLE_FRONTEND_API
#include <obs-frontend-api.h>
#endif

#include <obs.h>
#include <obs-module.h>
#include <obs-source.h>
#include <obs-properties.h>
#include <obs-data.h>
#include <graphics/graphics.h>
#include <util/platform.h>

#ifdef ENABLE_QT
class CanvasDockWidget : public QDockWidget {
	Q_OBJECT

private:
	QWidget *canvasWidget;
	QLabel *canvasLabel;
	QTimer *updateTimer;
	obs_source_t *mainCanvasSource;

public:
	CanvasDockWidget(QWidget *parent = nullptr);
	~CanvasDockWidget();

	void updateCanvas();
	void showEvent(QShowEvent *event) override;
	void hideEvent(QHideEvent *event) override;

private slots:
	void onTimerUpdate();
};

CanvasDockWidget::CanvasDockWidget(QWidget *parent)
	: QDockWidget(obs_module_text("CanvasDockTitle"), parent),
	  canvasWidget(new QWidget(this)),
	  canvasLabel(new QLabel(canvasWidget)),
	  updateTimer(new QTimer(this)),
	  mainCanvasSource(nullptr)
{
	setObjectName("canvasDock");
	setAllowedAreas(Qt::AllDockWidgetAreas);
	setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable |
		    QDockWidget::DockWidgetClosable);

	// Setup canvas widget
	canvasLabel->setAlignment(Qt::AlignCenter);
	canvasLabel->setMinimumSize(320, 240);
	canvasLabel->setStyleSheet("QLabel { background-color: #1a1a1a; border: 1px solid #333; }");

	QVBoxLayout *layout = new QVBoxLayout(canvasWidget);
	layout->addWidget(canvasLabel);
	layout->setContentsMargins(0, 0, 0, 0);

	setWidget(canvasWidget);

	// Setup update timer
	connect(updateTimer, &QTimer::timeout, this, &CanvasDockWidget::onTimerUpdate);

	// Create a canvas source for rendering
	mainCanvasSource = obs_source_create_private("scene", "MainCanvas", nullptr);
}

CanvasDockWidget::~CanvasDockWidget()
{
	if (updateTimer->isActive())
		updateTimer->stop();

	if (mainCanvasSource) {
		obs_source_release(mainCanvasSource);
		mainCanvasSource = nullptr;
	}
}

void CanvasDockWidget::showEvent(QShowEvent *event)
{
	QDockWidget::showEvent(event);
	if (!updateTimer->isActive())
		updateTimer->start(33); // ~30 FPS
}

void CanvasDockWidget::hideEvent(QHideEvent *event)
{
	QDockWidget::hideEvent(event);
	if (updateTimer->isActive())
		updateTimer->stop();
}

void CanvasDockWidget::onTimerUpdate()
{
	updateCanvas();
}

void CanvasDockWidget::updateCanvas()
{
	if (!isVisible())
		return;

	// Get the current scene
	obs_source_t *currentScene = obs_frontend_get_current_scene();
	if (!currentScene)
		return;

	// Create a temporary source for rendering
	uint32_t width = obs_source_get_width(currentScene);
	uint32_t height = obs_source_get_height(currentScene);

	if (width == 0 || height == 0) {
		obs_source_release(currentScene);
		return;
	}

	// Create a texture for rendering
	gs_texture_t *texture = gs_texture_create(width, height, GS_RGBA, 1, nullptr, GS_DYNAMIC);
	if (!texture) {
		obs_source_release(currentScene);
		return;
	}

	// Render the scene to texture
	gs_texrender_t *texrender = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
	gs_texrender_reset(texrender);

	if (gs_texrender_begin(texrender, width, height)) {
		gs_viewport_push();
		gs_projection_push();
		gs_ortho(0.0f, (float)width, 0.0f, (float)height, -100.0f, 100.0f);
		gs_set_viewport(0, 0, width, height);

		obs_source_video_render(currentScene);

		gs_texrender_end(texrender);
		gs_projection_pop();
		gs_viewport_pop();

		gs_texture_t *rendered_texture = gs_texrender_get_texture(texrender);
		if (rendered_texture) {
			// Copy texture data to CPU
			uint8_t *data = (uint8_t *)gs_texture_get_pixel_data(rendered_texture);

			if (data) {
				// Create QImage from texture data
				QImage image(data, width, height, QImage::Format_RGBA8888);
				QPixmap pixmap = QPixmap::fromImage(image.rgbSwapped());

				// Scale to fit the label while maintaining aspect ratio
				QSize labelSize = canvasLabel->size();
				QPixmap scaledPixmap = pixmap.scaled(labelSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

				canvasLabel->setPixmap(scaledPixmap);

				gs_texture_free_pixel_data(data);
			}
		}
	}

	gs_texrender_destroy(texrender);
	gs_texture_destroy(texture);
	obs_source_release(currentScene);
}
#endif

static CanvasDockWidget *canvasDock = nullptr;
static obs_hotkey_id toggle_hotkey = OBS_INVALID_HOTKEY_ID;

static void toggle_canvas_dock(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(data);
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);

	if (pressed && canvasDock) {
		if (canvasDock->isVisible()) {
			canvasDock->hide();
		} else {
			canvasDock->show();
			canvasDock->raise();
		}
	}
}

static void frontend_event_callback(enum obs_frontend_event event, void *private_data)
{
	UNUSED_PARAMETER(private_data);

	switch (event) {
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
		// Register hotkey after OBS is fully loaded
		if (toggle_hotkey == OBS_INVALID_HOTKEY_ID) {
			toggle_hotkey = obs_hotkey_register_frontend("toggle_canvas_dock",
								    obs_module_text("ToggleCanvasDock"),
								    toggle_canvas_dock, nullptr);
		}
		break;
	default:
		break;
	}
}

bool obs_module_load(void)
{
	obs_log(LOG_INFO, "Loading Main Canvas Toggle Plugin");

#ifdef ENABLE_QT
	// Create the dock widget
	QMainWindow *mainWindow = (QMainWindow *)obs_frontend_get_main_window();
	if (mainWindow) {
		canvasDock = new CanvasDockWidget(mainWindow);
		mainWindow->addDockWidget(Qt::RightDockWidgetArea, canvasDock);

		// Initially hide the dock
		canvasDock->hide();

		obs_log(LOG_INFO, "Canvas dock widget created and added to main window");
	} else {
		obs_log(LOG_ERROR, "Failed to get main window");
		return false;
	}
#endif

#ifdef ENABLE_FRONTEND_API
	// Register frontend event callback
	obs_frontend_add_event_callback(frontend_event_callback, nullptr);
#endif

	obs_log(LOG_INFO, "Main Canvas Toggle Plugin loaded successfully");
	return true;
}

void obs_module_unload(void)
{
	obs_log(LOG_INFO, "Unloading Main Canvas Toggle Plugin");

#ifdef ENABLE_FRONTEND_API
	obs_frontend_remove_event_callback(frontend_event_callback, nullptr);
#endif

#ifdef ENABLE_QT
	if (canvasDock) {
		delete canvasDock;
		canvasDock = nullptr;
	}
#endif

	if (toggle_hotkey != OBS_INVALID_HOTKEY_ID) {
		obs_hotkey_unregister(toggle_hotkey);
		toggle_hotkey = OBS_INVALID_HOTKEY_ID;
	}

	obs_log(LOG_INFO, "Main Canvas Toggle Plugin unloaded");
}

const char *obs_module_name(void)
{
	return obs_module_text("PluginName");
}

const char *obs_module_description(void)
{
	return obs_module_text("PluginDescription");
}

const char *obs_module_author(void)
{
	return "jonestown";
}
