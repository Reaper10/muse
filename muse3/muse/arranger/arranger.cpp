//=========================================================
//  MusE
//  Linux Music Editor
//    $Id: arranger.cpp,v 1.33.2.21 2009/11/17 22:08:22 terminator356 Exp $
//  (C) Copyright 1999-2004 Werner Schweer (ws@seh.de)
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
//
//=========================================================

#include "config.h"

#include <stdio.h>
#include <limits.h>
#include <math.h>

#include <QComboBox>
#include <QGridLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QList>
#include <QMainWindow>
#include <QScrollBar>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QPainter>
#include <QCursor>
#include <QPoint>
#include <QRect>

#include "arrangerview.h"
#include "arranger.h"
#include "song.h"
#include "app.h"
#include "mtscale.h"
#include "scrollscale.h"
#include "scrollbar.h"
#include "pcanvas.h"
#include "poslabel.h"
#include "xml.h"
#include "splitter.h"
#include "lcombo.h"
#include "mtrackinfo.h"
#include "midiport.h"
#include "mididev.h"
#include "utils.h"
#include "globals.h"
#include "tlist.h"
#include "icons.h"
#include "header.h"
#include "utils.h"
#include "widget_stack.h"
#include "trackinfo_layout.h"
#include "audio.h"
#include "event.h"
#include "midiseq.h"
#include "midictrl.h"
#include "mpevent.h"
#include "gconfig.h"
#include "mixer/astrip.h"
#include "mixer/mstrip.h"
#include "spinbox.h"
#include "shortcuts.h"

