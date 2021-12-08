#include "eventcaptioneditor.h"
#include "ui_eventcaptioneditor.h"

//---------------------------------------------------------------------------
// Constructor
//---------------------------------------------------------------------------
EventCaptionEditor::EventCaptionEditor(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::EventCaptionEditor)
{
    ui->setupUi(this);

    // Tree view
    ui->TW_TreeWidget->setColumnWidth(0, 80);
    ui->TW_TreeWidget->setColumnWidth(1, 80);
    ui->TW_TreeWidget->setColumnWidth(2, 100);
    ui->TW_TreeWidget->setColumnWidth(3, 52);
    ui->TW_TreeWidget->setColumnWidth(4, 52);

    // Create a label layout on top of the subtitle background
    QHBoxLayout* textHLayout = new QHBoxLayout(ui->L_Preview);
    textHLayout->addStretch();
    textHLayout->addStretch();
    QFontDatabase::addApplicationFont(":/resources/FOT-SeuratPro-B.otf");
    m_previewLabel = new QLabel();
    m_previewLabel->setStyleSheet("font: 15px \"FOT-Seurat Pro B\"; color: white;");
    textHLayout->insertWidget(1, m_previewLabel);
    m_previewIndex = 0;

    ResetEditor();
}

//---------------------------------------------------------------------------
// Destructor
//---------------------------------------------------------------------------
EventCaptionEditor::~EventCaptionEditor()
{
    delete ui;
}

//---------------------------------------------------------------------------
// Event filter
//---------------------------------------------------------------------------
bool EventCaptionEditor::eventFilter(QObject *object, QEvent *event)
{
    if (event->type() == QEvent::HoverEnter || event->type() == QEvent::HoverLeave || event->type() == QEvent::FocusIn)
    {
        if (m_previewIndex >= ui->VL_Cell->layout()->count() - 1)
        {
            m_previewIndex = 0;
            SetPreviewText("");
        }

        QWidget* widget = qobject_cast<QWidget*>(object);
        int index = ui->VL_Group->indexOf(widget);
        if (index == -1) index = ui->VL_Cell->indexOf(widget);
        if (index == -1) index = ui->VL_Start->indexOf(widget);
        if (index == -1) index = ui->VL_StartTime->indexOf(widget);
        if (index == -1) index = ui->VL_End->indexOf(widget);
        if (index == -1) index = ui->VL_EndTime->indexOf(widget);
        if (index == -1) index = ui->VL_Length->indexOf(widget);
        if (index == -1) index = ui->VL_Delete->indexOf(widget);

        if (event->type() == QEvent::FocusIn)
        {
            m_previewIndex = index;
        }

        if (event->type() == QEvent::HoverLeave)
        {
            if (m_previewIndex > 0)
            {
                index = m_previewIndex;
            }
            else
            {
                m_previewIndex = 0;
                index = -1;
                SetPreviewText("");
            }
        }

        if (index != -1)
        {
            UpdateSubtitlePreview(index);
        }
    }

    return false;
}

//---------------------------------------------------------------------------
// Set preview subtitle
//---------------------------------------------------------------------------
void EventCaptionEditor::SetPreviewText(QString _text)
{
    m_previewLabel->setText(_text);
}

//---------------------------------------------------------------------------
// Reset Editor
//---------------------------------------------------------------------------
void EventCaptionEditor::ResetEditor()
{
    // Default variables
    m_id = -1;
    m_path.clear();
    m_fileName.clear();
    m_edited = false;
    m_sortAscending = true;

    // Line text
    ui->L_FileName->setText("File Name: ---");
    ui->LE_fcoFile->setText("");
    ui->LE_Cutscene->setText("");
    ui->LE_Group->setText("");
    ui->TW_TreeWidget->clear();

    // Preview
    m_previewIndex = 0;
    SetPreviewText("");

    ClearSubtitles();
    CheckAllEnablility();
}

