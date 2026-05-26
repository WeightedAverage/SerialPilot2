#include "keyword_highlighter.h"

KeywordHighlighter::KeywordHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
}

void KeywordHighlighter::addKeyword(const QString &keyword, const QColor &color)
{
    if (keyword.isEmpty()) return;

    HighlightRule rule;
    rule.keyword = keyword;
    rule.format.setForeground(color);
    rule.format.setFontWeight(QFont::Bold);
    m_rules.append(rule);
    rehighlight();
}

void KeywordHighlighter::removeKeyword(const QString &keyword)
{
    for (int i = m_rules.size() - 1; i >= 0; --i) {
        if (m_rules[i].keyword == keyword) {
            m_rules.removeAt(i);
        }
    }
    rehighlight();
}

void KeywordHighlighter::clearKeywords()
{
    m_rules.clear();
    rehighlight();
}

QVector<QPair<QString, QColor>> KeywordHighlighter::keywords() const
{
    QVector<QPair<QString, QColor>> result;
    for (const HighlightRule &rule : m_rules) {
        result.append(qMakePair(rule.keyword, rule.format.foreground().color()));
    }
    return result;
}

void KeywordHighlighter::setKeywords(const QVector<QPair<QString, QColor>> &keywords)
{
    m_rules.clear();
    for (const auto &pair : keywords) {
        HighlightRule rule;
        rule.keyword = pair.first;
        rule.format.setForeground(pair.second);
        rule.format.setFontWeight(QFont::Bold);
        m_rules.append(rule);
    }
    rehighlight();
}

void KeywordHighlighter::highlightBlock(const QString &text)
{
    for (const HighlightRule &rule : m_rules) {
        int index = 0;
        while ((index = text.indexOf(rule.keyword, index, Qt::CaseInsensitive)) != -1) {
            setFormat(index, rule.keyword.length(), rule.format);
            index += rule.keyword.length();
        }
    }
}
