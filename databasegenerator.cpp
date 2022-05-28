#include "databasegenerator.h"
#include "ui_databasegenerator.h"

DatabaseGenerator::DatabaseGenerator(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DatabaseGenerator)
{
    ui->setupUi(this);

    m_graphic = new QGraphicsScene(this);
    ui->GV_Preview->setScene(m_graphic);
    m_buttonPixmap = Q_NULLPTR;

    m_settings = new QSettings("brianuuu", "fcoEditor", this);
    m_path = m_settings->value("DatabaseDefaultDirectory", QString()).toString();
    this->resize(m_settings->value("DatabaseDefaultSize", QSize(1024, 720)).toSize());

    for (int i = 0; i < BT_COUNT; i++)
    {
        QSpinBox* x = new QSpinBox(this);
        QSpinBox* y = new QSpinBox(this);
        QSpinBox* width = new QSpinBox(this);
        QSpinBox* height = new QSpinBox(this);
        connect(x, SIGNAL(valueChanged(int)), this, SLOT(on_GridLayout_SpinBox_valueChanged(int)));
        connect(y, SIGNAL(valueChanged(int)), this, SLOT(on_GridLayout_SpinBox_valueChanged(int)));
        connect(width, SIGNAL(valueChanged(int)), this, SLOT(on_GridLayout_SpinBox_valueChanged(int)));
        connect(height, SIGNAL(valueChanged(int)), this, SLOT(on_GridLayout_SpinBox_valueChanged(int)));

        ui->GL_Button->addWidget(x, i + 1, CT_X);
        ui->GL_Button->addWidget(y, i + 1, CT_Y);
        ui->GL_Button->addWidget(width, i + 1, CT_Width);
        ui->GL_Button->addWidget(height, i + 1, CT_Height);
        m_buttonHighlightPixmap[i] = Q_NULLPTR;
    }
}

DatabaseGenerator::~DatabaseGenerator()
{
    delete ui;
}

void DatabaseGenerator::closeEvent(QCloseEvent *event)
{
    m_settings->setValue("DatabaseDefaultDirectory", m_path);
    m_settings->setValue("DatabaseDefaultSize", this->size());
}

void DatabaseGenerator::on_PB_Import_clicked()
{
    QString path = "";
    if (!m_path.isEmpty())
    {
        path = m_path;
    }

    QString fteFile = QFileDialog::getOpenFileName(this, tr("Open"), path, "FTE File (*.fte)");
    if (fteFile == Q_NULLPTR) return;

    // Save directory
    QFileInfo info(fteFile);
    m_path = info.dir().absolutePath();

    // Load fco file
    string errorMsg;
    if (!m_fte.Import(fteFile.toStdString(), errorMsg))
    {
        UpdateDrawButtonTexture();

        ui->TE_Characters->clear();
        for (fte::Data const& data : m_fte.m_data)
        {
            ui->TE_Characters->insertPlainText(QString::fromWCharArray(&data.m_wchar, 1));
        }

        UpdateFontTextures();
    }
}

void DatabaseGenerator::on_PB_Export_clicked()
{

}

void DatabaseGenerator::on_SB_FontSize_valueChanged(int arg1)
{
    m_font.setPointSize(arg1);

    QFontMetrics fontMetrics(m_font);
    ui->SB_OffsetY->setValue(int(fontMetrics.height() * 20.0f / 28.0f));

    UpdateFontTextures();
}

void DatabaseGenerator::on_SB_Spacing_valueChanged(int arg1)
{
    m_font.setLetterSpacing(QFont::SpacingType::AbsoluteSpacing, arg1);
    UpdateFontTextures();
}

void DatabaseGenerator::on_SB_OffsetX_valueChanged(int arg1)
{
    UpdateFontTextures();
}

void DatabaseGenerator::on_SB_OffsetY_valueChanged(int arg1)
{
    UpdateFontTextures();
}

void DatabaseGenerator::on_PB_Texture_clicked()
{
    QString path = "";
    if (!m_path.isEmpty())
    {
        path = m_path;
    }

    QString ddsFile = QFileDialog::getOpenFileName(this, tr("Open"), path, "DDS File (*.dds)");
    if (ddsFile == Q_NULLPTR) return;

    // Save directory
    QFileInfo info(ddsFile);
    m_path = info.dir().absolutePath();

    // Set dds name
    int index = ddsFile.lastIndexOf('\\');
    if (index == -1) index = ddsFile.lastIndexOf('/');

    QString textureName = ddsFile.mid(index + 1);
    textureName = textureName.mid(0, textureName.size() - 4);

    ui->LE_ButtonTexture->setText(textureName);
    if (!m_fte.m_buttonData.empty())
    {
        fte::Data const& data = m_fte.m_buttonData[0];
        m_fte.m_textureNames[data.m_textureIndex] = textureName.toStdString();
    }

    ClearGraphicScene();
    UpdateDrawButtonTexture();
}

