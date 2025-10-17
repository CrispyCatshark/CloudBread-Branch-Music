#include "LrcParser.h"
#include "firstrunsetup.h"
#include "mainwindow.h"

#include <QApplication>
#include <QSettings>
#include <QFile>
#include <QStandardPaths>
#include <qprocess.h>

#include "ElaApplication.h"

#include "SqliteManager.h"

#include "GlobalConfig.h"

bool initDatabaseTables(const QString& dbPath);
void initApplicationSettings();

// 工具函数：将毫秒时间格式化为 "mm:ss.zzz" 或 "mm:ss"（适配普通/逐字歌词）
QString formatTime(qint64 ms) {
    if (ms < 0) return "Invalid Time";

    int minutes = ms / 60000;          // 分钟
    int seconds = (ms % 60000) / 1000; // 秒
    int milliseconds = ms % 1000;      // 毫秒

    // 格式：分钟补2位，秒补2位，毫秒补3位（如 00:03.750）
    return QString("%1:%2.%3")
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(seconds, 2, 10, QLatin1Char('0'))
        .arg(milliseconds, 3, 10, QLatin1Char('0'));
}

// 示例1：解析本地LRC文件（支持普通歌词/逐字歌词格式，自动识别编码）
void parseLrcFile(const QString &filePath) {
    qInfo() << "\n===== 示例1：解析本地LRC文件 =====";
    LrcParser parser;
    bool success = parser.parseFromFile(filePath);

    if (!success) {
        qCritical() << "文件解析失败：" << filePath;
        return;
    }

    // 1. 输出歌曲元信息
    qInfo() << "【歌曲信息】";
    qInfo() << "标题：" << (parser.getTitle().isEmpty() ? "未设置" : parser.getTitle());
    qInfo() << "歌手：" << (parser.getArtist().isEmpty() ? "未设置" : parser.getArtist());
    qInfo() << "专辑：" << (parser.getAlbum().isEmpty() ? "未设置" : parser.getAlbum());
    qInfo() << "歌词作者：" << (parser.getLyricist().isEmpty() ? "未设置" : parser.getLyricist());
    qInfo() << "时间偏移：" << parser.getOffset() << "毫秒";

    // 2. 输出普通歌词（若存在）
    QVector<LineLyric> charLyrics = parser.getCharLyrics();
    qInfo() << "\n【逐字歌词详情（共" << charLyrics.size() << "行）】";
    for (int lineIdx = 0; lineIdx < charLyrics.size(); ++lineIdx) {
        const LineLyric &line = charLyrics[lineIdx];
        qInfo() << QString("第%1行：").arg(lineIdx + 1);
        qInfo() << "  行时间范围：" << formatTime(line.lineStartTime) << "~" << formatTime(line.lineEndTime);
        qInfo() << "  行内容：" << line.lineContent;
        qInfo() << "  逐字分解：";
        for (const CharLyric &cl : line.charLyrics) {
            auto placeholder =
                qInfo() << QString("    '%1' → 开始：%2 | 结束：%3 | 时长：%4ms")
                               .arg(cl.character)
                               .arg(formatTime(cl.startTime))
                               .arg(formatTime(cl.endTime))
                               .arg(cl.duration);
        }
    }

    qInfo() << "=================================\n";
}

