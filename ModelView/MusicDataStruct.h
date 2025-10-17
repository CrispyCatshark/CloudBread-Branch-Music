// MusicDataStruct.h
#ifndef MUSICDATASTRUCT_H
#define MUSICDATASTRUCT_H

#include <QString>

// 歌单数据结构（对应 group_data 表）
struct GroupItem {
    QString id;       // 歌单ID（char32）
    QString name;     // 歌单名称（char32）
    QString img;      // 歌单封面路径（char128）
};

// 歌曲数据结构（对应 music_data 表）
struct MusicItem {
    QString id;         // 歌曲ID（char32）
    int sortId;         // 排序ID（int）
    QString imgName;    // 歌曲封面路径（char128）
    QString name;       // 歌曲名称（char128）
    QString auther;     // 歌手（char128）
    int duration;       // 时长（秒，int）
    int timestamp;      // 添加时间（时间戳，int）
    int pitch;          // 升降调（int）
    int count;          // 播放次数（int）
    QString filePath;   // 原始文件路径
    QString targetPath; // 复制后的目标路径
};

#endif // MUSICDATASTRUCT_H
