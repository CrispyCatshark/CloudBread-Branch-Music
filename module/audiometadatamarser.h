#ifndef AUDIO_METADATA_PARSER_H
#define AUDIO_METADATA_PARSER_H

// #define TAGLIB_EXPORTS

#include <QString>
#include <taglib/tag.h>
#include <taglib/audioproperties.h>
#include <taglib/mpegfile.h>
#include <taglib/mp4file.h>
#include <taglib/flacfile.h>
#include <taglib/oggfile.h>
#include <taglib/vorbisfile.h>
#include <taglib/asffile.h>
#include <taglib/id3v2tag.h>
#include <taglib/id3v2frame.h>
#include <taglib/mp4coverart.h>
#include <taglib/flacpicture.h>
#include <taglib/wavpackfile.h>
#include <taglib/wavfile.h>
#include <taglib/aifffile.h>
#include <taglib/attachedpictureframe.h>
#include <taglib/mp4tag.h>
#include <taglib/mp4coverart.h>
#include <taglib/apetag.h>
#include <taglib/apeitem.h>

// 音频元数据结构体
struct AudioMetadata {
    QString title;       // 歌名
    QString artist;      // 艺术家（作者）
    QString coverPath;   // 封面PNG文件路径（空字符串表示无封面）
    int duration;        // 时长（秒，-1表示获取失败）
    bool isParsed;       // 解析状态（true：成功，false：失败）
    QString format;      // 音频格式（新增字段）

    // 构造函数（初始化默认值）
    AudioMetadata() : duration(-1), isParsed(false) {}
};

class AudioMetadataParser {
public:
    // 解析音频文件元数据
    static AudioMetadata parse(const QString &filePath);

private:
    // 生成唯一封面文件路径
    static QString generateUniqueCoverPath(const QString &title);

    // 保存封面数据为PNG文件
    static bool saveCoverToPng(const TagLib::ByteVector &imageData, const QString &savePath);

    // 解析RIFF格式文件（WAV/AIFF等）
    static void parseRiffFile(TagLib::RIFF::File *riffFile, AudioMetadata &result);

    static void parseAiffFile(TagLib::RIFF::AIFF::File *aiffFile, AudioMetadata &result);

    static void parseWavFile(TagLib::RIFF::WAV::File *wavFile, AudioMetadata &result);

    // 解析MP3格式文件
    static void parseMpegFile(TagLib::MPEG::File *mpegFile, AudioMetadata &result);

    // 解析MP4格式文件
    static void parseMp4File(TagLib::MP4::File *mp4File, AudioMetadata &result);

    // 解析FLAC格式文件
    static void parseFlacFile(TagLib::FLAC::File *flacFile, AudioMetadata &result);

    // 解析Ogg格式文件
    static void parseOggFile(TagLib::Ogg::File *oggFile, AudioMetadata &result);

    // 解析ASF格式文件（WMA）
    static void parseAsfFile(TagLib::ASF::File *asfFile, AudioMetadata &result);

    void parseWavPackFile(TagLib::WavPack::File *wavPackFile, AudioMetadata &result);
};

#endif // AUDIO_METADATA_PARSER_H