//---------------------------------------------------------------------------
// Open .cap file
//---------------------------------------------------------------------------
void EventCaptionEditor::on_PB_Open_clicked()
{
    QString path = "";
    if (!m_path.isEmpty())
    {
        path = m_path;
    }

    QString capFile = QFileDialog::getOpenFileName(this, tr("Open"), path, "CAP File (*.cap)");
    if (capFile == Q_NULLPTR) return;

    ResetEditor();

    // Save directory
    QFileInfo info(capFile);
    m_path = info.dir().absolutePath();

    // Read .cap file
    QDomDocument document;
    QFile file(capFile);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QMessageBox::critical(this, "Error", "Fail to load .cap file!", QMessageBox::Ok);
        ResetEditor();
        return;
    }
    else
    {
        document.setContent(&file);
        file.close();
    }

    // version="1.0" encoding="utf-8" standalone="no"
    QDomProcessingInstruction processingInstruction = document.firstChild().toProcessingInstruction();
    QString instruction = processingInstruction.data();

    // Read associated fco file name
    QDomElement captionList = document.firstChildElement();
    QString fcoFile = captionList.attribute("File");
    if (!fcoFile.isEmpty())
    {
        ui->LE_fcoFile->setText(fcoFile);
    }
    else
    {
        QMessageBox::critical(this, "Error", "CaptionList missing attribute \"File\"!", QMessageBox::Ok);
        ResetEditor();
        return;
    }

    // Captions
    QDomNodeList captions = captionList.elementsByTagName("Caption");
    for (int i = 0; i < captions.count(); i++)
    {
        QDomElement caption = captions.at(i).toElement();
        QString name = caption.attribute("Name");
        QString groupDefault = caption.attribute("Group");
        if (!name.isEmpty())
        {
            TW_AddCutscene(name, groupDefault);
        }
        else
        {
            QMessageBox::critical(this, "Error", "Caption missing attribute \"Name\"!", QMessageBox::Ok);
            ResetEditor();
            return;
        }

        QDomNodeList texts = caption.elementsByTagName("Text");
        for (int j = 0; j < texts.count(); j++)
        {
            QDomElement text = texts.at(j).toElement();
            QString start = text.attribute("Start");
            QString length = text.attribute("Length");
            QString cell = text.attribute("Cell");
            QString groupOverride = text.attribute("Group");

            bool valid = !start.isEmpty() && !length.isEmpty() && !cell.isEmpty();
            if (valid)
            {
                bool startIsNumber = false;
                start.toInt(&startIsNumber);
                bool lengthIsNumber = false;
                length.toInt(&lengthIsNumber);

                valid &= startIsNumber;
                valid &= lengthIsNumber;
            }

            if (valid)
            {
                if (groupOverride.isEmpty())
                {
                    if (groupDefault.isEmpty())
                    {
                        QMessageBox::warning(this, "Warning", "In Caption (Name: " + name + ") -> Text (Start: " + start + ", Length: " + length + ", Cell: " + cell + "):\nGroup override is empty and no default to fall back to, adding dummy override!", QMessageBox::Ok);
                        TW_AddSubtitle(i, start.toInt(), length.toInt(), cell, "evXXX");
                    }
                    else
                    {
                        TW_AddSubtitle(i, start.toInt(), length.toInt(), cell);
                    }
                }
                else
                {
                    TW_AddSubtitle(i, start.toInt(), length.toInt(), cell, groupOverride);
                }
            }
            else
            {
                QMessageBox::critical(this, "Error", "In Caption (Name: " + name + "):\nText has invalid attribute \"Start\", \"Length\" or \"Cell\"!", QMessageBox::Ok);
                ResetEditor();
                return;
            }
        }
    }

    m_fileName = capFile;
    int index = m_fileName.lastIndexOf('/');
    if (index == -1) index = m_fileName.lastIndexOf('\\');
    ui->L_FileName->setText("File Name: " + m_fileName.mid(index + 1));

    CheckAllEnablility();
}

//---------------------------------------------------------------------------
// Reset editor
//---------------------------------------------------------------------------
void EventCaptionEditor::on_PB_Reset_clicked()
{
    ResetEditor();
}

//---------------------------------------------------------------------------
// Save
//---------------------------------------------------------------------------
void EventCaptionEditor::on_PB_Save_clicked()
{
    SaveFile(m_fileName);

    QMessageBox::information(this, "Save", "File save successful!", QMessageBox::Ok);
}

//---------------------------------------------------------------------------
// Save as
//---------------------------------------------------------------------------
void EventCaptionEditor::on_PB_SaveAs_clicked()
{
    QString path = "";
    if (!m_path.isEmpty())
    {
        path = m_path;
    }

    QString capFile = QFileDialog::getSaveFileName(this, tr("Save As .cap File"), path, "CAP File (*.cap)");
    if (capFile == Q_NULLPTR) return;

    // Save directory
    QFileInfo info(capFile);
    m_path = info.dir().absolutePath();

    SaveFile(capFile);

    QMessageBox::information(this, "Save As", "File save successful!", QMessageBox::Ok);
}

//---------------------------------------------------------------------------
// Apply changes
//---------------------------------------------------------------------------
void EventCaptionEditor::on_PB_Apply_clicked()
{
    QTreeWidgetItem* caption = ui->TW_TreeWidget->topLevelItem(m_id);
    if (caption == Q_NULLPTR) return;

    caption->setText(0, ui->LE_Cutscene->text());
    caption->setText(1, ui->LE_Group->text());

    while (caption->childCount() > 0)
    {
        delete caption->takeChild(0);
    }

    for (int i = 1; i < ui->VL_Group->layout()->count() - 1; i++)
    {
        QSpinBox* start = reinterpret_cast<QSpinBox*>(ui->VL_Start->layout()->itemAt(i)->widget());
        QSpinBox* length = reinterpret_cast<QSpinBox*>(ui->VL_Length->layout()->itemAt(i)->widget());
        QLineEdit* cell = reinterpret_cast<QLineEdit*>(ui->VL_Cell->layout()->itemAt(i)->widget());
        QLineEdit* groupOverride = reinterpret_cast<QLineEdit*>(ui->VL_Group->layout()->itemAt(i)->widget());

        TW_AddSubtitle(m_id, start->value(), length->value(), cell->text(), (groupOverride->text().isEmpty() || groupOverride->text() == ui->LE_Group->text()) ? "(default)" : groupOverride->text());

        // Re-highlight the subtitle in tree widget
        QTreeWidgetItem* child = caption->child(i - 1);
        child->setForeground(1, QColor(255,0,0));
        child->setForeground(2, QColor(255,0,0));
        child->setForeground(3, QColor(255,0,0));
        child->setForeground(4, QColor(255,0,0));
    }

    m_edited = false;
    CheckAllEnablility();
}

