//-----------------------------------------------------
// Name: fco.cpp
// Author: brianuuu
// Date: 3/5/2019
//-----------------------------------------------------

#include "fco.h"

#include <assert.h>
#include <stdlib.h>

#include <algorithm>
#include <cstring>
#include <sstream>
#include <fstream>
#include <iostream>
#include <codecvt>
#include <locale>

//-----------------------------------------------------
// Constructor
//-----------------------------------------------------
fco::fco()
{
    //DumpFteFile();
    //GenerateDatabase();
    m_init = Init();
    m_loaded = false;
}

//-----------------------------------------------------
// Destructor
//-----------------------------------------------------
fco::~fco()
{
}

//-----------------------------------------------------
// Read UTF-8 fco database
//-----------------------------------------------------
bool fco::Init()
{
    ifstream database("fcoDatabase.txt");

    if (!database.is_open())
    {
        // Missing database file
        return false;
    }

    m_database.clear();
    m_databaseRev.clear();

    stringstream ss;
    ss << database.rdbuf();

    // Compiler dependent!
    wstring_convert< codecvt_utf8<wchar_t> > utfconv;
    //wstring_convert< codecvt_utf8<wchar_t, consume_header, little_endian> > utfconv;

    wstring utf8Content = utfconv.from_bytes(ss.str());

    // Manually swap endian
    for (wchar_t& c : utf8Content)
    {
        c = (_byteswap_ushort(c));
    }

    unsigned int index = 1;
    while (index < utf8Content.size())
    {
        // Read the 4 literal bytes, convert to int
        unsigned int intBuffer[4];
        swscanf_s(utf8Content.substr(index, index + 10).c_str(), L"%02X %02X %02X %02X", &intBuffer[0], &intBuffer[1], &intBuffer[2], &intBuffer[3]);
        unsigned int value = intBuffer[0] << 24 | intBuffer[1] << 16 | intBuffer[2] << 8 | intBuffer[3];
        index += 11;

        // Ignore " = "
        index += 3;

        // Read the mapped unicode
        unsigned int keyEndIndex = utf8Content.find(L"\n", index);
        if (keyEndIndex == string::npos) keyEndIndex = utf8Content.size();
        wstring key = utf8Content.substr(index, keyEndIndex - index);
        index = keyEndIndex;

        // Insert symbol to int mapping
        if (m_database.find(key) == m_database.end())
        {
            m_database[key] = value;
        }
        else
        {
            // Duplicated Keys
            string u8str = utfconv.to_bytes(key);
            printf("UTF-8 symbol: ");
            for (char c : u8str)
            {
                printf("%02X", c);
            }
            unsigned int existEncoding = m_database[key];
            printf(" with 0x%08X encoding exist at 0x%08X\n", value, existEncoding);
        }

        // Insert int to symbol mapping
        if (m_databaseRev.find(value) == m_databaseRev.end())
        {
            m_databaseRev[value] = key;
        }
        else
        {
            // Duplicated Values
            printf("Redefinition of 0x%08X encoding.\n", value);
        }

        // Line break
        index++;
    }

    // Hardcode line break
    wstring lineBreak(L"\n");
    m_database[lineBreak] = 0x00000000;
    m_databaseRev[0x00000000] = lineBreak;

    database.close();
    return true;
}

