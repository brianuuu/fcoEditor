#include "fcoeditorwindow.h"
#include "ui_fcoeditorwindow.h"

#include "fcoaboutwindow.h"

//---------------------------------------------------------------------------
// Constructor
//---------------------------------------------------------------------------
fcoEditorWindow::fcoEditorWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::fcoEditorWindow)
{
    ui->setupUi(this);

    m_groupID = INT_MAX;
    m_subtitleID = INT_MAX;
    m_textEdited = false;
    m_textValid = false;
    m_textIsOmochao = true;
    m_colorEdited = false;
    m_fileEdited = false;

    m_moveGroup = false;
    m_moveSubtitle = false;

    // Reset relative path to load database correctly
    QDir::setCurrent(QCoreApplication::applicationDirPath());

    m_fco = new fco();
    if (!m_fco->IsInit())
    {
        UpdateStatus("Missing database file fcoDatabase.txt! Please reinstall!", "color: rgb(255, 0, 0);");
    }

    // Create a label layout on top of the subtitle background
    QHBoxLayout* textHLayout = new QHBoxLayout(ui->L_Preview);
    textHLayout->addStretch();
    textHLayout->addStretch();
    QFontDatabase::addApplicationFont(":/resources/FOT-SeuratPro-B.otf");
    m_previewLabel = new QLabel();
    m_previewLabel->setStyleSheet("font: 21px \"FOT-Seurat Pro B\"; color: white;");
    m_previewLabel->setTextFormat(Qt::TextFormat::RichText);
    textHLayout->insertWidget(1, m_previewLabel);

    m_initValue = 0;
    m_mouseY = 0;
    m_dragScale = 0;
    m_dragSpinBox = Q_NULLPTR;

    // Tree view
    ui->TW_TreeWidget->setColumnWidth(1, 52);

    // Default color
    ui->DefaultColor->installEventFilter(this);
    ui->DefaultAlpha->installEventFilter(this);
    ui->DefaultAlpha->setCursor(Qt::SizeVerCursor);
    connect(ui->DefaultAlpha, SIGNAL(valueChanged(int)), this, SLOT(ColorSpinBoxChanged(int)));
    connect(ui->DefaultHardcoded, SIGNAL(stateChanged(int)), this, SLOT(SetHardcoded(int)));

    // Load previous path and window size
    m_settings = new QSettings("brianuuu", "fcoEditor", this);
    m_path = m_settings->value("DefaultDirectory", QString()).toString();
    this->resize(m_settings->value("DefaultSize", QSize(1024, 720)).toSize());

    // Set relative path to user last used
    QDir::setCurrent(m_path);

    // Bind extra shortcuts
    QShortcut *applyChanges = new QShortcut(QKeySequence("Alt+S"), this);
    connect(applyChanges, SIGNAL(activated()), this, SLOT(on_Shortcut_ApplyChanges()));
    QShortcut *resetSubtitle = new QShortcut(QKeySequence("Alt+R"), this);
    connect(resetSubtitle, SIGNAL(activated()), this, SLOT(on_Shortcut_ResetSubtitle()));
    QShortcut *find = new QShortcut(QKeySequence("Ctrl+F"), this);
    connect(find, SIGNAL(activated()), this, SLOT(on_Shortcut_Find()));

    // Enable drag and drop to window
    setAcceptDrops(true);
}

//---------------------------------------------------------------------------
// Destructor
//---------------------------------------------------------------------------
fcoEditorWindow::~fcoEditorWindow()
{
    m_settings->setValue("DefaultDirectory", m_path);
    m_settings->setValue("DefaultSize", this->size());
    delete ui;
}

//---------------------------------------------------------------------------
// Handling mouse events
//---------------------------------------------------------------------------
bool fcoEditorWindow::eventFilter(QObject *object, QEvent *event)
{
    // Changing a color of a color block
    if (event->type() == QEvent::FocusIn)
    {
        QWidget* colorBrowser = qobject_cast<QWidget*>(object);
        int index = ui->VL_Color->indexOf(colorBrowser);
        bool isDefaultColor = (colorBrowser == ui->DefaultColor);
        if (index != -1 || isDefaultColor)
        {
            colorBrowser->clearFocus();
            QPalette pal = colorBrowser->palette();
            QColor color = pal.color(QPalette::Base);
            QColor newColor = QColorDialog::getColor(color, this, "Pick a Color");
            if (newColor.isValid())
            {
                // Update color
                pal.setColor(QPalette::Base, newColor);

                if (isDefaultColor)
                {
                    m_defaultColor.r = static_cast<unsigned char>(newColor.red());
                    m_defaultColor.g = static_cast<unsigned char>(newColor.green());
                    m_defaultColor.b = static_cast<unsigned char>(newColor.blue());
                    ui->DefaultColor->setPalette(pal);
                    if (m_defaultColor.IsHardcoded()) m_defaultColor.r = 0x01;
                }
                else
                {
                    unsigned int indexMinusOne = static_cast<size_t>(index) - 1;
                    m_colorBlocks[indexMinusOne].m_color.r = static_cast<unsigned char>(newColor.red());
                    m_colorBlocks[indexMinusOne].m_color.g = static_cast<unsigned char>(newColor.green());
                    m_colorBlocks[indexMinusOne].m_color.b = static_cast<unsigned char>(newColor.blue());
                    colorBrowser->setPalette(pal);
                    if (m_colorBlocks[indexMinusOne].m_color.IsHardcoded()) m_colorBlocks[indexMinusOne].m_color.r = 0x01;
                }

                // Save & Reset buttons
                m_colorEdited = true;
                ui->PB_Save->setEnabled(true);
                ui->PB_Reset->setEnabled(true);

                UpdateSubtitlePreview();
            }

            return true;
        }
    }

    // Dragging spin boxes
    if (!m_dragSpinBox && event->type() == QEvent::MouseButtonPress)
    {
        QWidget* widget = qobject_cast<QWidget*>(object);
        m_dragScale = 0;

        if (widget == ui->DefaultAlpha || ui->VL_Alpha->indexOf(widget) != -1)
        {
            m_dragScale = 1;
        }
        else if (ui->VL_Start->indexOf(widget) != -1 || ui->VL_End->indexOf(widget) != -1)
        {
            m_dragScale = 6;
        }

        if (m_dragScale != 0)
        {
            QMouseEvent *e = reinterpret_cast<QMouseEvent*>(event);
            m_mouseY = e->y();
            m_dragSpinBox = reinterpret_cast<QSpinBox*>(widget);
            m_initValue = m_dragSpinBox->value();
        }
    }
    else if (m_dragSpinBox && event->type() == QEvent::MouseButtonRelease)
    {
        m_dragSpinBox = Q_NULLPTR;
    }
    else if (m_dragSpinBox && event->type() == QEvent::MouseMove)
    {
        QMouseEvent *e = reinterpret_cast<QMouseEvent*>(event);
        int newVar = m_initValue + (m_mouseY - e->y()) / m_dragScale;

        if (newVar < m_dragSpinBox->minimum())
        {
            m_initValue = m_dragSpinBox->minimum();
            m_mouseY = e->y();
            newVar = m_initValue;
        }
        else if (newVar > m_dragSpinBox->maximum())
        {
            m_initValue = m_dragSpinBox->maximum();
            m_mouseY = e->y();
            newVar = m_initValue;
        }

        m_dragSpinBox->setValue(newVar);
    }

    return false;
}

//---------------------------------------------------------------------------
// Closing application
//---------------------------------------------------------------------------
void fcoEditorWindow::closeEvent (QCloseEvent *event)
{
    if (m_fco->IsLoaded())
    {
        if (!DiscardSaveMessage("Close", "Quit without saving?", true))
        {
            event->ignore();
            return;
        }
    }

    if (m_eventCaptionEditor != Q_NULLPTR)
    {
        m_eventCaptionEditor->close();
    }
    event->accept();
}

//---------------------------------------------------------------------------
// Drag event
//---------------------------------------------------------------------------
void fcoEditorWindow::dragEnterEvent(QDragEnterEvent *e)
{
    if (e->mimeData()->hasUrls())
    {
        QString fileName = e->mimeData()->urls()[0].toLocalFile();
        if (m_fco->IsInit() && fileName.endsWith(".fco"))
        {
            e->acceptProposedAction();
        }
    }
}

//---------------------------------------------------------------------------
// Drop event
//---------------------------------------------------------------------------
void fcoEditorWindow::dropEvent(QDropEvent *e)
{
    // Push window to front
    activateWindow();

    if (!DiscardSaveMessage("Open", "You have unsaved changes, continue without saving?", true))
    {
        return;
    }

    // Pass the first argument
    OpenFile(e->mimeData()->urls()[0].toLocalFile());
}

//---------------------------------------------------------------------------
// Drag drop file to .exe
//---------------------------------------------------------------------------
void fcoEditorWindow::passArgument(const std::string &_file)
{
    if (!m_fco->IsInit())
    {
        return;
    }

    OpenFile(QString::fromStdString(_file), false);
}

