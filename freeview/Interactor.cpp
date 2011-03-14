/**
 * @file  Interactor.cpp
 * @brief Base Interactor class manage mouse and key input in render view.
 *
 */
/*
 * Original Author: Ruopeng Wang
 * CVS Revision Info:
 *    $Author: rpwang $
 *    $Date: 2011/03/14 21:20:57 $
 *    $Revision: 1.12 $
 *
 * Copyright (C) 2008-2009,
 * The General Hospital Corporation (Boston, MA).
 * All rights reserved.
 *
 * Distribution, usage and copying of this software is covered under the
 * terms found in the License Agreement file named 'COPYING' found in the
 * FreeSurfer source code root directory, and duplicated here:
 * https://surfer.nmr.mgh.harvard.edu/fswiki/FreeSurferOpenSourceLicense
 *
 * General inquiries: freesurfer@nmr.mgh.harvard.edu
 * Bug reports: analysis-bugs@nmr.mgh.harvard.edu
 *
 */

#include "Interactor.h"
#include "RenderView.h"
#include "CursorFactory.h"
#include <vtkRenderer.h>
#include <QDebug>

#ifdef Q_WS_MAC
Qt::KeyboardModifier Interactor::CONTROL_MODIFIER = Qt::MetaModifier;
Qt::Key Interactor::CONTROL_KEY = Qt::Key_Meta;
#else
Qt::KeyboardModifier Interactor::CONTROL_MODIFIER = Qt::ControlModifier;
Qt::Key Interactor::CONTROL_KEY = Qt::Key_Control;
#endif

Interactor::Interactor( QObject* parent ) : QObject( parent )
{
  m_nAction = 0;
}

Interactor::~Interactor()
{}

int Interactor::GetAction()
{
  return m_nAction;
}

void Interactor::SetAction( int nAction )
{
  m_nAction = nAction;
}

void Interactor::SetUseCommandControl(bool b)
{
#ifdef Q_WS_MAC
    if (b)
    {
        CONTROL_MODIFIER = Qt::ControlModifier;
        CONTROL_KEY = Qt::Key_Control;
    }
    else
    {
       CONTROL_MODIFIER = Qt::MetaModifier;
       CONTROL_KEY = Qt::Key_Meta;
    }
#endif
}

bool Interactor::ProcessMouseDownEvent( QMouseEvent* event, RenderView* view )
{
  if ( event->button() == Qt::RightButton && event->modifiers() == Qt::NoModifier) //!(event->modifiers() | Qt::AltModifier) )
  {
    m_nDownPosX = event->x();
    m_nDownPosY = event->y();
  }
  UpdateCursor( event, view );

  return true;
}

bool Interactor::ProcessMouseUpEvent( QMouseEvent* event, RenderView* view )
{
  if ( event->button() == Qt::RightButton && m_nDownPosX == event->x() && m_nDownPosY == event->y() )
  {
    view->TriggerContextMenu( event );
    return true;
  }
  else
  {
    UpdateCursor( event, view );
    return true;
  }

  return true;
}

bool Interactor::ProcessMouseMoveEvent( QMouseEvent* event, RenderView* view )
{
  UpdateCursor( event, view );

  return true;
}

bool Interactor::ProcessKeyDownEvent( QKeyEvent* event, RenderView* view )
{
  UpdateCursor( event, view );

  return true;
}

bool Interactor::ProcessKeyUpEvent( QKeyEvent* event, RenderView* view )
{
  UpdateCursor( event, view );

  return true;
}

bool Interactor::ProcessMouseWheelEvent( QWheelEvent* event, RenderView* view )
{
  UpdateCursor( event, view );

  return true;
}

bool Interactor::ProcessMouseEnterEvent( QEvent* event, RenderView* view )
{
  UpdateCursor( event, view );

  return true;
}

bool Interactor::ProcessMouseLeaveEvent( QEvent* event, RenderView* view )
{
  UpdateCursor( event, view );

  return true;
}

void Interactor::UpdateCursor( QEvent* event, QWidget* wnd )
{
    if ( (event->type() == QEvent::MouseButtonPress && ((QMouseEvent*)event)->button() == Qt::MidButton) ||
         (event->type() == QEvent::MouseMove && ((QMouseEvent*)event)->buttons() & Qt::MidButton ))
  {
    wnd->setCursor( CursorFactory::CursorPan );
  }
  else if ((event->type() == QEvent::MouseButtonPress && ((QMouseEvent*)event)->button() == Qt::RightButton) ||
          (event->type() == QEvent::MouseMove && ((QMouseEvent*)event)->buttons() & Qt::RightButton ))
  {
    wnd->setCursor( CursorFactory::CursorZoom );
  }
  else if ( event->type() == QEvent::Wheel )
  {
    wnd->setCursor( CursorFactory::CursorZoom );
  }
  else
    wnd->unsetCursor();
}
