#include <QtTest/QTest>

#include "protocol/ProtocolFrame.h"
#include "protocol/ProtocolTypes.h"

class ProtocolFrameTest final : public QObject
{
    Q_OBJECT

private slots:
    void decodesPartialAndCombinedFrames();
    void rejectsUnknownFrameType();
    void rejectsOversizedFrame();
};

void ProtocolFrameTest::decodesPartialAndCombinedFrames()
{
    const auto first = loupe::protocol::encodeFrame(loupe::protocol::FrameType::ControlJson, QByteArrayLiteral("one"));
    const auto second = loupe::protocol::encodeFrame(loupe::protocol::FrameType::Geometry, QByteArrayLiteral("two"));
    loupe::protocol::FrameDecoder decoder;
    decoder.append(first.left(2));
    QVERIFY(!decoder.take().has_value());
    decoder.append(first.mid(2) + second);
    const auto decodedFirst = decoder.take();
    QVERIFY(decodedFirst.has_value());
    QCOMPARE(decodedFirst->type, loupe::protocol::FrameType::ControlJson);
    QCOMPARE(decodedFirst->payload, QByteArrayLiteral("one"));
    const auto decodedSecond = decoder.take();
    QVERIFY(decodedSecond.has_value());
    QCOMPARE(decodedSecond->type, loupe::protocol::FrameType::Geometry);
    QCOMPARE(decodedSecond->payload, QByteArrayLiteral("two"));
    QVERIFY(!decoder.take().has_value());
}

void ProtocolFrameTest::rejectsUnknownFrameType()
{
    loupe::protocol::FrameDecoder decoder;
    decoder.append(QByteArray::fromHex("00000003097879"));
    QVERIFY_THROWS_EXCEPTION(loupe::protocol::ProtocolError, static_cast<void>(decoder.take()));
}

void ProtocolFrameTest::rejectsOversizedFrame()
{
    loupe::protocol::FrameDecoder decoder;
    decoder.append(QByteArray::fromHex("0400000201"));
    QVERIFY_THROWS_EXCEPTION(loupe::protocol::ProtocolError, static_cast<void>(decoder.take()));
}

QTEST_MAIN(ProtocolFrameTest)

#include "test_protocol_frame.moc"
