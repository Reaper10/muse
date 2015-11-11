//=========================================================
//  MusE
//  Linux Music Editor
//
//  RoutePopupMenu.h 
//  (C) Copyright 2011-2015 Tim E. Real (terminator356 A T sourceforge D O T net)
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; version 2 of
//  the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//=============================================================================

#ifndef __ROUTEPOPUPMENU_H__
#define __ROUTEPOPUPMENU_H__

#include "type_defs.h"
#include "route.h"
#include "custom_widget_actions.h"
#include "popupmenu.h"

namespace MusECore {
class AudioTrack;
class Track;
class PendingOperationList;
}

class QWidget;
class QString;
class QAction;
class QPoint;
class QResizeEvent;

namespace MusEGui {

class RoutingMatrixWidgetAction;

class RoutePopupMenu : public PopupMenu
{
  Q_OBJECT
  
    // Route describing the caller (a track, device, jack port etc).
    MusECore::Route  _route;
    // Whether the route popup was shown by clicking the output routes button, or input routes button.
    bool _isOutMenu;
    // For keyboard navigation.
    RoutePopupHit _lastHoveredHit;
    // To inform a pending hover slot whether the hover was from mouse or not.
    bool _hoverIsFromMouse;
    
    void init();
    // Prepares (fills) the popup before display.
    void prepare();
    // Recursive. Returns true if any text was changed.
    bool updateItemTexts(PopupMenu* menu = 0); 
    
    int addMenuItem(MusECore::AudioTrack* track, MusECore::Track* route_track, PopupMenu* lb, int id, int channel, 
                    int channels, bool isOutput);
    int addAuxPorts(MusECore::AudioTrack* t, PopupMenu* lb, int id, int channel, int channels, bool isOutput);
    int addInPorts(MusECore::AudioTrack* t, PopupMenu* lb, int id, int channel, int channels, bool isOutput);
    int addOutPorts(MusECore::AudioTrack* t, PopupMenu* lb, int id, int channel, int channels, bool isOutput);
    int addGroupPorts(MusECore::AudioTrack* t, PopupMenu* lb, int id, int channel, int channels, bool isOutput);
    int addWavePorts(MusECore::AudioTrack* t, PopupMenu* lb, int id, int channel, int channels, bool isOutput);
    void addMidiPorts(MusECore::Track* t, MusEGui::PopupMenu* pup, bool isOutput, bool show_synths, bool want_readable);
    void addMidiTracks(MusECore::Track* t, MusEGui::PopupMenu* pup, bool isOutput);
    
    int addSynthPorts(MusECore::AudioTrack* t, PopupMenu* lb, int id, int channel, int channels, bool isOutput);
    void addJackPorts(const MusECore::Route& route, PopupMenu* lb);
    void jackRouteActivated(QAction* action, const MusECore::Route& route, const MusECore::Route& rem_route, MusECore::PendingOperationList& operations);
    void trackRouteActivated(QAction* action, MusECore::Route& rem_route, MusECore::PendingOperationList& operations);
    void audioTrackPopupActivated(QAction* action, MusECore::Route& rem_route, MusECore::PendingOperationList& operations);
    void midiTrackPopupActivated(QAction* action, MusECore::Route& rem_route, MusECore::PendingOperationList& operations);
    void trackPopupActivated(QAction* action, MusECore::Route& rem_route, MusECore::PendingOperationList& operations);
    
  private slots:
    void routePopupHovered(QAction*);
    void routePopupActivated(QAction*);
    void songChanged(MusECore::SongChangedFlags_t);
  
  protected:  
    virtual bool event(QEvent*);
    virtual void resizeEvent(QResizeEvent* e);
    virtual void mouseReleaseEvent(QMouseEvent*);
    virtual void mousePressEvent(QMouseEvent*);
    virtual void mouseMoveEvent(QMouseEvent*);
    virtual void keyPressEvent(QKeyEvent*);
    // Updates item texts and the 'preferred alias action'. Returns true if any action was changed.
    virtual bool preferredPortAliasChanged(); 

    // For auto-breakup of a too-wide menu. Virtual.
    virtual PopupMenu* cloneMenu(const QString& title, QWidget* parent = 0, bool /*stayOpen*/ = false)
      { return new RoutePopupMenu(_route, title, parent, _isOutMenu); }

  public:
    RoutePopupMenu(QWidget* parent = 0, bool isOutput = false);
    RoutePopupMenu(const MusECore::Route& route, QWidget* parent = 0, bool isOutput = false);
    RoutePopupMenu(const MusECore::Route& route, const QString& title, QWidget* parent = 0, bool isOutput = false);
    
    void updateRouteMenus();
    
    void exec(const MusECore::Route& route, bool isOutput = false);
    void exec(const QPoint& p, const MusECore::Route& route, bool isOutput = false);
    void popup(const QPoint& p, const MusECore::Route& route, bool isOutput = false);
};

} // namespace MusEGui

#endif