//---------------------------------------------------------------------------
// Add subtitle
//---------------------------------------------------------------------------
void EventCaptionEditor::on_PB_AddSubtitle_clicked()
{
    int id = ui->VL_Group->layout()->count() - 1;
    AddSubtitle(0, 1, "Subtitle" + (((id < 10) ? "0" : "") + QString::number(id)));

    m_edited = true;
    CheckAllEnablility();
}

//---------------------------------------------------------------------------
// Save as
//---------------------------------------------------------------------------
void EventCaptionEditor::on_PB_Clear_clicked()
{
    ClearSubtitles();
    QTreeWidgetItem* caption = ui->TW_TreeWidget->topLevelItem(m_id);
    if (caption == Q_NULLPTR) return;

    caption->setText(0, ui->LE_Cutscene->text());
    caption->setText(1, ui->LE_Group->text());

    while (caption->childCount() > 0)
    {
        delete caption->takeChild(0);
    }

    m_edited = true;
    CheckAllEnablility();
}

//---------------------------------------------------------------------------
// Add cutscene button
//---------------------------------------------------------------------------
void EventCaptionEditor::on_PB_AddCutscene_clicked()
{
    TW_AddCutscene();

    CheckAllEnablility();
}

//---------------------------------------------------------------------------
// Delete cutscene button
//---------------------------------------------------------------------------
void EventCaptionEditor::on_PB_DeleteCutscene_clicked()
{
    ClearSubtitles();
    delete ui->TW_TreeWidget->takeTopLevelItem(m_id);

    m_id = -1;
    m_edited = false;
    CheckAllEnablility();
}

//---------------------------------------------------------------------------
// Expand all items in tree view
//---------------------------------------------------------------------------
void EventCaptionEditor::on_PB_Expand_clicked()
{
    ui->TW_TreeWidget->expandAll();
}

//---------------------------------------------------------------------------
// Collapse all items in tree view
//---------------------------------------------------------------------------
void EventCaptionEditor::on_PB_Collapse_clicked()
{
    ui->TW_TreeWidget->collapseAll();
}

//---------------------------------------------------------------------------
// Sort tree view
//---------------------------------------------------------------------------
void EventCaptionEditor::on_PB_Sort_clicked()
{
    QTreeWidgetItem* item = Q_NULLPTR;
    if (m_id != -1)
    {
        item = ui->TW_TreeWidget->topLevelItem(m_id);
    }

    ui->TW_TreeWidget->sortByColumn(0, m_sortAscending ? Qt::SortOrder::AscendingOrder : Qt::SortOrder::DescendingOrder);
    m_sortAscending = !m_sortAscending;

    if (item != Q_NULLPTR)
    {
        m_id = ui->TW_TreeWidget->indexOfTopLevelItem(item);
    }
}

//---------------------------------------------------------------------------
// Shift frames backward
//---------------------------------------------------------------------------
void EventCaptionEditor::on_PB_ShiftLeft_clicked()
{
    ShiftFrames(true);
}

//---------------------------------------------------------------------------
// Shift frames forward
//---------------------------------------------------------------------------
void EventCaptionEditor::on_PB_ShiftRight_clicked()
{
    ShiftFrames(false);
}

//---------------------------------------------------------------------------
// Double clicked an entry in tree widget
//---------------------------------------------------------------------------
void EventCaptionEditor::on_TW_TreeWidget_itemDoubleClicked(QTreeWidgetItem *item, int column)
{
    // Un-highlight previous selection
    if (m_id != -1)
    {
        QTreeWidgetItem* parent = ui->TW_TreeWidget->topLevelItem(m_id);
        parent->setForeground(0, QColor(0,0,0));
        parent->setForeground(1, QColor(0,0,0));
        for (int i = 0; i < parent->childCount(); i++)
        {
            QTreeWidgetItem* child = parent->child(i);
            child->setForeground(1, QColor(0,0,0));
            child->setForeground(2, QColor(0,0,0));
            child->setForeground(3, QColor(0,0,0));
            child->setForeground(4, QColor(0,0,0));
        }
    }

    QTreeWidgetItem* caption = item->parent() == Q_NULLPTR ? item : item->parent();
    caption->setForeground(0, QColor(255,0,0));
    caption->setForeground(1, QColor(255,0,0));
    ui->LE_Cutscene->setText(caption->text(0));
    ui->LE_Group->setText(caption->text(1));

    ClearSubtitles();
    for (int i = 0; i < caption->childCount(); i++)
    {
        QTreeWidgetItem* text = caption->child(i);
        text->setForeground(1, QColor(255,0,0));
        text->setForeground(2, QColor(255,0,0));
        text->setForeground(3, QColor(255,0,0));
        text->setForeground(4, QColor(255,0,0));
        AddSubtitle(text->text(3).toInt(), text->text(4).toInt(), text->text(2), text->text(1));
    }

    m_edited = false;
    m_id = ui->TW_TreeWidget->indexOfTopLevelItem(item);
    ui->TW_TreeWidget->expandItem(caption);
    CheckAllEnablility();
}