namespace MusEGui {

std::vector<Arranger::custom_col_t> Arranger::custom_columns;     //FINDMICH TODO: eliminate all usage of new_custom_columns
std::vector<Arranger::custom_col_t> Arranger::new_custom_columns; //and instead let the arranger update without restarting muse!
QByteArray Arranger::header_state;
static const char* gArrangerRasterStrings[] = {
      QT_TRANSLATE_NOOP("MusEGui::Arranger", "Off"), QT_TRANSLATE_NOOP("MusEGui::Arranger", "Bar"), "1/2", "1/4", "1/8", "1/16"
      };
static int gArrangerRasterTable[] = { 1, 0, 768, 384, 192, 96 };

void Arranger::writeCustomColumns(int level, MusECore::Xml& xml)
{
  xml.tag(level++, "custom_columns");
  
  for (unsigned i=0;i<new_custom_columns.size();i++)
  {
    xml.tag(level++, "column");
    xml.strTag(level, "name", new_custom_columns[i].name);
    xml.intTag(level, "ctrl", new_custom_columns[i].ctrl);
    xml.intTag(level, "affected_pos", new_custom_columns[i].affected_pos);
    xml.etag(--level, "column");
  }
  
  xml.etag(--level, "custom_columns");
}

void Arranger::readCustomColumns(MusECore::Xml& xml)
{
      custom_columns.clear();
      
      for (;;) {
            MusECore::Xml::Token token(xml.parse());
            const QString& tag(xml.s1());
            switch (token) {
                  case MusECore::Xml::Error:
                  case MusECore::Xml::End:
                        new_custom_columns=custom_columns;
                        return;
                  case MusECore::Xml::TagStart:
                        if (tag == "column")
                              custom_columns.push_back(readOneCustomColumn(xml));
                        else
                              xml.unknown("Arranger::readCustomColumns");
                        break;
                  case MusECore::Xml::TagEnd:
                        if (tag == "custom_columns")
                        {
                              new_custom_columns=custom_columns;
                              return;
                        }
                  default:
                        break;
                  }
            }
}

Arranger::custom_col_t Arranger::readOneCustomColumn(MusECore::Xml& xml)
{
      custom_col_t temp(0, "-");
      
      for (;;) {
            MusECore::Xml::Token token(xml.parse());
            const QString& tag(xml.s1());
            switch (token) {
                  case MusECore::Xml::Error:
                  case MusECore::Xml::End:
                        return temp;
                  case MusECore::Xml::TagStart:
                        if (tag == "name")
                              temp.name=xml.parse1();
                        else if (tag == "ctrl")
                              temp.ctrl=xml.parseInt();
                        else if (tag == "affected_pos")
                              temp.affected_pos=(custom_col_t::affected_pos_t)xml.parseInt();
                        else
                              xml.unknown("Arranger::readOneCustomColumn");
                        break;
                  case MusECore::Xml::TagEnd:
                        if (tag == "column")
                              return temp;
                  default:
                        break;
                  }
            }
      return temp;
}

//---------------------------------------------------------
//   Arranger::setHeaderToolTips
//---------------------------------------------------------

void Arranger::setHeaderToolTips()
      {
      header->setToolTip(COL_RECORD,     tr("Enable Recording"));
      header->setToolTip(COL_MUTE,       tr("Mute/Off Indicator"));
      header->setToolTip(COL_SOLO,       tr("Solo Indicator"));
      header->setToolTip(COL_CLASS,      tr("Track Type"));
      header->setToolTip(COL_NAME,       tr("Track Name"));
      header->setToolTip(COL_OCHANNEL,   tr("Midi output channel number or audio channels"));
      header->setToolTip(COL_OPORT,      tr("Midi output port or synth midi port"));
      header->setToolTip(COL_TIMELOCK,   tr("Time Lock"));
      header->setToolTip(COL_AUTOMATION, tr("Automation parameter selection"));
      header->setToolTip(COL_CLEF,       tr("Notation clef"));
      }



//---------------------------------------------------------
//   Arranger::setHeaderWhatsThis
//---------------------------------------------------------

void Arranger::setHeaderWhatsThis()
      {
      header->setWhatsThis(COL_RECORD,   tr("Enable recording. Click to toggle."));
      header->setWhatsThis(COL_MUTE,     tr("Mute indicator. Click to toggle.\nRight-click to toggle track on/off.\nMute is designed for rapid, repeated action.\nOn/Off is not!"));
      header->setWhatsThis(COL_SOLO,     tr("Solo indicator. Click to toggle.\nConnected tracks are also 'phantom' soloed,\n indicated by a dark square."));
      header->setWhatsThis(COL_CLASS,    tr("Track type. Right-click to change\n midi and drum track types."));
      header->setWhatsThis(COL_NAME,     tr("Track name. Double-click to edit.\nRight-click for more options."));
      header->setWhatsThis(COL_OCHANNEL, tr("Midi/drum track: Output channel number.\nAudio track: Channels.\nMid/right-click to change."));
      header->setWhatsThis(COL_OPORT,    tr("Midi/drum track: Output port.\nSynth track: Assigned midi port.\nLeft-click to change.\nRight-click to show GUI."));
      header->setWhatsThis(COL_TIMELOCK, tr("Time lock"));
      header->setToolTip(COL_CLEF,       tr("Notation clef. Select this tracks notation clef."));
      }

//---------------------------------------------------------
//   Arranger
//    is the central widget in app
//---------------------------------------------------------

Arranger::Arranger(ArrangerView* parent, const char* name)
   : QWidget(parent)
      {
      setObjectName(name);
      _raster  = 0;      // measure
      selected = 0;
      showTrackinfoFlag = true;
      showTrackinfoAltFlag = false;
      
      cursVal = INT_MAX;
      
      _parentWin=parent;
      
      setFocusPolicy(Qt::NoFocus);
      
      //---------------------------------------------------
      //  ToolBar
      //    create toolbar in toplevel widget
      //---------------------------------------------------

      // NOTICE: Please ensure that any tool bar object names here match the names assigned 
      //          to identical or similar toolbars in class MusE or other TopWin classes. 
      //         This allows MusE::setCurrentMenuSharingTopwin() to do some magic
      //          to retain the original toolbar layout. If it finds an existing
      //          toolbar with the same object name, it /replaces/ it using insertToolBar(),
      //          instead of /appending/ with addToolBar().

      parent->addToolBarBreak();
      QToolBar* toolbar = parent->addToolBar(tr("Arranger"));
      toolbar->setObjectName("ArrangerToolbar");
      
      QLabel* label = new QLabel(tr("Cursor"));
      label->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
      label->setIndent(3);
      toolbar->addWidget(label);
      cursorPos = new PosLabel(0);
      cursorPos->setEnabled(false);
      cursorPos->setFixedHeight(22);
      toolbar->addWidget(cursorPos);

      label = new QLabel(tr("Snap"));
      label->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
      label->setIndent(3);
      toolbar->addWidget(label);
      _rasterCombo = new QComboBox();
      for (int i = 0; i < 6; i++)
            _rasterCombo->insertItem(i, tr(gArrangerRasterStrings[i]), gArrangerRasterTable[i]);
      _rasterCombo->setCurrentIndex(1);
      // Set the audio record part snapping. Set to 0 (bar), the same as this combo box intial raster.
      MusEGlobal::song->setArrangerRaster(0);
      toolbar->addWidget(_rasterCombo);
      connect(_rasterCombo, SIGNAL(activated(int)), SLOT(rasterChanged(int)));
      _rasterCombo->setFocusPolicy(Qt::TabFocus);

      // Song len
      label = new QLabel(tr("Len"));
      label->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
      label->setIndent(3);
      toolbar->addWidget(label);

      // song length is limited to 10000 bars; the real song len is limited
      // by overflows in tick computations
      lenEntry = new SpinBox(1, 10000, 1);
      lenEntry->setFocusPolicy(Qt::StrongFocus);
      lenEntry->setValue(MusEGlobal::song->len());
      lenEntry->setToolTip(tr("song length - bars"));
      lenEntry->setWhatsThis(tr("song length - bars"));
      toolbar->addWidget(lenEntry);
      connect(lenEntry, SIGNAL(valueChanged(int)), SLOT(songlenChanged(int)));

      label = new QLabel(tr("Pitch"));
      label->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
      label->setIndent(3);
      toolbar->addWidget(label);
      
      globalPitchSpinBox = new SpinBox(-127, 127, 1);
      globalPitchSpinBox->setFocusPolicy(Qt::StrongFocus);
      globalPitchSpinBox->setValue(MusEGlobal::song->globalPitchShift());
      globalPitchSpinBox->setToolTip(tr("midi pitch"));
      globalPitchSpinBox->setWhatsThis(tr("global midi pitch shift"));
      toolbar->addWidget(globalPitchSpinBox);
      connect(globalPitchSpinBox, SIGNAL(valueChanged(int)), SLOT(globalPitchChanged(int)));
      
      label = new QLabel(tr("Tempo"));
      label->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
      label->setIndent(3);
      toolbar->addWidget(label);
      
      globalTempoSpinBox = new SpinBox(50, 200, 1, toolbar);
      globalTempoSpinBox->setFocusPolicy(Qt::StrongFocus);
      globalTempoSpinBox->setSuffix(QString("%"));
      globalTempoSpinBox->setValue(MusEGlobal::tempomap.globalTempo());
      globalTempoSpinBox->setToolTip(tr("midi tempo"));
      globalTempoSpinBox->setWhatsThis(tr("midi tempo"));
      toolbar->addWidget(globalTempoSpinBox);
      connect(globalTempoSpinBox, SIGNAL(valueChanged(int)), SLOT(globalTempoChanged(int)));
      
      QToolButton* tempo50  = new QToolButton();
      tempo50->setText(QString("50%"));
      tempo50->setFocusPolicy(Qt::NoFocus);
      toolbar->addWidget(tempo50);
      connect(tempo50, SIGNAL(clicked()), SLOT(setTempo50()));
      
      QToolButton* tempo100 = new QToolButton();
      tempo100->setText(tr("N"));
      tempo100->setFocusPolicy(Qt::NoFocus);
      toolbar->addWidget(tempo100);
      connect(tempo100, SIGNAL(clicked()), SLOT(setTempo100()));
      
      QToolButton* tempo200 = new QToolButton();
      tempo200->setText(QString("200%"));
      tempo200->setFocusPolicy(Qt::NoFocus);
      toolbar->addWidget(tempo200);
      connect(tempo200, SIGNAL(clicked()), SLOT(setTempo200()));

      QVBoxLayout* box  = new QVBoxLayout(this);
      box->setContentsMargins(0, 0, 0, 0);
      box->setSpacing(0);
      box->addWidget(MusECore::hLine(this), Qt::AlignTop);

      //---------------------------------------------------
      //  Tracklist
      //---------------------------------------------------

      int xscale = -100;
      int yscale = 1;

      split  = new Splitter(Qt::Horizontal, this, "split");
      split->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
      box->addWidget(split, 1000);

      // REMOVE Tim. Trackinfo. Added.
      trackInfoWidget = new QWidget(split);
      split->setStretchFactor(split->indexOf(trackInfoWidget), 0);
//       QSizePolicy tipolicy = QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
      QSizePolicy tipolicy = QSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
      tipolicy.setHorizontalStretch(0);
      tipolicy.setVerticalStretch(100);
      trackInfoWidget->setSizePolicy(tipolicy);
      
      tracklist = new QWidget(split);
      split->setStretchFactor(split->indexOf(tracklist), 0);
      //split->setStretchFactor(split->indexOf(tracklist), 1);
//       QSizePolicy tpolicy = QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
      QSizePolicy tpolicy = QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
      tpolicy.setHorizontalStretch(0);
//       tpolicy.setHorizontalStretch(255);
      tpolicy.setVerticalStretch(100);
      tracklist->setSizePolicy(tpolicy);

      editor = new QWidget(split);
      split->setStretchFactor(split->indexOf(editor), 1);
      //split->setStretchFactor(split->indexOf(editor), 2);
      QSizePolicy epolicy = QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
      epolicy.setHorizontalStretch(255);
      epolicy.setVerticalStretch(100);
      editor->setSizePolicy(epolicy);

      //---------------------------------------------------
      //    Track Info
      //---------------------------------------------------

      // REMOVE Tim. Trackinfo. Changed.
//       infoScroll = new ScrollBar(Qt::Vertical, true, tracklist);
      infoScroll = new ScrollBar(Qt::Vertical, true, trackInfoWidget);
      infoScroll->setObjectName("infoScrollBar");
      //genTrackInfo(tracklist); // Moved below

      // Track-Info Button
      // REMOVE Tim. Trackinfo. Changed.
//       ib  = new QToolButton(tracklist);
      //ib  = new QToolButton(trackInfoWidget);
      trackInfoButton  = new QToolButton(this);
      trackInfoButton->setText(tr("TrackInfo"));
      trackInfoButton->setCheckable(true);
      trackInfoButton->setChecked(showTrackinfoFlag);
      trackInfoButton->setFocusPolicy(Qt::NoFocus);
      connect(trackInfoButton, SIGNAL(toggled(bool)), SLOT(showTrackInfo(bool)));

      // REMOVE Tim. Trackinfo. Added.
      trackInfoAltButton  = new QToolButton(this);
      trackInfoAltButton->setText(tr("Alt"));
      trackInfoAltButton->setCheckable(true);
      trackInfoAltButton->setChecked(showTrackinfoAltFlag);
      trackInfoAltButton->setFocusPolicy(Qt::NoFocus);
      connect(trackInfoAltButton, SIGNAL(toggled(bool)), SLOT(showTrackInfoAlt(bool)));
      
      // REMOVE Tim. Trackinfo. Added.
      genTrackInfo(trackInfoWidget);

      // set up the header
      header = new Header(tracklist, "header");
      header->setFixedHeight(31);

      QFontMetrics fm1(header->font());
      int fw = 11;

      header->setColumnLabel(tr("R"), COL_RECORD, fm1.width('R')+fw);
      header->setColumnLabel(tr("M"), COL_MUTE, fm1.width('M')+fw);
      header->setColumnLabel(tr("S"), COL_SOLO, fm1.width('S')+fw);
      header->setColumnLabel(tr("C"), COL_CLASS, fm1.width('C')+fw);
      header->setColumnLabel(tr("Track"), COL_NAME, 100);
      header->setColumnLabel(tr("Port"), COL_OPORT, 60);
      header->setColumnLabel(tr("Ch"), COL_OCHANNEL, 30);
      header->setColumnLabel(tr("T"), COL_TIMELOCK, fm1.width('T')+fw);
      header->setColumnLabel(tr("Automation"), COL_AUTOMATION, 75);
      header->setColumnLabel(tr("Clef"), COL_CLEF, 75);
      for (unsigned i=0;i<custom_columns.size();i++)
        header->setColumnLabel(custom_columns[i].name, COL_CUSTOM_MIDICTRL_OFFSET+i, MAX(fm1.width(custom_columns[i].name)+fw, 30));
      header->setSectionResizeMode(COL_RECORD, QHeaderView::Fixed);
      header->setSectionResizeMode(COL_MUTE, QHeaderView::Fixed);
      header->setSectionResizeMode(COL_SOLO, QHeaderView::Fixed);
      header->setSectionResizeMode(COL_CLASS, QHeaderView::Fixed);
      header->setSectionResizeMode(COL_NAME, QHeaderView::Interactive);
      header->setSectionResizeMode(COL_OPORT, QHeaderView::Interactive);
      header->setSectionResizeMode(COL_OCHANNEL, QHeaderView::Fixed);
      header->setSectionResizeMode(COL_TIMELOCK, QHeaderView::Fixed);
      header->setSectionResizeMode(COL_AUTOMATION, QHeaderView::Interactive);
      header->setSectionResizeMode(COL_CLEF, QHeaderView::Interactive);
      for (unsigned i=0;i<custom_columns.size();i++)
        header->setSectionResizeMode(COL_CUSTOM_MIDICTRL_OFFSET+i, QHeaderView::Interactive);

      setHeaderToolTips();
      setHeaderWhatsThis();
      header->setSectionsMovable (true);
      header->restoreState(header_state);

      list = new TList(header, tracklist, "tracklist");

      // REMOVE Tim. Trackinfo. Added.
      //tlistSpacerItem = new QSpacerItem();
      tlistLayout = new QVBoxLayout(tracklist);
      tlistLayout->setContentsMargins(0, 0, 0, 0);
      tlistLayout->setSpacing(0);
      tlistLayout->addWidget(header);
      tlistLayout->addWidget(list);
      //tlistLayout->addSpacerItem();
      
      // REMOVE Tim. Trackinfo. Removed. Moved above.
//       // Do this now that the list is available.
//       genTrackInfo(tracklist);
      
      connect(list, SIGNAL(selectionChanged(MusECore::Track*)), SLOT(trackSelectionChanged()));
      connect(list, SIGNAL(selectionChanged(MusECore::Track*)), SIGNAL(selectionChanged()));
      connect(list, SIGNAL(selectionChanged(MusECore::Track*)), midiTrackInfo, SLOT(setTrack(MusECore::Track*)));
      connect(header, SIGNAL(sectionResized(int,int,int)), list, SLOT(redraw()));
      connect(header, SIGNAL(sectionMoved(int,int,int)), list, SLOT(redraw()));

      //  tracklist:
      //
      //         0         1         2
      //   +-----------+--------+---------+
      //   | Trackinfo | scroll | Header  | 0
      //   |           | bar    +---------+
      //   |           |        | TList   | 1
      //   +-----------+--------+---------+
      //   |             hline            | 2
      //   +-----+------------------------+
      //   | ib  |                        | 3
      //   +-----+------------------------+

      connect(infoScroll, SIGNAL(valueChanged(int)), SLOT(trackInfoScroll(int)));
// REMOVE Tim. Trackinfo. Changed.
//       tgrid  = new TLLayout(tracklist); // layout manager for this
//       tgrid->wadd(0, trackInfo);
//       tgrid->wadd(1, infoScroll);
//       tgrid->wadd(2, header);
//       tgrid->wadd(3, list);
//       tgrid->wadd(4, MusECore::hLine(tracklist));
//       tgrid->wadd(5, ib);
      //tgrid  = new TLLayout(trackInfoWidget, trackInfo, infoScroll, ib, split, header, list); // layout manager for this
      //tgrid  = new TLLayout(trackInfoWidget, trackInfo, infoScroll, ib, split, tlistLayout); // layout manager for this
      //tgrid  = new TLLayout(trackInfoWidget, trackInfo, infoScroll, split); // layout manager for this
      tgrid  = new TrackInfoLayout(trackInfoWidget, trackInfo, infoScroll, split); // layout manager for this

      //---------------------------------------------------
      //    Editor
      //---------------------------------------------------

      int offset = AL::sigmap.ticksMeasure(0);
      // REMOVE Tim. Trackinfo. Changed.
//       hscroll = new ScrollScale(-2000, -5, xscale, MusEGlobal::song->len(), Qt::Horizontal, editor, -offset);
      hscroll = new ScrollScale(-2000, -5, xscale, MusEGlobal::song->len(), Qt::Horizontal, this, -offset);
      hscroll->setFocusPolicy(Qt::NoFocus);
      // REMOVE Tim. Trackinfo. Removed. Not required now?
//       ib->setFixedHeight(hscroll->sizeHint().height());

      // REMOVE Tim. Trackinfo. Added.
      bottomHLayout = new QHBoxLayout();
      box->addLayout(bottomHLayout);
      bottomHLayout->setContentsMargins(0, 0, 0, 0);
      bottomHLayout->setSpacing(0);
      bottomHLayout->addWidget(trackInfoButton);
      bottomHLayout->addWidget(trackInfoAltButton);
      bottomHLayout->addStretch();
      bottomHLayout->addWidget(hscroll);
      
      vscroll = new QScrollBar(editor);
      vscroll->setMinimum(0);
      vscroll->setMaximum(20*20);
      vscroll->setSingleStep(5);
      vscroll->setPageStep(25); // FIXME: too small steps here for me (flo), better control via window height?
      vscroll->setValue(0);
      vscroll->setOrientation(Qt::Vertical);      

      list->setScroll(vscroll);

// REMOVE Tim. Trackinfo. Changed.
//       QGridLayout* egrid  = new QGridLayout(editor);
      egrid  = new ArrangerCanvasLayout(editor, hscroll);
      egrid->setColumnStretch(0, 50);
      egrid->setRowStretch(2, 50);
      egrid->setContentsMargins(0, 0, 0, 0);  
      egrid->setSpacing(0);  

      time = new MTScale(&_raster, editor, xscale);
      time->setOrigin(-offset, 0);
      canvas = new PartCanvas(&_raster, editor, xscale, yscale);
      canvas->setBg(MusEGlobal::config.partCanvasBg);
      canvas->setCanvasTools(arrangerTools);
      canvas->setOrigin(-offset, 0);
      canvas->setFocus();

//       QList<int> vallist;
// // REMOVE Tim. Trackinfo. Changed.
// //       vallist.append(tgrid->maximumSize().width());
//       vallist.append(tgrid->sizeHint().width());
//       vallist.append(tlistLayout->sizeHint().width());
//       //vallist.append(editor->sizeHint().width());
//       vallist.append(box->sizeHint().width() - tlistLayout->sizeHint().width() - tgrid->sizeHint().width());
//       split->setSizes(vallist);
      
      list->setFocusProxy(canvas); // Make it easy for track list popup line editor to give focus back to canvas.

      connect(canvas, SIGNAL(setUsedTool(int)), this, SIGNAL(setUsedTool(int)));
      connect(canvas, SIGNAL(trackChanged(MusECore::Track*)), list, SLOT(selectTrack(MusECore::Track*)));
      connect(list, SIGNAL(keyPressExt(QKeyEvent*)), canvas, SLOT(redirKeypress(QKeyEvent*)));
      connect(canvas, SIGNAL(selectTrackAbove()), list, SLOT(selectTrackAbove()));
      connect(canvas, SIGNAL(selectTrackBelow()), list, SLOT(selectTrackBelow()));
      connect(canvas, SIGNAL(editTrackNameSig()), list, SLOT(editTrackNameSlot()));

      connect(canvas, SIGNAL(muteSelectedTracks()), list, SLOT(muteSelectedTracksSlot()));
      connect(canvas, SIGNAL(soloSelectedTracks()), list, SLOT(soloSelectedTracksSlot()));

      connect(canvas, SIGNAL(horizontalZoom(bool, const QPoint&)), SLOT(horizontalZoom(bool, const QPoint&)));
      connect(canvas, SIGNAL(horizontalZoom(int, const QPoint&)), SLOT(horizontalZoom(int, const QPoint&)));
      connect(lenEntry,           SIGNAL(returnPressed()), SLOT(focusCanvas()));
      connect(lenEntry,           SIGNAL(escapePressed()), SLOT(focusCanvas()));
      connect(globalPitchSpinBox, SIGNAL(returnPressed()), SLOT(focusCanvas()));
      connect(globalPitchSpinBox, SIGNAL(escapePressed()), SLOT(focusCanvas()));
      connect(globalTempoSpinBox, SIGNAL(returnPressed()), SLOT(focusCanvas()));
      connect(globalTempoSpinBox, SIGNAL(escapePressed()), SLOT(focusCanvas()));
      connect(midiTrackInfo,      SIGNAL(returnPressed()), SLOT(focusCanvas()));
      connect(midiTrackInfo,      SIGNAL(escapePressed()), SLOT(focusCanvas()));
      
      //connect(this,      SIGNAL(redirectWheelEvent(QWheelEvent*)), canvas, SLOT(redirectedWheelEvent(QWheelEvent*)));
      connect(list,      SIGNAL(redirectWheelEvent(QWheelEvent*)), canvas, SLOT(redirectedWheelEvent(QWheelEvent*)));
      connect(trackInfo, SIGNAL(redirectWheelEvent(QWheelEvent*)), infoScroll, SLOT(redirectedWheelEvent(QWheelEvent*)));
      
      egrid->addWidget(time, 0, 0, 1, 2);
      egrid->addWidget(MusECore::hLine(editor), 1, 0, 1, 2);

      egrid->addWidget(canvas,  2, 0);
      egrid->addWidget(vscroll, 2, 1);
// REMOVE Tim. Trackinfo. Removed.
//       egrid->addWidget(hscroll, 3, 0, Qt::AlignBottom);

      connect(vscroll, SIGNAL(valueChanged(int)), canvas, SLOT(setYPos(int)));
      connect(hscroll, SIGNAL(scrollChanged(int)), canvas, SLOT(setXPos(int)));
      connect(hscroll, SIGNAL(scaleChanged(int)),  canvas, SLOT(setXMag(int)));
      connect(vscroll, SIGNAL(valueChanged(int)), list,   SLOT(setYPos(int)));
      connect(hscroll, SIGNAL(scrollChanged(int)), time,   SLOT(setXPos(int)));
      connect(hscroll, SIGNAL(scaleChanged(int)),  time,   SLOT(setXMag(int)));
      connect(canvas,  SIGNAL(timeChanged(unsigned)),   SLOT(setTime(unsigned)));
      connect(canvas,  SIGNAL(verticalScroll(unsigned)),SLOT(verticalScrollSetYpos(unsigned)));
      connect(canvas,  SIGNAL(horizontalScroll(unsigned)),hscroll, SLOT(setPos(unsigned)));
      connect(canvas,  SIGNAL(horizontalScrollNoLimit(unsigned)),hscroll, SLOT(setPosNoLimit(unsigned))); 
      connect(time,    SIGNAL(timeChanged(unsigned)),   SLOT(setTime(unsigned)));

      connect(list, SIGNAL(verticalScrollSetYpos(int)), vscroll, SLOT(setValue(int)));

      connect(canvas, SIGNAL(tracklistChanged()), list, SLOT(tracklistChanged()));
      connect(canvas, SIGNAL(dclickPart(MusECore::Track*)), SIGNAL(editPart(MusECore::Track*)));
      connect(canvas, SIGNAL(startEditor(MusECore::PartList*,int)),   SIGNAL(startEditor(MusECore::PartList*, int)));

      connect(MusEGlobal::song,   SIGNAL(songChanged(MusECore::SongChangedFlags_t)), SLOT(songChanged(MusECore::SongChangedFlags_t)));
      connect(canvas, SIGNAL(followEvent(int)), hscroll, SLOT(setOffset(int)));
      connect(canvas, SIGNAL(selectionChanged()), SIGNAL(selectionChanged()));
      connect(canvas, SIGNAL(dropSongFile(const QString&)), SIGNAL(dropSongFile(const QString&)));
      connect(canvas, SIGNAL(dropMidiFile(const QString&)), SIGNAL(dropMidiFile(const QString&)));

      connect(canvas, SIGNAL(toolChanged(int)), SIGNAL(toolChanged(int)));
      connect(MusEGlobal::song,   SIGNAL(controllerChanged(MusECore::Track*, int)), SLOT(controllerChanged(MusECore::Track*, int)));

      configChanged();  // set configuration values
      if(canvas->part())
        midiTrackInfo->setTrack(canvas->part()->track());   
      showTrackInfo(showTrackinfoFlag);
      
      setDefaultSplitterSizes();
      
      // Take care of some tabbies!
      setTabOrder(tempo200, trackInfo);
      setTabOrder(trackInfo, infoScroll);
      setTabOrder(infoScroll, list);
      setTabOrder(list, canvas);
      //setTabOrder(canvas, ib);
      //setTabOrder(ib, hscroll);
      }


// DELETETHIS 20
//---------------------------------------------------------
//   updateHScrollRange
//---------------------------------------------------------

//void Arranger::updateHScrollRange()
//{
//      int s = 0, e = MusEGlobal::song->len();
      // Show one more measure.
//      e += AL::sigmap.ticksMeasure(e);  
      // Show another quarter measure due to imprecise drawing at canvas end point.
//      e += AL::sigmap.ticksMeasure(e) / 4;
      // Compensate for the fixed vscroll width. 
//      e += canvas->rmapxDev(-vscroll->width()); 
//      int s1, e1;
//      hscroll->range(&s1, &e1);
//      if(s != s1 || e != e1) 
//        hscroll->setRange(s, e);
//}


//---------------------------------------------------------
//   setDefaultSplitterSizes
//---------------------------------------------------------

void Arranger::setDefaultSplitterSizes()
{
      // REMOVE Tim. Trackinfo. Added.
//       fprintf(stderr, "Arranger: w:%d sizehint w:%d min sz w:%d max sz w:%d\n", 
//               width(),
//               sizeHint().width(), 
//               minimumSize().width(),
//               maximumSize().width());
//       
//       fprintf(stderr, "Arranger: tgrid sizehint w:%d min sz w:%d max sz w:%d\n", 
//               tgrid->sizeHint().width(), 
//               tgrid->minimumSize().width(),
//               tgrid->maximumSize().width());
//       
//       fprintf(stderr, "Arranger: tlist w:%d sizehint w:%d min sz w:%d max sz w:%d\n", 
//               tracklist->width(),
//               tracklist->sizeHint().width(), 
//               tracklist->minimumSize().width(),
//               tracklist->maximumSize().width());
//       
//       fprintf(stderr, "Arranger: tlistLayout sizehint w:%d min sz w:%d max sz w:%d\n", 
//               tlistLayout->sizeHint().width(), 
//               tlistLayout->minimumSize().width(),
//               tlistLayout->maximumSize().width());
//       
//       fprintf(stderr, "Arranger: header w:%d sizehint w:%d min sz w:%d max sz w:%d\n", 
//               header->width(),
//               header->sizeHint().width(), 
//               header->minimumSize().width(),
//               header->maximumSize().width());
//       
//       fprintf(stderr, "Arranger: editor w:%d sizehint w:%d min sz w:%d max sz w:%d\n", 
//               editor->width(),
//               editor->sizeHint().width(), 
//               editor->minimumSize().width(),
//               editor->maximumSize().width());
//       
//       fprintf(stderr, "Arranger: egrid sizehint w:%d min sz w:%d max sz w:%d\n", 
//               egrid->sizeHint().width(), 
//               egrid->minimumSize().width(),
//               egrid->maximumSize().width());
      
  QList<int> vallist;
//       vallist.append(tgrid->maximumSize().width());
  
  //vallist.append(tgrid->sizeHint().width());
  vallist.append(tgrid->minimumSize().width());
  vallist.append(tlistLayout->sizeHint().width());
  
  //vallist.append(editor->sizeHint().width());
  //vallist.append(box->sizeHint().width() - tlistLayout->sizeHint().width() - tgrid->sizeHint().width());
  //vallist.append(width() - tlistLayout->sizeHint().width() - tgrid->sizeHint().width());
  
  //vallist.append(1);
  //vallist.append(1);
  vallist.append(300);
  split->setSizes(vallist);
  
//   split->setPosition(2, tgrid->sizeHint().width() + tlistLayout->sizeHint().width());
//   split->setPosition(1, tgrid->sizeHint().width());
}

//---------------------------------------------------------
//   setTime
//---------------------------------------------------------

void Arranger::setTime(unsigned tick)
      {
      if (tick == INT_MAX)
            cursorPos->setEnabled(false);
      else {
            cursVal = tick;
            cursorPos->setEnabled(true);
            cursorPos->setValue(tick);
            time->setPos(3, tick, false);
            }
      }

//---------------------------------------------------------
//   toolChange
//---------------------------------------------------------

void Arranger::setTool(int t)
      {
      canvas->setTool(t);
      }

//---------------------------------------------------------
//   dclickPart
//---------------------------------------------------------

void Arranger::dclickPart(MusECore::Track* t)
      {
      emit editPart(t);
      }

//---------------------------------------------------------
//   configChanged
//---------------------------------------------------------

void Arranger::configChanged()
      {
      if (MusEGlobal::config.canvasBgPixmap.isEmpty()) {
            canvas->setBg(MusEGlobal::config.partCanvasBg);
            canvas->setBg(QPixmap());
      }
      else {
            canvas->setBg(QPixmap(MusEGlobal::config.canvasBgPixmap));
      }
      }

//---------------------------------------------------------
//   focusCanvas
//---------------------------------------------------------

void Arranger::focusCanvas()
{
  if(MusEGlobal::config.smartFocus)
  {
    canvas->setFocus();
    canvas->activateWindow();
  }
}

//---------------------------------------------------------
//   songlenChanged
//---------------------------------------------------------

void Arranger::songlenChanged(int n)
      {
      int newLen = AL::sigmap.bar2tick(n, 0, 0);
      MusEGlobal::song->setLen(newLen);
      }
//---------------------------------------------------------
//   songChanged
//---------------------------------------------------------

void Arranger::songChanged(MusECore::SongChangedFlags_t type)
      {
      // Is it simply a midi controller value adjustment? Forget it.
      if(type != SC_MIDI_CONTROLLER)
      {
        // Try these, may need more/less. 
        if(type & ( SC_TRACK_INSERTED | SC_TRACK_REMOVED | SC_TRACK_MODIFIED | 
           SC_PART_INSERTED | SC_PART_REMOVED | SC_PART_MODIFIED))  
        {
          unsigned endTick = MusEGlobal::song->len();
          int offset  = AL::sigmap.ticksMeasure(endTick);
          hscroll->setRange(-offset, endTick + offset);  //DEBUG
          canvas->setOrigin(-offset, 0);
          time->setOrigin(-offset, 0);
    
          int bar, beat;
          unsigned tick;
          AL::sigmap.tickValues(endTick, &bar, &beat, &tick);
          if (tick || beat)
                ++bar;
          lenEntry->blockSignals(true);
          lenEntry->setValue(bar);
          lenEntry->blockSignals(false);
        }
        
        if(type & (SC_TRACK_SELECTION | SC_TRACK_INSERTED | SC_TRACK_REMOVED | SC_TRACK_MODIFIED))
          trackSelectionChanged();
        
        // Keep this light, partsChanged is a heavy move! Try these, may need more. Maybe sig. Requires tempo.
        if(type & (SC_TRACK_INSERTED | SC_TRACK_REMOVED | SC_TRACK_MODIFIED | 
                   SC_PART_INSERTED | SC_PART_REMOVED | SC_PART_MODIFIED | 
                   SC_SIG | SC_TEMPO | SC_MASTER)) 
          canvas->partsChanged();
        
        if (type & SC_SIG)
              time->redraw();
        if (type & SC_TEMPO)
              setGlobalTempo(MusEGlobal::tempomap.globalTempo());
              
        if(type & SC_TRACK_REMOVED)
        {
          {
            AudioStrip* w = static_cast<AudioStrip*>(trackInfo->getWidget(2));
            if(w)
            {
              MusECore::Track* t = w->getTrack();
              if(t)
              {
                MusECore::TrackList* tl = MusEGlobal::song->tracks();
                MusECore::iTrack it = tl->find(t);
                if(it == tl->end())
                {
                  delete w;
                  trackInfo->addWidget(0, 2);
                  //trackInfo->insertWidget(2, 0);
                  selected = 0;
                } 
              }   
            } 
          }
          
          // REMOVE Tim. Trackinfo. Added.
          {
            MidiStrip* w = static_cast<MidiStrip*>(trackInfo->getWidget(3));
            if(w)
            {
              MusECore::Track* t = w->getTrack();
              if(t)
              {
                //MusECore::TrackList* tl = MusEGlobal::song->tracks();
                MusECore::MidiTrackList* tl = MusEGlobal::song->midis();
                //MusECore::iTrack it = tl->find(t);
                MusECore::iMidiTrack it = tl->find(t);
                if(it == tl->end())
                {
                  delete w;
                  trackInfo->addWidget(0, 3);
                  //trackInfo->insertWidget(3, 0);
                  selected = 0;
                } 
              }   
            } 
          }
        }
        
        // Try these:
        if(type & (SC_PART_INSERTED | SC_PART_REMOVED | SC_PART_MODIFIED | 
                   SC_EVENT_INSERTED | SC_EVENT_REMOVED | SC_EVENT_MODIFIED |
                   SC_CLIP_MODIFIED))
        canvas->redraw();
        
      }
            
      updateTrackInfo(type);
    }

//---------------------------------------------------------
//   trackSelectionChanged
//---------------------------------------------------------

void Arranger::trackSelectionChanged()
      {
// REMOVE Tim. Trackinfo. Removed
//       MusECore::TrackList* tracks = MusEGlobal::song->tracks();
//       MusECore::Track* track = 0;
//       for (MusECore::iTrack t = tracks->begin(); t != tracks->end(); ++t) {
//             if ((*t)->selected()) {
//                   track = *t;
//                   break;
//                   }
//             }
      MusECore::Track* track = MusEGlobal::song->tracks()->currentSelection();
      if (track == selected)
            return;
      selected = track;
      updateTrackInfo(-1);
      }

//---------------------------------------------------------
//   writeStatus
//---------------------------------------------------------

void Arranger::writeStatus(int level, MusECore::Xml& xml)
      {
      xml.tag(level++, "arranger");
      xml.intTag(level, "raster", _raster);
      xml.intTag(level, "info", trackInfoButton->isChecked());
      split->writeStatus(level, xml);

      xml.intTag(level, "xmag", hscroll->mag());
      xml.intTag(level, "xpos", hscroll->pos());
      xml.intTag(level, "ypos", vscroll->value());
      xml.etag(level, "arranger");
      }

void Arranger::writeConfiguration(int level, MusECore::Xml& xml)
      {
      xml.tag(level++, "arranger");
      writeCustomColumns(level, xml);
      xml.strTag(level, "tlist_header", header->saveState().toHex().constData());
      xml.etag(level, "arranger");
      }

//---------------------------------------------------------
//   readConfiguration
//---------------------------------------------------------

void Arranger::readConfiguration(MusECore::Xml& xml)
      {
      for (;;) {
            MusECore::Xml::Token token(xml.parse());
            const QString& tag(xml.s1());
            switch (token) {
                  case MusECore::Xml::Error:
                  case MusECore::Xml::End:
                        return;
                  case MusECore::Xml::TagStart:
                        if (tag == "tlist_header")
                              header_state = QByteArray::fromHex(xml.parse1().toLatin1());
                        else if (tag == "custom_columns")
                              readCustomColumns(xml);
                        else
                              xml.unknown("Arranger");
                        break;
                  case MusECore::Xml::TagEnd:
                        if (tag == "arranger")
                              return;
                  default:
                        break;
                  }
            }
      }

//---------------------------------------------------------
//   readStatus
//---------------------------------------------------------

void Arranger::readStatus(MusECore::Xml& xml)
      {
      int rast = -1;  
      for (;;) {
            MusECore::Xml::Token token(xml.parse());
            const QString& tag(xml.s1());
            switch (token) {
                  case MusECore::Xml::Error:
                  case MusECore::Xml::End:
                        return;
                  case MusECore::Xml::TagStart:
                        if (tag == "raster")
                              rast = xml.parseInt();
                        else if (tag == "info")
                              showTrackinfoFlag = xml.parseInt();
                        else if (tag == split->objectName())
                              split->readStatus(xml);
                        else if (tag == "xmag")
                              hscroll->setMag(xml.parseInt());
                        else if (tag == "xpos") 
                              hscroll->setPos(xml.parseInt());  
                        else if (tag == "ypos")
                              vscroll->setValue(xml.parseInt());
                        else
                              xml.unknown("Arranger");
                        break;
                  case MusECore::Xml::TagEnd:
                        if (tag == "arranger") {
                              trackInfoButton->setChecked(showTrackinfoFlag);
                              if(rast != -1)
                                setRasterVal(rast);
                              return;
                              }
                  default:
                        break;
                  }
            }
      }

//---------------------------------------------------------
//   rasterChanged
//---------------------------------------------------------

void Arranger::rasterChanged(int index)
      {
      _raster = gArrangerRasterTable[index];
      // Set the audio record part snapping.
      MusEGlobal::song->setArrangerRaster(_raster);
      canvas->redraw();
      focusCanvas();
      }

//---------------------------------------------------------
//   setRasterVal
//---------------------------------------------------------

bool Arranger::setRasterVal(int val)
{
  if(_raster == val)
    return true;
  int idx = _rasterCombo->findData(val);
  if(idx == -1)
  {
    fprintf(stderr, "Arranger::setRasterVal raster:%d not found\n", val);
    return false;
  }
  _raster = val;
  _rasterCombo->blockSignals(true);
  _rasterCombo->setCurrentIndex(idx);
  _rasterCombo->blockSignals(false);
  // Set the audio record part snapping.
  MusEGlobal::song->setArrangerRaster(_raster);
  canvas->redraw();
  return true;
}

//---------------------------------------------------------
//   reset
//---------------------------------------------------------

void Arranger::reset()
      {
      canvas->setXPos(0);
      canvas->setYPos(0);
      hscroll->setPos(0);
      vscroll->setValue(0);
      time->setXPos(0);
      time->setYPos(0);
      }

//---------------------------------------------------------
//   cmd
//---------------------------------------------------------

void Arranger::cmd(int cmd)
      {
      int ncmd;
      switch (cmd) {
            case CMD_CUT_PART:
                  ncmd = PartCanvas::CMD_CUT_PART;
                  break;
            case CMD_COPY_PART:
                  ncmd = PartCanvas::CMD_COPY_PART;
                  break;
            case CMD_COPY_PART_IN_RANGE:
                  ncmd = PartCanvas::CMD_COPY_PART_IN_RANGE;
                  break;
            case CMD_PASTE_PART:
                  ncmd = PartCanvas::CMD_PASTE_PART;
                  break;
            case CMD_PASTE_CLONE_PART:
                  ncmd = PartCanvas::CMD_PASTE_CLONE_PART;
                  break;
            case CMD_PASTE_PART_TO_TRACK:
                  ncmd = PartCanvas::CMD_PASTE_PART_TO_TRACK;
                  break;
            case CMD_PASTE_CLONE_PART_TO_TRACK:
                  ncmd = PartCanvas::CMD_PASTE_CLONE_PART_TO_TRACK;
                  break;
            case CMD_PASTE_DIALOG:
                  ncmd = PartCanvas::CMD_PASTE_DIALOG;
                  break;
            case CMD_INSERT_EMPTYMEAS:
                  ncmd = PartCanvas::CMD_INSERT_EMPTYMEAS;
                  break;
            default:
                  return;
            }
      canvas->cmd(ncmd);
      }

//---------------------------------------------------------
//   globalPitchChanged
//---------------------------------------------------------

void Arranger::globalPitchChanged(int val)
      {
      MusEGlobal::song->setGlobalPitchShift(val);
      }

//---------------------------------------------------------
//   globalTempoChanged
//---------------------------------------------------------

void Arranger::globalTempoChanged(int val)
      {
      MusEGlobal::audio->msgSetGlobalTempo(val);
      }

//---------------------------------------------------------
//   setTempo50
//---------------------------------------------------------

void Arranger::setTempo50()
      {
      MusEGlobal::audio->msgSetGlobalTempo(50);
      }

//---------------------------------------------------------
//   setTempo100
//---------------------------------------------------------

void Arranger::setTempo100()
      {
      MusEGlobal::audio->msgSetGlobalTempo(100);
      }

//---------------------------------------------------------
//   setTempo200
//---------------------------------------------------------

void Arranger::setTempo200()
      {
      MusEGlobal::audio->msgSetGlobalTempo(200);
      }

//---------------------------------------------------------
//   setGlobalTempo
//---------------------------------------------------------

void Arranger::setGlobalTempo(int val)
      {
      if(val != globalTempoSpinBox->value())
      {
        globalTempoSpinBox->blockSignals(true);
        globalTempoSpinBox->setValue(val);
        globalTempoSpinBox->blockSignals(false);
      }
      }

//---------------------------------------------------------
//   verticalScrollSetYpos
//---------------------------------------------------------
void Arranger::verticalScrollSetYpos(unsigned ypos)
      {
      vscroll->setValue(ypos);
      }

//---------------------------------------------------------
//   trackInfoScroll
//---------------------------------------------------------

void Arranger::trackInfoScroll(int y)
      {
      if (trackInfo->visibleWidget())
            trackInfo->visibleWidget()->move(0, -y);
      }

//---------------------------------------------------------
//   clear
//---------------------------------------------------------

void Arranger::clear()
      {
        
      {
        AudioStrip* w = static_cast<AudioStrip*>(trackInfo->getWidget(2));
        if (w)
              delete w;
        trackInfo->addWidget(0, 2);
      }
      
      // REMOVE Tim. Trackinfo. Added.
      {
        MidiStrip* w = static_cast<MidiStrip*>(trackInfo->getWidget(3));
        if (w)
              delete w;
        trackInfo->addWidget(0, 3);
      }
      
      selected = 0;
      midiTrackInfo->setTrack(0);
      }

//void Arranger::wheelEvent(QWheelEvent* ev)
//      {
//      emit redirectWheelEvent(ev);
//      }

void Arranger::controllerChanged(MusECore::Track *t, int ctrlId)
{
      canvas->controllerChanged(t, ctrlId);
}

//---------------------------------------------------------
//   showTrackInfo
//---------------------------------------------------------

void Arranger::showTrackInfo(bool flag)
      {
      showTrackinfoFlag = flag;
// REMOVE Tim. Trackinfo. Changed.
//       trackInfo->setVisible(flag);
//       infoScroll->setVisible(flag);
      trackInfoWidget->setVisible(flag);
      updateTrackInfo(-1);
      }

// REMOVE Tim. Trackinfo. Added.
//---------------------------------------------------------
//   showTrackInfo
//---------------------------------------------------------

void Arranger::showTrackInfoAlt(bool flag)
      {
      showTrackinfoAltFlag = flag;
      updateTrackInfo(-1);
//       if(flag)
//         updateTrackInfo(0);
//       else
//         updateTrackInfo(0);
      }

//---------------------------------------------------------
//   genTrackInfo
//---------------------------------------------------------

void Arranger::genTrackInfo(QWidget* parent)
      {
      trackInfo = new WidgetStack(parent, "trackInfoStack");
      // REMOVE Tim. Trackinfo. Added.
      // Set it to adjust to the current visible widget, rather than the max of all of them.
      trackInfo->setSizeHintMode(WidgetStack::VisibleHint);

      noTrackInfo          = new QWidget(trackInfo);
      noTrackInfo->setAutoFillBackground(true);
      QPixmap *noInfoPix   = new QPixmap(160, 1000);
      const QPixmap *logo  = new QPixmap(*museLeftSideLogo);
      noInfoPix->fill(noTrackInfo->palette().color(QPalette::Window) );
      QPainter p(noInfoPix);
      p.drawPixmap(10, 0, *logo, 0,0, logo->width(), logo->height());

      QPalette palette;
      palette.setBrush(noTrackInfo->backgroundRole(), QBrush(*noInfoPix));
      noTrackInfo->setPalette(palette);
      noTrackInfo->setGeometry(0, 0, 65, 200);
      noTrackInfo->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding));

      midiTrackInfo = new MidiTrackInfo(trackInfo);
      
      trackInfo->addWidget(noTrackInfo,   0);
      trackInfo->addWidget(midiTrackInfo, 1);
      trackInfo->addWidget(0, 2);  // AudioStrip placeholder.
      trackInfo->addWidget(0, 3);  // MidiStrip placeholder.
      }