//-----------------------------------------------------
// Load an fco file, return false if fail
//-----------------------------------------------------
bool fco::Load
(
    string const& _fileName,
    string& _errorMsg
)
{
    FILE* fcoFile;
    fopen_s(&fcoFile, _fileName.c_str(), "rb");
    if (!fcoFile)
    {
        _errorMsg = "Unable to open file!";
        return false;
    }

    // Header
    fseek(fcoFile, 0x0C, SEEK_SET);

    // Main group name
    m_groupName = ReadAscii(fcoFile);

    // Get all sub groups
    unsigned int subgroupsCount = ReadInt(fcoFile);
    m_subgroups.clear();
    m_subgroups.reserve(subgroupsCount);
    for (unsigned int i = 0; i < subgroupsCount; i++)
    {
        Subgroup subgroup;
        subgroup.m_name = ReadAscii(fcoFile);

        // Get all subtitles of a sub group
        unsigned int subtitlesCount = ReadInt(fcoFile);
        subgroup.m_subtitles.reserve(subtitlesCount);
        for (unsigned int j = 0; j < subtitlesCount; j++)
        {
            Subtitle subtitle;
            subtitle.m_label = ReadAscii(fcoFile);

            unsigned int subtitleLength = ReadInt(fcoFile);
            subtitle.m_subtitle.reserve(subtitleLength + 1);
            subtitle.m_subtitleSize = 0;
            for (unsigned int k = 0; k < subtitleLength; k++)
            {
                unsigned int encodedInt = ReadInt(fcoFile);
                if (encodedInt != 0x00000004)
                {
                    // Ignore premature termination of text
                    if (m_databaseRev.find(encodedInt) == m_databaseRev.end())
                    {
                        char buff[200];
                        snprintf(buff, sizeof(buff), "0x%08x is missing reference symbol, find the respective character in All.fte and update fcoDatabase.txt.\n", encodedInt);
                        _errorMsg = buff;
                        m_subgroups.clear();
                        return false;
                        //encodedInt = m_database[L"?"];
                    }

                    subtitle.m_subtitle.append(m_databaseRev[encodedInt]);
                    subtitle.m_subtitleSize++;
                }
            }

            // 00 00 00 04 Termination
            fseek(fcoFile, 0x04, SEEK_CUR);

            // Skip unknown 0x40 bytes of data
            unsigned char unknownData[0x40];
            fread(unknownData, 1, 0x40, fcoFile);

            // Get default color from unknown data
            subtitle.m_defaultColor.a = static_cast<unsigned char>(unknownData[0x0C]);
            subtitle.m_defaultColor.r = static_cast<unsigned char>(unknownData[0x0D]);
            subtitle.m_defaultColor.g = static_cast<unsigned char>(unknownData[0x0E]);
            subtitle.m_defaultColor.b = static_cast<unsigned char>(unknownData[0x0F]);

            // Get all color blocks of a subtitle
            unsigned int colorBlocksCount = ReadInt(fcoFile);
            subtitle.m_colorBlocks.reserve(colorBlocksCount);
            for (unsigned int c = 0; c < colorBlocksCount; c++)
            {
                ColorBlock colorBlock;
                colorBlock.m_start = ReadInt(fcoFile);
                colorBlock.m_end = ReadInt(fcoFile);

                // 00 00 00 02 Unknown data
                fseek(fcoFile, 0x4, SEEK_CUR);

                // Read ARGB
                fread(&colorBlock.m_color.a, 1, 1, fcoFile);
                fread(&colorBlock.m_color.r, 1, 1, fcoFile);
                fread(&colorBlock.m_color.g, 1, 1, fcoFile);
                fread(&colorBlock.m_color.b, 1, 1, fcoFile);

                // Store color block
                subtitle.m_colorBlocks.push_back(colorBlock);
            }

            // 00 00 00 00 Termination?
            fseek(fcoFile, 0x4, SEEK_CUR);

            // Store subtitle
            subgroup.m_subtitles.push_back(subtitle);
        }

        // Store sub group
        m_subgroups.push_back(subgroup);
    }

    fclose(fcoFile);
    m_loaded = true;
    return true;
}

//-----------------------------------------------------
// Read an int from 4 bytes
//-----------------------------------------------------
unsigned int fco::ReadInt
(
    FILE * _file
)
{
    // Read int, require flipping bytes
    unsigned int flippedInt;
    fread(&flippedInt, sizeof(unsigned int), 1, _file);
    return _byteswap_ulong(flippedInt);
}

