#pragma once

#include <QString>

#include <memory>
#include <vector>

namespace mindflow {

class Node;

// Parse external formats into a forest of nodes (empty on failure). The caller
// hands the result to Document::setImportedRoots.
namespace importers {

std::vector<std::unique_ptr<Node>> fromOpml(const QString& text);
std::vector<std::unique_ptr<Node>> fromFreeMind(const QString& text);
std::vector<std::unique_ptr<Node>> fromMarkdown(const QString& text);
std::vector<std::unique_ptr<Node>> fromPlainText(const QString& text);

// Read a UTF-8 text file; sets *ok.
QString readTextFile(const QString& path, bool* ok);

} // namespace importers
} // namespace mindflow
