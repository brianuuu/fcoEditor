#ifndef DATABASEGENERATOR_H
#define DATABASEGENERATOR_H

#include <QDialog>
#include <QDebug>
#include <QFileDialog>
#include <QFontDatabase>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QImageReader>
#include <QMessageBox>
#include <QSettings>
#include <QSpinBox>

#include "fte.h"

namespace Ui {
class DatabaseGenerator;
}

class DatabaseGenerator : public QDialog
{
    Q_OBJECT

public:
    enum ButtonType : int
    {
        BT_A,
        BT_B,
        BT_X,
        BT_Y,
        BT_LB,
        BT_RB,
        BT_LT,
        BT_RT,
        BT_Start,
        BT_Back,
        BT_LStick,
        BT_RStick,

        BT_COUNT
    };

    enum ColumnType : int
    {
        CT_X = 1,
        CT_Y,
        CT_Width,
        CT_Height
    };

    struct CharacterData
    {
        QChar m_char;
        int m_x;
        int m_y;
        int m_width;
        int m_height;
    };

    struct FontTextureData
    {
        QImage m_texture;
        QImage m_highlight;
        QVector<CharacterData> m_characterData;
    };

public:
    explicit DatabaseGenerator(QWidget *parent = nullptr);
    ~DatabaseGenerator();

    void closeEvent (QCloseEvent *event);

    void Reset();
    void ClearGraphicScene();
    void UpdateFontTextures(bool setToZero = true);
    void UpdateDrawButtonTexture();
    void UpdateDrawFontTexture(int id);
    void UpdateExportEnabled();

private slots:
    void on_PB_Import_clicked();
    void on_PB_Export_clicked();

    void on_PB_Texture_clicked();
    void on_PB_Font_clicked();

    void on_GridLayout_SpinBox_valueChanged(int arg1);

    void on_SB_FontSize_valueChanged(int arg1);
    void on_SB_Spacing_valueChanged(int arg1);
    void on_SB_OffsetX_valueChanged(int arg1);
    void on_SB_OffsetY_valueChanged(int arg1);
    void on_TE_Characters_textChanged();

    void on_RB_Button_toggled(bool checked);
    void on_RB_Font_toggled(bool checked);
    void on_SB_FontIndex_valueChanged(int arg1);

private:
    Ui::DatabaseGenerator *ui;
    QSettings *m_settings;

    // Members
    QString m_path;
    fte m_fte;

    QFont m_font;
    QGraphicsScene* m_graphic;
    QImage m_buttonImage;
    QGraphicsPixmapItem* m_buttonPixmap;
    QGraphicsRectItem* m_buttonHighlightPixmap[BT_COUNT];
    QVector<FontTextureData> m_fontTextures;
};

#endif // DATABASEGENERATOR_H
