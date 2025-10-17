#include "audiometadatamarser.h"
#include <QDir>
#include <QFile>
#include <QDateTime>
#include <QImage>
#include <QBuffer>
#include <QDebug>
#include <QRegularexpression>
#include "GlobalConfig.h"

// 修复点1：重构封面路径生成函数，接收标题参数
QString AudioMetadataParser::generateUniqueCoverPath(const QString &title) {
    // 处理标题：过滤文件系统不支持的特殊字符，为空则用默认值
    QString safeTitle = title;
    if (safeTitle.isEmpty()) {
        safeTitle = "unknown_title";
    }
    // 移除Windows/Linux/macOS均不支持的文件名字符
    safeTitle.remove(QRegularExpression("[\\\\/:*?\"<>|]"));
    // 限制标题长度，避免路径过长（保留前50个字符）
    if (safeTitle.length() > 50) {
        safeTitle = safeTitle.left(50) + "...";
    }

    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMddHHmmsszzz");
    QString coverDir = GlobalConfig::getInstance().getValue("App/DBpath","").toString() + "/AudioCovers/";
    QDir().mkpath(coverDir);
    // 组合：标题_时间戳.png
    return coverDir + safeTitle + "_" + timestamp + ".png";
}

bool AudioMetadataParser::saveCoverToPng(const TagLib::ByteVector &imageData, const QString &savePath) {
    // 尝试直接保存PNG数据
    if (imageData.startsWith("PNG")) {
        QFile file(savePath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(reinterpret_cast<const char*>(imageData.data()), imageData.size());
            file.close();
            return true;
        }
        return false;
    }

    // 处理其他格式（JPEG等）通过QImage转换为PNG
    QImage image;
    if (image.loadFromData(reinterpret_cast<const uchar*>(imageData.data()), imageData.size())) {
        return image.save(savePath, "PNG");
    }

    return false;
}

void AudioMetadataParser::parseRiffFile(TagLib::RIFF::File *riffFile, AudioMetadata &result) {
    // 专门处理WAV格式
    if (TagLib::RIFF::WAV::File *wavFile = dynamic_cast<TagLib::RIFF::WAV::File*>(riffFile)) {
        result.format = "WAV";
        parseWavFile(wavFile, result);
    }
    // 处理AIFF格式（强制转换为AIFF::File，避免基类功能缺失）
    else if (TagLib::RIFF::AIFF::File *aiffFile = dynamic_cast<TagLib::RIFF::AIFF::File*>(riffFile)) {
        result.format = "AIFF";
        parseAiffFile(aiffFile, result); // 入参改为AIFF专用类型
    }
    else {
        result.format = "RIFF";
        // 基本RIFF标签解析
        TagLib::Tag *tag = riffFile->tag();
        if (tag) {
            result.title = QString::fromStdString(tag->title().to8Bit(true));
            result.artist = QString::fromStdString(tag->artist().to8Bit(true));
        }
    }
}

void AudioMetadataParser::parseWavFile(TagLib::RIFF::WAV::File *wavFile, AudioMetadata &result) {
    // 1. 尝试解析INFO chunk中的元数据（WAV特有的标签存储方式）
    if (wavFile->hasInfoTag()) {
        const TagLib::RIFF::Info::Tag *info = wavFile->InfoTag();

        // 解析标题（优先使用INFO chunk中的Title）
        if (!info->title().isEmpty()) {
            result.title = QString::fromStdString(info->title().to8Bit(true));
        }

        // 解析艺术家（优先使用INFO chunk中的Artist）
        if (!info->artist().isEmpty()) {
            result.artist = QString::fromStdString(info->artist().to8Bit(true));
        }

        // 补充其他可能的INFO字段（如专辑、年份等）
        // if (!info.album().isEmpty()) result.album = ...;
    }

    // 2. 若INFO chunk中没有数据，尝试解析通用标签
    if (result.title.isEmpty() || result.artist.isEmpty()) {
        TagLib::Tag *tag = wavFile->tag();
        if (tag) {
            if (result.title.isEmpty()) {
                result.title = QString::fromStdString(tag->title().to8Bit(true));
            }
            if (result.artist.isEmpty()) {
                result.artist = QString::fromStdString(tag->artist().to8Bit(true));
            }
        }
    }

    // 3. 解析WAV文件中可能包含的ID3v2标签（通常用于存储封面）
    if (wavFile->hasID3v2Tag()) {
        TagLib::ID3v2::Tag *id3v2Tag = wavFile->ID3v2Tag();

        // 从ID3v2标签补充标题和艺术家（如果之前未获取到）
        if (result.title.isEmpty() && !id3v2Tag->title().isEmpty()) {
            result.title = QString::fromStdString(id3v2Tag->title().to8Bit(true));
        }
        if (result.artist.isEmpty() && !id3v2Tag->artist().isEmpty()) {
            result.artist = QString::fromStdString(id3v2Tag->artist().to8Bit(true));
        }

        // 解析封面（APIC帧）
        TagLib::ID3v2::FrameList frameList = id3v2Tag->frameList("APIC");
        if (!frameList.isEmpty()) {
            TagLib::ID3v2::AttachedPictureFrame *picFrame =
                dynamic_cast<TagLib::ID3v2::AttachedPictureFrame*>(frameList.front());

            if (picFrame) {
                // 修复点2：传入已解析的标题生成路径
                QString coverPath = generateUniqueCoverPath(result.title);
                if (saveCoverToPng(picFrame->picture(), coverPath)) {
                    result.coverPath = coverPath;
                }
            }
        }
    }
}


