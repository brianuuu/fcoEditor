//-----------------------------------------------------
// Name: fco.h
// Author: brianuuu
// Date: 3/5/2019
//-----------------------------------------------------

#pragma once
#include <string>
#include <map>
#include <vector>

using namespace std;

class fco
{
public:
    struct Color
    {
        Color() : a(static_cast<unsigned char>(0xFF)) { SetHardcoded(); }
        Color (unsigned char _r, unsigned char _g, unsigned char _b, unsigned char _a)
            : r(_r), g(_g), b(_b), a(_a) {}

        bool IsHardcoded() { return r == 0x00 && g == 0x00 && b == 0x00; }
        void SetHardcoded() { r = 0x00; g = 0x00; b = 0x00; }

        void SetWhite() { r = 0xFF; g = 0xFF; b = 0xFF; }
        void SetBlack() { r = 0x01; g = 0x00; b = 0x00; }

        unsigned char r;
        unsigned char g;
        unsigned char b;
        unsigned char a;
    };

    struct ColorBlock
    {
        ColorBlock() : m_start(0), m_end(1) {}
        ColorBlock(unsigned int _start, unsigned int _end, Color _color)
            : m_start(_start), m_end(_end), m_color(_color) {}

        unsigned int m_start;
        unsigned int m_end;
        Color m_color;
    };

public:
    fco();
    ~fco();

    bool IsInit() { return m_init; }
    bool IsLoaded() { return m_loaded; }

    // Import & Export
    bool Load(string const& _fileName, string& _errorMsg);
    bool Save(string const& _fileName, string& _errorMsg);

    // Helpers
    bool Search(wstring const& _wstring, unsigned int& _subgroupID, unsigned int& _subtitleID);
    bool ValidateString(wstring const& _wstring, wstring& _errorMsg, vector<wstring>& _characterArray);
    void GetGroupNames(vector<string>& _groupNames);
    void GetSubgroupSubtitles(unsigned int _subgroupID, vector<string>& _labels, vector<wstring>& _subtitles);
    string GetLabel(unsigned int _subgroupID, unsigned int _subtitleID);
    wstring GetSubtitle(unsigned int _subgroupID, unsigned int _subtitleID);
    void GetSubtitleColorBlocks(unsigned int _subgroupID, unsigned int _subtitleID, vector<ColorBlock>& _colorBlocks);

    // Modifiers for subtitles
    void AddGroup();
    void DeleteGroup(unsigned int _subgroupID);
    void SwapGroup(unsigned int _subgroupID1, unsigned int _subgroupID2);
    void ModifyGroupName(unsigned int _subgroupID, string const& _groupName);
    void AddSubtitle(unsigned int _subgroupID, string const& _label = "NO_NAME", wstring const& _subtitle = L"DUMMY SUBTITLE");
    void DeleteSubtitle(unsigned int _subgroupID, unsigned int _subtitleID);
    void SwapSubtitle(unsigned int _subgroupID, unsigned int _subtitleID1, unsigned int _subtitleID2);
    void ModifySubtitle(wstring const& _wstring, unsigned int _subgroupID, unsigned int _subtitleID);
    void ModifySubtitleName(unsigned int _subgroupID, unsigned int _subtitleID, string const& _subtitleName);

    // Modifiers for color blocks
    void ModifyColorBlock(unsigned int _subgroupID, unsigned int _subtitleID, unsigned int _colorBlockID, ColorBlock const& _colorBlock);
    void AddColorBlock(unsigned int _subgroupID, unsigned int _subtitleID);
    void RemoveColorBlock(unsigned int _subgroupID, unsigned int _subtitleID, unsigned int _colorBlockID);
    void RemoveAllColorBlocks(unsigned int _subgroupID, unsigned int _subtitleID);
    void SwapColorBlocks(unsigned int _subgroupID, unsigned int _subtitleID, unsigned int _colorBlockID1, unsigned int _colorBlockID2);
    Color GetDefaultColor(unsigned int _subgroupID, unsigned int _subtitleID);
    void ModifyDefaultColor(unsigned int _subgroupID, unsigned int _subtitleID, Color const& _color);

    // Debugging
    void DebugPrint();

private:
    bool Init();
    void DumpFteFile();
    void GenerateDatabase();

    // Reading from bytes
    unsigned int ReadInt(FILE* _file);
    string ReadAscii(FILE* _file);

    // Writing bytes
    void WriteInt(FILE* _file, unsigned int _writeInt);
    void WriteAscii(FILE* _file, string _writeString);

private:
    map<wstring, unsigned int> m_database;
    map<unsigned int, wstring> m_databaseRev;

    bool m_init;
    bool m_loaded;

    struct Subtitle
    {
        string m_label;

        unsigned int m_subtitleSize;	// not count '\' from '\A\'
        wstring m_subtitle;

        Color m_defaultColor;
        vector<ColorBlock> m_colorBlocks;
    };

    struct Subgroup
    {
        string m_name;
        vector<Subtitle> m_subtitles;
    };

    vector<Subgroup> m_subgroups;
    string m_groupName;
};