//---------------------------------------------------------------------------
// Importing .fco file
//---------------------------------------------------------------------------
void fcoEditorWindow::on_actionOpen_triggered()
{
    if (!m_fco->IsInit())
    {
        return;
    }

    if (!DiscardSaveMessage("Open", "You have unsaved changes, continue without saving?", true))
    {
        return;
    }

    QString path = "";
    if (!m_path.isEmpty())
    {
        path = m_path;
    }

    QString fcoFile = QFileDialog::getOpenFileName(this, tr("Open"), path, "FCO File (*.fco)");
    if (fcoFile == Q_NULLPTR) return;

    OpenFile(fcoFile);
}

//---------------------------------------------------------------------------
// Opening .mst file
//---------------------------------------------------------------------------
void fcoEditorWindow::OpenFile(QString const& fcoFile, bool showSuccess)
{
    if (fcoFile.endsWith(".fco"))
    {
        // Save directory
        QFileInfo info(fcoFile);
        m_path = info.dir().absolutePath();

        // Load fco file
        string errorMsg;
        if (!m_fco->Load(fcoFile.toStdString(), errorMsg))
        {
            ui->TW_TreeWidget->clear();

            // Reset search bar
            ui->RB_Top->setChecked(true);

            // Enable widgets
            ui->PB_Find->setEnabled(false);
            ui->PB_Expand->setEnabled(false);
            ui->PB_Collapse->setEnabled(false);
            ui->LE_Find->setEnabled(false);

            ResetSubtitleEditor();

            // Reset Tree View
            ui->PB_NewGroup->setEnabled(false);
            ui->PB_NewSubtitle->setEnabled(false);
            ui->PB_DeleteGroup->setEnabled(false);
            ui->PB_DeleteSubtitle->setEnabled(false);
            ui->PB_GroupUp->setEnabled(false);
            ui->PB_GroupDown->setEnabled(false);
            ui->PB_SubtitleUp->setEnabled(false);
            ui->PB_SubtitleDown->setEnabled(false);

            m_fileName = "";
            ui->L_FileName->setText("File Name: ---");

            QMessageBox::critical(this, "Error", QString::fromStdString(errorMsg), QMessageBox::Ok);
        }
        else
        {
            // Reset search bar
            ui->RB_Top->setChecked(true);

            // Enable widgets
            ui->PB_Find->setEnabled(true);
            ui->PB_Expand->setEnabled(true);
            ui->PB_Collapse->setEnabled(true);
            ui->LE_Find->setEnabled(true);

            ResetSubtitleEditor();

            // Reset Tree View
            ui->PB_NewGroup->setEnabled(true);
            ui->PB_NewSubtitle->setEnabled(false);
            ui->PB_DeleteGroup->setEnabled(false);
            ui->PB_DeleteSubtitle->setEnabled(false);
            ui->PB_GroupUp->setEnabled(false);
            ui->PB_GroupDown->setEnabled(false);
            ui->PB_SubtitleUp->setEnabled(false);
            ui->PB_SubtitleDown->setEnabled(false);
            TW_Refresh();

            m_fileName = fcoFile;
            int index = m_fileName.lastIndexOf('/');
            if (index == -1) index = m_fileName.lastIndexOf('\\');
            ui->L_FileName->setText("File Name: " + m_fileName.mid(index + 1));

            if (showSuccess)
            {
                QMessageBox::information(this, "Open", "File load successful!", QMessageBox::Ok);
            }
        }
    }
    else
    {
        QMessageBox::critical(this, "Error", "Unsupported format!", QMessageBox::Ok);
    }
}

//---------------------------------------------------------------------------
// Overriding .fco file
//---------------------------------------------------------------------------
void fcoEditorWindow::on_actionSave_triggered()
{
    if (!m_fco->IsLoaded())
    {
        return;
    }

    if (!DiscardSaveMessage("Save", "You have not \"Apply Changes\" yet, continue without applying?"))
    {
        return;
    }

    // Save fco file
    string errorMsg;
    if (!m_fco->Save(m_fileName.toStdString(), errorMsg))
    {
        QMessageBox::critical(this, "Error", QString::fromStdString(errorMsg), QMessageBox::Ok);
    }
    else
    {
        m_fileEdited = false;
        QMessageBox::information(this, "Save", "File save successful!", QMessageBox::Ok);
    }
}

//---------------------------------------------------------------------------
// Exporting .fco file
//---------------------------------------------------------------------------
void fcoEditorWindow::on_actionSave_as_triggered()
{
    if (!m_fco->IsLoaded())
    {
        return;
    }

    if (!DiscardSaveMessage("Save As", "You have not \"Apply Changes\" yet, continue without applying?"))
    {
        return;
    }

    QString path = "";
    if (!m_path.isEmpty())
    {
        path = m_path;
    }

    QString fcoFile = QFileDialog::getSaveFileName(this, tr("Save As .fco File"), path, "FCO File (*.fco)");
    if (fcoFile == Q_NULLPTR) return;

    // Save directory
    QFileInfo info(fcoFile);
    m_path = info.dir().absolutePath();

    // Save fco file
    string errorMsg;
    if (!m_fco->Save(fcoFile.toStdString(), errorMsg))
    {
        QMessageBox::critical(this, "Error", QString::fromStdString(errorMsg), QMessageBox::Ok);
    }
    else
    {
        m_fileName = fcoFile;
        int index = m_fileName.lastIndexOf('/');
        if (index == -1) index = m_fileName.lastIndexOf('\\');
        ui->L_FileName->setText("File Name: " + m_fileName.mid(index + 1));

        m_fileEdited = false;
        QMessageBox::information(this, "Save As", "File save successful!", QMessageBox::Ok);
    }
}

//---------------------------------------------------------------------------
// Close application
//---------------------------------------------------------------------------
void fcoEditorWindow::on_actionClose_triggered()
{
    if (m_fco->IsLoaded())
    {
        if (!DiscardSaveMessage("Close", "Quit without saving?", true))
        {
            return;
        }
    }

    if (m_eventCaptionEditor != Q_NULLPTR)
    {
        m_eventCaptionEditor->close();
    }
    QApplication::quit();
}

//---------------------------------------------------------------------------
// About
//---------------------------------------------------------------------------
void fcoEditorWindow::on_actionAbout_triggered()
{
    fcoAboutWindow aboutWindow;
    aboutWindow.setModal(true);
    aboutWindow.exec();
}

//---------------------------------------------------------------------------
// About
//---------------------------------------------------------------------------
void fcoEditorWindow::on_actionAdd_Unsupported_Characters_triggered()
{
    QString str = "You will need: HxD (with data inspector), GIMP 2.0 (with dds plugin), FOT-SeuratPro-B.otf font (provided) and Generations Archive Editor\n\n";
    str += "1. Open fcoDatabase.txt, add a new character at the end \"00 00 XX XX = Y\" (YOU MUST FOLLOW THIS FORMAT WITH ALL THE SPACING)\n";
    str += "2. Extract disk/bb2/ConverseCommon.ar.00\n";
    str += "3. Open All_002.dds with GIMP (required dds plugin)\n";
    str += "4. Use the installed font, size 22px, add the new character at empty space\n";
    str += "5. Open All.fte with HxD, set byte order to Big endian, add 1 to 0x68 (4-bytes)\n";
    str += "6. Goto the end of file, copy and paste the last 0x18 bytes\n";
    str += "7. First 4 bytes of what you pasted is the image ID, All_002.dds should be 2\n";
    str += "8. The next 16 bytes are left-top-right-bottom pixel in PERCENTAGE in float\n";
    str += "9. Check the pixel percentage in GIMP, highlight the 4 bytes and change the float number in Data inspector.\n";
    str += "10. The next 2 bytes is the new character in WideChar, highlight it and directly change it in Data inspector\n";
    str += "11. The last 2 bytes is always 00 15\n";
    str += "12. Open All.fco with HxD, add 1 to 0x24 (4-bytes)\n";
    str += "13. Goto the end of file, copy and paste the last 0x1C bytes\n";
    str += "14. At offset 0x0C (4-bytes) of what you pasted, match the number you added to fcoDatabase.txt.\n";
    str += "15. Archieve ConverseCommon.ar.00, reopen fcoEditor, and now you have the new character!\n";

    QMessageBox::about(this, "Add Unsupported Characters", str);
}

//---------------------------------------------------------------------------
// About Qt
//---------------------------------------------------------------------------
void fcoEditorWindow::on_actionAbout_Qt_triggered()
{
    QMessageBox::aboutQt(this, "About Qt");
}

