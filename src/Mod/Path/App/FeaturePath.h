/***************************************************************************
 *   Copyright (c) 2014 Yorik van Havre <yorik@uncreated.net>              *
 *                                                                         *
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

#ifndef PATH_FeaturePath_H
#define PATH_FeaturePath_H

#include <set>
#include <App/DocumentObject.h>
#include <App/GeoFeature.h>
#include <App/FeaturePython.h>
#include <Mod/Part/App/PartFeature.h>

#include "PropertyPath.h"


namespace Path
{

class PathExport Feature : public Part::Feature
{
    typedef Part::Feature inherited;
    PROPERTY_HEADER(Path::Feature);

public:
    /// Constructor
    Feature(void);
    virtual ~Feature();

    /// returns the type name of the ViewProvider
    virtual const char* getViewProviderName(void) const {
        return "PathGui::ViewProviderPath";
    }
    virtual App::DocumentObjectExecReturn *execute(void) {
        return App::DocumentObject::StdReturn;
    }
    virtual short mustExecute(void) const;
    virtual PyObject *getPyObject(void);

    PropertyPath                Path;
    App::PropertyBool           BuildShape;
    App::PropertyIntegerList    CommandFilter;

protected:
    /// get called by the container when a property has changed
    virtual void onChanged (const App::Property* prop);

};

Part::TopoShape PathExport shapeFromPath(const Toolpath &path, const std::set<int> &filter={});

using FeaturePython = App::FeaturePythonT<Feature>;

} //namespace Path


#endif // PATH_FeaturePath_H