//---------------------------------------------------------------------------
// Used changed fco file name
//---------------------------------------------------------------------------
void EventCaptionEditor::on_LE_fcoFile_textEdited(const QString &arg1)
{
    CheckAllEnablility();
}

//---------------------------------------------------------------------------
// Cutscene name edited by user
//---------------------------------------------------------------------------
void EventCaptionEditor::on_LE_Cutscene_textEdited(const QString &arg1)
{
    m_edited = true;
    CheckAllEnablility();
}

//---------------------------------------------------------------------------
// Default group name edited by user
//---------------------------------------------------------------------------
void EventCaptionEditor::on_LE_Group_textEdited(const QString &arg1)
{
    m_edited = true;
    CheckAllEnablility();
}

//---------------------------------------------------------------------------
// Used changed a line edit that doesn't need to update other things
//---------------------------------------------------------------------------
void EventCaptionEditor::on_Generic_textEdited()
{
    m_edited = true;
    CheckAllEnablility();
}

//---------------------------------------------------------------------------
// Spinbox of start, length
//---------------------------------------------------------------------------
void EventCaptionEditor::on_SpinBox_valueChanged(int i)
{
    QWidget* widget = reinterpret_cast<QWidget*>(sender());
    int index1 = ui->VL_Start->indexOf(widget);
    int index2 = ui->VL_End->indexOf(widget);
    int index3 = ui->VL_Length->indexOf(widget);

    if (index1 != -1)
    {
        QSpinBox* start = reinterpret_cast<QSpinBox*>(ui->VL_Start->layout()->itemAt(index1)->widget());
        QSpinBox* end = reinterpret_cast<QSpinBox*>(ui->VL_End->layout()->itemAt(index1)->widget());

        QLineEdit* startTime = reinterpret_cast<QLineEdit*>(ui->VL_StartTime->layout()->itemAt(index1)->widget());
        startTime->blockSignals(true);
        startTime->setText(ConvertFramesToTime(start->value()));
        startTime->blockSignals(false);

        if (start->value() <= end->value())
        {
            QSpinBox* length = reinterpret_cast<QSpinBox*>(ui->VL_Length->layout()->itemAt(index1)->widget());
            length->blockSignals(true);
            length->setValue(end->value() - start->value() + 1);
            length->blockSignals(false);
        }
    }
    else if (index2 != -1)
    {
        QSpinBox* start = reinterpret_cast<QSpinBox*>(ui->VL_Start->layout()->itemAt(index2)->widget());
        QSpinBox* end = reinterpret_cast<QSpinBox*>(ui->VL_End->layout()->itemAt(index2)->widget());

        QLineEdit* endTime = reinterpret_cast<QLineEdit*>(ui->VL_EndTime->layout()->itemAt(index2)->widget());
        endTime->blockSignals(true);
        endTime->setText(ConvertFramesToTime(end->value()));
        endTime->blockSignals(false);

        if (start->value() <= end->value())
        {
            QSpinBox* length = reinterpret_cast<QSpinBox*>(ui->VL_Length->layout()->itemAt(index2)->widget());
            length->blockSignals(true);
            length->setValue(end->value() - start->value() + 1);
            length->blockSignals(false);
        }
    }
    else if (index3 != -1)
    {
        QSpinBox* length = reinterpret_cast<QSpinBox*>(ui->VL_Length->layout()->itemAt(index3)->widget());
        QSpinBox* start = reinterpret_cast<QSpinBox*>(ui->VL_Start->layout()->itemAt(index3)->widget());
        QSpinBox* end = reinterpret_cast<QSpinBox*>(ui->VL_End->layout()->itemAt(index3)->widget());
        end->setValue(start->value() + length->value() - 1);
    }
    else
    {
        return;
    }

    m_edited = true;
    CheckAllEnablility();
}

//---------------------------------------------------------------------------
// Start time or end time edited
//---------------------------------------------------------------------------
void EventCaptionEditor::on_StartEndTime_textEdited()
{
    QWidget* widget = reinterpret_cast<QWidget*>(sender());
    int index1 = ui->VL_StartTime->indexOf(widget);
    int index2 = ui->VL_EndTime->indexOf(widget);

    if (index1 != -1)
    {
        QSpinBox* start = reinterpret_cast<QSpinBox*>(ui->VL_Start->layout()->itemAt(index1)->widget());
        QLineEdit* startTime = reinterpret_cast<QLineEdit*>(ui->VL_StartTime->layout()->itemAt(index1)->widget());

        int frames = ConvertTimeToFrames(startTime->text(), start->maximum());
        if (frames != -1)
        {
            start->setValue(frames);
        }
    }
    else if (index2 != -1)
    {
        QSpinBox* end = reinterpret_cast<QSpinBox*>(ui->VL_End->layout()->itemAt(index2)->widget());
        QLineEdit* endTime = reinterpret_cast<QLineEdit*>(ui->VL_EndTime->layout()->itemAt(index2)->widget());

        int frames = ConvertTimeToFrames(endTime->text(), end->maximum());
        if (frames != -1)
        {
            end->setValue(frames);
        }
    }

    m_edited = true;
    CheckAllEnablility();
}

