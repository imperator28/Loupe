#pragma once

#include <QQuick3DInstancing>
#include <QStringList>

namespace loupe::app::render {

class DefinitionInstances final : public QQuick3DInstancing {
    Q_OBJECT
public:
    explicit DefinitionInstances(QQuick3DObject* parent = nullptr);
    void replaceOccurrences(const QStringList& occurrenceIds);
    [[nodiscard]] const QStringList& occurrenceIds() const noexcept { return occurrenceIds_; }

protected:
    QByteArray getInstanceBuffer(int* instanceCount) override;

private:
    QStringList occurrenceIds_;
};

} // namespace loupe::app::render
