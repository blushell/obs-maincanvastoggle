/*
Plugin Name
Copyright (C) <Year> <Developer> <Email Address>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/
#include <obs-module.h>
#include <obs-frontend-api.h>

#include <QMainWindow>
#include <QDockWidget>
#include <QWidget>
#include <QDebug>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-dock-preview", "en-US")

static QDockWidget *previewDock = nullptr;

bool obs_module_load(void)
{
    blog(LOG_INFO, "[dock-preview] Loading...");

    QWidget *mainWindow = (QWidget*)obs_frontend_get_main_window();
    if (!mainWindow) {
        blog(LOG_ERROR, "[dock-preview] Could not find OBS main window.");
        return true;
    }

    QWidget *preview = mainWindow->findChild<QWidget*>("OBSBasicPreview", Qt::FindChildrenRecursively);
    if (!preview) {
        blog(LOG_ERROR, "[dock-preview] Could not find OBSBasicPreview widget.");
        return true;
    }

    previewDock = new QDockWidget("Preview", mainWindow);
    previewDock->setObjectName("DockPreview");
    previewDock->setWidget(preview);

    QMainWindow *mw = qobject_cast<QMainWindow*>(mainWindow);
    mw->addDockWidget(Qt::RightDockWidgetArea, previewDock);

    // Register with OBS Docks menu
    obs_frontend_add_dock(previewDock);

    // Hide original placeholder central widget
    QWidget *central = mw->centralWidget();
    if (central && central != preview) {
        central->hide();
    }

    blog(LOG_INFO, "[dock-preview] Preview moved into dock (registered with Docks menu).");
    return true;
}

void obs_module_unload(void)
{
    if (previewDock) {
        obs_frontend_remove_dock(previewDock);
        previewDock->deleteLater();
        previewDock = nullptr;
    }

    blog(LOG_INFO, "[dock-preview] Unloaded.");
}

