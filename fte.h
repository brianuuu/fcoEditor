#ifndef FTE_H
#define FTE_H

#include <string>
#include <map>
#include <vector>

using namespace std;

class fte
{
public:
    struct Data
    {
        uint32_t m_textureIndex;
        float m_left;
        float m_top;
        float m_right;
        float m_bottom;
        wchar_t m_wchar;
    };

    fte();

    // Import & Export
    bool Import(string const& _fileName, string& _errorMsg);
    bool Export(string const& _fileName, string& _errorMsg);

private:
    // Reading from bytes
    unsigned int ReadInt(FILE* _file);
    float ReadFloat(FILE* _file);
    string ReadAscii(FILE* _file);

    // Writing bytes
    void WriteInt(FILE* _file, unsigned int _writeInt);
    void WriteFloat(FILE* _file, float _writeFloat);
    void WriteAscii(FILE* _file, string _writeString);

public:
    string m_buttonTextureName;
    vector<string> m_textureNames;

    vector<Data> m_buttonData;
    vector<Data> m_data;
};

#endif // FTE_H
