#include "fte.h"

#include <assert.h>
#include <stdlib.h>

#include <algorithm>
#include <cstring>
#include <sstream>
#include <fstream>
#include <iostream>
#include <codecvt>
#include <locale>

fte::fte()
{
    Reset();
}

void fte::Reset()
{
    m_buttonTextureIndex = 0xFFFFFFFF;
    m_textures.clear();
    m_buttonData.clear();
    m_data.clear();
}

//-----------------------------------------------------
// Read an int from 4 bytes
//-----------------------------------------------------
unsigned int fte::ReadInt
(
    FILE * _file
)
{
    // Read int, require flipping bytes
    unsigned int flippedInt;
    fread(&flippedInt, sizeof(unsigned int), 1, _file);
    return _byteswap_ulong(flippedInt);
}

float fte::ReadFloat(FILE *_file)
{
    float f;
    unsigned int i = ReadInt(_file);

    assert(sizeof f == sizeof(i));
    memcpy(&f, &i, sizeof(f));

    return f;
}

//-----------------------------------------------------
// Read a string from bytes
//-----------------------------------------------------
string fte::ReadAscii
(
    FILE* _file
)
{
    // Get string length, require flipping bytes
    unsigned int stringLength = ReadInt(_file);

    // Read bytes as ascii
    char *stringBuffer = new char[stringLength + 1];
    stringBuffer[stringLength] = '\0';
    fread(stringBuffer, 1, stringLength, _file);

    // Check for @ padding
    while (ftell(_file) % 0x04 != 0x00)
    {
        char padding;
        fread(&padding, 1, 1, _file);
        assert(padding == '@');
    }

    // Cast to string and clean up
    string stringReturn(stringBuffer);
    delete[] stringBuffer;

    return stringReturn;
}

//-----------------------------------------------------
// Write 4 bytes from int
//-----------------------------------------------------
void fte::WriteInt
(
    FILE * _file,
    unsigned int _writeInt
)
{
    _writeInt = _byteswap_ulong(_writeInt);
    fwrite(&_writeInt, sizeof(unsigned int), 1, _file);
}

void fte::WriteFloat(FILE *_file, float _writeFloat)
{
    unsigned int i;
    assert(sizeof _writeFloat == sizeof(i));
    memcpy(&i, &_writeFloat, sizeof(i));
    WriteInt(_file, i);
}

//-----------------------------------------------------
// Read a bytes from string
//-----------------------------------------------------
void fte::WriteAscii
(
    FILE * _file,
    string _writeString
)
{
    // Write length
    unsigned int stringLength = _writeString.size();
    WriteInt(_file, stringLength);

    // Write ascii bytes
    char *stringBuffer = new char[stringLength + 1];
    strcpy_s(stringBuffer, stringLength + 1, _writeString.c_str());
    fwrite(stringBuffer, 1, stringLength, _file);

    // Write @ paddings
    while (ftell(_file) % 0x04 != 0x00)
    {
        char padding = '@';
        fwrite(&padding, 1, 1, _file);
    }

    delete[] stringBuffer;
}

bool fte::Import(const string &_fileName, string &_errorMsg)
{
    Reset();

    FILE* fteFile;
    fopen_s(&fteFile, _fileName.c_str(), "rb");
    if (!fteFile)
    {
        _errorMsg = "Unable to open file!";
        return false;
    }

    // Header
    fseek(fteFile, 0x8, SEEK_SET);

    uint32_t textureCount = ReadInt(fteFile);
    for (uint32_t i = 0; i < textureCount; i++)
    {
        Texture texture;
        texture.m_name = ReadAscii(fteFile);
        texture.m_width = ReadInt(fteFile);
        texture.m_height = ReadInt(fteFile);
        m_textures.push_back(texture);
    }

    uint32_t characterCount = ReadInt(fteFile);
    m_buttonData.reserve(13);
    m_data.reserve(characterCount - 29);
    uint32_t index = 0x64; // A button
    for (uint32_t i = 0; i < characterCount; i++)
    {
        Data data;
        data.Read(fteFile);

        if (index <= 0x6F || index == 0x78)
        {
            m_buttonData.push_back(data);
            m_buttonTextureIndex = data.m_textureIndex;
        }
        else if (index >= 0x82)
        {
            m_data.push_back(data);
        }

        index++;
    }

    fclose(fteFile);
    return true;
}

