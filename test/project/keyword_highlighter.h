#ifndef KEYWORD_HIGHLIGHTER_H
#define KEYWORD_HIGHLIGHTER_H

#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QVector>
#include <QPair>

class QTextDocument;

class KeywordHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    explicit KeywordHighlighter(QTextDocument *parent = nullptr);

    void addKeyword(const QString &keyword, const QColor &color);
    void removeKeyword(const QString &keyword);
    void clearKeywords();
    QVector<QPair<QString, QColor>> keywords() const;
    void setKeywords(const QVector<QPair<QString, QColor>> &keywords);

protected:
    void highlightBlock(const QString &text) override;

private:
    struct HighlightRule {
        QString keyword;
        QTextCharFormat format;
    };
    QVector<HighlightRule> m_rules;
};

#endif // KEYWORD_HIGHLIGHTER_H