//---------------------------------------------------------------------------
// Start time or end time pressed enter in XXYYZZ format
//---------------------------------------------------------------------------
void EventCaptionEditor::on_StartEndTime_returnPressed()
{
    QWidget* widget = reinterpret_cast<QWidget*>(sender());
    int index1 = ui->VL_StartTime->indexOf(widget);
    int index2 = ui->VL_EndTime->indexOf(widget);

    if (index1 != -1)
    {
        QSpinBox* start = reinterpret_cast<QSpinBox*>(ui->VL_Start->layout()->itemAt(index1)->widget());
        QLineEdit* startTime = reinterpret_cast<QLineEdit*>(ui->VL_StartTime->layout()->itemAt(index1)->widget());

        int frames = ConvertTimeToFrames(startTime->text(), start->maximum());
        if (frames == -1)
        {
            frames = ConvertTimeToFramesSimple(startTime->text(), start->maximum());
        }

        if (frames != -1)
        {
            start->setValue(frames);

            // Goto the next end box and highlight
            QLineEdit* endTime = reinterpret_cast<QLineEdit*>(ui->VL_EndTime->layout()->itemAt(index1)->widget());
            endTime->setFocus();
            endTime->selectAll();
        }
    }
    else if (index2 != -1)
    {
        QSpinBox* end = reinterpret_cast<QSpinBox*>(ui->VL_End->layout()->itemAt(index2)->widget());
        QLineEdit* endTime = reinterpret_cast<QLineEdit*>(ui->VL_EndTime->layout()->itemAt(index2)->widget());

        int frames = ConvertTimeToFrames(endTime->text(), end->maximum());
        if (frames == -1)
        {
            frames = ConvertTimeToFramesSimple(endTime->text(), end->maximum());
        }

        if (frames != -1)
        {
            end->setValue(frames);

            // Try goto the next row start box and highlight
            if (index2 < ui->VL_Group->layout()->count() - 2)
            {
                QLineEdit* startTime = reinterpret_cast<QLineEdit*>(ui->VL_StartTime->layout()->itemAt(index2 + 1)->widget());
                startTime->setFocus();
                startTime->selectAll();
            }
        }
    }

    m_edited = true;
    CheckAllEnablility();
}

//---------------------------------------------------------------------------
// Check all buttons if they are enabled
//---------------------------------------------------------------------------
void EventCaptionEditor::CheckAllEnablility()
{
    bool treeWidgetEmpty = ui->TW_TreeWidget->topLevelItemCount() == 0;
    ui->PB_Sort->setEnabled(!treeWidgetEmpty);

    bool cutsceneLoaded = m_id != -1;
    bool hasSubtitles = ui->VL_Group->layout()->count() > 2;
    bool applyValid = !ui->LE_Cutscene->text().isEmpty() && ValidateAllSubtitles();
    ui->LE_fcoFile->setEnabled(!treeWidgetEmpty);
    ui->LE_Cutscene->setEnabled(cutsceneLoaded);
    ui->LE_Group->setEnabled(cutsceneLoaded);
    ui->PB_Apply->setEnabled(cutsceneLoaded && m_edited && applyValid);
    ui->PB_AddSubtitle->setEnabled(cutsceneLoaded);
    ui->PB_Clear->setEnabled(cutsceneLoaded && hasSubtitles);
    ui->PB_DeleteCutscene->setEnabled(cutsceneLoaded && !treeWidgetEmpty);

    bool saveValid = !treeWidgetEmpty && !ui->LE_fcoFile->text().isEmpty() && !m_edited;
    ui->PB_Save->setEnabled(saveValid && !m_fileName.isEmpty());
    ui->PB_SaveAs->setEnabled(saveValid);

    // Highlight LE_fcoFile and LE_Cutscene red if empty
    QPalette pal = palette();
    if (ui->LE_fcoFile->isEnabled())
    {
        if (ui->LE_fcoFile->text().isEmpty())
        {
            pal.setColor(QPalette::Base, QColor(255,60,60));
        }
        else
        {
            pal.setColor(QPalette::Base, QColor(255,255,255));
        }
    }
    else
    {
        pal.setColor(QPalette::Base, QColor(240,240,240));
    }
    ui->LE_fcoFile->setPalette(pal);

    if (ui->LE_Cutscene->isEnabled())
    {
        if (ui->LE_Cutscene->text().isEmpty())
        {
            pal.setColor(QPalette::Base, QColor(255,60,60));
        }
        else
        {
            pal.setColor(QPalette::Base, QColor(255,255,255));
        }
    }
    else
    {
        pal.setColor(QPalette::Base, QColor(240,240,240));
    }
    ui->LE_Cutscene->setPalette(pal);

    // Preview text
    if (m_previewIndex >= ui->VL_Cell->layout()->count() - 1)
    {
        m_previewIndex = 0;
        SetPreviewText("");
    }
    else
    {
        UpdateSubtitlePreview(m_previewIndex);
    }
}

