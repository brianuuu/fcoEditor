#ifndef DATABASEGENERATOR_H
#define DATABASEGENERATOR_H

#include <QDialog>
#include <QDebug>
#include <QFileDialog>
#include <QFontDatabase>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
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

private slots:
    void on_PB_Import_clicked();
    void on_PB_Export_clicked();

    void on_PB_Texture_clicked();
    void on_PB_Font_clicked();
    void on_SB_FontIndex_valueChanged(int arg1);
    void on_RB_Font_toggled(bool checked);

private:
    Ui::DatabaseGenerator *ui;
    QSettings *m_settings;

    // Members
    QString m_path;
    fte m_fte;

    QFont m_font;
    QGraphicsScene* m_graphic;
    QGraphicsPixmapItem* m_buttomPixmap;
    QVector<FontTextureData> m_fontTextures;
};

#endif // DATABASEGENERATOR_H