// 示例2：解析普通字符串歌词（格式：[mm:ss.xx]歌词内容）
void parseNormalLrcString() {
    qInfo() << "\n===== 示例2：解析普通字符串歌词 =====";
    // 普通歌词字符串（模拟你的歌词格式）
    QString charLrcStr = "[ti:小孩]\n[ar:苏星婕]\n[al:小孩]\n[by:]\n[offset:0]\n[00:00.00]小孩 - 苏星婕\n[00:00.03]词：罗森涛\n[00:00.05]曲：罗森涛\n[00:00.08]编曲：贺翔基\n[00:00.11]吉他：贺翔基\n[00:00.14]录音：TuJay\n[00:00.16]录音棚：莫非录音棚（成都）\n[00:00.23]混音/母带：TuJay\n[00:00.27]制作人：莫非\n[00:00.30]制作团队：慕乐事务所\n[00:00.35]OP：莫非音乐\n[00:00.38]（未经许可,不得翻唱或使用）\n[00:00.45]听了几遍苦情歌\n[00:03.18]歌词比我们快乐\n[00:06.11]不敢放的手干脆就这样耗着\n[00:11.40]分分合合 重蹈覆辙\n[00:17.16]热情褪去只剩下不舍\n[00:23.59]那些惹人哭的承诺\n[00:27.46]让两个人沉默\n[00:30.21]都知道没结果\n[00:32.75]放任回忆 变成折磨 噢噢\n[00:36.50]不适的关系一拖再拖\n[00:39.41]你也默契的配合我\n[00:41.40]该怎么做 该怎么说\n[00:44.38]该怎么去放过\n[00:47.27]爱让人像小孩\n[00:50.19]怕一个人孤单\n[00:53.08]明知道有的人走不到最后\n[00:57.97]却还是太依赖\n[01:01.79]不愿说bye\n[01:04.74]一觉醒来又对我们充满好多期待\n[01:10.41]爱让我像小孩\n[01:13.33]怕没人说晚安\n[01:16.24]习惯有人陪伴就不愿再分开\n[01:21.96]不想时间停摆 不想要有遗憾\n[01:27.91]却忽略了我们不会再有任何 未来\n[01:44.93]那些惹人哭的承诺\n[01:48.70]让两个人沉默\n[01:51.58]都知道没结果\n[01:53.95]放任回忆 变成折磨 噢噢\n[01:57.73]不适的关系一拖再拖\n[02:00.65]你也默契的配合我\n[02:02.75]该怎么做 该怎么说\n[02:05.60]该怎么去放过\n[02:08.54]爱让人像小孩\n[02:11.43]怕一个人孤单\n[02:14.30]明知道有的人走不到最后\n[02:19.07]却还是太依赖\n[02:22.87]不愿说bye\n[02:25.96]一觉醒来又对我们充满好多期待\n[02:31.65]爱让我像小孩\n[02:34.61]怕没人说晚安\n[02:37.55]习惯有人陪伴就不愿再分开\n[02:43.25]不想时间停摆 不想要有遗憾\n[02:49.20]却忽略了我们不会再有任何 未来\n[03:00.72]爱让我像小孩\n[03:03.65]怕没人说晚安\n[03:06.55]习惯有人陪伴就不愿再分开\n[03:12.46]不想时间停摆 不想要有遗憾\n[03:18.24]却忽略了我们不会再有任何 未来";

    LrcParser parser;
    bool success = parser.parseFromString(charLrcStr);

    if (!success) {
        qCritical() << "普通字符串歌词解析失败";
        return;
    }

    // 输出歌曲信息
    qInfo() << "【歌曲信息】";
    qInfo() << "标题：" << (parser.getTitle().isEmpty() ? "未设置" : parser.getTitle());
    qInfo() << "歌手：" << (parser.getArtist().isEmpty() ? "未设置" : parser.getArtist());
    qInfo() << "专辑：" << (parser.getAlbum().isEmpty() ? "未设置" : parser.getAlbum());
    qInfo() << "歌词作者：" << (parser.getLyricist().isEmpty() ? "未设置" : parser.getLyricist());
    qInfo() << "时间偏移：" << parser.getOffset() << "毫秒";

    // 输出普通歌词
    QVector<LineLyric> charLyrics = parser.getCharLyrics();
    qInfo() << "\n【逐字歌词详情（共" << charLyrics.size() << "行）】";
    for (int lineIdx = 0; lineIdx < charLyrics.size(); ++lineIdx) {
        const LineLyric &line = charLyrics[lineIdx];
        qInfo() << QString("第%1行：").arg(lineIdx + 1);
        qInfo() << "  行时间范围：" << formatTime(line.lineStartTime) << "~" << formatTime(line.lineEndTime);
        qInfo() << "  行内容：" << line.lineContent;
        qInfo() << "  逐字分解：";
        for (const CharLyric &cl : line.charLyrics) {
            auto placeholder =
                qInfo() << QString("    '%1' → 开始：%2 | 结束：%3 | 时长：%4ms")
                               .arg(cl.character)
                               .arg(formatTime(cl.startTime))
                               .arg(formatTime(cl.endTime))
                               .arg(cl.duration);
        }
    }

    // 测试：根据时间获取歌词（模拟播放到 00:18.00 时的歌词）
    qint64 testTime = 18000; // 18秒 = 18000毫秒
    QString testLyric = parser.getNormalLyricAtTime(testTime);
    qInfo() << "\n【测试】播放到" << formatTime(testTime) << "时的歌词：" << testLyric;

    qInfo() << "=================================\n";
}

