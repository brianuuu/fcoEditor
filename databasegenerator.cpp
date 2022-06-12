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
    m_ftePath = m_settings->value("DatabaseDefaultDirectory", QString()).toString();
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
    m_settings->setValue("DatabaseDefaultDirectory", m_ftePath);
    m_settings->setValue("DatabaseDefaultSize", this->size());
}

bool DatabaseGenerator::CofirmChangeMode()
{
    if (!m_fte.IsLoaded()) return true;

    QMessageBox::StandardButton resBtn = QMessageBox::Yes;
    QString str = "Changing mode will reset everything and unsaved changes will be lost, continue?";
    resBtn = QMessageBox::warning(this, "Change Mode", str, QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (resBtn == QMessageBox::No)
    {
        return false;
    }
    return true;
}

void DatabaseGenerator::Reset()
{
    m_fte = fte();

    ClearGraphicScene();
    for (int i = 0; i < BT_COUNT; i++)
    {
        qobject_cast<QSpinBox*>(ui->GL_Button->itemAtPosition(i + 1, CT_X)->widget())->setValue(0);
        qobject_cast<QSpinBox*>(ui->GL_Button->itemAtPosition(i + 1, CT_Y)->widget())->setValue(0);
        qobject_cast<QSpinBox*>(ui->GL_Button->itemAtPosition(i + 1, CT_Width)->widget())->setValue(0);
        qobject_cast<QSpinBox*>(ui->GL_Button->itemAtPosition(i + 1, CT_Height)->widget())->setValue(0);
    }

    ui->LE_Font->clear();
    ui->LE_ButtonTexture->clear();
    ui->TE_Characters->clear();

    ui->PB_Export->setEnabled(false);
    ui->PB_Export->setText(ui->RB_ModeEdit->isChecked() ? "Export .fte" : "Export .fte and Textures");
    ui->TE_Characters->setReadOnly(ui->RB_ModeEdit->isChecked());
}

void DatabaseGenerator::on_RB_ModeEdit_clicked()
{
    if (CofirmChangeMode())
    {
        Reset();
    }
    else
    {
        ui->RB_ModeNew->setChecked(true);
    }
}

void DatabaseGenerator::on_RB_ModeNew_clicked()
{
    if (CofirmChangeMode())
    {
        Reset();
    }
    else
    {
        ui->RB_ModeEdit->setChecked(true);
    }
}

void DatabaseGenerator::on_PB_Import_clicked()
{
    QString path = "";
    if (!m_ftePath.isEmpty())
    {
        path = m_ftePath;
    }

    QString fteFile = QFileDialog::getOpenFileName(this, tr("Open"), path, "FTE File (*.fte)");
    if (fteFile == Q_NULLPTR) return;

    // Save directory
    QFileInfo info(fteFile);
    m_ftePath = info.dir().absolutePath();

    Reset();

    // Load fte file
    string errorMsg;
    if (m_fte.Import(fteFile.toStdString(), errorMsg))
    {
        m_texturePath = m_ftePath;
        UpdateDrawButtonTexture();

        QString characters;
        for (fte::Data const& data : m_fte.m_data)
        {
            characters += QString::fromWCharArray(&data.m_wchar, 1);
        }
        ui->TE_Characters->setText(characters);

        UpdateFontTextures();
    }
}

void DatabaseGenerator::on_PB_Export_clicked()
{
    QString path = "";
    if (!m_ftePath.isEmpty())
    {
        path = m_ftePath;
    }

    QString dir = QFileDialog::getExistingDirectory(this, tr("Open Directory"), path, QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (dir == Q_NULLPTR) return;
    m_ftePath = dir;

    uint32_t textureIndex = 0;
    m_fte.m_data.clear();
    m_fte.m_textures.clear();
    for (int i = 0; i < m_fontTextures.size(); i++)
    {
        FontTextureData const& fontTex = m_fontTextures[i];
        QSize size = fontTex.m_texture.size();
        for (CharacterData const& characterData : fontTex.m_characterData)
        {
            fte::Data data;
            data.m_left = characterData.m_x / float(size.width());
            data.m_top = characterData.m_y / float(size.height());
            data.m_right = (characterData.m_x + characterData.m_width) / float(size.width());
            data.m_bottom = (characterData.m_y + characterData.m_height) / float(size.height());
            data.m_textureIndex = textureIndex;
            data.m_wchar = characterData.m_char.unicode();
            m_fte.m_data.push_back(data);
        }

        QString name = "All_" + QStringLiteral("%1").arg(i, 3, 10, QLatin1Char('0'));
        fontTex.m_texture.save(dir + "/" + name + ".dds");
        fte::Texture texture;
        texture.m_name = name.toStdString();
        texture.m_width = uint32_t(size.width());
        texture.m_height = uint32_t(size.height());
        m_fte.m_textures.push_back(texture);
        textureIndex++;
    }

    m_fte.m_buttonData.clear();
    QSize size = m_buttonImage.size();
    for (int i = 0; i < BT_COUNT; i++)
    {
        QSpinBox* x = qobject_cast<QSpinBox*>(ui->GL_Button->itemAtPosition(i + 1, CT_X)->widget());
        QSpinBox* y = qobject_cast<QSpinBox*>(ui->GL_Button->itemAtPosition(i + 1, CT_Y)->widget());
        QSpinBox* width = qobject_cast<QSpinBox*>(ui->GL_Button->itemAtPosition(i + 1, CT_Width)->widget());
        QSpinBox* height = qobject_cast<QSpinBox*>(ui->GL_Button->itemAtPosition(i + 1, CT_Height)->widget());

        fte::Data data;
        data.m_left = x->value() / float(size.width());
        data.m_top = y->value() / float(size.height());
        data.m_right = (x->value() + width->value()) / float(size.width());
        data.m_bottom = (y->value() + height->value()) / float(size.height());
        data.m_textureIndex = textureIndex;
        data.m_wchar = 0;
        m_fte.m_buttonData.push_back(data);
    }

    m_buttonImage.save(dir + "/" + ui->LE_ButtonTexture->text() + ".dds");
    fte::Texture texture;
    texture.m_name = ui->LE_ButtonTexture->text().toStdString();
    texture.m_width = uint32_t(size.width());
    texture.m_height = uint32_t(size.height());
    m_fte.m_textures.push_back(texture);
    textureIndex++;

    string errorMsg;
    m_fte.Export(dir.toStdString(), errorMsg);
}

void DatabaseGenerator::on_SB_FontSize_valueChanged(int arg1)
{
    m_font.setPointSize(arg1);

    QFontMetrics fontMetrics(m_font);
    ui->SB_OffsetY->setValue(int(fontMetrics.height() * 20.0f / 28.0f));

    UpdateFontTextures(false);
}

void DatabaseGenerator::on_SB_Spacing_valueChanged(int arg1)
{
    m_font.setLetterSpacing(QFont::SpacingType::AbsoluteSpacing, arg1);
    UpdateFontTextures(false);
}

void DatabaseGenerator::on_SB_OffsetX_valueChanged(int arg1)
{
    UpdateFontTextures(false);
}

void DatabaseGenerator::on_SB_OffsetY_valueChanged(int arg1)
{
    UpdateFontTextures(false);
}

void DatabaseGenerator::on_TE_Characters_textChanged()
{
    UpdateFontTextures(false);
    ui->PB_ExportDatabase->setEnabled(!ui->TE_Characters->toPlainText().isEmpty());
}

void DatabaseGenerator::on_PB_ExportDatabase_clicked()
{
    QMessageBox::StandardButton resBtn = QMessageBox::Yes;
    QString str = "Generate fcoDatabase.txt with the current character list? This will allow fcoEditor to use the new font database.\n(You must restart fcoEditor after for this to take effect)";
    resBtn = QMessageBox::warning(this, "Generate fcoDatabase.txt", str, QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (resBtn == QMessageBox::No)
    {
        return;
    }

    QDir::setCurrent(QApplication::applicationDirPath());
    fte::GenerateFcoDatabase(ui->TE_Characters->toPlainText().toStdWString());
    QMessageBox::information(this, "Generate fcoDatabase.txt", "New fcoDatabase.txt has been generated, please restart fcoEditor.");
}

void DatabaseGenerator::on_PB_Texture_clicked()
{
    QString path = "";
    if (!m_texturePath.isEmpty())
    {
        path = m_ftePath;
    }

    QString ddsFile = QFileDialog::getOpenFileName(this, tr("Open"), path, "DDS File (*.dds)");
    if (ddsFile == Q_NULLPTR) return;

    // Save directory
    QFileInfo info(ddsFile);
    m_texturePath = info.dir().absolutePath();

    // Set dds name
    int index = ddsFile.lastIndexOf('\\');
    if (index == -1) index = ddsFile.lastIndexOf('/');

    QString textureName = ddsFile.mid(index + 1);
    textureName = textureName.mid(0, textureName.size() - 4);

    ui->LE_ButtonTexture->setText(textureName);
    UpdateExportEnabled();
    if (!m_fte.m_buttonData.empty())
    {
        fte::Data const& data = m_fte.m_buttonData[0];
        m_fte.m_textures[data.m_textureIndex].m_name = textureName.toStdString();
    }

    ClearGraphicScene();
    UpdateDrawButtonTexture();
}

void DatabaseGenerator::on_PB_Font_clicked()
{
    QString path = "";
    if (!m_fontPath.isEmpty())
    {
        path = m_ftePath;
    }

    QString fontFile = QFileDialog::getOpenFileName(this, tr("Open"), path, "font File (*.otf *.ttf)");
    if (fontFile == Q_NULLPTR) return;

    // Save directory
    QFileInfo info(fontFile);
    m_fontPath = info.dir().absolutePath();

    // Set font name
    int index = fontFile.lastIndexOf('\\');
    if (index == -1) index = fontFile.lastIndexOf('/');
    ui->LE_Font->setText(fontFile.mid(index + 1));
    UpdateExportEnabled();

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
        if (ui->LE_Font->text().isEmpty())
        {
            QMessageBox::warning(this, "Font Texture", "Please import a font first!");
            ui->RB_Button->setChecked(true);
            return;
        }

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

void DatabaseGenerator::UpdateFontTextures(bool setToZero)
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
    FontTextureData data;
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
                CharacterData characterData;
                characterData.m_char = allCharacters.front();
                characterData.m_x = posXStart;
                characterData.m_y = countY * fontMetrics.height();
                characterData.m_width = posXEnd - posXStart;
                characterData.m_height = fontMetrics.height();
                data.m_characterData.push_back(characterData);
                painterHighlight.drawRect(characterData.m_x, characterData.m_y, characterData.m_width - 1, characterData.m_height - 1);

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
            data.m_texture = image;
            data.m_highlight = highlight;
            m_fontTextures.push_back(data);

            data.m_characterData.clear();
            image.fill(0);
            highlight.fill(0);
            countY = 0;
        }
    }

    ui->SB_FontIndex->setMaximum(m_fontTextures.size() - 1);
    UpdateDrawFontTexture(setToZero ? 0 : ui->SB_FontIndex->value());
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
            ui->LE_ButtonTexture->setText(QString::fromStdString(m_fte.m_textures[data.m_textureIndex].m_name));
            QString buttonImagePath = m_texturePath + "/" + ui->LE_ButtonTexture->text() + ".dds";
            if (!QFile::exists(buttonImagePath))
            {
                ui->LE_ButtonTexture->clear();
                QMessageBox::warning(this, "Import", "Unable to find button texture " + buttonImagePath);
            }
            else
            {
                m_buttonImage = QImage(buttonImagePath);
                buttonImageWidth = m_buttonImage.width();
                buttonImageHeight = m_buttonImage.height();
                m_graphic->setSceneRect(m_buttonImage.rect());

                ClearGraphicScene();
                m_buttonPixmap = m_graphic->addPixmap(QPixmap::fromImage(m_buttonImage));
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

    UpdateExportEnabled();
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

void DatabaseGenerator::UpdateExportEnabled()
{
    ui->PB_Export->setEnabled(!ui->LE_Font->text().isEmpty() && !ui->LE_ButtonTexture->text().isEmpty());
}
