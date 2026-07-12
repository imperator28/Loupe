#include "app/render/DefinitionInstances.h"

namespace loupe::app::render {

DefinitionInstances::DefinitionInstances(QQuick3DObject* parent)
    : QQuick3DInstancing(parent)
{
}

void DefinitionInstances::replaceOccurrences(const QStringList& occurrenceIds)
{
    occurrenceIds_ = occurrenceIds;
    markDirty();
}

QByteArray DefinitionInstances::getInstanceBuffer(int* instanceCount)
{
    *instanceCount = occurrenceIds_.size();
    QByteArray buffer;
    buffer.reserve(*instanceCount * static_cast<int>(sizeof(InstanceTableEntry)));
    for (int index = 0; index < *instanceCount; ++index) {
        const auto entry = calculateTableEntry({}, {1.0F, 1.0F, 1.0F}, {}, Qt::white, {static_cast<float>(index), 0.0F, 0.0F, 0.0F});
        buffer.append(reinterpret_cast<const char*>(&entry), static_cast<int>(sizeof(entry)));
    }
    return buffer;
}

} // namespace loupe::app::render
