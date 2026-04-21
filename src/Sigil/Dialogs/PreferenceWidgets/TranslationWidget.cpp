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

#include "TranslationWidget.h"
#include "Misc/Translator.h"
#include "Misc/SettingsStore.h"

TranslationWidget::TranslationWidget()
    : m_translator(new Translator(this))
{
    ui.setupUi(this);
    connect(ui.btnTestServer, SIGNAL(clicked()), this, SLOT(testServer()));
    connect(ui.btnRefreshModels, SIGNAL(clicked()), this, SLOT(refreshModels()));
    connect(ui.btnDefaultUrl, SIGNAL(clicked()), this, SLOT(defaultUrl()));
    connect(m_translator, &Translator::modelsRefreshed, this, &TranslationWidget::onModelsRefreshed);
    readSettings();
}

PreferencesWidget::ResultAction TranslationWidget::saveSettings()
{
    SettingsStore settings;
    settings.setSigorestServerUrl(ui.leServerUrl->text().trimmed());
    return PreferencesWidget::ResultAction_None;
}

void TranslationWidget::readSettings()
{
    SettingsStore settings;
    ui.leServerUrl->setText(settings.sigorestServerUrl());
    refreshModels();
}

void TranslationWidget::testServer()
{
    SettingsStore settings;
    settings.setSigorestServerUrl(ui.leServerUrl->text().trimmed());
    ui.lblStatus->setText(tr("Testing..."));
    refreshModels();
}

void TranslationWidget::refreshModels()
{
    m_translator->refreshModels();
    ui.lblStatus->setText(tr("Querying..."));
}

void TranslationWidget::defaultUrl()
{
    ui.leServerUrl->setText("http://localhost:9080");
}

void TranslationWidget::onModelsRefreshed()
{
    if (m_translator->isServerAvailable()) {
        ui.lblStatus->setText(tr("Available (%1 models)").arg(m_translator->availableModels().size()));
        ui.lblModels->setText(m_translator->availableModels().join(", "));
    } else {
        ui.lblStatus->setText(tr("Not reachable"));
        ui.lblModels->setText("");
    }
}