//---------------------------------------------------------------------------
// Save .cap file with specified file name
//---------------------------------------------------------------------------
void EventCaptionEditor::SaveFile(const QString &_fileName)
{
    // Save .cap file
    QDomDocument document;

    // Processing Instruction
    QDomProcessingInstruction processingInstruction = document.createProcessingInstruction("xml", "version=\"1.0\" encoding=\"utf-8\" standalone=\"no\"");
    document.appendChild(processingInstruction);

    // Caption List
    QDomElement captionList = document.createElement("CaptionList");
    captionList.setAttribute("File", ui->LE_fcoFile->text());
    captionList.setAttribute("TimeUnit", "frame");
    document.appendChild(captionList);

    // Captions
    for (int i = 0; i < ui->TW_TreeWidget->topLevelItemCount(); i++)
    {
        QTreeWidgetItem const* captionItem = ui->TW_TreeWidget->topLevelItem(i);
        QString name = captionItem->text(0);
        QString groupDefault = captionItem->text(1);

        QDomElement caption = document.createElement("Caption");
        caption.setAttribute("Name", name);
        if (!groupDefault.isEmpty())
        {
            caption.setAttribute("Group", groupDefault);
        }
        captionList.appendChild(caption);

        // Texts
        for (int j = 0; j < captionItem->childCount(); j++)
        {
            QTreeWidgetItem const* textItem = captionItem->child(j);
            QString start = textItem->text(3);
            QString length = textItem->text(4);
            QString cell = textItem->text(2);
            QString groupOverride = textItem->text(1);

            QDomElement text = document.createElement("Text");
            text.setAttribute("Start", start);
            text.setAttribute("Length", length);
            if (groupOverride != "(default)")
            {
                text.setAttribute("Group", groupOverride);
            }
            text.setAttribute("Cell", cell);
            caption.appendChild(text);
        }
    }

    QFile file(_fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QMessageBox::critical(this, "Error", "Fail to write .cap file!", QMessageBox::Ok);
        ResetEditor();
        return;
    }
    else
    {
        QTextStream stream(&file);
        stream << document.toString();
        file.close();
    }

    // Update file name
    m_fileName = _fileName;
    int index = m_fileName.lastIndexOf('/');
    if (index == -1) index = m_fileName.lastIndexOf('\\');
    ui->L_FileName->setText("File Name: " + m_fileName.mid(index + 1));
    CheckAllEnablility();
}

//---------------------------------------------------------------------------
// Add a new subtitle to the editor
//---------------------------------------------------------------------------
void EventCaptionEditor::AddSubtitle(int _start, int _length, const QString &_cell, const QString &_groupOverride)
{
    // ALL WIDGETS MUST HAVE MINIMUM HEIGHT 26
    // 0: Group override, 1: Cell, 2: Start, 3: Start(time), 4: End, 5: End(time), 6: Length, 7: Delete

    int const insertTo = ui->VL_Group->layout()->count() - 1;

    // Group override
    QLineEdit *group = new QLineEdit();
    group->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    group->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    group->setMinimumHeight(26);
    group->setText(_groupOverride == "(default)" ? "" : _groupOverride);
    group->installEventFilter(this);
    ui->VL_Group->insertWidget(insertTo, group);
    connect(group, SIGNAL(textEdited(QString)), this, SLOT(on_Generic_textEdited()));

    QFont font = group->font();
    font.setPointSize(8);
    group->setFont(font);

    // Cell
    QLineEdit *cell = new QLineEdit();
    cell->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    cell->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    cell->setMinimumHeight(26);
    cell->setText(_cell);
    cell->installEventFilter(this);
    cell->setFont(font);
    ui->VL_Cell->insertWidget(insertTo, cell);
    connect(cell, SIGNAL(textEdited(QString)), this, SLOT(on_Generic_textEdited()));

    // Start
    QSpinBox *start = new QSpinBox();
    start->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    start->setMinimumHeight(26);
    start->setMinimum(0);
    start->setMaximum(99999);
    qBound(0, _start, 99999);
    start->setValue(_start);
    start->installEventFilter(this);
    start->setFont(font);
    ui->VL_Start->insertWidget(insertTo, start);
    connect(start, SIGNAL(valueChanged(int)), this, SLOT(on_SpinBox_valueChanged(int)));

    // Start in (min:sec:frame)
    QLineEdit *startTime = new QLineEdit();
    startTime->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    startTime->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    startTime->setMinimumHeight(26);
    startTime->setText(ConvertFramesToTime(_start));
    startTime->installEventFilter(this);
    startTime->setFont(font);
    ui->VL_StartTime->insertWidget(insertTo, startTime);
    connect(startTime, SIGNAL(textEdited(QString)), this, SLOT(on_StartEndTime_textEdited()));
    connect(startTime, SIGNAL(returnPressed()), this, SLOT(on_StartEndTime_returnPressed()));

    // Length
    QSpinBox *length = new QSpinBox();
    length->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    length->setMinimumHeight(26);
    length->setMinimum(1);
    length->setMaximum(100000);
    qBound(1, _length, 100000);
    length->setValue(_length);
    length->installEventFilter(this);
    length->setFont(font);
    ui->VL_Length->insertWidget(insertTo, length);
    connect(length, SIGNAL(valueChanged(int)), this, SLOT(on_SpinBox_valueChanged(int)));

    // End
    int _end = _start + _length - 1;
    QSpinBox *end = new QSpinBox();
    end->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    end->setMinimumHeight(26);
    end->setMinimum(0);
    end->setMaximum(99999);
    end->setValue(_end);
    end->installEventFilter(this);
    end->setFont(font);
    ui->VL_End->insertWidget(insertTo, end);
    connect(end, SIGNAL(valueChanged(int)), this, SLOT(on_SpinBox_valueChanged(int)));

    // End in (min:sec:frame)
    QLineEdit *endTime = new QLineEdit();
    endTime->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    endTime->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    endTime->setMinimumHeight(26);
    endTime->setText(ConvertFramesToTime(_end));
    endTime->installEventFilter(this);
    endTime->setFont(font);
    ui->VL_EndTime->insertWidget(insertTo, endTime);
    connect(endTime, SIGNAL(textEdited(QString)), this, SLOT(on_StartEndTime_textEdited()));
    connect(endTime, SIGNAL(returnPressed()), this, SLOT(on_StartEndTime_returnPressed()));

    // Delete
    QPushButton *del = new QPushButton();
    del->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    del->setMinimumHeight(26);
    del->setMaximumHeight(26);
    del->setText("X");
    del->setMaximumWidth(20);
    del->installEventFilter(this);
    del->setFont(font);
    ui->VL_Delete->insertWidget(insertTo, del);
    connect(del, SIGNAL(clicked()), this, SLOT(DeleteSubtitle()));
}

