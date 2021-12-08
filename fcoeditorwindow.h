#ifndef FCOEDITORWINDOW_H
#define FCOEDITORWINDOW_H

#include <QMainWindow>
#include <QSettings>
#include <QFileDialog>
#include <QFileInfo>
#include <QFile>
#include <QFontDatabase>
#include <QSpinBox>
#include <QMessageBox>
#include <QMouseEvent>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QTreeWidgetItem>
#include <QTextBrowser>
#include <QToolButton>
#include <QCommonStyle>
#include <QColorDialog>
#include <QCloseEvent>
#include <QShortcut>
#include <QMimeData>
#include <QDebug>

#include "fco.h"
#include "eventcaptioneditor.h"

using namespace std;

namespace Ui {
class fcoEditorWindow;
}

class fcoEditorWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit fcoEditorWindow(QWidget *parent = nullptr);
    ~fcoEditorWindow();

    bool eventFilter(QObject *object, QEvent *event);
    void closeEvent (QCloseEvent *event);

    // Easy file reading
    void dragEnterEvent(QDragEnterEvent *e);
    void dropEvent(QDropEvent *e);
    void passArgument(std::string const& _file);

private slots:
    // Menu bar
    void on_actionOpen_triggered();
    void on_actionSave_triggered();
    void on_actionSave_as_triggered();
    void on_actionClose_triggered();
    void on_actionAbout_triggered();
    void on_actionAdd_Unsupported_Characters_triggered();
    void on_actionAbout_Qt_triggered();
    void on_actionEvent_Caption_Editor_cap_triggered();

    // Push buttons
    void on_PB_Find_clicked();
    void on_PB_Reset_clicked();
    void on_PB_Save_clicked();
    void on_PB_AddColorBlock_clicked();
    void on_PB_Expand_clicked();
    void on_PB_Collapse_clicked();
    void on_PB_NewGroup_clicked();
    void on_PB_DeleteGroup_clicked();
    void on_PB_NewSubtitle_clicked();
    void on_PB_DeleteSubtitle_clicked();
    void on_PB_GroupUp_clicked();
    void on_PB_GroupDown_clicked();
    void on_PB_SubtitleUp_clicked();
    void on_PB_SubtitleDown_clicked();
    void on_PB_SerifuID_clicked();

    // Tree view
    void on_TW_TreeWidget_itemDoubleClicked(QTreeWidgetItem *item, int column);

    // Line edit
    void on_LE_Find_textEdited(const QString &text);
    void on_LE_Find_returnPressed();
    void on_LE_GroupName_textEdited(const QString &text);
    void on_LE_SubtitleName_textEdited(const QString &text);

    // Subtitle editor
    void on_TE_TextEditor_textChanged();
    void on_TE_TextEditor_undoAvailable(bool b);

    // Color blocks editor
    void RemoveColorBlock();
    void ColorSpinBoxChanged(int _value);
    void SwapColorBlocks();
    void SetHardcoded(int _state);

    // Shortcuts
    void on_Shortcut_ApplyChanges();
    void on_Shortcut_ResetSubtitle();
    void on_Shortcut_Find();

    // Pass subtitle to caption editor
    void SearchForSubtitle(QString _group, QString _cell);

private:
    void OpenFile(QString const& fcoFile, bool showSuccess = true);
    bool DiscardSaveMessage(QString _title, QString _message, bool _checkFileEdited = false);

    // Tree widget
    void TW_Refresh();
    void TW_UpdateSerifuID();
    void TW_UpdateUpDownButtons();
    void TW_AddSubgroup(QString _groupName, unsigned int _groupID);
    void TW_AddSubtitle(QTreeWidgetItem *_parent, QString _label, QString _subtitle);
    void TW_FocusItem(unsigned int _groupID, unsigned int _subtitleID);

    // Subtitle editor
    void ResetSubtitleEditor();
    void UpdateStatus(QString _status, QString _styleSheet = "color: rgb(0, 0, 0);");
    void UpdateSubtitlePreview();
    void LoadSubtitle(unsigned int _groupID, unsigned int _subtitleID);

    // Color blocks editor
    void ClearColorBlocks();
    void LoadColorBlocks(unsigned int _groupID, unsigned int _subtitleID);
    void AddColorBlock(unsigned int _id, fco::ColorBlock _colorBlock = fco::ColorBlock());
    void EnableUpDownButtons();
    void UpdateStartEndLimits(unsigned int _id);

private:
    Ui::fcoEditorWindow *ui;
    QSettings *m_settings;
    EventCaptionEditor *m_eventCaptionEditor = Q_NULLPTR;

    fco* m_fco;
    QString m_path;
    QString m_fileName;
    bool m_fileEdited;

    // Tree view button warning
    bool m_moveGroup;
    bool m_moveSubtitle;

    // Subtitle editor
    vector<wstring> m_characterArray;
    unsigned int m_groupID;
    unsigned int m_subtitleID;
    bool m_textValid;
    bool m_textEdited;
    bool m_textIsOmochao;
    QLabel* m_previewLabel;

    // Color blocks editor
    fco::Color m_defaultColor;
    vector<fco::ColorBlock> m_colorBlocks;
    bool m_colorEdited;

    // Dragging spin box
    int m_initValue;
    int m_mouseY;
    int m_dragScale;
    QSpinBox* m_dragSpinBox;
};

#endif // FCOEDITORWINDOW_H
