#include "io/DocumentStore.h"

#include "model/Document.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSaveFile>

namespace mindflow {

bool DocumentStore::save(const Document& doc, const QString& path, QString* error) {
    const QJsonDocument json(doc.toJson());

    // QSaveFile writes atomically: the original file is untouched until commit().
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    file.write(json.toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    return true;
}

bool DocumentStore::load(Document& doc, const QString& path, QString* error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    const QByteArray bytes = file.readAll();

    QJsonParseError parseError{};
    const QJsonDocument json = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !json.isObject()) {
        if (error)
            *error = parseError.errorString();
        return false;
    }

    doc.loadFromJson(json.object());
    return true;
}

} // namespace mindflow