//---------------------------------------------------------
//   updateTrackInfo
//---------------------------------------------------------

void Arranger::updateTrackInfo(MusECore::SongChangedFlags_t flags)
      {
      if (!showTrackinfoFlag) {
            switchInfo(-1);
            return;
            }
      if (selected == 0) {
            switchInfo(0);
            return;
            }
      if (selected->isMidiTrack()) {
// REMOVE Tim. Trackinfo. Changed. TODO Maybe keep for later as part of switching stack.
//             switchInfo(1);
//             // If a different part was selected
//             if(midiTrackInfo->track() != selected)
//               // Set a new track and do a complete update.
//               midiTrackInfo->setTrack(selected);
//             else  
//               // Otherwise just regular update with specific flags.
//               midiTrackInfo->updateTrackInfo(flags);
            if(showTrackinfoAltFlag)
            {
              switchInfo(1);
              // If a different part was selected
              if(midiTrackInfo->track() != selected)
                // Set a new track and do a complete update.
                midiTrackInfo->setTrack(selected);
              else  
                // Otherwise just regular update with specific flags.
                midiTrackInfo->updateTrackInfo(flags);
            }
            else
              switchInfo(3);
            }
      else {
            switchInfo(2);
            }
      }

//---------------------------------------------------------
//   switchInfo
//---------------------------------------------------------