bool fte::Export(const string &_path, string &_errorMsg)
{
    FILE* output;
    fopen_s(&output, (_path + "/All.fte").c_str(), "wb");

    WriteInt(output, 4);
    WriteInt(output, 1);

    WriteInt(output, m_textures.size());
    for (Texture const& texture : m_textures)
    {
        WriteAscii(output, texture.m_name);
        WriteInt(output, texture.m_width);
        WriteInt(output, texture.m_height);
    }

    WriteInt(output, m_buttonData.size() + 18 + m_data.size());
    uint32_t index = 0x64;
    for (Data const& data : m_buttonData)
    {
        // Skip to DPad
        while (index > 0x6F && index < 0x78)
        {
            fseek(output, 0x18, SEEK_CUR);
            index++;
        }

        data.Write(output, 0);
        index++;
    }

    // Skip to characters
    while (index < 0x82)
    {
        fseek(output, 0x18, SEEK_CUR);
        index++;
    }

    for (Data const& data : m_data)
    {
        data.Write(output, 0x1500);
        index++;
    }

    fclose(output);

    FILE* output2;
    fopen_s(&output2, (_path + "/All.fco").c_str(), "wb");

    WriteInt(output2, 4);
    WriteInt(output2, 1);
    WriteInt(output2, 1);
    WriteAscii(output2, "All");

    WriteInt(output2, 1);
    WriteAscii(output2, "direct");

    index = 0x82;
    string list = " ?0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz!?,.:;'\"/-+*#$%&()=[]<>@";
    WriteInt(output2, m_data.size());
    for (size_t i = 0; i < m_data.size(); i++)
    {
        if (i < list.size())
        {
            WriteAscii(output2, list.substr(i, 1));
        }
        else
        {
            WriteAscii(output2, "?");
        }
        WriteInt(output2, 1);
        WriteInt(output2, index);
        WriteInt(output2, 0);
        WriteInt(output2, 0);
        WriteInt(output2, 0);
        index++;
    }

    fclose(output2);

    return true;
}

void fte::GenerateFcoDatabase(const wstring &content)
{
    wstring_convert< codecvt_utf8<wchar_t> > utfconv;

    FILE* database;
    fopen_s(&database, "fcoDatabase.txt", "w+,ccs=UTF-8");

    // Hardcode button special symbols
    fwprintf_s(database, L"00 00 00 64 = \\A\\\n");
    fwprintf_s(database, L"00 00 00 65 = \\B\\\n");
    fwprintf_s(database, L"00 00 00 66 = \\Y\\\n");
    fwprintf_s(database, L"00 00 00 67 = \\X\\\n");
    fwprintf_s(database, L"00 00 00 68 = \\LB\\\n");
    fwprintf_s(database, L"00 00 00 69 = \\RB\\\n");
    fwprintf_s(database, L"00 00 00 6A = \\LT\\\n");
    fwprintf_s(database, L"00 00 00 6B = \\RT\\\n");
    fwprintf_s(database, L"00 00 00 6C = \\Start\\\n");
    fwprintf_s(database, L"00 00 00 6D = \\Back\\\n");
    fwprintf_s(database, L"00 00 00 6E = \\LStick\\\n");
    fwprintf_s(database, L"00 00 00 6F = \\RStick\\\n");
    fwprintf_s(database, L"00 00 00 78 = \\DPad\\\n");

    // Start index
    unsigned int hex = 0x00000082;

    unsigned int index = 0;
    while (index < content.size())
    {
        // Mask out four bytes from offset
        unsigned int intBuffer[4];
        intBuffer[0] = (hex >> 24) & 0x000000FF;
        intBuffer[1] = (hex >> 16) & 0x000000FF;
        intBuffer[2] = (hex >> 8) & 0x000000FF;
        intBuffer[3] = hex & 0x000000FF;

        wchar_t symbol = content[index];
        fwprintf_s(database, L"%02X %02X %02X %02X = %c\n", intBuffer[0], intBuffer[1], intBuffer[2], intBuffer[3], symbol);

        index++;
        hex++;
    }
    fclose(database);
}


void fte::Data::Read(FILE *_file)
{
    m_textureIndex = ReadInt(_file);
    m_left = ReadFloat(_file);
    m_top = ReadFloat(_file);
    m_right = ReadFloat(_file);
    m_bottom = ReadFloat(_file);
    fread(&m_wchar, 2, 1, _file);
    m_wchar = _byteswap_ushort(m_wchar);
    fseek(_file, 0x2, SEEK_CUR);
}

void fte::Data::Write(FILE *_file, uint16_t unknown) const
{
    WriteInt(_file, m_textureIndex);
    WriteFloat(_file, m_left);
    WriteFloat(_file, m_top);
    WriteFloat(_file, m_right);
    WriteFloat(_file, m_bottom);
    wchar_t flipped_wchar = _byteswap_ushort(m_wchar);
    fwrite(&flipped_wchar, 2, 1, _file);
    fwrite(&unknown, 2, 1, _file);
}
