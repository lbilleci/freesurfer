/**
 * @file  VolumeCropper.h
 * @brief Class to crop volume.
 *
 */
/*
 * Original Author: Ruopeng Wang
 * CVS Revision Info:
 *    $Author: rpwang $
 *    $Date: 2011/03/14 21:20:59 $
 *    $Revision: 1.7 $
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

#ifndef VolumeCropper_h
#define VolumeCropper_h

#include "vtkSmartPointer.h"
#include <QObject>

class vtkBox;
class vtkCubeSource;
class vtkSphereSource;
class vtkPlaneSource;
class vtkActor;
class vtkProp;
class vtkRenderer;
class vtkClipPolyData;
class LayerMRI;
class RenderView;
class RenderView2D;

class VolumeCropper : public QObject
{
    Q_OBJECT
public:
  VolumeCropper( QObject* parent = NULL );
  virtual ~VolumeCropper();
  
  void SetVolume( LayerMRI* mri );
  
  LayerMRI* GetVolume()
  {
    return m_mri;
  }
  
  void SetEnabled( bool bEnable );
  
  bool GetEnabled()
  {
    return m_bEnabled;
  }
  
  void Show( bool bShow = true );
  
  vtkActor* GetProp();
  
  void Append3DProps( vtkRenderer* renderer );
  void Append2DProps( vtkRenderer* renderer, int n );
  
  bool IsShown();
  
  bool PickActiveBound( vtkProp* prop );   
  void MoveActiveBound( RenderView* view, int nx, int ny );
  void ReleaseActiveBound(); 
  
  bool PickActiveBound2D( RenderView2D* view, int nX, int nY );
  void MoveActiveBound2D( RenderView2D* view, int x, int y );
  
  double* GetBounds()
  {
    return m_bounds;
  }
  
  int* GetExtent()
  {
    return m_extent;
  }
  
  void SetExtent( int nComp, int nValue );
  
  void UpdateProps();
  
  int GetActivePlane()
  {
    return m_nActivePlane;
  }
  
Q_SIGNALS:
  void CropBoundChanged( LayerMRI* );

public slots:
  void Reset();
  void Apply();

protected: 
  void UpdateExtent();
  void UpdateSliceActorVisibility();
  void UpdateActivePlane();
  void ValidateActiveBound();
  
  vtkSmartPointer<vtkBox>         m_box;
  vtkSmartPointer<vtkCubeSource>  m_boxSource;
  vtkSmartPointer<vtkPlaneSource> m_planeSource;
  vtkSmartPointer<vtkActor>     m_actorBox;
  vtkSmartPointer<vtkActor>     m_actorFrame;
  vtkSmartPointer<vtkActor>     m_actorActivePlane;
  vtkSmartPointer<vtkActor>     m_actorSphere[6];
  vtkSmartPointer<vtkActor>     m_actorBox2D;
  vtkSmartPointer<vtkActor>     m_actorFrame2D;
  vtkSmartPointer<vtkActor>     m_actorActivePlane2D[3];
  vtkSmartPointer<vtkSphereSource>  m_sphereSource[6];
  vtkSmartPointer<vtkClipPolyData>  m_clipper;
  
  LayerMRI*         m_mri;
  double            m_bounds[6];
  int               m_extent[6];
  bool              m_bEnabled;
  
  int               m_nActivePlane;
};

#endif


