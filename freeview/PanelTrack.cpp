#include "PanelTrack.h"
#include "ui_PanelTrack.h"
#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "LayerTrack.h"
#include "MyUtils.h"
#include "LayerCollection.h"

PanelTrack::PanelTrack(QWidget *parent) :
    PanelLayer(parent),
    ui(new Ui::PanelTrack)
{
    ui->setupUi(this);
    MainWindow* mainwnd = MainWindow::GetMainWindow();
    if (mainwnd)
    {
        ui->toolbar->addAction(mainwnd->ui->actionLoadTrack);
        ui->toolbar->addAction(mainwnd->ui->actionCloseTrack);
    }

    LayerCollection* lc = mainwnd->GetLayerCollection("Track");
    PanelLayer::InitializeLayerList( ui->treeWidgetLayers, lc );
}

PanelTrack::~PanelTrack()
{
    delete ui;
}

void PanelTrack::ConnectLayer(Layer *layer_in)
{
    PanelLayer::ConnectLayer( layer_in );

    LayerTrack* layer = qobject_cast<LayerTrack*>(layer_in);
    if ( !layer )
        return;
}

void PanelTrack::DoUpdateWidgets()
{
    BlockAllSignals( true );
    for ( int i = 0; i < ui->treeWidgetLayers->topLevelItemCount(); i++ )
    {
        QTreeWidgetItem* item = ui->treeWidgetLayers->topLevelItem( i );
        Layer* layer = qobject_cast<Layer*>( item->data(0, Qt::UserRole).value<QObject*>() );
        if ( layer )
        {
            item->setCheckState( 0, (layer->IsVisible() ? Qt::Checked : Qt::Unchecked) );
        }
    }

    LayerTrack* layer = GetCurrentLayer<LayerTrack*>();
    for ( int i = 0; i < this->allWidgets.size(); i++ )
    {
        if ( allWidgets[i] != ui->toolbar && allWidgets[i]->parentWidget() != ui->toolbar )
            allWidgets[i]->setEnabled(layer);
    }

    ui->lineEditFileName->clear();
    if ( layer )
    {
      ui->lineEditFileName->setText( MyUtils::Win32PathProof(layer->GetFileName()) );
      ui->lineEditFileName->setCursorPosition( ui->lineEditFileName->text().size() );
    }
    ui->labelFileName->setEnabled( layer );
    ui->lineEditFileName->setEnabled( layer );

    BlockAllSignals( false );
}

void PanelTrack::DoIdle()
{

}
