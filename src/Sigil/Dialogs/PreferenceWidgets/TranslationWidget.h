/************************************************************************
**
**  Copyright (C) 2026  Sigil Development Team
**
**  This file is part of Sigil.
**
**  Sigil is free software: you can redistribute it and/or modify
**  it under the terms of the GNU General Public License as published by
**  the Free Software Foundation, either version 3 of the License, or
**  (at your option) any later version.
**
**  Sigil is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License for more details.
**
**  You should have received a copy of the GNU General Public License
**  along with Sigil.  If not, see <http://www.gnu.org/licenses/>.
**
*************************************************************************/

#pragma once
#ifndef TRANSLATIONWIDGET_H
#define TRANSLATIONWIDGET_H

#include "PreferencesWidget.h"
#include "ui_PTranslationWidget.h"

class Translator;

class TranslationWidget : public PreferencesWidget
{
    Q_OBJECT

public:
    TranslationWidget();
    PreferencesWidget::ResultAction saveSettings() override;

private slots:
    void testServer();
    void refreshModels();
    void defaultUrl();
    void onModelsRefreshed();

private:
    void readSettings();

    Ui::TranslationWidget ui;
    Translator *m_translator;
};

#endif // TRANSLATIONWIDGET_H
