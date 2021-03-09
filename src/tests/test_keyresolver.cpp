/*
    test_keyresolver.cpp

    This file is part of libkleopatra's test suite.
    SPDX-FileCopyrightText: 2018 Intevation GmbH

    SPDX-License-Identifier: GPL-2.0-only
*/


#include "kleo/keyresolver.h"

#include <QCommandLineParser>
#include <QApplication>
#include <QDebug>
#include <QTimer>

#include <gpgme++/key.h>

using namespace Kleo;

void dumpKeys(const QMap <CryptoMessageFormat, QMap<QString, std::vector<GpgME::Key> > > &fmtMap)
{
    for (const CryptoMessageFormat fmt: fmtMap.keys()) {
        qDebug () << "Format:" << cryptoMessageFormatToLabel(fmt) << fmt;
        for (const auto &mbox: fmtMap[fmt].keys()) {
            qDebug() << "Address:" << mbox;
            qDebug() << "Keys:";
            for (const auto &key: fmtMap[fmt][mbox]) {
                qDebug () << key.primaryFingerprint();
            }
        }
    }
}

void dumpSigKeys(const QMap <CryptoMessageFormat, std::vector<GpgME::Key> > &fmtMap)
{
    for (const CryptoMessageFormat fmt: fmtMap.keys()) {
        qDebug () << "Format:" << cryptoMessageFormatToLabel(fmt) << fmt;
        qDebug() << "Keys:";
        for (const auto &key: fmtMap[fmt]) {
            qDebug () << key.primaryFingerprint();
        }
    }
}

class SignalRecipient: public QObject
{
    Q_OBJECT
public:
    SignalRecipient(KeyResolver *res) : resolver(res) {}

    void keysResolved(bool success, bool sendUnencrypted)
    {
        if (!success) {
            qDebug() << "Canceled";
            exit(1);
        }
        qDebug() << "Resolved Signing keys:";
        dumpSigKeys (resolver->signingKeys());
        qDebug() << "Resolved Encryption keys:";
        dumpKeys (resolver->encryptionKeys());
        qDebug() << "Resolved Hidden keys:";
        dumpKeys (resolver->hiddenKeys());
        qDebug() << "Send Unencrypted:" << sendUnencrypted;
        exit(0);
    }
private:
    KeyResolver *resolver;
};

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Test KeyResolver class"));
    parser.addHelpOption();
    parser.addPositionalArgument(QStringLiteral("recipients"),
                                  QStringLiteral("Recipients to resolve"),
                                  QStringLiteral("[mailboxes]"));
    parser.addOption(QCommandLineOption(QStringList() << QStringLiteral("overrides")
                                        << QStringLiteral("o"),
                                        QStringLiteral("Override where format can be:\n"
                                                       "InlineOpenPGP\n"
                                                       "OpenPGPMIME\n"
                                                       "SMIME\n"
                                                       "SMIMEOpaque\n"
                                                       "AnyOpenPGP\n"
                                                       "AnySMIME\n"
                                                       "Auto"),
                                        QStringLiteral("mailbox:fpr,fpr,..:format")));
    parser.addOption(QCommandLineOption(QStringList() << QStringLiteral("sender")
                                        << QStringLiteral("s"),
                                        QStringLiteral("Mailbox of the sender"),
                                        QStringLiteral("mailbox")));
    parser.addOption(QCommandLineOption(QStringList() << QStringLiteral("hidden")
                                        << QStringLiteral("h"),
                                        QStringLiteral("hidden recipients"),
                                        QStringLiteral("A hidden / bcc recipient")));
    parser.addOption(QCommandLineOption(QStringList() << QStringLiteral("sigkeys")
                                        << QStringLiteral("k"),
                                        QStringLiteral("signing key"),
                                        QStringLiteral("Explicit signing keys")));
    parser.addOption(QCommandLineOption(QStringList() << QStringLiteral("encrypt")
                                        << QStringLiteral("e"),
                                        QStringLiteral("Only select encryption keys")));
    parser.addOption(QCommandLineOption(QStringList() << QStringLiteral("approval")
                                        << QStringLiteral("a"),
                                        QStringLiteral("Always show approval dlg")));

    parser.process(app);

    const QStringList recps = parser.positionalArguments();
    if (recps.size() < 1) {
        parser.showHelp(1);
    }

    KeyResolver resolver(true, !parser.isSet(QStringLiteral("encrypt")));
    resolver.setRecipients(recps);
    resolver.setSender(parser.value(QStringLiteral("sender")));

    QMap <CryptoMessageFormat, QMap <QString, QStringList> > overrides;

    for (const QString &oride: parser.values(QStringLiteral("overrides"))) {
        const QStringList split = oride.split(QLatin1Char(':'));
        CryptoMessageFormat fmt = AutoFormat;
        if (split.size() < 2 || split.size() > 3) {
            parser.showHelp(1);
        }

        if (split.size() == 3) {
            const QString fmtStr = split[2].toLower();
            if (fmtStr == QLatin1String("inlineopenpgp")) {
                fmt = InlineOpenPGPFormat;
            } else if (fmtStr == QLatin1String("openpgpmime")) {
                fmt = OpenPGPMIMEFormat;
            } else if (fmtStr == QLatin1String("smime")) {
                fmt = SMIMEFormat;
            } else if (fmtStr == QLatin1String("smimeopaque")) {
                fmt = SMIMEOpaqueFormat;
            } else if (fmtStr == QLatin1String("anyopenpgp")) {
                fmt = AnyOpenPGP;
            } else if (fmtStr == QLatin1String("anysmime")) {
                fmt = AnySMIME;
            } else if (fmtStr == QLatin1String("auto")) {
                fmt = AutoFormat;
            } else {
                parser.showHelp(1);
            }
        }
        const QStringList fingerprints = split[1].split(QLatin1Char(','));

        auto map = overrides.value(fmt);
        map.insert(split[0], fingerprints);
        overrides.insert(fmt, map);
    }
    resolver.setOverrideKeys(overrides);

    auto recp = new SignalRecipient(&resolver);
    QObject::connect (&resolver, &KeyResolver::keysResolved, recp, &SignalRecipient::keysResolved);

    QTimer::singleShot(1000, [&parser, &resolver]() {
        resolver.start(parser.isSet(QStringLiteral("approval")));
    });

    app.exec();
    return 0;
}

#include "test_keyresolver.moc"
