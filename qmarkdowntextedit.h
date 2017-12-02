#ifndef QMARKDOWNTEXTEDIT_H
#define QMARKDOWNTEXTEDIT_H

#include <QTextEdit>
#include <QEvent>
#include <lib/peg-markdown-highlight/highlighter.h>

class QMarkdownTextEdit : public QTextEdit
{
    Q_OBJECT
public:
    explicit QMarkdownTextEdit(QWidget *parent = nullptr);

public:
    HGMarkdownHighlighter *markdownHighlighter();
    void setIgnoredClickUrlSchemata(QStringList ignoredUrlSchemata);
    static void openUrl(QUrl url);
    QUrl getMarkdownUrlAtPosition(QString text, int position);

protected:
    HGMarkdownHighlighter *m_markdownHighlighter;
    QStringList m_ignoredClickUrlSchemata;
    bool eventFilter(QObject *obj, QEvent *event);
    bool increaseSelectedTextIndention(bool reverse);
    void openLinkAtCursorPosition();
    QMap<QString,QString> parseMarkdownUrlsFromText(QString text);

signals:
    void urlClicked(QUrl url);
};

#endif // QMARKDOWNTEXTEDIT_H