void Arranger::switchInfo(int n)
      {
      if (n == 2) {
          {
            MidiStrip* w = static_cast<MidiStrip*>(trackInfo->getWidget(3));
            if (w)
            {
              //fprintf(stderr, "Arranger::switchInfo audio strip: deleting midi strip\n");
              delete w;
              //w->deleteLater();
              trackInfo->addWidget(0, 3);
            }
          }
          {
              AudioStrip* w = static_cast<AudioStrip*>(trackInfo->getWidget(2));
              if (w == 0 || selected != w->getTrack()) {
                    if (w)
                    {
                          //fprintf(stderr, "Arranger::switchInfo deleting strip\n");
                          //delete w;
                          w->deleteLater();
                    }
                    w = new AudioStrip(trackInfo, static_cast<MusECore::AudioTrack*>(selected));
                    //w->setFocusPolicy(Qt::TabFocus);
                    connect(MusEGlobal::song, SIGNAL(songChanged(MusECore::SongChangedFlags_t)), w, SLOT(songChanged(MusECore::SongChangedFlags_t)));
                    connect(MusEGlobal::muse, SIGNAL(configChanged()), w, SLOT(configChanged()));
  //                   w->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed));  // REMOVE Tim. Trackinfo. Changed.
                    w->setSizePolicy(QSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed));
                    trackInfo->addWidget(w, 2);
                    w->show();
                    //setTabOrder(midiTrackInfo, w);
                    tgrid->activate();
                    tgrid->update();   // muse-2 Qt4
                    }
          }
        }

      // REMOVE Tim. Trackinfo. Added.
      else if (n == 3) {
          {
            AudioStrip* w = static_cast<AudioStrip*>(trackInfo->getWidget(2));
            if (w)
            {
              //fprintf(stderr, "Arranger::switchInfo midi strip: deleting audio strip\n");
              delete w;
              //w->deleteLater();
              trackInfo->addWidget(0, 2);
            }
          }
          {
            MidiStrip* w = static_cast<MidiStrip*>(trackInfo->getWidget(3));
            if (w == 0 || selected != w->getTrack()) {
                  if (w)
                  {
                        //fprintf(stderr, "Arranger::switchInfo deleting strip\n");
                        //delete w;
                        w->deleteLater();
                  }
                  w = new MidiStrip(trackInfo, static_cast<MusECore::MidiTrack*>(selected));
                  //w->setFocusPolicy(Qt::TabFocus);
                  connect(MusEGlobal::song, SIGNAL(songChanged(MusECore::SongChangedFlags_t)), w, SLOT(songChanged(MusECore::SongChangedFlags_t)));
                  connect(MusEGlobal::muse, SIGNAL(configChanged()), w, SLOT(configChanged()));
//                   w->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed));  // REMOVE Tim. Trackinfo. Changed.
                  w->setSizePolicy(QSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed));
                  trackInfo->addWidget(w, 3);
                  w->show();
                  //setTabOrder(midiTrackInfo, w);
                  tgrid->activate();
                  tgrid->update();   // muse-2 Qt4
                  }
          }
        }
            
      if (trackInfo->curIdx() == n)
            return;
      trackInfo->raiseWidget(n);
      tgrid->activate();
      tgrid->update();   // muse-2 Qt4
      }

