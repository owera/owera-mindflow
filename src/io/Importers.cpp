#include "io/Importers.h"

#include "model/Node.h"

#include <QFile>
#include <QRegularExpression>
#include <QTextStream>
#include <QXmlStreamReader>

namespace mindflow {
namespace importers {

namespace {

// Generic level-indexed tree builder. Adds a node at `level` (0 = root) carrying
// `text`, returns the created node so the caller can set extra fields. byLevel
// tracks the current node at each depth; deeper entries are dropped on each add.
Node* addLeveled(int level, const QString& text,
                 std::vector<std::unique_ptr<Node>>& roots,
                 std::vector<Node*>& byLevel) {
    auto node = std::make_unique<Node>(text);
    Node* raw = node.get();
    if (level <= 0 || byLevel.empty() ||
        level - 1 >= static_cast<int>(byLevel.size()) || !byLevel[level - 1]) {
        roots.push_back(std::move(node)); // top-level / orphaned -> a new root
    } else {
        raw = byLevel[level - 1]->insertChild(std::move(node));
    }
    byLevel.resize(level + 1, nullptr); // also drops any deeper stale entries
    byLevel[level] = raw;
    return raw;
}

} // namespace

QString readTextFile(const QString& path, bool* ok) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (ok)
            *ok = false;
        return QString();
    }
    QTextStream ts(&file);
    if (ok)
        *ok = true;
    return ts.readAll();
}

std::vector<std::unique_ptr<Node>> fromOpml(const QString& text) {
    std::vector<std::unique_ptr<Node>> roots;
    std::vector<Node*> stack; // owning chain (raw pointers; roots own the tops)
    QXmlStreamReader xml(text);
    while (!xml.atEnd()) {
        const auto token = xml.readNext();
        if (token == QXmlStreamReader::StartElement &&
            xml.name() == QLatin1String("outline")) {
            const auto attrs = xml.attributes();
            QString t = attrs.value(QLatin1String("text")).toString();
            if (t.isEmpty())
                t = attrs.value(QLatin1String("title")).toString();
            auto node = std::make_unique<Node>(t);
            node->note = attrs.value(QLatin1String("_note")).toString();
            Node* raw = node.get();
            if (stack.empty())
                roots.push_back(std::move(node));
            else
                raw = stack.back()->insertChild(std::move(node));
            stack.push_back(raw);
        } else if (token == QXmlStreamReader::EndElement &&
                   xml.name() == QLatin1String("outline")) {
            if (!stack.empty())
                stack.pop_back();
        }
    }
    return roots;
}

std::vector<std::unique_ptr<Node>> fromFreeMind(const QString& text) {
    std::vector<std::unique_ptr<Node>> roots;
    std::vector<Node*> stack;
    QXmlStreamReader xml(text);
    while (!xml.atEnd()) {
        const auto token = xml.readNext();
        if (token == QXmlStreamReader::StartElement &&
            xml.name() == QLatin1String("node")) {
            const QString t = xml.attributes().value(QLatin1String("TEXT")).toString();
            auto node = std::make_unique<Node>(t);
            Node* raw = node.get();
            if (stack.empty())
                roots.push_back(std::move(node));
            else
                raw = stack.back()->insertChild(std::move(node));
            stack.push_back(raw);
        } else if (token == QXmlStreamReader::EndElement &&
                   xml.name() == QLatin1String("node")) {
            if (!stack.empty())
                stack.pop_back();
        }
    }
    return roots;
}

std::vector<std::unique_ptr<Node>> fromMarkdown(const QString& text) {
    std::vector<std::unique_ptr<Node>> roots;
    std::vector<Node*> byLevel;
    Node* last = nullptr;

    static const QRegularExpression headingRe(QStringLiteral("^(#{1,6})\\s+(.*)$"));
    static const QRegularExpression bulletRe(
        QStringLiteral("^([ \\t]*)[-*+]\\s+(.*)$"));
    static const QRegularExpression tagRe(QStringLiteral("\\s+#([^\\s#]+)"));

    const QStringList lines = text.split(QLatin1Char('\n'));
    for (const QString& raw : lines) {
        if (raw.trimmed().isEmpty())
            continue;

        auto h = headingRe.match(raw);
        if (h.hasMatch()) {
            last = addLeveled(0, h.captured(2).trimmed(), roots, byLevel);
            continue;
        }

        const QString trimmed = raw.trimmed();
        if (trimmed.startsWith(QLatin1String(">")) && last) {
            const QString note = trimmed.mid(1).trimmed();
            last->note = last->note.isEmpty() ? note
                                              : last->note + QLatin1Char('\n') + note;
            continue;
        }

        auto b = bulletRe.match(raw);
        if (!b.hasMatch())
            continue;
        const QString indent = b.captured(1);
        int level = 1;
        for (const QChar ch : indent)
            level += (ch == QLatin1Char('\t')) ? 1 : 0;
        level += indent.count(QLatin1Char(' ')) / 2;

        QString body = b.captured(2);
        bool isTask = false, done = false;
        if (body.startsWith(QLatin1String("[ ] "))) {
            isTask = true;
            body = body.mid(4);
        } else if (body.startsWith(QLatin1String("[x] ")) ||
                   body.startsWith(QLatin1String("[X] "))) {
            isTask = true;
            done = true;
            body = body.mid(4);
        }
        QStringList tags;
        auto it = tagRe.globalMatch(body);
        while (it.hasNext())
            tags << it.next().captured(1);
        body.remove(tagRe);

        Node* node = addLeveled(level, body.trimmed(), roots, byLevel);
        node->isTask = isTask;
        node->taskDone = done;
        node->tags = tags;
        last = node;
    }
    return roots;
}

std::vector<std::unique_ptr<Node>> fromPlainText(const QString& text) {
    std::vector<std::unique_ptr<Node>> roots;
    std::vector<Node*> byLevel;
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (const QString& raw : lines) {
        if (raw.trimmed().isEmpty())
            continue;
        int level = 0;
        for (const QChar ch : raw) {
            if (ch == QLatin1Char('\t'))
                ++level;
            else
                break;
        }
        // Allow 2-space indentation as a fallback when no tabs are used.
        if (level == 0) {
            int spaces = 0;
            for (const QChar ch : raw) {
                if (ch == QLatin1Char(' '))
                    ++spaces;
                else
                    break;
            }
            level = spaces / 2;
        }
        addLeveled(level, raw.trimmed(), roots, byLevel);
    }
    return roots;
}

} // namespace importers
} // namespace mindflow