//-----------------------------------------------------
// Read a string from bytes
//-----------------------------------------------------
string fco::ReadAscii
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
// Save an fco file
//-----------------------------------------------------
bool fco::Save
(
    string const& _fileName,
    string& _errorMsg
)
{
    FILE* output;
    fopen_s(&output, _fileName.c_str(), "wb");

    // Header
    WriteInt(output, 0x04);
    WriteInt(output, 0x01);
    WriteInt(output, 0x01);

    // Group Name
    WriteAscii(output, m_groupName);

    // Write each sub-groups
    WriteInt(output, m_subgroups.size());
    for (unsigned int subgroupID = 0; subgroupID < m_subgroups.size(); subgroupID++)
    {
        Subgroup& subgroup = m_subgroups[subgroupID];

        // Sub-group name
        WriteAscii(output, subgroup.m_name);

        // Write each subtitles
        WriteInt(output, subgroup.m_subtitles.size());
        for (unsigned int subtitleID = 0; subtitleID < subgroup.m_subtitles.size(); subtitleID++)
        {
            Subtitle& subtitle = subgroup.m_subtitles[subtitleID];

            // Subtitle label
            WriteAscii(output, subtitle.m_label);

            // Subtitle size check
            /*if (subtitle.m_subtitle.size() == 0)
            {
                fclose(output);
                _errorMsg = "(GroupID: " + to_string(subgroupID) + ", SubtitleID: " + to_string(subtitleID) + ") Subtitle cannot be empty!\n";
                return false;
            }*/

            // Write each symbols
            WriteInt(output, subtitle.m_subtitleSize);
            unsigned int index = 0;
            while(index < subtitle.m_subtitle.size())
            {
                wstring subString;
                if (subtitle.m_subtitle[index] == L'\\')
                {
                    // Special characters \A\, \B\ etc.
                    unsigned int endOfSpecial = subtitle.m_subtitle.find(L'\\', index + 1);
                    if (endOfSpecial != string::npos)
                    {
                        subString = subtitle.m_subtitle.substr(index, endOfSpecial - index + 1);
                        index = endOfSpecial;
                    }
                    else
                    {
                        fclose(output);
                        _errorMsg = "(GroupID: " + to_string(subgroupID) + ", SubtitleID: " + to_string(subtitleID) + ") Error in special character formating, must be \\xxxx\\!\n";
                        return false;
                    }
                }
                else
                {
                    // Single UTF-8 symbol
                    subString = subtitle.m_subtitle.substr(index, 1);
                }

                // Retrieve and write encoded int
                if (m_database.find(subString) != m_database.end())
                {
                    WriteInt(output, m_database[subString]);
                }
                else
                {
                    fclose(output);
                    _errorMsg = "(GroupID: " + to_string(subgroupID) + ", SubtitleID: " + to_string(subtitleID) + ") Unsupported character found at index " + to_string(index) + "!\n";
                    return false;
                }

                index++;
            }

            // 00 00 00 04 Termination
            WriteInt(output, 0x04);

            // Write unknown data, update size blocks
            unsigned char charBuffer[4];
            charBuffer[0] = static_cast<unsigned char>(((subtitle.m_subtitleSize - 1) >> 24) & 0x000000FF);
            charBuffer[1] = static_cast<unsigned char>(((subtitle.m_subtitleSize - 1) >> 16) & 0x000000FF);
            charBuffer[2] = static_cast<unsigned char>(((subtitle.m_subtitleSize - 1) >> 8) & 0x000000FF);
            charBuffer[3] = static_cast<unsigned char>((subtitle.m_subtitleSize - 1) & 0x000000FF);

            unsigned char unknownData[] = {0x00,0x00,0x00,0x00,charBuffer[0],charBuffer[1],charBuffer[2],charBuffer[3],0x00,0x00,0x00,0x02,0xFF,0x00,0x00,0x00,
                                           0x00,0x00,0x00,0x00,charBuffer[0],charBuffer[1],charBuffer[2],charBuffer[3],0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x15,
                                           0x00,0x00,0x00,0x00,charBuffer[0],charBuffer[1],charBuffer[2],charBuffer[3],0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,
                                           0x00,0x00,0x00,0x00,charBuffer[0],charBuffer[1],charBuffer[2],charBuffer[3],0x00,0x00,0x00,0x03,0x00,0x00,0x00,0x00};

            // Set default color
            unknownData[0x0C] = static_cast<unsigned char>(subtitle.m_defaultColor.a);
            unknownData[0x0D] = static_cast<unsigned char>(subtitle.m_defaultColor.r);
            unknownData[0x0E] = static_cast<unsigned char>(subtitle.m_defaultColor.g);
            unknownData[0x0F] = static_cast<unsigned char>(subtitle.m_defaultColor.b);

            fwrite(unknownData, 1, 0x40, output);

            // Write each color blocks
            WriteInt(output, subtitle.m_colorBlocks.size());
            for (unsigned int colorBlockID = 0; colorBlockID < subtitle.m_colorBlocks.size(); colorBlockID++)
            {
                ColorBlock& colorBlock = subtitle.m_colorBlocks[colorBlockID];

                WriteInt(output, colorBlock.m_start);
                WriteInt(output, colorBlock.m_end);

                // 00 00 00 02 Unknown data
                WriteInt(output, 0x02);

                // Write ARGB value
                fwrite(&colorBlock.m_color.a, 1, 1, output);
                fwrite(&colorBlock.m_color.r, 1, 1, output);
                fwrite(&colorBlock.m_color.g, 1, 1, output);
                fwrite(&colorBlock.m_color.b, 1, 1, output);
            }

            // 00 00 00 00 Termination?
            WriteInt(output, 0x00);
        }
    }

    fclose(output);
    return true;
}

