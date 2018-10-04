﻿/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

#pragma once

#include "Classes.h"
#include "Texture.h"

typedef int material_handle;

// a collection of parameters for the rendering setup.
// for modern opengl this translates to set of attributes for the active shaders,
// for legacy opengl this is basically just texture(s) assigned to geometry
struct opengl_material {

    texture_handle texture1 { null_handle }; // primary texture, typically diffuse+apha
    texture_handle texture2 { null_handle }; // secondary texture, typically normal+reflection

    bool has_alpha { false }; // alpha state, calculated from presence of alpha in texture1
    std::string name;
    glm::vec2 size { -1.f, -1.f }; // 'physical' size of bound texture, in meters

// constructors
    opengl_material() = default;

// methods
    bool
        deserialize( cParser &Input, bool const Loadnow );

private:
// methods
    // imports member data pair from the config file, overriding existing parameter values of lower priority
    bool
        deserialize_mapping( cParser &Input, int const Priority, bool const Loadnow );
    // extracts name of the sound file from provided data stream
    std::string
        deserialize_filename( cParser &Input );

// members
    int priority1 { -1 }; // priority of last loaded primary texture
    int priority2 { -1 }; // priority of last loaded secondary texture
};

class material_manager {

public:
    material_manager() { m_materials.emplace_back( opengl_material() ); } // empty bindings for null material

    material_handle
        create( std::string const &Filename, bool const Loadnow );
    opengl_material const &
        material( material_handle const Material ) const { return m_materials[ Material ]; }

private:
// types
    typedef std::vector<opengl_material> material_sequence;
    typedef std::unordered_map<std::string, std::size_t> index_map;
// methods:
    // checks whether specified texture is in the texture bank. returns texture id, or npos.
    material_handle
        find_in_databank( std::string const &Materialname ) const;
    // checks whether specified file exists. returns name of the located file, or empty string.
    std::pair<std::string, std::string>
        find_on_disk( std::string const &Materialname ) const;
// members:
    material_sequence m_materials;
    index_map m_materialmappings;

};

//---------------------------------------------------------------------------