// 示例3：解析逐字字符串歌词（格式：[行开始,行时长]字1(字开始,字时长)字2(字开始,字时长)...）
void parseCharLrcString() {
    qInfo() << "\n===== 示例3：解析逐字字符串歌词 =====";
    // 逐字歌词字符串（模拟你的逐字格式，截取部分内容）
    QString charLrcStr = "[ti:小孩]\n[ar:苏星婕]\n[al:小孩]\n[by:]\n[offset:0]\n[00:00.00]小孩 - 苏星婕\n[00:00.03]词：罗森涛\n[00:00.05]曲：罗森涛\n[00:00.08]编曲：贺翔基\n[00:00.11]吉他：贺翔基\n[00:00.14]录音：TuJay\n[00:00.16]录音棚：莫非录音棚（成都）\n[00:00.23]混音/母带：TuJay\n[00:00.27]制作人：莫非\n[00:00.30]制作团队：慕乐事务所\n[00:00.35]OP：莫非音乐\n[00:00.38]（未经许可,不得翻唱或使用）\n[00:00.45]听了几遍苦情歌\n[00:03.18]歌词比我们快乐\n[00:06.11]不敢放的手干脆就这样耗着\n[00:11.40]分分合合 重蹈覆辙\n[00:17.16]热情褪去只剩下不舍\n[00:23.59]那些惹人哭的承诺\n[00:27.46]让两个人沉默\n[00:30.21]都知道没结果\n[00:32.75]放任回忆 变成折磨 噢噢\n[00:36.50]不适的关系一拖再拖\n[00:39.41]你也默契的配合我\n[00:41.40]该怎么做 该怎么说\n[00:44.38]该怎么去放过\n[00:47.27]爱让人像小孩\n[00:50.19]怕一个人孤单\n[00:53.08]明知道有的人走不到最后\n[00:57.97]却还是太依赖\n[01:01.79]不愿说bye\n[01:04.74]一觉醒来又对我们充满好多期待\n[01:10.41]爱让我像小孩\n[01:13.33]怕没人说晚安\n[01:16.24]习惯有人陪伴就不愿再分开\n[01:21.96]不想时间停摆 不想要有遗憾\n[01:27.91]却忽略了我们不会再有任何 未来\n[01:44.93]那些惹人哭的承诺\n[01:48.70]让两个人沉默\n[01:51.58]都知道没结果\n[01:53.95]放任回忆 变成折磨 噢噢\n[01:57.73]不适的关系一拖再拖\n[02:00.65]你也默契的配合我\n[02:02.75]该怎么做 该怎么说\n[02:05.60]该怎么去放过\n[02:08.54]爱让人像小孩\n[02:11.43]怕一个人孤单\n[02:14.30]明知道有的人走不到最后\n[02:19.07]却还是太依赖\n[02:22.87]不愿说bye\n[02:25.96]一觉醒来又对我们充满好多期待\n[02:31.65]爱让我像小孩\n[02:34.61]怕没人说晚安\n[02:37.55]习惯有人陪伴就不愿再分开\n[02:43.25]不想时间停摆 不想要有遗憾\n[02:49.20]却忽略了我们不会再有任何 未来\n[03:00.72]爱让我像小孩\n[03:03.65]怕没人说晚安\n[03:06.55]习惯有人陪伴就不愿再分开\n[03:12.46]不想时间停摆 不想要有遗憾\n[03:18.24]却忽略了我们不会再有任何 未来";
    LrcParser parser;
    bool success = parser.parseFromString(charLrcStr);

    if (!success) {
        qCritical() << "逐字字符串歌词解析失败";
        return;
    }

    // 输出歌曲信息
    qInfo() << "【歌曲信息】";
    qInfo() << "标题：" << (parser.getTitle().isEmpty() ? "未设置" : parser.getTitle());
    qInfo() << "歌手：" << (parser.getArtist().isEmpty() ? "未设置" : parser.getArtist());
    qInfo() << "专辑：" << (parser.getAlbum().isEmpty() ? "未设置" : parser.getAlbum());
    qInfo() << "歌词作者：" << (parser.getLyricist().isEmpty() ? "未设置" : parser.getLyricist());
    qInfo() << "时间偏移：" << parser.getOffset() << "毫秒";

    // 输出逐字歌词详情
    QVector<LineLyric> charLyrics = parser.getCharLyrics();
    qInfo() << "\n【逐字歌词详情（共" << charLyrics.size() << "行）】";
    for (int lineIdx = 0; lineIdx < charLyrics.size(); ++lineIdx) {
        const LineLyric &line = charLyrics[lineIdx];
        qInfo() << QString("第%1行：").arg(lineIdx + 1);
        qInfo() << "  行时间范围：" << formatTime(line.lineStartTime) << "~" << formatTime(line.lineEndTime);
        qInfo() << "  行内容：" << line.lineContent;
        qInfo() << "  逐字分解：";
        for (const CharLyric &cl : line.charLyrics) {
            auto placeholder =
                qInfo() << QString("    '%1' → 开始：%2 | 结束：%3 | 时长：%4ms")
                               .arg(cl.character)
                               .arg(formatTime(cl.startTime))
                               .arg(formatTime(cl.endTime))
                               .arg(cl.duration);
        }
    }

    // 测试：根据时间获取逐字歌词（模拟播放到 00:17.00 时的字符）
    qint64 testTime = 17000; // 17秒 = 17000毫秒
    QString testChar = parser.getCharLyricAtTime(testTime);
    QPair<int, int> testIndex = parser.getCharLyricIndexAtTime(testTime);
    qInfo() << "\n【测试】播放到" << formatTime(testTime) << "时：";
    qInfo() << "  当前字符：" << (testChar.isEmpty() ? "无匹配" : testChar);
    qInfo() << "  所在位置：第" << (testIndex.first + 1) << "行，第" << (testIndex.second + 1) << "字";

    qInfo() << "=================================\n";
}

