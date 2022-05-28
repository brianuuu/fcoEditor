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
        m_textureNames.push_back(ReadAscii(fteFile));
        fseek(fteFile, 0x8, SEEK_CUR);
    }

    uint32_t characterCount = ReadInt(fteFile);
    m_buttonData.reserve(12);
    m_data.reserve(characterCount - 30);
    for (uint32_t i = 0; i < characterCount; i++)
    {
        if (i >= 12 && i < 30)
        {
            fseek(fteFile, 0x18, SEEK_CUR);
            continue;
        }

        Data data;
        data.m_textureIndex = ReadInt(fteFile);
        data.m_left = ReadFloat(fteFile);
        data.m_top = ReadFloat(fteFile);
        data.m_right = ReadFloat(fteFile);
        data.m_bottom = ReadFloat(fteFile);
        fread(&data.m_wchar, 2, 1, fteFile);
        data.m_wchar = _byteswap_ushort(data.m_wchar);
        fseek(fteFile, 0x2, SEEK_CUR);

        if (i < 12)
        {
            m_buttonData.push_back(data);
        }
        else
        {
            m_data.push_back(data);
        }
    }

    fclose(fteFile);
}

bool fte::Export(const string &_fileName, string &_errorMsg)
{

}