void DatabaseGenerator::on_PB_Font_clicked()
{
    QString path = "";
    if (!m_path.isEmpty())
    {
        path = m_path;
    }

    QString fontFile = QFileDialog::getOpenFileName(this, tr("Open"), path, "font File (*.otf *.ttf)");
    if (fontFile == Q_NULLPTR) return;

    // Save directory
    QFileInfo info(fontFile);
    m_path = info.dir().absolutePath();

    // Set font name
    int index = fontFile.lastIndexOf('\\');
    if (index == -1) index = fontFile.lastIndexOf('/');
    ui->LE_Font->setText(fontFile.mid(index + 1));

    int id = QFontDatabase::addApplicationFont(fontFile);
    QString family = QFontDatabase::applicationFontFamilies(id).at(0);
    m_font = QFont(family);
    m_font.setPointSize(ui->SB_FontSize->value());
    m_font.setLetterSpacing(QFont::SpacingType::AbsoluteSpacing, ui->SB_Spacing->value());

    QFontMetrics fontMetrics(m_font);
    ui->SB_OffsetY->setValue(int(fontMetrics.height() * 20.0f / 28.0f));

    UpdateFontTextures();
}

void DatabaseGenerator::on_GridLayout_SpinBox_valueChanged(int arg1)
{
    if (!m_buttonPixmap)
    {
        UpdateDrawButtonTexture();
    }

    QSpinBox* sb = qobject_cast<QSpinBox*>(sender());
    int idx = ui->GL_Button->indexOf(sb);
    if (idx >= 0)
    {
        int row, column, rowSpan, columnSpan;
        ui->GL_Button->getItemPosition(idx, &row, &column, &rowSpan, &columnSpan);
        if (row > 0 && column > 0 && m_buttonHighlightPixmap[row - 1])
        {
            QSpinBox* x = qobject_cast<QSpinBox*>(ui->GL_Button->itemAtPosition(row, CT_X)->widget());
            QSpinBox* y = qobject_cast<QSpinBox*>(ui->GL_Button->itemAtPosition(row, CT_Y)->widget());
            QSpinBox* width = qobject_cast<QSpinBox*>(ui->GL_Button->itemAtPosition(row, CT_Width)->widget());
            QSpinBox* height = qobject_cast<QSpinBox*>(ui->GL_Button->itemAtPosition(row, CT_Height)->widget());
            m_buttonHighlightPixmap[row - 1]->setRect(x->value() + 0.5, y->value() + 0.5, width->value() - 1, height->value() - 1);
        }
    }
}

void DatabaseGenerator::on_RB_Button_toggled(bool checked)
{
    if (checked)
    {
        UpdateDrawButtonTexture();
    }
}

void DatabaseGenerator::on_RB_Font_toggled(bool checked)
{
    if (checked)
    {
        UpdateDrawFontTexture(ui->SB_FontIndex->value());
    }
}

void DatabaseGenerator::on_SB_FontIndex_valueChanged(int arg1)
{
    UpdateDrawFontTexture(arg1);
}

void DatabaseGenerator::ClearGraphicScene()
{
    m_graphic->clear();
    m_buttonPixmap = Q_NULLPTR;
    for (int i = 0; i < BT_COUNT; i++)
    {
        m_buttonHighlightPixmap[i] = Q_NULLPTR;
    }
}