//-----------------------------------------------------
// Write 4 bytes from int
//-----------------------------------------------------
void fco::WriteInt
(
    FILE * _file,
    unsigned int _writeInt
)
{
    _writeInt = _byteswap_ulong(_writeInt);
    fwrite(&_writeInt, sizeof(unsigned int), 1, _file);
}

//-----------------------------------------------------
// Read a bytes from string
//-----------------------------------------------------
void fco::WriteAscii
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


//-----------------------------------------------------
// Search user-input text, return false if not found
//-----------------------------------------------------
bool fco::Search
(
    wstring const& _wstring,
    unsigned int& _subgroupID,
    unsigned int& _subtitleID
)
{
    for (; _subgroupID < m_subgroups.size(); _subgroupID++)
    {
        Subgroup const& subgroup = m_subgroups[_subgroupID];
        for (; _subtitleID < subgroup.m_subtitles.size(); _subtitleID++)
        {
            Subtitle const& subtitle = subgroup.m_subtitles[_subtitleID];
            if (subtitle.m_subtitle.find(_wstring.c_str()) != wstring::npos)
            {
                return true;
            }
        }
        _subtitleID = 0;
    }

    return false;
}

//-----------------------------------------------------
// Check if string is valid
//-----------------------------------------------------
bool fco::ValidateString
(
    wstring const& _wstring,
    wstring& _errorMsg,
    vector<wstring>& _characterArray
)
{
    _characterArray.clear();

    // Size check
    /*if (_wstring.size() == 0)
    {
        _errorMsg = L"Subtitle cannot be empty";
        return false;
    }*/

    unsigned int index = 0;
    while (index < _wstring.size())
    {
        wstring subString;
        if (_wstring[index] == L'\\')
        {
            // Special characters \A\, \B\ etc.
            unsigned int endOfSpecial = _wstring.find(L'\\', index + 1);
            if (endOfSpecial != string::npos)
            {
                subString = _wstring.substr(index, endOfSpecial - index + 1);
                index = endOfSpecial;
            }
            else
            {
                _errorMsg = L"Error in special character formating, must be \\xxxx\\";
                _characterArray.clear();
                return false;
            }
        }
        else
        {
            // Single UTF-8 symbol
            subString = _wstring.substr(index, 1);
        }

        // Retrieve from database
        if (m_database.find(subString) == m_database.end())
        {
            _errorMsg = L"Unsupported character \"" + subString + L"\"";
            _characterArray.clear();
            return false;
        }

        _characterArray.push_back(subString);
        index++;
    }

    return true;
}

//-----------------------------------------------------
// Retrieve all sub-group names (for toolbar)
//-----------------------------------------------------
void fco::GetGroupNames
(
    vector<string>& _groupNames
)
{
    _groupNames.clear();
    for (Subgroup const& subgroup : m_subgroups)
    {
        _groupNames.push_back(subgroup.m_name);
    }
}

