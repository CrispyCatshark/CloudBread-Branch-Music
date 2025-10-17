#ifndef LYRICSWEIGHT_H
#define LYRICSWEIGHT_H

#include <QScrollArea>
#include <QVBoxLayout>
#include <QPropertyAnimation>
#include <QVector>
#include "LrcParser.h"
#include "lyriclabel.h"

class LyricsWeight : public QScrollArea
{
    Q_OBJECT
public:
    explicit LyricsWeight(QWidget *parent = nullptr);
    ~LyricsWeight() override;

    void setLyrics(const QVector<LineLyric>& lyricData,
                   const QVector<LineLyric>& transData = QVector<LineLyric>(),
                   const QVector<LineLyric>& romaData = QVector<LineLyric>());

public slots:
    void updateLyrics(qint64 positionMs, qint64 durationMs);

private:
    void deleteItems();

private:
    QPropertyAnimation* m_scrollAni;
    int m_lyricsNowPoint;
    QVector<qint64> m_lyricsTimeList;

    QWidget* m_scrollWidget;
    QVBoxLayout* m_expandLayout;

    QVector<LineLyric> m_lyricData;
    QVector<LineLyric> m_transData;
    QVector<LineLyric> m_romaData;

    QVector<LyricLabel*> m_lyricsLabelList;
    qint64 m_lastPositionMs = -1;
};

#endif // LYRICSWEIGHT_H
