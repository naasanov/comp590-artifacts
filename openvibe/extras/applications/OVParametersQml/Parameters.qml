import QtQuick
import QtQuick.Controls

import "." as OV

Item {

    id: _self;

    property var parameters;
    property alias params_model: params_model;

    Component {
        id: _dummy_component
        Text {text: lparam.name}
    }

    Component {
        id: _num_component
        //C.Numeric {param: lparam; paramType:  lparam? lparam.type : ""}
        OV.NumericParameter {param: lparam}
    }

    Component {
        id: _bool_component
        OV.BoolParameter {param: lparam}
    }

    Component {
        id: _string_component
        OV.StringParameter {param: lparam}
    }

    Component {
        id: _liststring_component
        OV.ListParameter {param: lparam}
    }

/*
    Component {
        id: _path_component
        //C.Path {param: lparam}
        OV.PathParameter {param: lparam}
    }

    Component {
        id: _liststringlist_component
        OV.InListStringList {param: lparam}
    }

    Component {
        id: _range_component
        OV.RangeParameter {param: lparam; decimals: 2}
    }

    Component {
        id: _colormap_component
        OV.Colormap {param: lparam}
    }

    Component {
        id: _lookuptable_component
        OV.LookupTable {param: lparam}
    }

    Component {
        id: _colortable_component
        OV.ColorTableParameter {param: lparam}
    }

    Component {
        id: _nurbs_component
        OV.NurbsParameter {param: lparam}
    }

    Component {
        id: _graphical_component
        OV.GraphicalParameter {param: lparam}
    }
*/
    ListModel {
        id: params_model;
        dynamicRoles: true;
    }

    function getComponent(type) {
        type = type.replace(',void', '')
        if(type ==  "bool") {
            return _bool_component;
        }
        if (type == "double") {
            return _num_component;
        }
        if(type ==  "int") {
            return _num_component;
        }
        if(type ==  "string") {
            return _string_component;
        }
        if(type ==  "stringlist") {
            return _liststring_component;
        }

        return _dummy_component;
    }

    function updateParametersModel() {
        params_model.clear();
        for (var p of _self.parameters) {
            params_model.append({"component": _self.getComponent(p.type), "param": p} )
        }
    }
}