//-----------------------------------------------------
// Retrieve all subtitles of a sub-group (for toolbar)
//-----------------------------------------------------
void fco::GetSubgroupSubtitles
(
    unsigned int _subgroupID,
    vector<string>& _labels,
    vector<wstring>& _subtitles
)
{
    if (_subgroupID < m_subgroups.size())
    {
        _subtitles.clear();
        Subgroup const& subgroup = m_subgroups[_subgroupID];
        for (Subtitle const& subtitle : subgroup.m_subtitles)
        {
            _labels.push_back(subtitle.m_label);
            _subtitles.push_back(subtitle.m_subtitle);
        }
    }
}

//-----------------------------------------------------
// Retrive subtitle's label
//-----------------------------------------------------
string fco::GetLabel
(
    unsigned int _subgroupID,
    unsigned int _subtitleID
)
{
    string str;
    if (_subgroupID < m_subgroups.size())
    {
        Subgroup const& subgroup = m_subgroups[_subgroupID];
        if (_subtitleID < subgroup.m_subtitles.size())
        {
            Subtitle const& subtitle = subgroup.m_subtitles[_subtitleID];
            str = subtitle.m_label;
        }
    }

    return str;
}

//-----------------------------------------------------
// Retrive one subtitle (for editing)
//-----------------------------------------------------
wstring fco::GetSubtitle
(
    unsigned int _subgroupID,
    unsigned int _subtitleID
)
{
    wstring str;
    if (_subgroupID < m_subgroups.size())
    {
        Subgroup const& subgroup = m_subgroups[_subgroupID];
        if (_subtitleID < subgroup.m_subtitles.size())
        {
            Subtitle const& subtitle = subgroup.m_subtitles[_subtitleID];
            str = subtitle.m_subtitle;
        }
    }

    return str;
}

//-----------------------------------------------------
// Retrieve all color blocks of a subtitle (for editing)
//-----------------------------------------------------
void fco::GetSubtitleColorBlocks
(
    unsigned int _subgroupID,
    unsigned int _subtitleID,
    vector<ColorBlock>& _colorBlocks
)
{
    if (_subgroupID < m_subgroups.size())
    {
        Subgroup const& subgroup = m_subgroups[_subgroupID];
        if (_subtitleID < subgroup.m_subtitles.size())
        {
            _colorBlocks.clear();
            Subtitle const& subtitle = subgroup.m_subtitles[_subtitleID];
            for (ColorBlock const& colorBlock : subtitle.m_colorBlocks)
            {
                _colorBlocks.push_back(colorBlock);
            }
        }
    }
}

//-----------------------------------------------------
// Add a new group at the buttom
//-----------------------------------------------------
void fco::AddGroup()
{
    if (IsLoaded())
    {
        Subgroup newSubgroup;
        newSubgroup.m_name = "NO_NAME";
        m_subgroups.push_back(newSubgroup);

        AddSubtitle(m_subgroups.size() - 1, "Subtitle01");
    }
}

//-----------------------------------------------------
// Delete a group
//-----------------------------------------------------
void fco::DeleteGroup
(
        unsigned int _subgroupID
)
{
    if (_subgroupID < m_subgroups.size())
    {
        m_subgroups.erase(m_subgroups.begin() + static_cast<int>(_subgroupID));
    }
}

//-----------------------------------------------------
// Swap two groups
//-----------------------------------------------------
void fco::SwapGroup(unsigned int _subgroupID1, unsigned int _subgroupID2)
{
    if (_subgroupID1 < m_subgroups.size() && _subgroupID2 < m_subgroups.size())
    {
        Subgroup temp = m_subgroups[_subgroupID1];
        m_subgroups[_subgroupID1] = m_subgroups[_subgroupID2];
        m_subgroups[_subgroupID2] = temp;
    }
}

//-----------------------------------------------------
// Modify name of a group
//-----------------------------------------------------
void fco::ModifyGroupName
(
        unsigned int _subgroupID,
        string const& _groupName
)
{
    if (_subgroupID < m_subgroups.size())
    {
        Subgroup& subgroup = m_subgroups[_subgroupID];
        subgroup.m_name = _groupName;
    }
}

