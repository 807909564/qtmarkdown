#include "qmarkdowntextedit.h"
#include <QRegularExpression>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QSettings>
#include <QDebug>

QMarkdownTextEdit::QMarkdownTextEdit(QWidget *parent) : QTextEdit(parent) {
    installEventFilter(this);
    viewport()->installEventFilter(this);

    // 设置高亮
    m_markdownHighlighter = new HGMarkdownHighlighter(document(),1000);

    QSettings settings;
    QFont font = this->font();
    QString fontString = settings.value("MainWindow/noteTextEdit.font").toString();

    if (fontString != "") {
        font.fromString(fontString);
        setFont(font);
        m_markdownHighlighter->setDefaultStyles(font.pointSize());
    }
    const int tabStop = 4;
    QFontMetrics metrics(font);
    setTabStopWidth(tabStop * metrics.width(' '));
}

HGMarkdownHighlighter *QMarkdownTextEdit::markdownHighlighter() {
    return m_markdownHighlighter;
}

void QMarkdownTextEdit::setIgnoredClickUrlSchemata(QStringList ignoredUrlSchemata) {
    m_ignoredClickUrlSchemata = ignoredUrlSchemata;
}

void QMarkdownTextEdit::openUrl(QUrl url) {
    qDebug() << __func__ << " - 'url': " << url;

    QString urlString = url.toString();
    QSettings settings;
    QString notesPath = settings.value("notesPath").toString();

    // 解析相关文件的URL并使其成为绝对的
    urlString.replace(QRegularExpression("^file:\\/\\/([^\\/].+)$"),"file://" + notesPath + "/\\1");
    QDesktopServices::openUrl(QUrl(urlString));
}

QUrl QMarkdownTextEdit::getMarkdownUrlAtPosition(QString text, int position) {
    QUrl url;
    QMap<QString,QString> urlMap = parseMarkdownUrlsFromText(text);
    QMapIterator<QString,QString> i(urlMap);
    while (i.hasNext()) {
        i.next();
        QString linkText = i.key();
        QString urlString = i.value();

        int foundPositionStart = text.indexOf(linkText);

        if (foundPositionStart >= 0) {
            int foundPositionEnd = foundPositionStart + linkText.size();
            if ((position >= foundPositionStart) && (position <= foundPositionEnd)) {
                url = QUrl(urlString);
            }
        }
    }
    return url;
}

bool QMarkdownTextEdit::eventFilter(QObject *obj, QEvent *event) {
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        if ((keyEvent->key() == Qt::Key_Tab) || (keyEvent->key() == Qt::Key_Backtab)) {
            return increaseSelectedTextIndention(keyEvent->key() == Qt::Key_Backtab);
        } else if ((keyEvent->key() == Qt::Key_F) && QGuiApplication::keyboardModifiers() == Qt::ExtraButton24) {
            qDebug() << "Ctrl + F";
            return false;
        } else if (keyEvent->key() == Qt::Key_Control) {
            QWidget *viewPort = this->viewport();
            viewPort->setCursor(Qt::PointingHandCursor);
            return false;
        }
        return false;
    } else if (event->type() == QEvent::KeyRelease) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Control) {
            QWidget *viewPort = this->viewport();
            viewPort->setCursor(Qt::IBeamCursor);
        }
        return false;
    } else if (event->type() == QEvent::MouseButtonRelease) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
        if ((obj == this->viewport()) &&
                (mouseEvent->button() == Qt::LeftButton) &&
                (QGuiApplication::keyboardModifiers() == Qt::ExtraButton24)) {
            openLinkAtCursorPosition();
            return false;
        }
    }
    return QTextEdit::eventFilter(obj,event);
}

bool QMarkdownTextEdit::increaseSelectedTextIndention(bool reverse) {
    QTextCursor cursor = this->textCursor();
    QString selectedText = cursor.selectedText();
    if (selectedText != "") {
        QString newLine = QString::fromUtf8(QByteArray::fromHex("e2809a"));
        QString newText;
        if (reverse) {
            newText = selectedText.replace(newLine + "\t","\n");
            newText.replace(QRegularExpression("^\\t"),"");
        } else {
            // 缩进文字
            newText = selectedText.replace(newLine,"\n\t").prepend("\t");
            // 删除末尾的\t
            newText.replace(QRegularExpression("\\t$"),"");
        }
        // 插入新的文本
        cursor.insertText(newText);
        // 更新为新的文本
        cursor.setPosition(cursor.position() - newText.size(),QTextCursor::KeepAnchor);
        this->setTextCursor(cursor);
        return true;
    } else if (reverse) {
        int pos = cursor.position();
        // 在光标前检查\t
        cursor.setPosition(pos - 1,QTextCursor::KeepAnchor);
        if (cursor.selectedText() != "\t") {
            // 在光标后检查\t
            cursor.setPosition(pos);
            cursor.setPosition(pos + 1,QTextCursor::KeepAnchor);
        }
        if (cursor.selectedText() == "\t") {
            cursor.removeSelectedText();
        }
        return true;
    }
    return false;
}

void QMarkdownTextEdit::openLinkAtCursorPosition() {
    QTextCursor cursor = this->textCursor();
    int clickedPosition = cursor.position();
    // 选择单击块中的文本，找出我们点击的位置
    cursor.movePosition(QTextCursor::StartOfBlock);
    int positionFromStart = clickedPosition - cursor.position();
    cursor.movePosition(QTextCursor::EndOfBlock,QTextCursor::KeepAnchor);

    QString selectedText = cursor.selectedText();
    // 找出所选文字中的哪个url被点击
    QUrl url = getMarkdownUrlAtPosition(selectedText,positionFromStart);
    if (url.isValid()) {
        qDebug() << __func__ << " - 'emit urlClicke(url)': " << url;
        emit urlClicked(url);
        // 忽略一些模式
        if (!m_ignoredClickUrlSchemata.contains(url.scheme())) {
            openUrl(url);
        }
    }
}

QMap<QString, QString> QMarkdownTextEdit::parseMarkdownUrlsFromText(QString text) {
    QMap<QString,QString> urlMap;

    // 匹配urls [this url](http://mylink)
    QRegularExpression re("(\\[.*?\\]\\((.+?://.+?)\\))");
    QRegularExpressionMatchIterator i = re.globalMatch(text);
    while (i.hasNext()) {
        QRegularExpressionMatch match = i.next();
        QString linkText = match.captured(1);
        QString url = match.captured(2);
        urlMap[linkText] = url;
    }
    // 匹配urls <http://mylink>
    re = QRegularExpression("(<(.+?://.+?)>)");
    i = re.globalMatch(text);
    while (i.hasNext()) {
        QRegularExpressionMatch match = i.next();
        QString linkText = match.captured(1);
        QString url = match.captured(2);
        urlMap[linkText] = url;
    }
    return urlMap;
}