//---------------------------------------------------------------------------
// Open Event Caption editor
//---------------------------------------------------------------------------
void fcoEditorWindow::on_actionEvent_Caption_Editor_cap_triggered()
{
    if (m_eventCaptionEditor == Q_NULLPTR)
    {
        m_eventCaptionEditor = new EventCaptionEditor();
        connect(m_eventCaptionEditor, SIGNAL(UpdatePreview(QString, QString)), this, SLOT(SearchForSubtitle(QString, QString)));
    }

    if (!m_eventCaptionEditor->isVisible())
    {
        m_eventCaptionEditor->ResetEditor();
        m_eventCaptionEditor->SetDefaultPath(m_path);
    }

    m_eventCaptionEditor->show();
    m_eventCaptionEditor->raise();
}

//---------------------------------------------------------------------------
// Search for subtitle
//---------------------------------------------------------------------------
void fcoEditorWindow::on_PB_Find_clicked()
{
    QString str = ui->LE_Find->text();
    if (str.isEmpty())
    {
         return;
    }

    if (!DiscardSaveMessage("Find", "You have not \"Apply Changes\" yet, continue without applying?"))
    {
        return;
    }

    // Search from start or current
    unsigned int findGroupID = m_groupID;
    unsigned int findSubtitleID = m_subtitleID;
    if (ui->RB_Top->isChecked() || m_groupID == INT_MAX || m_subtitleID == INT_MAX)
    {
        findGroupID = 0;
        findSubtitleID = 0;
    }
    else
    {
        findGroupID = m_groupID;
        findSubtitleID = m_subtitleID + 1;
    }

    // Start searching
    if (m_fco->Search(str.toStdWString(), findGroupID, findSubtitleID))
    {
        TW_FocusItem(findGroupID, findSubtitleID);
        LoadSubtitle(findGroupID, findSubtitleID);
        ui->RB_Current->setChecked(true);
        ui->LE_Find->setFocus();
    }
    else
    {
        QMessageBox::information(this, "Find", "Subtitle not found or reached the end of search.", QMessageBox::Ok);
        ui->RB_Top->setChecked(true);
    }
}

//---------------------------------------------------------------------------
// Replace subtitle
//---------------------------------------------------------------------------
void fcoEditorWindow::on_PB_Save_clicked()
{
    if (!m_textValid)
    {
        QMessageBox::critical(this, "Save", "Subtitle is not valid!", QMessageBox::Ok);
        return;
    }

    QString const newSubtitle = ui->TE_TextEditor->toPlainText();

    QTreeWidgetItem* parent = ui->TW_TreeWidget->topLevelItem(static_cast<int>(m_groupID));
    QTreeWidgetItem* child = parent->child(static_cast<int>(m_subtitleID));
    child->setText(2, newSubtitle);

    // Save and reload
    m_fco->RemoveAllColorBlocks(m_groupID, m_subtitleID);
    for (unsigned int colorBlockID = 0; colorBlockID < m_colorBlocks.size(); colorBlockID++)
    {
        m_fco->AddColorBlock(m_groupID, m_subtitleID);
        m_fco->ModifyColorBlock(m_groupID, m_subtitleID, colorBlockID, m_colorBlocks[colorBlockID]);
    }

    m_fileEdited = true;
    m_fco->ModifyDefaultColor(m_groupID, m_subtitleID, m_defaultColor);
    m_fco->ModifySubtitle(newSubtitle.toStdWString(), m_groupID, m_subtitleID);
    LoadSubtitle(m_groupID, m_subtitleID);
}

//---------------------------------------------------------------------------
// Reload subtitle
//---------------------------------------------------------------------------
void fcoEditorWindow::on_PB_Reset_clicked()
{
    if (DiscardSaveMessage("Reset", "Reset subtitle & color blocks? (Unsaved changes will be lost!)"))
    {
        LoadSubtitle(m_groupID, m_subtitleID);
    }
}

//---------------------------------------------------------------------------
// Add a new color block
//---------------------------------------------------------------------------
void fcoEditorWindow::on_PB_AddColorBlock_clicked()
{
    m_colorEdited = true;
    m_colorBlocks.push_back(fco::ColorBlock());
    AddColorBlock(m_colorBlocks.size() - 1);
    EnableUpDownButtons();
    ui->PB_Save->setEnabled(m_textEdited || m_colorEdited);
    ui->PB_Reset->setEnabled(m_textEdited || m_colorEdited);
    UpdateSubtitlePreview();
}

//---------------------------------------------------------------------------
// Expand all items in tree view
//---------------------------------------------------------------------------
void fcoEditorWindow::on_PB_Expand_clicked()
{
    ui->TW_TreeWidget->expandAll();
}

//---------------------------------------------------------------------------
// Collapse all items in tree view
//---------------------------------------------------------------------------
void fcoEditorWindow::on_PB_Collapse_clicked()
{
    ui->TW_TreeWidget->collapseAll();
}

//---------------------------------------------------------------------------
// Add a new group at the buttom
//---------------------------------------------------------------------------
void fcoEditorWindow::on_PB_NewGroup_clicked()
{
    m_fileEdited = true;
    m_fco->AddGroup();

    // Update tree view
    vector<string> groupNames;
    m_fco->GetGroupNames(groupNames);

    // Add sub-group name
    unsigned int groupID = groupNames.size() - 1;
    string const& groupName = groupNames[groupID];
    TW_AddSubgroup(QString::fromStdString(groupName), groupID);

    // Add subtitle (should only have one)
    string label = m_fco->GetLabel(groupID, 0);
    wstring subtitle = m_fco->GetSubtitle(groupID, 0);
    QTreeWidgetItem *item = ui->TW_TreeWidget->topLevelItem(static_cast<int>(groupID));
    TW_AddSubtitle(item, QString::fromStdString(label), QString::fromStdWString(subtitle));

    // Focus on the new subtitle
    TW_FocusItem(groupID, 0);
}