//-----------------------------------------------------
// Add a new subtitle at the buttom
//-----------------------------------------------------
void fco::AddSubtitle
(
        unsigned int _subgroupID,
        string const& _label,
        wstring const& _subtitle
)
{
    if (_subgroupID < m_subgroups.size())
    {
        Subtitle newSubtitle;
        newSubtitle.m_label = _label.empty() ? "NO_NAME" : _label;
        newSubtitle.m_subtitle = _subtitle.empty() ? L"DUMMY SUBTITLE" : _subtitle;
        newSubtitle.m_subtitleSize = newSubtitle.m_subtitle.size();

        Subgroup& subgroup = m_subgroups[_subgroupID];
        subgroup.m_subtitles.push_back(newSubtitle);
    }
}

//-----------------------------------------------------
// Delete a subtitle
//-----------------------------------------------------
void fco::DeleteSubtitle
(
        unsigned int _subgroupID,
        unsigned int _subtitleID
)
{
    if (_subgroupID < m_subgroups.size())
    {
        Subgroup& subgroup = m_subgroups[_subgroupID];
        if (_subtitleID < subgroup.m_subtitles.size())
        {
            subgroup.m_subtitles.erase(subgroup.m_subtitles.begin() + static_cast<int>(_subtitleID));
        }
    }
}

//-----------------------------------------------------
// Swap two subtitles in the same group
//-----------------------------------------------------
void fco::SwapSubtitle(unsigned int _subgroupID, unsigned int _subtitleID1, unsigned int _subtitleID2)
{
    if (_subgroupID < m_subgroups.size())
    {
        Subgroup& subgroup = m_subgroups[_subgroupID];
        if (_subtitleID1 < subgroup.m_subtitles.size() && _subtitleID2 < subgroup.m_subtitles.size())
        {
            Subtitle temp = subgroup.m_subtitles[_subtitleID1];
            subgroup.m_subtitles[_subtitleID1] = subgroup.m_subtitles[_subtitleID2];
            subgroup.m_subtitles[_subtitleID2] = temp;
        }
    }
}

//-----------------------------------------------------
// Modify a subtitle: MUST do ValidateString() first!
//-----------------------------------------------------
void fco::ModifySubtitle
(
    wstring const& _wstring,
    unsigned int _subgroupID,
    unsigned int _subtitleID
)
{
    if (_subgroupID < m_subgroups.size())
    {
        Subgroup& subgroup = m_subgroups[_subgroupID];
        if (_subtitleID < subgroup.m_subtitles.size())
        {
            Subtitle& subtitle = subgroup.m_subtitles[_subtitleID];
            subtitle.m_subtitle = _wstring;
            subtitle.m_subtitleSize = _wstring.size();

            unsigned int index = 0;
            while (index < _wstring.size())
            {
                if (_wstring[index] == L'\\')
                {
                    // Special characters \A\, \B\ etc., if validated should always find pairs
                    unsigned int endOfSpecial = _wstring.find(L'\\', index + 1);
                    subtitle.m_subtitleSize -= (endOfSpecial - index);
                    index = endOfSpecial;
                }
                index++;
            }
        }
    }
}

//-----------------------------------------------------
// Modify name of a subtitle
//-----------------------------------------------------
void fco::ModifySubtitleName(unsigned int _subgroupID, unsigned int _subtitleID, string const& _subtitleName)
{
    if (_subgroupID < m_subgroups.size())
    {
        Subgroup& subgroup = m_subgroups[_subgroupID];
        if (_subtitleID < subgroup.m_subtitles.size())
        {
            Subtitle& subtitle = subgroup.m_subtitles[_subtitleID];
            subtitle.m_label = _subtitleName;
        }
    }
}

//-----------------------------------------------------
// Modify a color block
//-----------------------------------------------------
void fco::ModifyColorBlock
(
    unsigned int _subgroupID,
    unsigned int _subtitleID,
    unsigned int _colorBlockID,
    ColorBlock const& _colorBlock
)
{
    if (_subgroupID < m_subgroups.size())
    {
        Subgroup& subgroup = m_subgroups[_subgroupID];
        if (_subtitleID < subgroup.m_subtitles.size())
        {
            Subtitle& subtitle = subgroup.m_subtitles[_subtitleID];
            if (_colorBlockID < subtitle.m_colorBlocks.size())
            {
                subtitle.m_colorBlocks[_colorBlockID] = _colorBlock;
            }
        }
    }
}