void DatabaseGenerator::UpdateFontTextures()
{
    if (ui->LE_Font->text().isEmpty()) return;

    QImage image(512, 512, QImage::Format_RGB888);
    image.fill(0);
    QPainter painterImage(&image);
    painterImage.setFont(m_font);
    painterImage.setPen(Qt::white);

    m_fontTextures.clear();
    QFontMetrics fontMetrics(m_font);
    int maxCountY = 512 / fontMetrics.height();

    QImage highlight(512, 512, QImage::Format_RGBA8888);
    highlight.fill(0);
    QPainter painterHighlight(&highlight);
    painterHighlight.setPen(Qt::red);

    int countY = 0;
    QString allCharacters = ui->TE_Characters->toPlainText();
    while (!allCharacters.isEmpty())
    {
        QString subCharacters;
        int stringWidth = 0;
        while (!allCharacters.isEmpty())
        {
            int posXStart = stringWidth;
            int newStringWidth = fontMetrics.horizontalAdvance(allCharacters.front());
            stringWidth = fontMetrics.horizontalAdvance(subCharacters) + newStringWidth;
            if (stringWidth >= 512)
            {
                break;
            }
            else
            {
                int posXEnd = stringWidth;
                painterHighlight.drawRect(posXStart, countY * fontMetrics.height(), posXEnd - posXStart, fontMetrics.height() - 1);

                subCharacters.append(allCharacters.front());
                allCharacters = allCharacters.mid(1);
            }
        }

        // Draw current row
        painterImage.drawText(ui->SB_OffsetX->value(), countY * fontMetrics.height() + ui->SB_OffsetY->value(), subCharacters);

        // Next row
        countY++;
        if (countY == maxCountY || allCharacters.isEmpty())
        {
            // Image is filled
            FontTextureData data;
            data.m_texture = image;
            data.m_highlight = highlight;
            m_fontTextures.push_back(data);

            image.fill(0);
            highlight.fill(0);
            countY = 0;
        }
    }

    ui->SB_FontIndex->setMaximum(m_fontTextures.size() - 1);
    UpdateDrawFontTexture(0);
}

void DatabaseGenerator::UpdateDrawButtonTexture()
{
    if (m_fte.m_buttonData.empty()) return;

    ui->RB_Button->setChecked(true);
    if (m_buttonPixmap)
    {
        ClearGraphicScene();
    }

    int buttonImageWidth = 0;
    int buttonImageHeight = 0;
    ui->LE_ButtonTexture->clear();
    for (int i = 0; i < BT_COUNT; i++)
    {
        fte::Data const& data = m_fte.m_buttonData[i];

        if (ui->LE_ButtonTexture->text().isEmpty())
        {
            ui->LE_ButtonTexture->setText(QString::fromStdString(m_fte.m_textureNames[data.m_textureIndex]));
            QString buttomImagePath = m_path + "/" + ui->LE_ButtonTexture->text() + ".dds";
            if (!QFile::exists(buttomImagePath))
            {
                QMessageBox::warning(this, "Import", "Unable to find button texture " + ui->LE_ButtonTexture->text() + ".dds");
            }
            else
            {
                QImage image(buttomImagePath);
                buttonImageWidth = image.width();
                buttonImageHeight = image.height();
                m_graphic->setSceneRect(image.rect());

                ClearGraphicScene();
                m_buttonPixmap = m_graphic->addPixmap(QPixmap::fromImage(image));
            }
        }

        QSpinBox* x = qobject_cast<QSpinBox*>(ui->GL_Button->itemAtPosition(i + 1, CT_X)->widget());
        QSpinBox* y = qobject_cast<QSpinBox*>(ui->GL_Button->itemAtPosition(i + 1, CT_Y)->widget());
        QSpinBox* width = qobject_cast<QSpinBox*>(ui->GL_Button->itemAtPosition(i + 1, CT_Width)->widget());
        QSpinBox* height = qobject_cast<QSpinBox*>(ui->GL_Button->itemAtPosition(i + 1, CT_Height)->widget());

        x->setRange(0, buttonImageWidth);
        y->setRange(0, buttonImageHeight);
        width->setRange(0, buttonImageWidth);
        height->setRange(0, buttonImageHeight);

        x->setValue(qRound(data.m_left * buttonImageWidth));
        y->setValue(qRound(data.m_top * buttonImageHeight));
        width->setValue(qRound(data.m_right * buttonImageWidth - x->value()));
        height->setValue(qRound(data.m_bottom * buttonImageHeight - y->value()));
        m_buttonHighlightPixmap[i] = m_graphic->addRect(x->value() + 0.5, y->value() + 0.5, width->value() - 1, height->value() - 1, QPen(Qt::red));
    }
}

void DatabaseGenerator::UpdateDrawFontTexture(int id)
{
    ui->RB_Font->setChecked(true);
    ui->SB_FontIndex->setValue(id);

    if (!m_fontTextures.isEmpty() && id < m_fontTextures.size())
    {
        ClearGraphicScene();
        m_graphic->setSceneRect(0,0,512,512);
        m_graphic->addPixmap(QPixmap::fromImage(m_fontTextures[id].m_texture));
        m_graphic->addPixmap(QPixmap::fromImage(m_fontTextures[id].m_highlight));
    }
}
