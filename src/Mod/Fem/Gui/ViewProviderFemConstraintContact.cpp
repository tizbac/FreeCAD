/***************************************************************************
 *   Copyright (c) 2015 FreeCAD Developers                                 *
 *   Authors: Michael Hindley <hindlemp@eskom.co.za>                       *
 *            Ruan Olwagen <olwager@eskom.co.za>                           *
 *            Oswald van Ginkel <vginkeo@eskom.co.za>                      *
 *   Based on Force constraint by Jan Rheinländer                          *
 *   This file is part of the FreeCAD CAx development system.              *
 *                                                                         *
 *   This library is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU Library General Public           *
 *   License as published by the Free Software Foundation; either          *
 *   version 2 of the License, or (at your option) any later version.      *
 *                                                                         *
 *   This library  is distributed in the hope that it will be useful,      *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU Library General Public License for more details.                  *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this library; see the file COPYING.LIB. If not,    *
 *   write to the Free Software Foundation, Inc., 59 Temple Place,         *
 *   Suite 330, Boston, MA  02111-1307, USA                                *
 *                                                                         *
 ***************************************************************************/

#include "PreCompiled.h"

#ifndef _PreComp_
# include <Inventor/nodes/SoCube.h>
# include <Inventor/nodes/SoMaterial.h>
# include <Inventor/nodes/SoRotation.h>
# include <Inventor/nodes/SoSeparator.h>
# include <Inventor/nodes/SoTranslation.h>
#endif

#include "Mod/Fem/App/FemConstraintContact.h"
#include <Gui/Control.h>
#include "ViewProviderFemConstraintContact.h"
#include "TaskFemConstraintContact.h"


using namespace FemGui;

PROPERTY_SOURCE(FemGui::ViewProviderFemConstraintContact, FemGui::ViewProviderFemConstraint)

ViewProviderFemConstraintContact::ViewProviderFemConstraintContact()
{
    sPixmap = "FEM_ConstraintContact";
    //Note change "Contact" in line above to new constraint name, make sure it is the same as in taskFem* cpp file
    ADD_PROPERTY(FaceColor, (0.2f, 0.3f, 0.2f));
}

ViewProviderFemConstraintContact::~ViewProviderFemConstraintContact()
{
}

//FIXME setEdit needs a careful review
bool ViewProviderFemConstraintContact::setEdit(int ModNum)
{
    if (ModNum == ViewProvider::Default) {
        // When double-clicking on the item for this constraint the
        // object unsets and sets its edit mode without closing
        // the task panel
        Gui::TaskView::TaskDialog *dlg = Gui::Control().activeDialog();
        TaskDlgFemConstraintContact *constrDlg = qobject_cast<TaskDlgFemConstraintContact *>(dlg);
        if (constrDlg && constrDlg->getConstraintView() != this)
            constrDlg = nullptr; // another constraint left open its task panel
        if (dlg && !constrDlg) {
            if (constraintDialog) {
                // Ignore the request to open another dialog
                return false;
            } else {
                constraintDialog = new TaskFemConstraintContact(this);
                return true;
            }
        }

        // clear the selection (convenience)
        Gui::Selection().clearSelection();

        // start the edit dialog
        if (constrDlg)
            Gui::Control().showDialog(constrDlg);
        else
            Gui::Control().showDialog(new TaskDlgFemConstraintContact(this));
        return true;
    }
    else {
        return ViewProviderDocumentObject::setEdit(ModNum); // clazy:exclude=skipped-base-method
    }
}

#define HEIGHT (0.5)
#define LENGTH (1.5)
#define WIDTH (0.5)

//#define USE_MULTIPLE_COPY  //OvG: MULTICOPY fails to update scaled display on initial drawing - so disable

void ViewProviderFemConstraintContact::updateData(const App::Property* prop)
{
    // Gets called whenever a property of the attached object changes
    Fem::ConstraintContact* pcConstraint = static_cast<Fem::ConstraintContact*>(this->getObject());
    float scaledlength = LENGTH * pcConstraint->Scale.getValue(); //OvG: Calculate scaled values once only
    float scaledheight = HEIGHT * pcConstraint->Scale.getValue();
    float scaledwidth = WIDTH * pcConstraint->Scale.getValue();

    if (prop == &pcConstraint->Points) {
        const std::vector<Base::Vector3d>& points = pcConstraint->Points.getValues();
        const std::vector<Base::Vector3d>& normals = pcConstraint->Normals.getValues();
        if (points.size() != normals.size())
            return;
        std::vector<Base::Vector3d>::const_iterator n = normals.begin();

        // Points and Normals are always updated together
        Gui::coinRemoveAllChildren(pShapeSep);

        for (const auto & point : points) {
            //Define base and normal directions
            SbVec3f base(point.x, point.y, point.z);
            SbVec3f dir(n->x, n->y, n->z);//normal

            ///Visual indication
            //define separator
            SoSeparator* sep = new SoSeparator();

            //first move to correct position
            SoTranslation* trans = new SoTranslation();
            SbVec3f newPos = base + scaledheight * dir * 0.12f;
            trans->translation.setValue(newPos);
            sep->addChild(trans);

            //adjust orientation
            SoRotation *rot = new SoRotation();
            rot->rotation.setValue(SbRotation(SbVec3f(0, 1, 0), dir));
            sep->addChild(rot);

            //define color of shape
            SoMaterial *myMaterial = new SoMaterial;
            myMaterial->diffuseColor.set1Value(0, SbColor(1, 1, 1));//RGB
            //myMaterial->diffuseColor.set1Value(1,SbColor(0,0,1));//possible to adjust sides separately
            sep->addChild(myMaterial);

            //draw a cube
            SoCube *cbe = new SoCube();
            cbe->depth.setValue(scaledlength * 0.5);
            cbe->height.setValue(scaledheight * 0.25);
            cbe->width.setValue(scaledwidth * 0.75);
            sep->addChild(cbe);
            //translate position
            SoTranslation *trans2 = new SoTranslation();
            trans2->translation.setValue(SbVec3f(0, 0, 0));
            sep->addChild(trans2);

            pShapeSep->addChild(sep);
            n++;
        }
    }
    // Gets called whenever a property of the attached object changes
    ViewProviderFemConstraint::updateData(prop);
}