// 辅助函数：检查端口是否可用
bool isPortAvailable(quint16 port) {
    QTcpSocket socket;
    // 尝试连接到本地的该端口
    socket.connectToHost(QHostAddress::LocalHost, port);
    // 如果连接成功，说明端口已被占用
    bool isAvailable = (socket.waitForConnected(100) ? false : true);
    socket.disconnectFromHost();
    return isAvailable;
}

int main(int argc, char *argv[])
{
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
    QGuiApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
    QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
#else
    //根据实际屏幕缩放比例更改
    qputenv("QT_SCALE_FACTOR", "1.5");
#endif
#endif

    QApplication a(argc, argv);

    // if (!SqliteManager::getInstance().init("database.db")) {
    //     // 处理初始化失败
    //     qDebug() << "数据库初始化失败:" << SqliteManager::getInstance().getLastError();
    // }

    // qDebug() << QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/database.db";

    // 指定要创建的文件夹路径
    QString folderPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

    // 创建QDir对象
    QDir dir;

    // 检查文件夹是否存在，如果不存在则创建
    if (!dir.exists(folderPath)) {
        if (dir.mkpath(folderPath)) {
            qDebug() << "文件夹创建成功：" << folderPath;
        } else {
            qDebug() << "文件夹创建失败！";
        }
    } else {
        qDebug() << "文件夹已存在：" << folderPath;
    }
    // initDatabaseTables(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/database.db");

    // qDebug() << QQMusicApi().searchMusic("水星记").mid;

    eApp->init();
    initApplicationSettings();

    quint16 webPort = GlobalConfig::getInstance().getValue("Server/WebPort", 5320).toInt();
    while (!isPortAvailable(webPort)) {
        qCritical() << "WebSocket服务器端口" << webPort << "已被占用，无法启动服务器";
        webPort ++;
    }
    GlobalConfig::getInstance().setValue("Server/WebPort", webPort);

    quint16 wsPort = GlobalConfig::getInstance().getValue("Server/WsPort", 5310).toInt();
    // 检查端口是否可用
    while (!isPortAvailable(wsPort)) {
        qCritical() << "WebSocket服务器端口" << wsPort << "已被占用，无法启动服务器";
        wsPort ++;
    }
    GlobalConfig::getInstance().setValue("Server/WsPort", wsPort);

    bool firstRun = GlobalConfig::getInstance().getValue("App/DBpath","").toString().isEmpty();

    if (firstRun) {
        // 首次运行，显示设置页面
        FirstRunSetup w;
        w.show();
        return a.exec();
    } else {
        // 非首次运行，直接显示主窗口
        SqliteManager& dbManager = SqliteManager::getInstance();
        if (!dbManager.init(GlobalConfig::getInstance().getValue("App/DBpath","").toString() + "/database.db")) {
            qWarning() << "数据库初始化失败:" << dbManager.getLastError();
            a.quit();
        } else {
            MainWindow w;
            w.show();
            return a.exec();
        }
    }

    return a.exec();
}

// 初始化应用程序设置
void initApplicationSettings()
{
    // 初始化全局配置
    GlobalConfig::getInstance();

    // 应用主题设置
    int themeIndex = GlobalConfig::getInstance().getValue("Appearance/Theme", 0).toInt();
    if (themeIndex == 0)
    {
        eTheme->setThemeMode(ElaThemeType::Light);
    }
    else
    {
        eTheme->setThemeMode(ElaThemeType::Dark);
    }

    // 应用云母效果设置
    bool enableMica = GlobalConfig::getInstance().getValue("Appearance/EnableMica", false).toBool();
    eApp->setIsEnableMica(enableMica);
}
