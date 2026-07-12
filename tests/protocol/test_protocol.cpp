#include <QtTest/QTest>

#include "protocol/ProtocolTypes.h"

class ProtocolTest final : public QObject
{
    Q_OBJECT

private slots:
    void openFileRoundTrips();
    void rejectsUnknownMajorVersion();
    void ignoresUnknownFieldsFromNewerMinorVersion();
};

void ProtocolTest::openFileRoundTrips()
{
    const loupe::protocol::OpenFile command{42, R"(C:\fixtures\assembly.step)", std::nullopt};

    const QByteArray bytes = loupe::protocol::encode(command);
    const auto decoded = loupe::protocol::decodeCommand(bytes);

    QCOMPARE(std::get<loupe::protocol::OpenFile>(decoded).requestId, 42ULL);
}

void ProtocolTest::rejectsUnknownMajorVersion()
{
    QVERIFY_THROWS_EXCEPTION(
        loupe::protocol::ProtocolError,
        loupe::protocol::decodeEvent(QByteArrayLiteral("{\"version\":{\"major\":99,\"minor\":0},\"type\":\"ready\"}\n")));
}

void ProtocolTest::ignoresUnknownFieldsFromNewerMinorVersion()
{
    const auto event = loupe::protocol::decodeEvent(
        QByteArrayLiteral("{\"version\":{\"major\":1,\"minor\":99},\"type\":\"ready\",\"futureField\":\"ignored\"}\n"));

    QVERIFY(std::holds_alternative<loupe::protocol::Ready>(event));
}

QTEST_MAIN(ProtocolTest)

#include "test_protocol.moc"
