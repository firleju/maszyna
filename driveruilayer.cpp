﻿/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

#include "stdafx.h"
#include "driveruilayer.h"

#include "Globals.h"
#include "application.h"
#include "translation.h"
#include "simulation.h"
#include "Train.h"
#include "AnimModel.h"
#include "renderer.h"

driver_ui::driver_ui() {

    clear_panels();
    // bind the panels with ui object. maybe not the best place for this but, eh
    push_back( &m_aidpanel );
    push_back( &m_scenariopanel );
    push_back( &m_timetablepanel );
    push_back( &m_debugpanel );
    push_back( &m_transcriptspanel );

    m_aidpanel.title = locale::strings[ locale::string::driver_aid_header ];

    m_scenariopanel.title = locale::strings[ locale::string::driver_scenario_header ];
    m_scenariopanel.size_min = { 435, 85 };
    m_scenariopanel.size_max = { Global.iWindowWidth * 0.95, Global.iWindowHeight * 0.95 };

    m_timetablepanel.title = locale::strings[ locale::string::driver_timetable_header ];
    m_timetablepanel.size_min = { 435, 70 };
    m_timetablepanel.size_max = { 435, Global.iWindowHeight * 0.95 };

    m_transcriptspanel.title = locale::strings[ locale::string::driver_transcripts_header ];
    m_transcriptspanel.size_min = { 435, 85 };
    m_transcriptspanel.size_max = { Global.iWindowWidth * 0.95, Global.iWindowHeight * 0.95 };
}

// potentially processes provided input key. returns: true if key was processed, false otherwise
bool
driver_ui::on_key( int const Key, int const Action ) {
    // TODO: pass the input first through an active ui element if there's any
    // if the ui element shows no interest or we don't have one, try to interpret the input yourself:

    if( Key == GLFW_KEY_ESCAPE ) {
        // toggle pause
        if( Action != GLFW_PRESS ) { return true; } // recognized, but ignored

        if (Global.iMultiplayer & 2)
			return true; // in network communication pause is steered by vd
		if( Global.iPause & 1 ) {
            // jeśli pauza startowa
            // odpauzowanie, gdy po wczytaniu miało nie startować
            Global.iPause ^= 1;
        }
        else if( ( Global.iMultiplayer & 2 ) == 0 ) {
            // w multiplayerze pauza nie ma sensu
            Global.iPause ^= 2; // zmiana stanu zapauzowania
        }
        return true;
    }

    // if the pause is on ignore block other input
    if( m_paused ) { return true; }

    switch( Key ) {

        case GLFW_KEY_F1:
        case GLFW_KEY_F2:
        case GLFW_KEY_F3:
        case GLFW_KEY_F10:
        case GLFW_KEY_F12: { // ui mode selectors

            if( ( true == Global.ctrlState )
             || ( true == Global.shiftState ) ) {
                // only react to keys without modifiers
                return false;
            }

            if( Action != GLFW_PRESS ) { return true; } // recognized, but ignored
        }

        default: { // everything else
            break;
        }
    }

    switch (Key) {
            
        case GLFW_KEY_F1: {
            // basic consist info
            auto state = (
                ( m_aidpanel.is_open == false ) ? 0 :
                ( m_aidpanel.is_expanded == false ) ? 1 :
                2 );
            state = clamp_circular( ++state, 3 );
            
            m_aidpanel.is_open = ( state > 0 );
            m_aidpanel.is_expanded = ( state > 1 );

            return true;
        }

        case GLFW_KEY_F2: {
            // timetable
            auto state = (
                ( m_timetablepanel.is_open == false ) ? 0 :
                ( m_timetablepanel.is_expanded == false ) ? 1 :
                2 );
            state = clamp_circular( ++state, 3 );
            
            m_timetablepanel.is_open = ( state > 0 );
            m_timetablepanel.is_expanded = ( state > 1 );

            return true;
        }

        case GLFW_KEY_F3: {
            // debug panel
            m_scenariopanel.is_open = !m_scenariopanel.is_open;
            return true;
        }

        case GLFW_KEY_F12: {
            // debug panel
            m_debugpanel.is_open = !m_debugpanel.is_open;
            return true;
        }

        default: {
            break;
        }
    }

    return false;
}

// potentially processes provided mouse movement. returns: true if the input was processed, false otherwise
bool
driver_ui::on_cursor_pos( double const Horizontal, double const Vertical ) {
    // intercept mouse movement when the pause window is on
    return m_paused;
}

// potentially processes provided mouse button. returns: true if the input was processed, false otherwise
bool
driver_ui::on_mouse_button( int const Button, int const Action ) {
    // intercept mouse movement when the pause window is on
    return m_paused;
}

// updates state of UI elements
void
driver_ui::update() {

    auto const pausemask { 1 | 2 };
    auto ispaused { ( false == DebugModeFlag ) && ( ( Global.iPause & pausemask ) != 0 ) };
    if( ( ispaused != m_paused )
     && ( false == Global.ControlPicking ) ) {
        set_cursor( ispaused );
    }
    m_paused = ispaused;

    set_tooltip( "" );

    auto const *train { simulation::Train };

    if( ( train != nullptr ) && ( false == FreeFlyModeFlag ) ) {
        if( false == DebugModeFlag ) {
            // in regular mode show control functions, for defined controls
            set_tooltip( locale::label_cab_control( train->GetLabel( GfxRenderer.Pick_Control() ) ) );
        }
        else {
            // in debug mode show names of submodels, to help with cab setup and/or debugging
            auto const cabcontrol = GfxRenderer.Pick_Control();
            set_tooltip( ( cabcontrol ? cabcontrol->pName : "" ) );
        }
    }
    if( ( true == Global.ControlPicking ) && ( true == FreeFlyModeFlag ) && ( true == DebugModeFlag ) ) {
        auto const scenerynode = GfxRenderer.Pick_Node();
        set_tooltip(
            ( scenerynode ?
                scenerynode->name() :
                "" ) );
    }

    ui_layer::update();
}

void
driver_ui::set_cursor( bool const Visible ) {

    if( Visible ) {
        Application.set_cursor( GLFW_CURSOR_NORMAL );
        Application.set_cursor_pos( Global.iWindowWidth / 2, Global.iWindowHeight / 2 );
    }
    else {
        Application.set_cursor( GLFW_CURSOR_DISABLED );
        Application.set_cursor_pos( 0, 0 );
    }
}

// render() subclass details
void
driver_ui::render_() {
    // pause/quit modal
    auto const popupheader { locale::strings[ locale::string::driver_pause_header ].c_str() };
	if (m_paused && !m_pause_modal_opened)
	{
		m_pause_modal_opened = true;
		ImGui::OpenPopup(popupheader);
	}
    if( ImGui::BeginPopupModal( popupheader, &m_pause_modal_opened, ImGuiWindowFlags_AlwaysAutoResize ) ) {
        auto const popupwidth{ locale::strings[ locale::string::driver_pause_header ].size() * 7 };
        if( ImGui::Button( locale::strings[ locale::string::driver_pause_resume ].c_str(), ImVec2( popupwidth, 0 ) ) ) {
            auto const pausemask { 1 | 2 };
            Global.iPause &= ~pausemask;
        }
        if( ImGui::Button( locale::strings[ locale::string::driver_pause_quit ].c_str(), ImVec2( popupwidth, 0 ) ) ) {
            glfwSetWindowShouldClose( m_window, 1 );
        }
		if (!m_paused)
		{
			m_pause_modal_opened = false;
			ImGui::CloseCurrentPopup();
		}
        ImGui::EndPopup();
    }
}