//---------------------------------------------------------------------------
// Signal sent when delete button is pressed
//---------------------------------------------------------------------------
void EventCaptionEditor::DeleteSubtitle()
{
    QWidget* widget = reinterpret_cast<QWidget*>(sender());
    int index = ui->VL_Delete->indexOf(widget);
    if (index != -1)
    {
        int layoutID[8] = {0, 2, 4, 5, 7, 8, 10, 12};
        for (int i = 0; i < 8; i++)
        {
            QLayout* vBox = ui->HL_Subtitles->layout()->itemAt(layoutID[i])->layout();
            delete vBox->takeAt(index)->widget();
        }

        m_edited = true;
        CheckAllEnablility();
    }
}

//---------------------------------------------------------------------------
// Clear all subtitles in the editor
//---------------------------------------------------------------------------
void EventCaptionEditor::ClearSubtitles()
{
    int layoutID[8] = {0, 2, 4, 5, 7, 8, 10, 12};
    for (int i = 0; i < 8; i++)
    {
        QLayout* vBox = ui->HL_Subtitles->layout()->itemAt(layoutID[i])->layout();
        while (vBox->count() > 2)
        {
            delete vBox->takeAt(1)->widget();
        }
    }

    m_edited = true;
    CheckAllEnablility();
}

//---------------------------------------------------------------------------
// Emits a singal to find the subtitle for preview
//---------------------------------------------------------------------------
void EventCaptionEditor::UpdateSubtitlePreview(int _index)
{
    if (_index != 0 && _index < ui->VL_Group->layout()->count() - 1)
    {
        QLineEdit* group = reinterpret_cast<QLineEdit*>(ui->VL_Group->layout()->itemAt(_index)->widget());
        QLineEdit* cell = reinterpret_cast<QLineEdit*>(ui->VL_Cell->layout()->itemAt(_index)->widget());
        emit UpdatePreview(group->text().isEmpty() ? ui->LE_Group->text() : group->text(), cell->text());
    }
}

//---------------------------------------------------------------------------
// Validate one row of subtitle
//---------------------------------------------------------------------------
bool EventCaptionEditor::ValidateSubtitle(int _id)
{
    if (ui->VL_Group->layout()->count() <= 2 || _id == 0 || _id == ui->VL_Group->layout()->count() - 1) return false;

    QPalette palRed = palette();
    QPalette palWhite = palette();
    palRed.setColor(QPalette::Base, QColor(255,60,60));
    palWhite.setColor(QPalette::Base, QColor(255,255,255));

    bool valid = true;

    // Group override
    QLineEdit* groupOverride = reinterpret_cast<QLineEdit*>(ui->VL_Group->layout()->itemAt(_id)->widget());
    if (groupOverride->text().isEmpty() && ui->LE_Group->text().isEmpty())
    {
        groupOverride->setPalette(palRed);
        valid = false;
    }
    else
    {
        groupOverride->setPalette(palWhite);
    }

    // Cell
    QLineEdit* cell = reinterpret_cast<QLineEdit*>(ui->VL_Cell->layout()->itemAt(_id)->widget());
    if (cell->text().isEmpty())
    {
        cell->setPalette(palRed);
        valid = false;
    }
    else
    {
        cell->setPalette(palWhite);
    }

    // Start and End
    QSpinBox* start = reinterpret_cast<QSpinBox*>(ui->VL_Start->layout()->itemAt(_id)->widget());
    QSpinBox* end = reinterpret_cast<QSpinBox*>(ui->VL_End->layout()->itemAt(_id)->widget());
    if (start->value() > end->value())
    {
        start->setPalette(palRed);
        end->setPalette(palRed);
        valid = false;
    }
    else
    {
        start->setPalette(palWhite);
        end->setPalette(palWhite);
    }

    // Start time
    QLineEdit* startTime = reinterpret_cast<QLineEdit*>(ui->VL_StartTime->layout()->itemAt(_id)->widget());
    if (ConvertTimeToFrames(startTime->text(), start->maximum()) == -1)
    {
        startTime->setPalette(palRed);
        valid = false;
    }
    else
    {
        startTime->setPalette(palWhite);
    }

    // End time
    QLineEdit* endTime = reinterpret_cast<QLineEdit*>(ui->VL_EndTime->layout()->itemAt(_id)->widget());
    if (ConvertTimeToFrames(endTime->text(), end->maximum()) == -1)
    {
        endTime->setPalette(palRed);
        valid = false;
    }
    else
    {
        endTime->setPalette(palWhite);
    }

    return valid;
}

