#include "app/cache/SourceFingerprint.h"

#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>

namespace loupe::app::cache {

std::optional<SourceIdentity> SourceFingerprint::fromFile(const QString& path)
{
    const QFileInfo info(path);
    if (!info.isFile()) return std::nullopt;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return std::nullopt;
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd()) {
        const auto bytes = file.read(1 << 20);
        if (bytes.isEmpty() && file.error() != QFileDevice::NoError) return std::nullopt;
        hash.addData(bytes);
    }
    return SourceIdentity{QString::fromLatin1(hash.result().toHex()), info.size(), info.lastModified().toMSecsSinceEpoch()};
}

} // namespace loupe::app::cache