//---------------------------------------------------------------------------
// Delete an entire subtitle group (Use with your own risk)
//---------------------------------------------------------------------------
void fcoEditorWindow::on_PB_DeleteGroup_clicked()
{
    if (m_groupID == INT_MAX || m_subtitleID == INT_MAX)
    {
        return;
    }

    vector<string> junk;
    m_fco->GetGroupNames(junk);
    if (junk.size() <= 1)
    {
        QMessageBox::critical(this, "Delete Group", "Cannot delete the last remaining group!");
        return;
    }

    QMessageBox::StandardButton resBtn = QMessageBox::Yes;
    QString str = "Delete group \"" + ui->TW_TreeWidget->topLevelItem(static_cast<int>(m_groupID))->text(0) + "\" and all containing subtitles?\nThis may change all the SerifuID in other groups.";
    resBtn = QMessageBox::warning(this, "Delete Group", str, QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (resBtn == QMessageBox::No)
    {
        return;
    }

    m_fileEdited = true;
    m_fco->DeleteGroup(m_groupID);
    delete ui->TW_TreeWidget->takeTopLevelItem(static_cast<int>(m_groupID));

    ui->PB_NewSubtitle->setEnabled(false);
    ui->PB_DeleteGroup->setEnabled(false);
    ui->PB_DeleteSubtitle->setEnabled(false);
    ui->PB_GroupUp->setEnabled(false);
    ui->PB_GroupDown->setEnabled(false);
    ui->PB_SubtitleUp->setEnabled(false);
    ui->PB_SubtitleDown->setEnabled(false);

    TW_UpdateSerifuID();
    ResetSubtitleEditor();
}

//---------------------------------------------------------------------------
// Add a new subtitle at the buttom of the current group
//---------------------------------------------------------------------------
void fcoEditorWindow::on_PB_NewSubtitle_clicked()
{
    if (m_groupID == INT_MAX || m_subtitleID == INT_MAX)
    {
        return;
    }

    m_fileEdited = true;

    vector<string> labels;
    vector<wstring> subtitles;
    m_fco->GetSubgroupSubtitles(m_groupID, labels, subtitles);
    QString string = "Subtitle" + (((subtitles.size() + 1 < 10) ? "0" : "") + QString::number(subtitles.size() + 1));
    m_fco->AddSubtitle(m_groupID, string.toStdString());

    // Add subtitle
    labels.clear();
    subtitles.clear();
    m_fco->GetSubgroupSubtitles(m_groupID, labels, subtitles);
    unsigned int subtitleID = subtitles.size() - 1;
    QTreeWidgetItem *item = ui->TW_TreeWidget->topLevelItem(static_cast<int>(m_groupID));
    TW_AddSubtitle(item, QString::fromStdString(labels[subtitleID]), QString::fromStdWString(subtitles[subtitleID]));

    // Focus on the new subtitle
    TW_FocusItem(m_groupID, subtitleID);
}

//---------------------------------------------------------------------------
// Delete a subtitle (Use with your own risk)
//---------------------------------------------------------------------------
void fcoEditorWindow::on_PB_DeleteSubtitle_clicked()
{
    if (m_groupID == INT_MAX || m_subtitleID == INT_MAX)
    {
        return;
    }

    QTreeWidgetItem *item = ui->TW_TreeWidget->topLevelItem(static_cast<int>(m_groupID));
    if (item->childCount() == 1)
    {
        QMessageBox::critical(this, "Delete Subtitle", "Cannot delete the last remaining subtitle in a group!\nDelete the group instead.");
        return;
    }

    QMessageBox::StandardButton resBtn = QMessageBox::Yes;
    QString str = "Delete current subtitle \"" + ui->TW_TreeWidget->topLevelItem(static_cast<int>(m_groupID))->child(static_cast<int>(m_subtitleID))->text(0) + "\"?\nThis may change the SerifuID in other subtitles.";
    resBtn = QMessageBox::warning(this, "Delete Subtitle", str, QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (resBtn == QMessageBox::No)
    {
        return;
    }

    m_fileEdited = true;
    m_fco->DeleteSubtitle(m_groupID, m_subtitleID);
    delete item->takeChild(static_cast<int>(m_subtitleID));

    m_subtitleID = (m_subtitleID == 0 ? 0 : m_subtitleID - 1);
    LoadSubtitle(m_groupID, m_subtitleID);

    TW_UpdateSerifuID();
    TW_UpdateUpDownButtons();
    TW_FocusItem(m_groupID, m_subtitleID);
}

//---------------------------------------------------------------------------
// Move group up (Use with your own risk)
//---------------------------------------------------------------------------
void fcoEditorWindow::on_PB_GroupUp_clicked()
{
    if (m_groupID == INT_MAX || m_subtitleID == INT_MAX)
    {
        return;
    }

    if (!m_moveGroup)
    {

        QMessageBox::StandardButton resBtn = QMessageBox::Yes;
        QString str = "Move up the current group \"" + ui->TW_TreeWidget->topLevelItem(static_cast<int>(m_groupID))->text(0) + "\"?\nThis will change the SerifuID in subtitles.\n(This message will not be shown again upon confirmation)";
        resBtn = QMessageBox::warning(this, "Move Group", str, QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
        if (resBtn == QMessageBox::No)
        {
            return;
        }
    }
    m_moveGroup = true;

    m_fileEdited = true;
    m_fco->SwapGroup(m_groupID, m_groupID - 1);
    QTreeWidgetItem *group = ui->TW_TreeWidget->takeTopLevelItem(static_cast<int>(m_groupID));
    m_groupID--;
    ui->TW_TreeWidget->insertTopLevelItem(static_cast<int>(m_groupID), group);
    TW_UpdateSerifuID();
    TW_UpdateUpDownButtons();
    TW_FocusItem(m_groupID, m_subtitleID);
}

//---------------------------------------------------------------------------
// Move group down (Use with your own risk)
//---------------------------------------------------------------------------
void fcoEditorWindow::on_PB_GroupDown_clicked()
{
    if (m_groupID == INT_MAX || m_subtitleID == INT_MAX)
    {
        return;
    }

    if (!m_moveGroup)
    {
        QMessageBox::StandardButton resBtn = QMessageBox::Yes;
        QString str = "Move down the current group \"" + ui->TW_TreeWidget->topLevelItem(static_cast<int>(m_groupID))->text(0) + "\"?\nThis will change the SerifuID in subtitles.\n(This message will not be shown again upon confirmation)";
        resBtn = QMessageBox::warning(this, "Move Group", str, QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
        if (resBtn == QMessageBox::No)
        {
            return;
        }
    }
    m_moveGroup = true;

    m_fileEdited = true;
    m_fco->SwapGroup(m_groupID, m_groupID + 1);
    QTreeWidgetItem *group = ui->TW_TreeWidget->takeTopLevelItem(static_cast<int>(m_groupID));
    m_groupID++;
    ui->TW_TreeWidget->insertTopLevelItem(static_cast<int>(m_groupID), group);
    TW_UpdateSerifuID();
    TW_UpdateUpDownButtons();
    TW_FocusItem(m_groupID, m_subtitleID);
}

//---------------------------------------------------------------------------
// Move subtitle down (Use with your own risk)
//---------------------------------------------------------------------------
void fcoEditorWindow::on_PB_SubtitleUp_clicked()
{
    if (m_groupID == INT_MAX || m_subtitleID == INT_MAX)
    {
        return;
    }

    if (!m_moveSubtitle)
    {
        QMessageBox::StandardButton resBtn = QMessageBox::Yes;
        QString str = "Move up current subtitle \"" + ui->TW_TreeWidget->topLevelItem(static_cast<int>(m_groupID))->child(static_cast<int>(m_subtitleID))->text(0) + "\"?\nThis will change the SerifuID in subtitles.\n(This message will not be shown again upon confirmation)";
        resBtn = QMessageBox::warning(this, "Move Subtitle", str, QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
        if (resBtn == QMessageBox::No)
        {
            return;
        }
    }
    m_moveSubtitle = true;

    m_fileEdited = true;
    m_fco->SwapSubtitle(m_groupID, m_subtitleID, m_subtitleID - 1);
    QTreeWidgetItem *group = ui->TW_TreeWidget->topLevelItem(static_cast<int>(m_groupID));
    QTreeWidgetItem *subtitle = group->takeChild(static_cast<int>(m_subtitleID));
    m_subtitleID--;
    group->insertChild(static_cast<int>(m_subtitleID), subtitle);
    TW_UpdateSerifuID();
    TW_UpdateUpDownButtons();
    TW_FocusItem(m_groupID, m_subtitleID);
}

//---------------------------------------------------------------------------
// Move subtitle down (Use with your own risk)
//---------------------------------------------------------------------------
void fcoEditorWindow::on_PB_SubtitleDown_clicked()
{
    if (m_groupID == INT_MAX || m_subtitleID == INT_MAX)
    {
        return;
    }

    if (!m_moveSubtitle)
    {
        QMessageBox::StandardButton resBtn = QMessageBox::Yes;
        QString str = "Move down current subtitle \"" + ui->TW_TreeWidget->topLevelItem(static_cast<int>(m_groupID))->child(static_cast<int>(m_subtitleID))->text(0) + "\"?\nThis will change the SerifuID in subtitles.\n(This message will not be shown again upon confirmation)";
        resBtn = QMessageBox::warning(this, "Move Subtitle", str, QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
        if (resBtn == QMessageBox::No)
        {
            return;
        }
    }
    m_moveSubtitle = true;

    m_fileEdited = true;
    m_fco->SwapSubtitle(m_groupID, m_subtitleID, m_subtitleID + 1);
    QTreeWidgetItem *group = ui->TW_TreeWidget->topLevelItem(static_cast<int>(m_groupID));
    QTreeWidgetItem *subtitle = group->takeChild(static_cast<int>(m_subtitleID));
    m_subtitleID++;
    group->insertChild(static_cast<int>(m_subtitleID), subtitle);
    TW_UpdateSerifuID();
    TW_UpdateUpDownButtons();
    TW_FocusItem(m_groupID, m_subtitleID);
}

//---------------------------------------------------------------------------
// Show/Hide SerifuID
//---------------------------------------------------------------------------
void fcoEditorWindow::on_PB_SerifuID_clicked()
{
    if (ui->TW_TreeWidget->isColumnHidden(1))
    {
        ui->TW_TreeWidget->showColumn(1);
        ui->PB_SerifuID->setText("Hide SerifuID");
    }
    else
    {
        ui->TW_TreeWidget->hideColumn(1);
        ui->PB_SerifuID->setText("Show SerifuID");
    }
}

//---------------------------------------------------------------------------
// Selected a subtitle to edit
//---------------------------------------------------------------------------
void fcoEditorWindow::on_TW_TreeWidget_itemDoubleClicked(QTreeWidgetItem *item, int column)
{
    // Check if clicking on subtitle and not sub-group
    QTreeWidgetItem *parent = item->parent();
    if (parent != Q_NULLPTR)
    {
        if (!DiscardSaveMessage("Discard", "Discard unsaved changes?"))
        {
           return;
        }

        int groupID = ui->TW_TreeWidget->indexOfTopLevelItem(parent);
        int subtitleID = parent->indexOfChild(item);
        LoadSubtitle(static_cast<unsigned int>(groupID), static_cast<unsigned int>(subtitleID));
    }
}

//---------------------------------------------------------------------------
// Search text changed
//---------------------------------------------------------------------------
void fcoEditorWindow::on_LE_Find_textEdited(const QString &text)
{
    // Assume user want to search from the beginning
     ui->RB_Top->setChecked(true);
}

//---------------------------------------------------------------------------
// Start searching
//---------------------------------------------------------------------------
void fcoEditorWindow::on_LE_Find_returnPressed()
{
    on_PB_Find_clicked();
}

//---------------------------------------------------------------------------
// Group Name changed
//---------------------------------------------------------------------------
void fcoEditorWindow::on_LE_GroupName_textEdited(const QString &text)
{
    if (m_groupID != INT_MAX && m_subtitleID != INT_MAX)
    {
        m_fileEdited = true;

        QTreeWidgetItem *group = ui->TW_TreeWidget->topLevelItem(static_cast<int>(m_groupID));

        QString finalText = text.isEmpty() ? "NO_NAME" : text;
        group->setText(0, finalText);
        m_fco->ModifyGroupName(m_groupID, finalText.toStdString());
    }
}

//---------------------------------------------------------------------------
// Subtitle Name changed
//---------------------------------------------------------------------------
void fcoEditorWindow::on_LE_SubtitleName_textEdited(const QString &text)
{
    if (m_groupID != INT_MAX && m_subtitleID != INT_MAX)
    {
        m_fileEdited = true;

        QTreeWidgetItem *group = ui->TW_TreeWidget->topLevelItem(static_cast<int>(m_groupID));
        QTreeWidgetItem *subtitle = group->child(static_cast<int>(m_subtitleID));

        QString finalText = text.isEmpty() ? "NO_NAME" : text;
        subtitle->setText(0, finalText);
        m_fco->ModifySubtitleName(m_groupID, m_subtitleID, finalText.toStdString());
    }
}

//---------------------------------------------------------------------------
// Text editor changed
//---------------------------------------------------------------------------
void fcoEditorWindow::on_TE_TextEditor_textChanged()
{
    if (ui->TE_TextEditor->isEnabled())
    {
        m_textEdited = true;

        QString str = ui->TE_TextEditor->toPlainText();
        wstring errorMsg;
        if (!m_fco->ValidateString(str.toStdWString(), errorMsg, m_characterArray))
        {
            UpdateStatus(QString::fromStdWString(errorMsg), "color: rgb(255, 0, 0);");
            m_textValid = false;
            ui->PB_Save->setEnabled(false);
            ui->PB_Reset->setEnabled(true);
        }
        else
        {
            UpdateStatus("OK", "color: rgb(0, 170, 0);");
            m_textValid = true;
            ui->PB_Save->setEnabled(m_textEdited || m_colorEdited);
            ui->PB_Reset->setEnabled(m_textEdited || m_colorEdited);
        }

        if (m_characterArray.empty())
        {
            // Empty string auto removes all color blocks
            ui->PB_AddColorBlock->setEnabled(false);
            ClearColorBlocks();
        }
        else
        {
            // Update limits of color blocks
            ui->PB_AddColorBlock->setEnabled(true);
            for (unsigned int i = 0; i < m_colorBlocks.size(); i++)
            {
                UpdateStartEndLimits(i);
            }
        }

        UpdateSubtitlePreview();
    }
}

//---------------------------------------------------------------------------
// Set Reset button enable bease on if undo is abailable
//---------------------------------------------------------------------------
void fcoEditorWindow::on_TE_TextEditor_undoAvailable(bool b)
{
    m_textEdited = b;
    ui->PB_Save->setEnabled(m_textEdited || m_colorEdited);
    ui->PB_Reset->setEnabled(m_textEdited || m_colorEdited);
}

//---------------------------------------------------------------------------
// Alt+S
//---------------------------------------------------------------------------
void fcoEditorWindow::on_Shortcut_ApplyChanges()
{
    if (ui->PB_Save->isEnabled() && m_textEdited && m_textValid)
    {
        on_PB_Save_clicked();
    }
}

//---------------------------------------------------------------------------
// Alt+R
//---------------------------------------------------------------------------
void fcoEditorWindow::on_Shortcut_ResetSubtitle()
{
    if (ui->PB_Reset->isEnabled() && m_textEdited)
    {
        on_PB_Reset_clicked();
    }
}

//---------------------------------------------------------------------------
// Ctrl+F
//---------------------------------------------------------------------------
void fcoEditorWindow::on_Shortcut_Find()
{
    if (ui->LE_Find->isEnabled())
    {
        ui->LE_Find->setFocus();
        ui->LE_Find->setSelection(0, ui->LE_Find->text().size());
    }
}

//---------------------------------------------------------------------------
// Try and find subtitle and pass to caption editor
//---------------------------------------------------------------------------
void fcoEditorWindow::SearchForSubtitle(QString _group, QString _cell)
{
    if (!m_fco->IsLoaded())
    {
        m_eventCaptionEditor->SetPreviewText("***Load .fco file to preview subtitle!***");
        return;
    }

    for (int i = 0; i < ui->TW_TreeWidget->topLevelItemCount() ; i++)
    {
        // Go through tree view
        QTreeWidgetItem const* item = ui->TW_TreeWidget->topLevelItem(i);
        if (item->text(0) == _group)
        {
            for (int j = 0; j < item->childCount(); j++)
            {
                QTreeWidgetItem const* child = item->child(j);
                if (child->text(0) == _cell)
                {
                    m_eventCaptionEditor->SetPreviewText(child->text(2));
                    return;
                }
            }
        }
    }

    m_eventCaptionEditor->SetPreviewText("***Subtitle Not Found***");
}

//---------------------------------------------------------------------------
// Message box when user have unsaved changed
//---------------------------------------------------------------------------
bool fcoEditorWindow::DiscardSaveMessage(QString _title, QString _message, bool _checkFileEdited)
{
    if (m_textEdited || m_colorEdited || (_checkFileEdited && m_fileEdited))
    {
        QMessageBox::StandardButton resBtn = QMessageBox::Yes;
        resBtn = QMessageBox::warning(this, _title, _message, QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
        if (resBtn == QMessageBox::No)
        {
            return false;
        }
    }

    return true;
}

//---------------------------------------------------------------------------
// Add subgroup to tree view widget
//---------------------------------------------------------------------------
void fcoEditorWindow::TW_Refresh()
{
    // Reset tree view and push buttons
    ui->TW_TreeWidget->clear();

    vector<string> groupNames;
    m_fco->GetGroupNames(groupNames);
    for (unsigned int groupID = 0; groupID < groupNames.size(); groupID++)
    {
        // Add sub-group names
        string const& groupName = groupNames[groupID];
        TW_AddSubgroup(QString::fromStdString(groupName), groupID);

        // Add subtitles
        vector<string> labels;
        vector<wstring> subtitles;
        m_fco->GetSubgroupSubtitles(groupID, labels, subtitles);
        QTreeWidgetItem *item = ui->TW_TreeWidget->topLevelItem(static_cast<int>(groupID));
        for (unsigned int subtitleID = 0; subtitleID < subtitles.size(); subtitleID++)
        {
            string const& label = labels[subtitleID];
            wstring const& subtitle = subtitles[subtitleID];
            TW_AddSubtitle(item, QString::fromStdString(label), QString::fromStdWString(subtitle));
        }
    }
}

//---------------------------------------------------------------------------
// Update serifu ID (after deleting group or subtitle)
//---------------------------------------------------------------------------
void fcoEditorWindow::TW_UpdateSerifuID()
{
    for (int i = 0; i < ui->TW_TreeWidget->topLevelItemCount(); i++)
    {
        QTreeWidgetItem *group = ui->TW_TreeWidget->topLevelItem(i);
        group->setText(1, i == 0 ? "" : QString::number(i) + "xxx");
        for (int j = 0; j < group->childCount(); j++)
        {
            QTreeWidgetItem *subtitle = group->child(j);
            unsigned int serifuID = static_cast<unsigned int>(i * 1000 + j);
            subtitle->setText(1, serifuID < 1000 ? "" : QString::number(serifuID));
        }
    }
}

//---------------------------------------------------------------------------
// Update serifu ID (after loading subtitle or reordering tree view)
//---------------------------------------------------------------------------
void fcoEditorWindow::TW_UpdateUpDownButtons()
{
    ui->PB_GroupUp->setEnabled(m_groupID != 0);
    ui->PB_GroupDown->setEnabled(static_cast<int>(m_groupID) < ui->TW_TreeWidget->topLevelItemCount() - 1);
    ui->PB_SubtitleUp->setEnabled(m_subtitleID != 0);
    ui->PB_SubtitleDown->setEnabled(static_cast<int>(m_subtitleID) < ui->TW_TreeWidget->topLevelItem(static_cast<int>(m_groupID))->childCount() - 1);
}

//---------------------------------------------------------------------------
// Add subgroup to tree view widget
//---------------------------------------------------------------------------
void fcoEditorWindow::TW_AddSubgroup(QString _groupName, unsigned int _groupID)
{
    QTreeWidgetItem *item = new QTreeWidgetItem(ui->TW_TreeWidget);
    item->setText(0, _groupName);
    item->setText(1, _groupID == 0 ? "" : QString::number(_groupID) + "xxx");
    item->setTextAlignment(1, Qt::AlignCenter);
    ui->TW_TreeWidget->addTopLevelItem(item);
}

//---------------------------------------------------------------------------
// Add subtitle to tree view widget
//---------------------------------------------------------------------------
void fcoEditorWindow::TW_AddSubtitle(QTreeWidgetItem *_parent, QString _label, QString _subtitle)
{
    int serifuID = ui->TW_TreeWidget->indexOfTopLevelItem(_parent) * 1000 + _parent->childCount();
    QTreeWidgetItem *item = new QTreeWidgetItem();
    item->setText(0, _label);
    item->setText(1, serifuID < 1000 ? "" : QString::number(serifuID));
    item->setText(2, _subtitle);
    item->setTextAlignment(1, Qt::AlignCenter);
    _parent->addChild(item);
}

//---------------------------------------------------------------------------
// Focus item in tree view (default current group and subtitle)
//---------------------------------------------------------------------------
void fcoEditorWindow::TW_FocusItem(unsigned int _groupID, unsigned int _subtitleID)
{
    // Focus on item
    QTreeWidgetItem *child = ui->TW_TreeWidget->topLevelItem(static_cast<int>(_groupID))->child(static_cast<int>(_subtitleID));
    ui->TW_TreeWidget->setCurrentItem(child);
    ui->TW_TreeWidget->scrollTo(ui->TW_TreeWidget->currentIndex(), QAbstractItemView::ScrollHint::PositionAtCenter);
    ui->TW_TreeWidget->setFocus();
}

//---------------------------------------------------------------------------
// Refresh subtitle editor
//---------------------------------------------------------------------------
void fcoEditorWindow::ResetSubtitleEditor()
{
    // Reset text editor
    ui->LE_GroupName->setEnabled(false);
    ui->LE_GroupName->setText("");
    ui->LE_SubtitleName->setEnabled(false);
    ui->LE_SubtitleName->setText("");
    UpdateStatus("Loaded");
    ui->TE_TextEditor->setEnabled(false);
    ui->TE_TextEditor->setText("");
    ui->PB_Save->setEnabled(false);
    ui->PB_Reset->setEnabled(false);
    m_previewLabel->setText("");
    ClearColorBlocks();
    ui->DefaultAlpha->setEnabled(false);
    QColor currentColor;
    currentColor.setRgb(240, 240, 240);
    QPalette pal = palette();
    pal.setColor(QPalette::Base, currentColor);
    ui->DefaultColor->setPalette(pal);
    ui->DefaultColor->setEnabled(false);
    ui->DefaultHardcoded->setEnabled(false);

    // Reset Variables
    m_groupID = INT_MAX;
    m_subtitleID = INT_MAX;
    m_textEdited = false;
    m_textValid = false;
    m_colorEdited = false;
    m_fileEdited = false;
}

//---------------------------------------------------------------------------
// Update status
//---------------------------------------------------------------------------
void fcoEditorWindow::UpdateStatus(QString _status, QString _styleSheet)
{
    ui->L_Status->setText("Status: " + _status);
    ui->L_Status->setStyleSheet(_styleSheet);
}

//---------------------------------------------------------------------------
// Update subtitle preview from reading m_characterArray
//---------------------------------------------------------------------------
void fcoEditorWindow::UpdateSubtitlePreview()
{
    if (m_characterArray.empty())
    {
        m_previewLabel->setText("");
        return;
    }

    // Initialize color index for each character
    vector<int> colorIndices;
    colorIndices.resize(m_characterArray.size(), -1);

    // Read color blocks with priority and fill the array
    for (unsigned int id = 0; id < m_colorBlocks.size(); id++)
    {
        QSpinBox* start = reinterpret_cast<QSpinBox*>(ui->VL_Start->layout()->itemAt(static_cast<int>(id + 1))->widget());
        QSpinBox* end = reinterpret_cast<QSpinBox*>(ui->VL_End->layout()->itemAt(static_cast<int>(id + 1))->widget());
        for (unsigned int i = static_cast<unsigned int>(start->value()); i <= static_cast<unsigned int>(end->value()); i++)
        {
            if (i < m_characterArray.size())
            {
                QString chr = QString::fromStdWString(m_characterArray[i]);
                if (chr != " " && chr != "　")
                {
                    colorIndices[i] = static_cast<int>(id);
                }
            }
        }
    }

    // Count how many line breaks
    int lineBreakCount = 0;
    for (unsigned int i = 0; i < m_characterArray.size(); i++)
    {
        QString chr = QString::fromStdWString(m_characterArray[i]);
        if (chr == "\n") lineBreakCount++;
    }

    QString htmlString;
    for (unsigned int i = 0; i < m_characterArray.size(); i++)
    {
        QString chr = QString::fromStdWString(m_characterArray[i]);

        // Larger than and smaller than
        if (chr == "<") chr = "&lt;";
        if (chr == ">") chr = "&gt;";

        // Line break in html
        if (chr == "\n") chr = "<br>";

        // Space in html
        if (chr == " ") chr = "<font color=\"#00000000\">]</font>";

        // Japanese Space in html
        if (chr == "　") chr = "<font color=\"#00000000\">]ii</font>";

        // Button images
        if (chr.startsWith('\\'))
        {
            chr = "<img src=\":/resources/" + chr.mid(1, chr.size() - 2) + ".png\">";
        }

        if (colorIndices[i] != -1)
        {
            // New start (current not equal to previous)
            if (i == 0 || colorIndices[i] != colorIndices[i - 1])
            {
                fco::Color color = m_colorBlocks[static_cast<unsigned int>(colorIndices[i])].m_color;
                QString r,g,b,a;

                QCheckBox* checkbox = reinterpret_cast<QCheckBox*>(ui->VL_Hardcoded->layout()->itemAt(colorIndices[i] + 1)->widget());
                if (checkbox->isChecked())
                {
                    if (lineBreakCount < 2)
                    {
                        r = "FF"; g = "FF"; b = "FF";
                    }
                    else
                    {
                        r = "00"; g = "00"; b = "00";
                    }
                }
                else
                {
                    r.setNum(color.r, 16); if (color.r < 0x10) r = "0" + r;
                    g.setNum(color.g, 16); if (color.g < 0x10) g = "0" + g;
                    b.setNum(color.b, 16); if (color.b < 0x10) b = "0" + b;
                }
                a.setNum(color.a, 16); if (color.a < 0x10) a = "0" + a;
                chr = "<font color=\"#" + a + r + g + b + "\">" + chr;
            }

            // Color block ended (reached the end or next index is different)
            if (i == m_characterArray.size() - 1 || colorIndices[i] != colorIndices[i + 1])
            {
                chr = chr + "</font>";
            }
        }

        htmlString += chr;
    }

    // Check if default color is using hardcoded or not
    QString r,g,b,a;
    if (!m_defaultColor.IsHardcoded())
    {
        r.setNum(m_defaultColor.r, 16); if (m_defaultColor.r < 0x10) r = "0" + r;
        g.setNum(m_defaultColor.g, 16); if (m_defaultColor.g < 0x10) g = "0" + g;
        b.setNum(m_defaultColor.b, 16); if (m_defaultColor.b < 0x10) b = "0" + b;
    }
    else
    {
        if (lineBreakCount < 2)
        {
            r = "FF"; g = "FF"; b = "FF";
        }
        else
        {
            r = "00"; g = "00"; b = "00";
        }
    }
    a.setNum(m_defaultColor.a, 16); if (m_defaultColor.a < 0x10) a = "0" + a;
    htmlString = "<font color=\"#" + a + r + g + b + "\">" + htmlString + "</font>";

    m_previewLabel->setText(htmlString);

    if(lineBreakCount < 2)
    {
        m_textIsOmochao = true;
        ui->L_Preview->setMinimumHeight(86);
        ui->L_Preview->setMaximumHeight(86);
        ui->SA_Preview->setMinimumHeight(110);
        ui->L_Preview->setPixmap(QPixmap(":/resources/SubtitleBG.png"));
    }
    else
    {
        m_textIsOmochao = false;
        ui->L_Preview->setMinimumHeight(86 + (lineBreakCount - 1) * 27);
        ui->L_Preview->setMaximumHeight(86 + (lineBreakCount - 1) * 27);
        ui->SA_Preview->setMinimumHeight(110 + qMin((lineBreakCount - 1), 6) * 27);
        ui->L_Preview->setPixmap(QPixmap());
    }
}

//---------------------------------------------------------------------------
// Load a subtitle for editor
//---------------------------------------------------------------------------
void fcoEditorWindow::LoadSubtitle(unsigned int _groupID, unsigned int _subtitleID)
{
    // Un-highlight previous selection
    if (m_groupID != INT_MAX && m_subtitleID != INT_MAX)
    {
        QTreeWidgetItem* parent = ui->TW_TreeWidget->topLevelItem(static_cast<int>(m_groupID));
        parent->setForeground(0, QColor(0,0,0));
        parent->setForeground(1, QColor(0,0,0));
        QTreeWidgetItem* child = parent->child(static_cast<int>(m_subtitleID));
        child->setForeground(0, QColor(0,0,0));
        child->setForeground(1, QColor(0,0,0));
        child->setForeground(2, QColor(0,0,0));
    }

    // Save IDs
    m_groupID = _groupID;
    m_subtitleID = _subtitleID;

    // Highlight selected
    QTreeWidgetItem* parent = ui->TW_TreeWidget->topLevelItem(static_cast<int>(m_groupID));
    QString group = parent->text(0);
    parent->setForeground(0, QColor(255,0,0));
    parent->setForeground(1, QColor(255,0,0));
    QTreeWidgetItem* child = parent->child(static_cast<int>(m_subtitleID));
    child->setForeground(0, QColor(255,0,0));
    child->setForeground(1, QColor(255,0,0));
    child->setForeground(2, QColor(255,0,0));

    ui->PB_NewSubtitle->setEnabled(true);
    ui->PB_DeleteGroup->setEnabled(true);
    ui->PB_DeleteSubtitle->setEnabled(true);
    TW_UpdateUpDownButtons();

    // Load subtitle
    QString label = QString::fromStdString(m_fco->GetLabel(m_groupID, m_subtitleID));
    QString subtitle = QString::fromStdWString(m_fco->GetSubtitle(m_groupID, m_subtitleID));
    ui->LE_GroupName->setText(group);
    ui->LE_SubtitleName->setText(label);
    ui->LE_GroupName->setEnabled(true);
    ui->LE_SubtitleName->setEnabled(true);
    UpdateStatus("OK", "color: rgb(0, 170, 0);");
    ui->TE_TextEditor->setText(subtitle);
    ui->TE_TextEditor->setEnabled(true);

    m_textValid = true;
    m_textEdited = false;

    // Validate once to get individual characters (should always be valid)
    wstring errorMsg;
    m_fco->ValidateString(ui->TE_TextEditor->toPlainText().toStdWString(), errorMsg, m_characterArray);

    // Load default color
    ui->DefaultHardcoded->setChecked(false);
    m_defaultColor = m_fco->GetDefaultColor(m_groupID, m_subtitleID);
    QColor currentColor;
    if (m_defaultColor.IsHardcoded())
    {
        currentColor.setRgb(240, 240, 240);
        ui->DefaultColor->setEnabled(false);
        ui->DefaultHardcoded->setChecked(true);
    }
    else
    {
        currentColor.setRgb(m_defaultColor.r, m_defaultColor.g, m_defaultColor.b);
        ui->DefaultColor->setEnabled(true);
    }
    QPalette pal = palette();
    pal.setColor(QPalette::Base, currentColor);
    ui->DefaultColor->setPalette(pal);
    ui->DefaultHardcoded->setEnabled(true);
    ui->DefaultAlpha->setEnabled(true);
    ui->DefaultAlpha->setValue(m_defaultColor.a);

    // Load color blocks
    LoadColorBlocks(m_groupID, m_subtitleID);
    m_colorEdited = false;

    // Highlight text edit text
    ui->TE_TextEditor->selectAll();
    ui->TE_TextEditor->setFocus();

    ui->PB_Save->setEnabled(false);
    ui->PB_Reset->setEnabled(false);
    UpdateSubtitlePreview();
}

//---------------------------------------------------------------------------
// Remove all color blocks in the editor
//---------------------------------------------------------------------------
void fcoEditorWindow::ClearColorBlocks()
{
    ui->PB_AddColorBlock->setEnabled(false);

    // Reset color block editor
    m_colorBlocks.clear();
    for (unsigned int i = 0; i <= 8; i++)
    {
        QLayout* vBox = ui->HL_ColorBlocks->layout()->itemAt(static_cast<int>(i))->layout();
        while (vBox->count() > 2)
        {
            delete vBox->takeAt(1)->widget();
        }
    }
}

//---------------------------------------------------------------------------
// Get all color blocks of a subtitle
//---------------------------------------------------------------------------
void fcoEditorWindow::LoadColorBlocks(unsigned int _groupID, unsigned int _subtitleID)
{
    ClearColorBlocks();

    // Enable add button
    ui->PB_AddColorBlock->setEnabled(true);

    // Load color blocks
    m_fco->GetSubtitleColorBlocks(_groupID, _subtitleID, m_colorBlocks);
    for (unsigned int colorBlockID = 0; colorBlockID < m_colorBlocks.size(); colorBlockID++)
    {
        AddColorBlock(colorBlockID, m_colorBlocks[colorBlockID]);
    }
    EnableUpDownButtons();
}

//---------------------------------------------------------------------------
// Add a new color block for the editor
//---------------------------------------------------------------------------
void fcoEditorWindow::AddColorBlock(unsigned int _id, fco::ColorBlock _colorBlock)
{
    // ALL WIDGETS MUST HAVE MINIMUM HEIGHT 26
    // 0: ID, 1: Hardcoded, 2: Color, 3: Alpha, 4: Start, 5: End, 6: Move Up, 7: Move Down, 8: Delete

    QLabel *label = new QLabel();
    label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    label->setMinimumHeight(26);
    label->setText(QString::number(_id + 1) + ": ");
    label->setStyleSheet("font-size: 12px;");
    ui->VL_ID->insertWidget(static_cast<int>(_id + 1), label);

    QCheckBox *checkbox = new QCheckBox();
    checkbox->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    checkbox->setMinimumHeight(26);
    checkbox->setChecked(_colorBlock.m_color.IsHardcoded());
    checkbox->setStyleSheet("margin-left:32%;");
    ui->VL_Hardcoded->insertWidget(static_cast<int>(_id + 1), checkbox);
    connect(checkbox, SIGNAL(stateChanged(int)), this, SLOT(SetHardcoded(int)));

    QTextBrowser *colorBrowser = new QTextBrowser();
    colorBrowser->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    colorBrowser->setMaximumHeight(26);
    QColor currentColor;
    if (_colorBlock.m_color.IsHardcoded())
    {
        currentColor.setRgb(240, 240, 240);
        colorBrowser->setEnabled(false);
    }
    else
    {
        currentColor.setRgb(_colorBlock.m_color.r, _colorBlock.m_color.g, _colorBlock.m_color.b);
        colorBrowser->setEnabled(true);
    }
    QPalette pal = palette();
    pal.setColor(QPalette::Base, currentColor);
    colorBrowser->setPalette(pal);
    colorBrowser->installEventFilter(this);
    ui->VL_Color->insertWidget(static_cast<int>(_id + 1), colorBrowser);

    QSpinBox *alpha = new QSpinBox();
    alpha->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    alpha->setMinimumHeight(26);
    alpha->setMinimum(0);
    alpha->setMaximum(255);
    alpha->setValue(_colorBlock.m_color.a);
    alpha->setCursor(Qt::SizeVerCursor);
    alpha->installEventFilter(this);
    ui->VL_Alpha->insertWidget(static_cast<int>(_id + 1), alpha);
    connect(alpha, SIGNAL(valueChanged(int)), this, SLOT(ColorSpinBoxChanged(int)));

    QSpinBox *start = new QSpinBox();
    start->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    start->setMinimumHeight(26);
    start->setMinimum(0);
    start->setMaximum(static_cast<int>(m_characterArray.size() - 1));
    start->setValue(static_cast<int>(_colorBlock.m_start));
    start->setCursor(Qt::SizeVerCursor);
    start->installEventFilter(this);
    ui->VL_Start->insertWidget(static_cast<int>(_id + 1), start);
    connect(start, SIGNAL(valueChanged(int)), this, SLOT(ColorSpinBoxChanged(int)));

    QSpinBox *end = new QSpinBox();
    end->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    end->setMinimumHeight(26);
    end->setMinimum(0);
    end->setMaximum(static_cast<int>(m_characterArray.size() - 1));
    end->setValue(static_cast<int>(_colorBlock.m_end));
    end->setCursor(Qt::SizeVerCursor);
    end->installEventFilter(this);
    ui->VL_End->insertWidget(static_cast<int>(_id + 1), end);
    connect(end, SIGNAL(valueChanged(int)), this, SLOT(ColorSpinBoxChanged(int)));

    UpdateStartEndLimits(_id);

    QToolButton *moveUp = new QToolButton();
    moveUp->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    moveUp->setMinimumHeight(26);
    moveUp->setArrowType(Qt::ArrowType::UpArrow);
    ui->VL_Up->insertWidget(static_cast<int>(_id + 1), moveUp);
    connect(moveUp, SIGNAL(clicked(bool)), this, SLOT(SwapColorBlocks()));

    QToolButton *moveDown = new QToolButton();
    moveDown->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    moveDown->setMinimumHeight(26);
    moveDown->setArrowType(Qt::ArrowType::DownArrow);
    ui->VL_Down->insertWidget(static_cast<int>(_id + 1), moveDown);
    connect(moveDown, SIGNAL(clicked(bool)), this, SLOT(SwapColorBlocks()));

    QPushButton *del = new QPushButton();
    del->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    del->setMinimumHeight(26);
    del->setMaximumHeight(26);
    del->setText("X");
    del->setMaximumWidth(20);
    ui->VL_Delete->insertWidget(static_cast<int>(_id + 1), del);
    connect(del, SIGNAL(clicked()), this, SLOT(RemoveColorBlock()));
}

//---------------------------------------------------------------------------
// Signal sent when delete button is pressed
//---------------------------------------------------------------------------
void fcoEditorWindow::RemoveColorBlock()
{
    QWidget* widget = reinterpret_cast<QWidget*>(sender());
    int index = ui->VL_Delete->indexOf(widget);
    m_colorBlocks.erase(m_colorBlocks.begin() + index - 1);

    delete ui->VL_ID->layout()->takeAt(index)->widget();
    delete ui->VL_Hardcoded->layout()->takeAt(index)->widget();
    delete ui->VL_Color->layout()->takeAt(index)->widget();
    delete ui->VL_Alpha->layout()->takeAt(index)->widget();
    delete ui->VL_Start->layout()->takeAt(index)->widget();
    delete ui->VL_End->layout()->takeAt(index)->widget();
    delete ui->VL_Up->layout()->takeAt(index)->widget();
    delete ui->VL_Down->layout()->takeAt(index)->widget();
    delete ui->VL_Delete->layout()->takeAt(index)->widget();

    // Rename id
    for (int i = 1; i < ui->VL_ID->layout()->count() - 1; i++)
    {
        QLabel* label = reinterpret_cast<QLabel*>(ui->VL_ID->itemAt(i)->widget());
        label->setText(QString::number(i) + ": ");
    }

    // Save & Reset buttons
    m_colorEdited = true;
    ui->PB_Save->setEnabled(true);
    ui->PB_Reset->setEnabled(true);

    EnableUpDownButtons();
    UpdateSubtitlePreview();
}

//---------------------------------------------------------------------------
// When color blocks changed, need to fix up down button enability
//---------------------------------------------------------------------------
void fcoEditorWindow::EnableUpDownButtons()
{
    int count = ui->VL_Up->layout()->count();
    for (int i = 1; i < count - 1; i++)
    {
        QToolButton* upButton = reinterpret_cast<QToolButton*>(ui->VL_Up->itemAt(i)->widget());
        QToolButton* downButton = reinterpret_cast<QToolButton*>(ui->VL_Down->itemAt(i)->widget());

        upButton->setEnabled(i != 1);
        downButton->setEnabled(i != count - 2);
    }
}

//---------------------------------------------------------------------------
// Aplha, start or end spin box changed
//---------------------------------------------------------------------------
void fcoEditorWindow::ColorSpinBoxChanged(int _value)
{
    QWidget* widget = reinterpret_cast<QWidget*>(sender());
    bool isHardcoded = (widget == ui->DefaultAlpha);
    int index1 = ui->VL_Alpha->indexOf(widget);
    int index2 = ui->VL_Start->indexOf(widget);
    int index3 = ui->VL_End->indexOf(widget);

    if (isHardcoded)
    {
        m_defaultColor.a = static_cast<unsigned char>(_value);
    }
    else if (index1 > 0)
    {
        m_colorBlocks[static_cast<unsigned int>(index1 - 1)].m_color.a = static_cast<unsigned char>(_value);
    }
    else if (index2 > 0)
    {
        m_colorBlocks[static_cast<unsigned int>(index2 - 1)].m_start = static_cast<unsigned int>(_value);
        UpdateStartEndLimits(static_cast<unsigned int>(index2 - 1));
    }
    else if (index3 > 0)
    {
        m_colorBlocks[static_cast<unsigned int>(index3 - 1)].m_end = static_cast<unsigned int>(_value);
        UpdateStartEndLimits(static_cast<unsigned int>(index3 - 1));
    }
    else
    {
        return;
    }

    // Save & Reset buttons
    m_colorEdited = true;
    ui->PB_Save->setEnabled(true);
    ui->PB_Reset->setEnabled(true);

    UpdateSubtitlePreview();
}

//---------------------------------------------------------------------------
// Swap two color blocks
//---------------------------------------------------------------------------
void fcoEditorWindow::SwapColorBlocks()
{
    QWidget* widget = reinterpret_cast<QWidget*>(sender());
    int index1 = ui->VL_Up->indexOf(widget);
    int index2 = ui->VL_Down->indexOf(widget);

    if (index1 > 1)
    {
        // Move up
        fco::ColorBlock temp = m_colorBlocks[static_cast<unsigned int>(index1 - 1)];
        m_colorBlocks[static_cast<unsigned int>(index1 - 1)] = m_colorBlocks[static_cast<unsigned int>(index1 - 2)];
        m_colorBlocks[static_cast<unsigned int>(index1 - 2)] = temp;

        // 1:Hardcoded, 2: Color, 3: Alpha, 4: Start, 5: End
        for (int i = 1; i <= 5; i++)
        {
            QVBoxLayout* vBox = reinterpret_cast<QVBoxLayout*>(ui->HL_ColorBlocks->layout()->itemAt(i)->layout());
            widget = vBox->itemAt(index1)->widget();
            vBox->removeWidget(widget);
            vBox->insertWidget(index1 - 1, widget);
        }
    }
    else if (index2 > 0)
    {
        // Move down
        fco::ColorBlock temp = m_colorBlocks[static_cast<unsigned int>(index2 - 1)];
        m_colorBlocks[static_cast<unsigned int>(index2 - 1)] = m_colorBlocks[static_cast<unsigned int>(index2)];
        m_colorBlocks[static_cast<unsigned int>(index2)] = temp;

        // 1:Hardcoded, 2: Color, 3: Alpha, 4: Start, 5: End
        for (int i = 1; i <= 5; i++)
        {
            QVBoxLayout* vBox = reinterpret_cast<QVBoxLayout*>(ui->HL_ColorBlocks->layout()->itemAt(i)->layout());
            widget = vBox->itemAt(index2)->widget();
            vBox->removeWidget(widget);
            vBox->insertWidget(index2 + 1, widget);
        }
    }
    else
    {
        return;
    }

    // Save & Reset buttons
    m_colorEdited = true;
    ui->PB_Save->setEnabled(true);
    ui->PB_Reset->setEnabled(true);

    UpdateSubtitlePreview();
}

//---------------------------------------------------------------------------
// Update the upper and lower limit of start and end spin box
//---------------------------------------------------------------------------
void fcoEditorWindow::UpdateStartEndLimits(unsigned int _id)
{
    if (m_characterArray.empty())
    {
        return;
    }

    QSpinBox* start = reinterpret_cast<QSpinBox*>(ui->VL_Start->layout()->itemAt(static_cast<int>(_id + 1))->widget());
    QSpinBox* end = reinterpret_cast<QSpinBox*>(ui->VL_End->layout()->itemAt(static_cast<int>(_id + 1))->widget());

    // Clamp it first
    start->setMinimum(0);
    end->setMinimum(0);
    start->setMaximum(static_cast<int>(m_characterArray.size() - 1));
    end->setMaximum(static_cast<int>(m_characterArray.size() - 1));

    start->setMaximum(end->value());
    end->setMinimum(start->value());
}

//---------------------------------------------------------------------------
// Hardcoded checkbox is changed
//---------------------------------------------------------------------------
void fcoEditorWindow::SetHardcoded(int _state)
{
    QCheckBox* checkbox = reinterpret_cast<QCheckBox*>(sender());

    int index = ui->VL_Hardcoded->indexOf(checkbox);
    bool isHardcoded = (checkbox == ui->DefaultHardcoded);
    QTextBrowser *colorBrowser;
    if (isHardcoded)
    {
        colorBrowser = ui->DefaultColor;
    }
    else
    {
        colorBrowser = reinterpret_cast<QTextBrowser*>(ui->VL_Color->itemAt(index)->widget());
    }

    QColor currentColor;
    fco::Color& color = isHardcoded ? m_defaultColor : m_colorBlocks[static_cast<unsigned int>(index - 1)].m_color;
    if (_state == Qt::CheckState::Unchecked)
    {
        if (m_textIsOmochao)
        {
            color.SetWhite();
            currentColor.setRgb(255, 255, 255);
        }
        else
        {
            color.SetBlack();
            currentColor.setRgb(1, 0, 0);
        }
        colorBrowser->setEnabled(true);
    }
    else
    {
        color.SetHardcoded();
        currentColor.setRgb(240, 240, 240);
        colorBrowser->setEnabled(false);
    }

    QPalette pal = palette();
    pal.setColor(QPalette::Base, currentColor);
    colorBrowser->setPalette(pal);

    // Save & Reset buttons
    m_colorEdited = true;
    ui->PB_Save->setEnabled(true);
    ui->PB_Reset->setEnabled(true);

    UpdateSubtitlePreview();
}
