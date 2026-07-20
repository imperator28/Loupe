#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantList>

#include <QHash>
#include <QSet>
#include <QVector>

#include <cstdint>

namespace loupe::app::exporting {

class ExportWorkspaceController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList components READ components NOTIFY componentsChanged)
    Q_PROPERTY(int checkedCount READ checkedCount NOTIFY selectionChanged)
    Q_PROPERTY(int selectionRevision READ selectionRevision NOTIFY selectionChanged)
    Q_PROPERTY(QString focusedNodeId READ focusedNodeId WRITE setFocusedNodeId NOTIFY previewChanged)
    Q_PROPERTY(QString hoveredNodeId READ hoveredNodeId WRITE setHoveredNodeId NOTIFY previewChanged)
    Q_PROPERTY(QString previewNodeId READ previewNodeId NOTIFY previewChanged)
    Q_PROPERTY(QVariantList bucketRows READ bucketRows NOTIFY planChanged)
    Q_PROPERTY(QString destination READ destination WRITE setDestination NOTIFY settingsChanged)
    Q_PROPERTY(QString format READ format WRITE setFormat NOTIFY settingsChanged)
    Q_PROPERTY(QString coordinates READ coordinates WRITE setCoordinates NOTIFY settingsChanged)
    Q_PROPERTY(QString grouping READ grouping WRITE setGrouping NOTIFY settingsChanged)
    Q_PROPERTY(QString namingStrategy READ namingStrategy WRITE setNamingStrategy NOTIFY settingsChanged)
    Q_PROPERTY(QString namingValue READ namingValue WRITE setNamingValue NOTIFY settingsChanged)
    Q_PROPERTY(bool canExport READ canExport NOTIFY planChanged)
    Q_PROPERTY(QVariantList planRows READ planRows NOTIFY planChanged)
    Q_PROPERTY(QString planFingerprint READ planFingerprint NOTIFY planChanged)
    Q_PROPERTY(QString planError READ planError NOTIFY planChanged)
    Q_PROPERTY(bool documentReady READ documentReady NOTIFY exportStateChanged)
    Q_PROPERTY(bool exporting READ exporting NOTIFY exportStateChanged)
    Q_PROPERTY(double exportProgress READ exportProgress NOTIFY exportStateChanged)
    Q_PROPERTY(QString exportStage READ exportStage NOTIFY exportStateChanged)
    Q_PROPERTY(QString exportSummary READ exportSummary NOTIFY exportStateChanged)
    Q_PROPERTY(bool exportSucceeded READ exportSucceeded NOTIFY exportStateChanged)