// 修复点1：入参改为TagLib::RIFF::AIFF::File*，启用AIFF专用接口
void AudioMetadataParser::parseAiffFile(TagLib::RIFF::AIFF::File *aiffFile, AudioMetadata &result) {
    // 1. 优先解析AIFF的"COMM" chunk（音频属性，TagLib已封装为audioProperties）
    // if (aiffFile->audioProperties()) {
    //    result.duration = aiffFile->audioProperties()->length(); // 时长（秒）
    // 可选：补充AIFF专用音频属性（如采样率、声道数）
    // result.sampleRate = aiffFile->audioProperties()->sampleRate();
    // result.channels = aiffFile->audioProperties()->channels();
    // }

    TagLib::Tag *generalTag = aiffFile->tag();
    if (generalTag) {
        if (result.title.isEmpty()) {
            result.title = QString::fromStdString(generalTag->title().to8Bit(true));
        }
        if (result.artist.isEmpty()) {
            result.artist = QString::fromStdString(generalTag->artist().to8Bit(true));
        }
    }

    // 3. 解析AIFF中的ID3v2标签（用于补充元数据和封面，AIFF原生支持嵌入ID3v2）
    if (aiffFile->hasID3v2Tag()) {
        TagLib::ID3v2::Tag *id3v2Tag = aiffFile->tag();
        if (id3v2Tag) {
            // 从ID3v2补充标题/艺术家（若之前未获取到）
            if (result.title.isEmpty() && !id3v2Tag->title().isEmpty()) {
                result.title = QString::fromStdString(id3v2Tag->title().to8Bit(true));
            }
            if (result.artist.isEmpty() && !id3v2Tag->artist().isEmpty()) {
                result.artist = QString::fromStdString(id3v2Tag->artist().to8Bit(true));
            }

            // 解析封面（APIC帧，与MP3/WAV的ID3v2封面逻辑一致）
            TagLib::ID3v2::FrameList apicFrames = id3v2Tag->frameList("APIC");
            if (!apicFrames.isEmpty()) {
                TagLib::ID3v2::AttachedPictureFrame *coverFrame =
                    dynamic_cast<TagLib::ID3v2::AttachedPictureFrame*>(apicFrames.front());
                if (coverFrame) {
                    // 修复点3：传入已解析的标题生成路径
                    QString coverPath = generateUniqueCoverPath(result.title);
                    if (saveCoverToPng(coverFrame->picture(), coverPath)) {
                        result.coverPath = coverPath;
                    }
                }
            }
        }
    }
}

void AudioMetadataParser::parseMpegFile(TagLib::MPEG::File *mpegFile, AudioMetadata &result) {
    result.format = "MP3";

    TagLib::Tag *tag = mpegFile->tag();
    if (tag) {
        result.title = QString::fromStdString(tag->title().to8Bit(true));
        result.artist = QString::fromStdString(tag->artist().to8Bit(true));
    }

    if (mpegFile->ID3v2Tag()) {
        TagLib::ID3v2::FrameList frameList = mpegFile->ID3v2Tag()->frameList("APIC");
        if (!frameList.isEmpty()) {
            TagLib::ID3v2::AttachedPictureFrame *picFrame =
                dynamic_cast<TagLib::ID3v2::AttachedPictureFrame*>(frameList.front());

            if (picFrame) {
                // 修复点4：传入已解析的标题生成路径
                QString coverPath = generateUniqueCoverPath(result.title);
                if (saveCoverToPng(picFrame->picture(), coverPath)) {
                    result.coverPath = coverPath;
                }
            }
        }
    }
}

