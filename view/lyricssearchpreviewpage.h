#ifndef LYRICSSEARCHPREVIEWPAGE_H
#define LYRICSSEARCHPREVIEWPAGE_H

#include "ElaText.h"
#include "ElaWidget.h"

class lyricsSearchPreviewPage : public ElaWidget
{
    Q_OBJECT
public:
    lyricsSearchPreviewPage(QWidget *parent = nullptr);
    void setLayrics(QString lyricText);

private:
    void initPage();
    ElaText* m_layricText;
};

#endif // LYRICSSEARCHPREVIEWPAGE_H
