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

    public:
        void Read(FILE* _file);
        void Write(FILE* _file, uint16_t unknown) const;
    };

    struct Texture
    {
        string m_name;
        uint32_t m_width;
        uint32_t m_height;
    };

    fte();
    void Reset();
    bool IsLoaded() { return !m_data.empty(); }

    // Import & Export
    bool Import(string const& _fileName, string& _errorMsg);
    bool Export(string const& _path, string& _errorMsg);

    static void GenerateFcoDatabase(wstring const& content);

private:
    // Reading from bytes
    static unsigned int ReadInt(FILE* _file);
    static float ReadFloat(FILE* _file);
    static string ReadAscii(FILE* _file);

    // Writing bytes
    static void WriteInt(FILE* _file, unsigned int _writeInt);
    static void WriteFloat(FILE* _file, float _writeFloat);
    static void WriteAscii(FILE* _file, string _writeString);

public:
    uint32_t m_buttonTextureIndex;
    vector<Texture> m_textures;

    vector<Data> m_buttonData;
    vector<Data> m_data;
};

#endif // FTE_H