void AudioMetadataParser::parseMp4File(TagLib::MP4::File *mp4File, AudioMetadata &result) {
    result.format = "MP4/AAC";

    // 1. 解析基础标签（歌名、艺术家）
    TagLib::MP4::Tag *mp4Tag = mp4File->tag();
    if (mp4Tag) {
        result.title = QString::fromStdString(mp4Tag->title().to8Bit(true));
        result.artist = QString::fromStdString(mp4Tag->artist().to8Bit(true));

        TagLib::MP4::ItemMap itemListMap = mp4File->tag()->itemMap();
        TagLib::MP4::Item albumArtItem = itemListMap["CoverArtList"];

        // 2. 修复核心：MP4封面解析（适配TagLib::MP4::CoverArt特性）
        if (mp4Tag->contains("covr")) { // 检查是否存在封面原子（"covr"为MP4封面标准原子名）
            TagLib::MP4::CoverArtList coverList = albumArtItem.toCoverArtList();
            if (!coverList.isEmpty()) {
                // 遍历封面列表，优先选择标准图像类型（避免非图像数据）
                TagLib::MP4::CoverArt targetCover = coverList.front();

                // 获取封面二进制数据（cover.data()返回TagLib::ByteVector，包含完整图像文件数据）
                TagLib::ByteVector coverData = targetCover.data();
                if (!coverData.isEmpty()) {
                    // 修复点5：传入已解析的标题生成路径
                    QString coverPath = generateUniqueCoverPath(result.title);
                    // 调用已有函数转换为PNG（自动适配JPEG/PNG等格式）
                    if (saveCoverToPng(coverData, coverPath)) {
                        result.coverPath = coverPath;
                    } else {
                        // 清理无效文件（避免临时目录残留）
                        QFile::remove(coverPath);
                    }
                }
            }
        }
    }
}

void AudioMetadataParser::parseFlacFile(TagLib::FLAC::File *flacFile, AudioMetadata &result) {
    result.format = "FLAC";

    TagLib::Tag *tag = flacFile->tag();
    if (tag) {
        result.title = QString::fromStdString(tag->title().to8Bit(true));
        result.artist = QString::fromStdString(tag->artist().to8Bit(true));
    }

    if (!flacFile->pictureList().isEmpty()) {
        TagLib::FLAC::Picture *pic = flacFile->pictureList().front();
        if (pic) {
            // 修复点6：传入已解析的标题生成路径
            QString coverPath = generateUniqueCoverPath(result.title);
            if (saveCoverToPng(pic->data(), coverPath)) {
                result.coverPath = coverPath;
            }
        }
    }
}

void AudioMetadataParser::parseOggFile(TagLib::Ogg::File *oggFile, AudioMetadata &result) {
    // 检查是否为Vorbis格式
    if (dynamic_cast<TagLib::Ogg::Vorbis::File*>(oggFile)) {
        result.format = "Ogg Vorbis";
    } else {
        result.format = "Ogg";
    }

    TagLib::Tag *tag = oggFile->tag();
    if (tag) {
        result.title = QString::fromStdString(tag->title().to8Bit(true));
        result.artist = QString::fromStdString(tag->artist().to8Bit(true));
    }

    // Ogg格式封面解析
    TagLib::Ogg::XiphComment *comment = dynamic_cast<TagLib::Ogg::XiphComment*>(oggFile->tag());
    if (comment) {
        TagLib::List<TagLib::FLAC::Picture *> pictures = comment->pictureList();
        if (!pictures.isEmpty()) {
            // 修复点7：传入已解析的标题生成路径
            QString coverPath = generateUniqueCoverPath(result.title);
            if (saveCoverToPng(pictures.front()->data(), coverPath)) {
                result.coverPath = coverPath;
            }
        }
    }
}

void AudioMetadataParser::parseAsfFile(TagLib::ASF::File *asfFile, AudioMetadata &result) {
    result.format = "WMA";

    TagLib::ASF::Tag *asfTag = asfFile->tag();
    if (asfTag) {
        result.title = QString::fromStdString(asfTag->title().to8Bit(true));
        result.artist = QString::fromStdString(asfTag->artist().to8Bit(true));

        TagLib::ASF::AttributeListMap attrMap = asfTag->attributeListMap();
        if (attrMap.contains("WM/Picture")) {
            TagLib::ASF::AttributeList attrList = attrMap["WM/Picture"];
            if (!attrList.isEmpty()) {
                // 修复点8：传入已解析的标题生成路径
                QString coverPath = generateUniqueCoverPath(result.title);
                if (saveCoverToPng(attrList.front().toByteVector(), coverPath)) {
                    result.coverPath = coverPath;
                }
            }
        }
    }
}

void AudioMetadataParser::parseWavPackFile(TagLib::WavPack::File *wavPackFile, AudioMetadata &result) {
    // 设置格式标识
    result.format = "WavPack";

    // 1. 解析基本标签信息（歌名、艺术家）
    TagLib::Tag *tag = wavPackFile->tag();
    if (tag) {
        // 解析标题（歌名）
        if (!tag->title().isEmpty()) {
            result.title = QString::fromStdString(tag->title().to8Bit(true));
        }

        // 解析艺术家（作者）
        if (!tag->artist().isEmpty()) {
            result.artist = QString::fromStdString(tag->artist().to8Bit(true));
        }
    }
}

