/******************************************************************************
 *   Copyright (c) 2012 Jan Rheinländer <jrheinlaender@users.sourceforge.net> *
 *                                                                            *
 *   This file is part of the FreeCAD CAx development system.                 *
 *                                                                            *
 *   This library is free software; you can redistribute it and/or            *
 *   modify it under the terms of the GNU Library General Public              *
 *   License as published by the Free Software Foundation; either             *
 *   version 2 of the License, or (at your option) any later version.         *
 *                                                                            *
 *   This library  is distributed in the hope that it will be useful,         *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of           *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            *
 *   GNU Library General Public License for more details.                     *
 *                                                                            *
 *   You should have received a copy of the GNU Library General Public        *
 *   License along with this library; see the file COPYING.LIB. If not,       *
 *   write to the Free Software Foundation, Inc., 59 Temple Place,            *
 *   Suite 330, Boston, MA  02111-1307, USA                                   *
 *                                                                            *
 ******************************************************************************/


#ifndef PARTGUI_ViewProviderPolarPattern_H
#define PARTGUI_ViewProviderPolarPattern_H

#include "ViewProviderTransformed.h"

namespace PartDesignGui {

class PartDesignGuiExport ViewProviderPolarPattern : public ViewProviderTransformed
{
    Q_DECLARE_TR_FUNCTIONS(PartDesignGui::ViewProviderPolarPattern)
    PROPERTY_HEADER_WITH_OVERRIDE(PartDesignGui::ViewProviderPolarPattern);
public:
    ViewProviderPolarPattern() {
        sPixmap = "PartDesign_PolarPattern.svg";
    }

    QString getMenuName() const override {
        return tr("PolarPattern parameters");
    }

    const std::string & featureName() const override;
    void setupContextMenu(QMenu*, QObject*, const char*) override;

protected:
    /// Returns a newly create dialog for the part to be placed in the task view
    TaskDlgFeatureParameters *getEditDialog() override;

};


} // namespace PartDesignGui


#endif // PARTGUI_ViewProviderPolarPattern_H
