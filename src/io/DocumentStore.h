#pragma once

#include <QString>

namespace mindflow {

class Document;

// Reads and writes the `.mindflow` document. M1 persists a single JSON object
// (schema versioned via the "version" field). When images land (M4) this becomes
// a zip container holding contents.json + assets/ without changing the JSON schema.
class DocumentStore {
public:
    // Returns true on success; on failure sets *error (if provided).
    static bool save(const Document& doc, const QString& path, QString* error = nullptr);
    static bool load(Document& doc, const QString& path, QString* error = nullptr);
};

} // namespace mindflow