void Arranger::keyPressEvent(QKeyEvent* event)
{
  int key = event->key();
  if (((QInputEvent*)event)->modifiers() & Qt::ShiftModifier)
        key += Qt::SHIFT;
  if (((QInputEvent*)event)->modifiers() & Qt::AltModifier)
        key += Qt::ALT;
  if (((QInputEvent*)event)->modifiers() & Qt::ControlModifier)
        key+= Qt::CTRL;

  if (key == shortcuts[SHRT_ZOOM_IN].key) {
        horizontalZoom(true, QCursor::pos());
        return;
        }
  else if (key == shortcuts[SHRT_ZOOM_OUT].key) {
        horizontalZoom(false, QCursor::pos());
        return;
        }

  QWidget::keyPressEvent(event);
}

void Arranger::horizontalZoom(bool zoom_in, const QPoint& glob_pos)
{
  int mag = hscroll->mag();
  int zoomlvl = ScrollScale::getQuickZoomLevel(mag);
  if(zoom_in)
  {
    if (zoomlvl < MusEGui::ScrollScale::zoomLevels-1)
        zoomlvl++;
  }
  else
  {
    if (zoomlvl > 1)
        zoomlvl--;
  }
  int newmag = ScrollScale::convertQuickZoomLevelToMag(zoomlvl);

  QPoint cp = canvas->mapFromGlobal(glob_pos);
  QPoint sp = editor->mapFromGlobal(glob_pos);
  if(cp.x() >= 0 && cp.x() < canvas->width() && sp.y() >= 0 && sp.y() < editor->height())
    hscroll->setMag(newmag, cp.x());
}

void Arranger::horizontalZoom(int mag, const QPoint& glob_pos)
{
  QPoint cp = canvas->mapFromGlobal(glob_pos);
  QPoint sp = editor->mapFromGlobal(glob_pos);
  if(cp.x() >= 0 && cp.x() < canvas->width() && sp.y() >= 0 && sp.y() < editor->height())
    hscroll->setMag(hscroll->mag() + mag, cp.x());
}

} // namespace MusEGui