//-----------------------------------------------------
// Add a new color block (default orange)
//-----------------------------------------------------
void fco::AddColorBlock
(
    unsigned int _subgroupID,
    unsigned int _subtitleID
)
{
    if (_subgroupID < m_subgroups.size())
    {
        Subgroup& subgroup = m_subgroups[_subgroupID];
        if (_subtitleID < subgroup.m_subtitles.size())
        {
            Subtitle& subtitle = subgroup.m_subtitles[_subtitleID];
            subtitle.m_colorBlocks.push_back(ColorBlock());
        }
    }
}

//-----------------------------------------------------
// Remove a color block
//-----------------------------------------------------
void fco::RemoveColorBlock
(
    unsigned int _subgroupID,
    unsigned int _subtitleID,
    unsigned int _colorBlockID
)
{
    if (_subgroupID < m_subgroups.size())
    {
        Subgroup& subgroup = m_subgroups[_subgroupID];
        if (_subtitleID < subgroup.m_subtitles.size())
        {
            Subtitle& subtitle = subgroup.m_subtitles[_subtitleID];
            if (_colorBlockID < subtitle.m_colorBlocks.size())
            {
                subtitle.m_colorBlocks.erase(subtitle.m_colorBlocks.begin() + static_cast<int>(_colorBlockID));
            }
        }
    }
}

//-----------------------------------------------------
// Remove all color blocks
//-----------------------------------------------------
void fco::RemoveAllColorBlocks
(
    unsigned int _subgroupID,
    unsigned int _subtitleID
)
{
    if (_subgroupID < m_subgroups.size())
    {
        Subgroup& subgroup = m_subgroups[_subgroupID];
        if (_subtitleID < subgroup.m_subtitles.size())
        {
            Subtitle& subtitle = subgroup.m_subtitles[_subtitleID];
            subtitle.m_colorBlocks.clear();
        }
    }
}

//-----------------------------------------------------
// Swap two color blocks
//-----------------------------------------------------
void fco::SwapColorBlocks
(
    unsigned int _subgroupID,
    unsigned int _subtitleID,
    unsigned int _colorBlockID1,
    unsigned int _colorBlockID2
)
{
    if (_subgroupID < m_subgroups.size())
    {
        Subgroup& subgroup = m_subgroups[_subgroupID];
        if (_subtitleID < subgroup.m_subtitles.size())
        {
            Subtitle& subtitle = subgroup.m_subtitles[_subtitleID];
            if (_colorBlockID1 < subtitle.m_colorBlocks.size() && _colorBlockID2 < subtitle.m_colorBlocks.size())
            {
                ColorBlock temp = subtitle.m_colorBlocks[_colorBlockID1];
                subtitle.m_colorBlocks[_colorBlockID1] = subtitle.m_colorBlocks[_colorBlockID2];
                subtitle.m_colorBlocks[_colorBlockID2] = temp;
            }
        }
    }
}

//-----------------------------------------------------
// Get Default Color
//-----------------------------------------------------
fco::Color fco::GetDefaultColor
(
    unsigned int _subgroupID,
    unsigned int _subtitleID
)
{
    if (_subgroupID < m_subgroups.size())
    {
        Subgroup& subgroup = m_subgroups[_subgroupID];
        if (_subtitleID < subgroup.m_subtitles.size())
        {
            Subtitle& subtitle = subgroup.m_subtitles[_subtitleID];
            return subtitle.m_defaultColor;
        }
    }

    return Color();
}

//-----------------------------------------------------
// Set Default Color
//-----------------------------------------------------
void fco::ModifyDefaultColor
(
    unsigned int _subgroupID,
    unsigned int _subtitleID,
    Color const& _color
)
{
    if (_subgroupID < m_subgroups.size())
    {
        Subgroup& subgroup = m_subgroups[_subgroupID];
        if (_subtitleID < subgroup.m_subtitles.size())
        {
            Subtitle& subtitle = subgroup.m_subtitles[_subtitleID];
            subtitle.m_defaultColor = _color;
        }
    }
}