AudioMetadata AudioMetadataParser::parse(const QString &filePath) {
    AudioMetadata result;
    TagLib::FileName fileName = filePath.toUtf8().constData();

    // 1. 校验文件存在性
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        qDebug() << "文件不存在或不是常规文件: " << filePath;
        return result; // 解析失败，返回默认结构体
    }

    // 2. 提取文件后缀名（转为小写，忽略大小写）
    QString fileExt = fileInfo.suffix().toLower();
    TagLib::File *file = nullptr;

    // 3. 根据后缀名构造对应格式的TagLib文件对象（参考网页打开方式）
    if (fileExt == "mp3") {
        file = new TagLib::MPEG::File(fileName); // MP3格式
        result.format = "MP3";
    } else if (fileExt == "m4a" || fileExt == "mp4" || fileExt == "aac") {
        file = new TagLib::MP4::File(fileName);  // MP4/AAC/M4A格式
        result.format = fileExt.toUpper();
    } else if (fileExt == "wav") {
        file = new TagLib::RIFF::WAV::File(fileName); // WAV格式
        result.format = "WAV";
    } else if (fileExt == "aiff" || fileExt == "aif") {
        file = new TagLib::RIFF::AIFF::File(fileName); // AIFF格式
        result.format = "AIFF";
    } else if (fileExt == "flac") {
        file = new TagLib::FLAC::File(fileName); // FLAC格式
        result.format = "FLAC";
    } else if (fileExt == "ogg" || fileExt == "oga") {
        file = new TagLib::Ogg::Vorbis::File(fileName); // Ogg Vorbis格式
        result.format = "Ogg Vorbis";
    } else if (fileExt == "wma") {
        file = new TagLib::ASF::File(fileName); // WMA格式（ASF容器）
        result.format = "WMA";
    } else if (fileExt == "wv") {
        file = new TagLib::WavPack::File(fileName); // WavPack格式
        result.format = "WavPack";
    } else {
        qDebug() << "不支持的文件格式: " << fileExt;
        return result; // 不支持的格式，直接返回
    }

    // 4. 校验文件是否有效（可打开 + 格式匹配）
    if (!file || !file->isValid() || !file->isOpen()) {
        qDebug() << "文件打开失败或格式无效: " << filePath;
        delete file;
        return result;
    }

    // 5. 解析音频属性（时长）
    if (file->audioProperties()) {
        result.duration = file->audioProperties()->length();
    } else {
        qDebug() << "无法获取音频属性: " << filePath;
        result.duration = -1; // 标记时长获取失败
    }

    // 6. 根据具体文件类型调用对应解析方法
    if (TagLib::MPEG::File *mpegFile = dynamic_cast<TagLib::MPEG::File*>(file)) {
        parseMpegFile(mpegFile, result);
    } else if (TagLib::MP4::File *mp4File = dynamic_cast<TagLib::MP4::File*>(file)) {
        parseMp4File(mp4File, result);
    } else if (TagLib::RIFF::WAV::File *wavFile = dynamic_cast<TagLib::RIFF::WAV::File*>(file)) {
        parseWavFile(wavFile, result);
    } else if (TagLib::RIFF::AIFF::File *aiffFile = dynamic_cast<TagLib::RIFF::AIFF::File*>(file)) {
        parseAiffFile(aiffFile, result);
    } else if (TagLib::FLAC::File *flacFile = dynamic_cast<TagLib::FLAC::File*>(file)) {
        parseFlacFile(flacFile, result);
    } else if (TagLib::Ogg::Vorbis::File *oggFile = dynamic_cast<TagLib::Ogg::Vorbis::File*>(file)) {
        qDebug() << "ogg";
        parseOggFile(oggFile, result);
    } else if (TagLib::ASF::File *asfFile = dynamic_cast<TagLib::ASF::File*>(file)) {
        parseAsfFile(asfFile, result);
    } else if (TagLib::WavPack::File *wavPackFile = dynamic_cast<TagLib::WavPack::File*>(file)) {
        // 若未实现parseWavPackFile，直接解析基础标签
        TagLib::Tag *tag = wavPackFile->tag();
        if (tag) {
            result.title = QString::fromStdString(tag->title().to8Bit(true));
            result.artist = QString::fromStdString(tag->artist().to8Bit(true));
        }
    }

    // 7. 标记解析状态并清理资源
    result.isParsed = true;
    delete file;
    return result;
}
