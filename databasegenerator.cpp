#include "databasegenerator.h"
#include "ui_databasegenerator.h"

DatabaseGenerator::DatabaseGenerator(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DatabaseGenerator)
{
    ui->setupUi(this);

    m_graphic = new QGraphicsScene(this);
    ui->GV_Preview->setScene(m_graphic);
    m_buttomPixmap = Q_NULLPTR;

    m_settings = new QSettings("brianuuu", "fcoEditor", this);
    m_path = m_settings->value("DatabaseDefaultDirectory", QString()).toString();
    this->resize(m_settings->value("DatabaseDefaultSize", QSize(1024, 720)).toSize());

    for (int i = 0; i < BT_COUNT; i++)
    {
        QSpinBox* x = new QSpinBox(this);
        QSpinBox* y = new QSpinBox(this);
        QSpinBox* width = new QSpinBox(this);
        QSpinBox* height = new QSpinBox(this);

        ui->GL_Button->addWidget(x, i + 1, CT_X);
        ui->GL_Button->addWidget(y, i + 1, CT_Y);
        ui->GL_Button->addWidget(width, i + 1, CT_Width);
        ui->GL_Button->addWidget(height, i + 1, CT_Height);
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

                    m_graphic->clear();
                    m_buttomPixmap = m_graphic->addPixmap(QPixmap::fromImage(image));
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

            QImage rect(width->value(), height->value(), QImage::Format_RGBA8888);
            rect.fill(0);
            QPainter painter(&rect);
            painter.setPen(Qt::red);
            painter.drawRect(0, 0, width->value() - 1, height->value() - 1);
            auto* pixmap = m_graphic->addPixmap(QPixmap::fromImage(rect));
            pixmap->setPos(x->value(), y->value());
        }

        ui->TE_Characters->clear();
        for (fte::Data const& data : m_fte.m_data)
        {
            ui->TE_Characters->insertPlainText(QString::fromWCharArray(&data.m_wchar, 1));
        }
    }
}

void DatabaseGenerator::on_PB_Export_clicked()
{

}

void DatabaseGenerator::on_PB_Texture_clicked()
{

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

    QImage image(512, 512, QImage::Format_RGB888);
    image.fill(0);
    QPainter painterImage(&image);
    painterImage.setFont(m_font);
    painterImage.setPen(Qt::white);

    m_fontTextures.clear();
    QFontMetrics fontMetrics(m_font);
    int maxCountY = 512 / fontMetrics.height();
    ui->SB_OffsetY->setValue(int(fontMetrics.height() * 20.0f / 28.0f));

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
    ui->SB_FontIndex->setValue(0);
    ui->RB_Font->setChecked(true);
    on_SB_FontIndex_valueChanged(0);
}

void DatabaseGenerator::on_SB_FontIndex_valueChanged(int arg1)
{
    if (!m_fontTextures.isEmpty() && arg1 < m_fontTextures.size())
    {
        m_graphic->clear();
        m_graphic->setSceneRect(0,0,512,512);
        m_graphic->addPixmap(QPixmap::fromImage(m_fontTextures[arg1].m_texture));
        m_graphic->addPixmap(QPixmap::fromImage(m_fontTextures[arg1].m_highlight));
    }
}

void DatabaseGenerator::on_RB_Font_toggled(bool checked)
{
    if (checked)
    {
        on_SB_FontIndex_valueChanged(ui->SB_FontIndex->value());
    }
}
