#ifndef EVENTCAPTIONEDITOR_H
#define EVENTCAPTIONEDITOR_H

#include <QMainWindow>
#include <QDialog>
#include <QFileDialog>
#include <QFileInfo>
#include <QFile>
#include <QFontDatabase>
#include <QSpinBox>
#include <QMessageBox>
#include <QTreeWidgetItem>
#include <QtXml>

namespace Ui {
class EventCaptionEditor;
}

class EventCaptionEditor : public QMainWindow
{
    Q_OBJECT

public:
    explicit EventCaptionEditor(QWidget *parent = nullptr);
    ~EventCaptionEditor();

    bool eventFilter(QObject *object, QEvent *event);

    void SetDefaultPath(QString _path) {m_path = _path;}
    void SetPreviewText(QString _text);
    void ResetEditor();

signals:
    void UpdatePreview(QString _group, QString _cell);

private slots:
    void on_PB_Open_clicked();
    void on_PB_Save_clicked();
    void on_PB_SaveAs_clicked();
    void on_PB_Reset_clicked();

    void on_PB_Apply_clicked();
    void on_PB_AddSubtitle_clicked();
    void on_PB_Clear_clicked();

    void on_PB_AddCutscene_clicked();
    void on_PB_DeleteCutscene_clicked();

    void on_PB_Expand_clicked();
    void on_PB_Collapse_clicked();
    void on_PB_Sort_clicked();

    void on_PB_ShiftLeft_clicked();
    void on_PB_ShiftRight_clicked();

    void on_TW_TreeWidget_itemDoubleClicked(QTreeWidgetItem *item, int column);

    void on_LE_fcoFile_textEdited(const QString &arg1);
    void on_LE_Cutscene_textEdited(const QString &arg1);
    void on_LE_Group_textEdited(const QString &arg1);

    void on_Generic_textEdited();
    void on_SpinBox_valueChanged(int i);
    void on_StartEndTime_textEdited();
    void on_StartEndTime_returnPressed();

    void DeleteSubtitle();

private:
    void CheckAllEnablility();
    void SaveFile(QString const& _fileName);
    void AddSubtitle(int _start, int _length, QString const& _cell, QString const& _groupOverride = "");
    void ClearSubtitles();
    void UpdateSubtitlePreview(int _index);

    bool ValidateSubtitle(int _id);
    bool ValidateAllSubtitles();

    int ConvertTimeToFramesSimple(QString const& _time, int const _max);
    int ConvertTimeToFrames(QString const& _time, int const _max);
    QString ConvertFramesToTime(int _frames);

    void TW_AddCutscene(QString const& _name = "evXXX", QString const& _group = "evXXX");
    void TW_AddSubtitle(int _id, int _start, int _length, QString const& _cell, QString const& _groupOverride = "(default)");

    void ShiftFrames(bool _backward);

private:
    Ui::EventCaptionEditor *ui;

    // Members
    int m_id;
    bool m_edited;
    bool m_sortAscending;
    QString m_path;
    QString m_fileName;

    QLabel* m_previewLabel;
    int m_previewIndex;
};

#endif // EVENTCAPTIONEDITOR_H