public:
    explicit ExportWorkspaceController(QObject* parent = nullptr);

    [[nodiscard]] QVariantList components() const;
    [[nodiscard]] int checkedCount() const noexcept { return static_cast<int>(checkedNodeIds_.size()); }
    [[nodiscard]] int selectionRevision() const noexcept { return selectionRevision_; }
    [[nodiscard]] const QString& focusedNodeId() const noexcept { return focusedNodeId_; }
    [[nodiscard]] const QString& hoveredNodeId() const noexcept { return hoveredNodeId_; }
    [[nodiscard]] QString previewNodeId() const noexcept { return hoveredNodeId_.isEmpty() ? focusedNodeId_ : hoveredNodeId_; }
    [[nodiscard]] const QVariantList& bucketRows() const noexcept { return planRows_; }
    [[nodiscard]] const QString& destination() const noexcept { return destination_; }
    [[nodiscard]] const QString& format() const noexcept { return format_; }
    [[nodiscard]] const QString& coordinates() const noexcept { return coordinates_; }
    [[nodiscard]] const QString& grouping() const noexcept { return grouping_; }
    [[nodiscard]] const QString& namingStrategy() const noexcept { return namingStrategy_; }
    [[nodiscard]] const QString& namingValue() const noexcept { return namingValue_; }
    [[nodiscard]] bool canExport() const noexcept;
    [[nodiscard]] const QVariantList& planRows() const noexcept { return planRows_; }
    [[nodiscard]] const QString& planFingerprint() const noexcept { return planFingerprint_; }
    [[nodiscard]] const QString& planError() const noexcept { return planError_; }
    [[nodiscard]] bool documentReady() const noexcept { return documentReady_; }
    [[nodiscard]] bool exporting() const noexcept { return exporting_; }
    [[nodiscard]] double exportProgress() const noexcept { return exportProgress_; }
    [[nodiscard]] const QString& exportStage() const noexcept { return exportStage_; }
    [[nodiscard]] const QString& exportSummary() const noexcept { return exportSummary_; }
    [[nodiscard]] bool exportSucceeded() const noexcept { return exportSucceeded_; }

    void replaceSnapshot(const QString& snapshotJson);
    void reset();
    void setDocumentReady(bool ready);
    void setExportRequestId(std::uint64_t requestId);
    void handleExportProgress(int rowIndex, int rowCount, const QString& stage, double fraction);
    void handleExportRowResult(int rowIndex, const QString& nodeId, const QString& path,
                               bool passed, const QString& message);
    void handleExportCompleted(int succeededCount, int failedCount);
    void handleExportFailed(const QString& message);
    void handleExportCanceled();

    Q_INVOKABLE void setChecked(const QString& nodeId, bool checked);
    Q_INVOKABLE bool isChecked(const QString& nodeId) const;
    Q_INVOKABLE bool containsNode(const QString& selectionId, const QString& nodeId) const;
    Q_INVOKABLE void setFocusedNodeId(const QString& nodeId);
    Q_INVOKABLE void setHoveredNodeId(const QString& nodeId);
    Q_INVOKABLE void setDestination(const QString& destination);
    Q_INVOKABLE void setDestinationUrl(const QUrl& destinationUrl);
    Q_INVOKABLE void setFormat(const QString& format);
    Q_INVOKABLE void setCoordinates(const QString& coordinates);
    Q_INVOKABLE void setGrouping(const QString& grouping);
    Q_INVOKABLE void setNamingStrategy(const QString& strategy);
    Q_INVOKABLE void setNamingValue(const QString& value);
    Q_INVOKABLE void moveBucketItem(int from, int to);
    Q_INVOKABLE void moveBucketItemById(const QString& nodeId, int to);
    Q_INVOKABLE void setFilenameOverride(const QString& nodeId, const QString& filename);
    Q_INVOKABLE QString focusSceneNode(const QString& nodeId);
    Q_INVOKABLE bool rebuildPlan();
    Q_INVOKABLE bool exportReviewedPlan();
    Q_INVOKABLE void cancelExport();

signals:
    void componentsChanged();
    void selectionChanged();
    void previewChanged();
    void settingsChanged();
    void planChanged();
    void exportStateChanged();
    void executeRequested(const QByteArray& planJson, const QString& fingerprint);
    void cancelRequested(quint64 requestId);

private:
    struct Component final {
        QString id;
        QString parentId;
        QString name;
        QString hierarchyPath;
        int kind{};
        int depth{};
        bool exportable{};
        bool visibleInPicker{true};
    };

    [[nodiscard]] QString hierarchyPathFor(const QString& nodeId, QSet<QString>& resolving) const;
    [[nodiscard]] QString generatedLeafName(const QString& nodeId, int bucketIndex) const;
    [[nodiscard]] QString pickerNodeForSceneNode(const QString& nodeId) const;
    void refreshPlan();
    void clearPlan();
    [[nodiscard]] QByteArray reviewedPlanJson() const;
    void clearExportResult();

    QVector<Component> components_;
    QHash<QString, int> componentIndexById_;
    QVector<QString> checkedNodeIds_;
    int selectionRevision_{};
    QHash<QString, QString> filenameOverrides_;
    QString focusedNodeId_;
    QString hoveredNodeId_;
    QString destination_;
    QString format_{QStringLiteral("STEP")};
    QString coordinates_{QStringLiteral("Assembly")};
    QString grouping_{QStringLiteral("Separate files")};
    QString namingStrategy_{QStringLiteral("keep")};
    QString namingValue_;
    QString effectiveUnit_{QStringLiteral("mm")};
    double sourceToMillimeters_{1.0};
    QVariantList planRows_;
    QString planFingerprint_;
    QString planError_;
    bool documentReady_{};
    bool exporting_{};
    double exportProgress_{};
    QString exportStage_;
    QString exportSummary_;
    bool exportSucceeded_{};
    std::uint64_t exportRequestId_{};
};

} // namespace loupe::app::exporting