//---------------------------------------------------------------------------
// Validate all rows of subtitle
//---------------------------------------------------------------------------
bool EventCaptionEditor::ValidateAllSubtitles()
{
    bool valid = true;
    for (int i = 1; i < ui->VL_Group->layout()->count() - 1; i++)
    {
        valid &= ValidateSubtitle(i);
    }
    return valid;
}

//---------------------------------------------------------------------------
// Convert (XXYYZZ) to frames
//---------------------------------------------------------------------------
int EventCaptionEditor::ConvertTimeToFramesSimple(const QString &_time, const int _max)
{
    if (_time.size() <= 6)
    {
        bool valid = false;
        int number = _time.toInt(&valid);
        if (valid)
        {
            int frame = number % 100;
            if (frame > 29) return -1;

            int sec = (number / 100) % 100;
            if (sec > 59) return -1;

            int min = (number / 10000);
            if (min > 55) return -1;

            int total = frame + sec * 30 + min * 30 * 60;
            if (total > _max) return -1;

            return total;
        }
    }

    return -1;
}

//---------------------------------------------------------------------------
// Convert (min:sec:frame) to frames
//---------------------------------------------------------------------------
int EventCaptionEditor::ConvertTimeToFrames(const QString &_time, const int _max)
{
    if (_time.size() == 8)
    {
        QStringList query = _time.split(':');
        if (query.size() == 3)
        {
            if (query[0].size() == 2 && query[1].size() == 2 && query[2].size() == 2)
            {
                bool valid1 = false;
                bool valid2 = false;
                bool valid3 = false;

                int min = query[0].toInt(&valid1);
                int sec = query[1].toInt(&valid2);
                int frame = query[2].toInt(&valid3);
                int total = frame + sec * 30 + min * 30 * 60;

                bool valid = valid1 && valid2 && valid3;
                valid &= frame < 30;
                valid &= sec < 60;
                valid &= total <= _max;

                if (valid)
                {
                    return total;
                }
            }
        }
    }

    return -1;
}

//---------------------------------------------------------------------------
// Convert frames to (min:sec:frame)
//---------------------------------------------------------------------------
QString EventCaptionEditor::ConvertFramesToTime(int _frames)
{
    int frame = _frames % 30;
    int sec = (_frames / 30) % 60;
    int min = (_frames / 30 / 60);

    return (min < 10 ? "0" : "" ) + QString::number(min) + ":" + (sec < 10 ? "0" : "" ) + QString::number(sec) + ":" + (frame < 10 ? "0" : "" ) + QString::number(frame);
}

//---------------------------------------------------------------------------
// Add a new cutscene to tree widget
//---------------------------------------------------------------------------
void EventCaptionEditor::TW_AddCutscene(const QString &_name, const QString &_group)
{
    QTreeWidgetItem *item = new QTreeWidgetItem(ui->TW_TreeWidget);
    item->setText(0, _name);
    item->setText(1, _group);
    ui->TW_TreeWidget->addTopLevelItem(item);
}

//---------------------------------------------------------------------------
// Add a new subtitle to tree widget
//---------------------------------------------------------------------------
void EventCaptionEditor::TW_AddSubtitle(int _id, int _start, int _length, const QString &_cell, const QString &_groupOverride)
{
    QTreeWidgetItem *parent = ui->TW_TreeWidget->topLevelItem(_id);
    if (parent)
    {
        QTreeWidgetItem *item = new QTreeWidgetItem();
        item->setText(1, _groupOverride);
        item->setText(2, _cell);
        item->setText(3, QString::number(_start));
        item->setText(4, QString::number(_length));
        parent->addChild(item);
    }
}

//---------------------------------------------------------------------------
// Shift amount of frames
//---------------------------------------------------------------------------
void EventCaptionEditor::ShiftFrames(bool _backward)
{
    int count = ui->VL_Group->layout()->count() - 1;
    int shift = (_backward ? -1 : 1) * ui->SB_Shift->value();

    for (int i = 1; i < count; i++)
    {
        QSpinBox* start = reinterpret_cast<QSpinBox*>(ui->VL_Start->layout()->itemAt(i)->widget());
        start->setValue(start->value() + shift);

        QSpinBox* end = reinterpret_cast<QSpinBox*>(ui->VL_End->layout()->itemAt(i)->widget());
        end->setValue(end->value() + shift);
    }
}