//-----------------------------------------------------
// Debug
//-----------------------------------------------------
void fco::DebugPrint()
{
    cout << "Group Name: " << m_groupName << endl;
    for (Subgroup const& subgroup : m_subgroups)
    {
        cout << "\tSub-group Name: " << subgroup.m_name << endl;
        for (Subtitle const& subtitle : subgroup.m_subtitles)
        {
            cout << "\t\tSubtitle Label: " << subtitle.m_label << endl;

            unsigned int indexPrev = 0;
            unsigned int index = subtitle.m_subtitle.find(L"\n", indexPrev);
            if (index == string::npos)
            {
                wcout << L"\t\t\t" << subtitle.m_subtitle << endl;
            }
            else
            {
                // TODO: Fail if any characters can't be displayed in terminal
                while (index != string::npos)
                {
                    wcout << L"\t\t\t" << subtitle.m_subtitle.substr(indexPrev, index - indexPrev) << endl;
                    indexPrev = index + 1;
                    index = subtitle.m_subtitle.find(L"\n", indexPrev);
                }
                wcout << L"\t\t\t" << subtitle.m_subtitle.substr(indexPrev, subtitle.m_subtitle.size() - indexPrev) << endl;
            }

            for (ColorBlock const& colorBlock : subtitle.m_colorBlocks)
            {
                cout << "\t\tColor Block: " << colorBlock.m_start << " to " << colorBlock.m_end;
                cout << ", RGBA = [" << static_cast<unsigned int>(colorBlock.m_color.r);
                cout << "," << static_cast<unsigned int>(colorBlock.m_color.g);
                cout << "," << static_cast<unsigned int>(colorBlock.m_color.b);
                cout << "," << static_cast<unsigned int>(colorBlock.m_color.a) << "]" << endl;
            }
        }
    }
}

//-----------------------------------------------------
// Dump UTF16 characters from All.fte (Sonic Generations)
//-----------------------------------------------------
void fco::DumpFteFile()
{
    FILE* fteFile;
    fopen_s(&fteFile, "All.fte", "rb");
    if (!fteFile)
    {
        return;
    }

    FILE* databaseRaw;
    fopen_s(&databaseRaw, "fcoDatabaseRAW.txt", "w+,ccs=UTF-8");

    fseek(fteFile, 0, SEEK_END);
    long size = ftell(fteFile);

    // Skip to first character (small space)
    fseek(fteFile, 0x350, SEEK_SET);

    while (ftell(fteFile) < size)
    {
        wchar_t symbol;
        fread(&symbol, 2, 1, fteFile);
        symbol = _byteswap_ushort(symbol);

        fwprintf_s(databaseRaw, L"%c", symbol);
        fseek(fteFile, 0x16, SEEK_CUR);
    }

    fclose(databaseRaw);
    fclose(fteFile);
}

//-----------------------------------------------------
// Generate fcoDatabase.txt from fcoDatabaseRAW.txt
//-----------------------------------------------------
void fco::GenerateDatabase()
{
    ifstream databaseRaw("fcoDatabaseRAW.txt");

    if (databaseRaw.is_open())
    {
        stringstream ss;
        ss << databaseRaw.rdbuf();
        databaseRaw.close();

        wstring_convert< codecvt_utf8<wchar_t> > utfconv;
        wstring utf8Content = utfconv.from_bytes(ss.str());

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
        while (index < utf8Content.size())
        {
            // Mask out four bytes from offset
            unsigned int intBuffer[4];
            intBuffer[0] = (hex >> 24) & 0x000000FF;
            intBuffer[1] = (hex >> 16) & 0x000000FF;
            intBuffer[2] = (hex >> 8) & 0x000000FF;
            intBuffer[3] = hex & 0x000000FF;

            wchar_t symbol = utf8Content[index];
            fwprintf_s(database, L"%02X %02X %02X %02X = %c\n", intBuffer[0], intBuffer[1], intBuffer[2], intBuffer[3], symbol);

            index++;
            hex++;
        }
        fclose(database);
    }
}
